#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_THREADS 64

typedef struct {
    int id;
    int is_writer;
} ThreadArg;

static int reader_count = 0;
static int shared_data = 0;
static int completed_readers = 0;
static int completed_writers = 0;
static pthread_mutex_t reader_mutex;
static pthread_mutex_t output_mutex;
static sem_t resource;
static int quiet_mode = 0;
static double total_writer_wait_ms = 0.0;
static int writer_wait_samples = 0;

static void short_pause(void) {
    struct timespec delay = {0, 2000000};
    nanosleep(&delay, NULL);
}

static double elapsed_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

static void *reader(void *opaque) {
    ThreadArg *arg = (ThreadArg *)opaque;

    pthread_mutex_lock(&reader_mutex);
    reader_count++;
    if (reader_count == 1)
        sem_wait(&resource);
    pthread_mutex_unlock(&reader_mutex);

    pthread_mutex_lock(&output_mutex);
    completed_readers++;
    if (!quiet_mode)
        printf("Reader %d is reading data = %d\n", arg->id, shared_data);
    pthread_mutex_unlock(&output_mutex);
    short_pause();

    pthread_mutex_lock(&reader_mutex);
    reader_count--;
    if (reader_count == 0)
        sem_post(&resource);
    pthread_mutex_unlock(&reader_mutex);
    return NULL;
}

static void *writer(void *opaque) {
    ThreadArg *arg = (ThreadArg *)opaque;
    struct timespec wait_start, wait_end;

    clock_gettime(CLOCK_MONOTONIC, &wait_start);
    sem_wait(&resource);
    clock_gettime(CLOCK_MONOTONIC, &wait_end);
    double wait_ms = elapsed_ms(wait_start, wait_end);

    pthread_mutex_lock(&output_mutex);
    total_writer_wait_ms += wait_ms;
    writer_wait_samples++;
    shared_data++;
    completed_writers++;
    if (!quiet_mode)
        printf("Writer %d is writing data = %d\n", arg->id, shared_data);
    pthread_mutex_unlock(&output_mutex);
    short_pause();

    sem_post(&resource);
    return NULL;
}

static int run_case(int readers, int writers, int show_output) {
    pthread_t reader_threads[MAX_THREADS];
    pthread_t writer_threads[MAX_THREADS];
    ThreadArg reader_args[MAX_THREADS];
    ThreadArg writer_args[MAX_THREADS];
    int created_readers = 0;
    int created_writers = 0;
    int failed = 0;
    struct timespec run_start, run_end;
    double run_ms = 0.0;

    reader_count = 0;
    shared_data = 0;
    completed_readers = 0;
    completed_writers = 0;
    total_writer_wait_ms = 0.0;
    writer_wait_samples = 0;
    quiet_mode = !show_output;
    pthread_mutex_init(&reader_mutex, NULL);
    pthread_mutex_init(&output_mutex, NULL);
    if (sem_init(&resource, 0, 1) != 0) {
        perror("sem_init");
        pthread_mutex_destroy(&reader_mutex);
        pthread_mutex_destroy(&output_mutex);
        return 0;
    }

    clock_gettime(CLOCK_MONOTONIC, &run_start);

    /* Interleave creation so readers and writers contend for the resource. */
    for (int i = 0; i < readers || i < writers; i++) {
        if (i < readers) {
            reader_args[i].id = i + 1;
            reader_args[i].is_writer = 0;
            if (pthread_create(&reader_threads[i], NULL, reader, &reader_args[i]) != 0) {
                failed = 1;
                break;
            }
            created_readers++;
        }
        if (i < writers) {
            writer_args[i].id = i + 1;
            writer_args[i].is_writer = 1;
            if (pthread_create(&writer_threads[i], NULL, writer, &writer_args[i]) != 0) {
                failed = 1;
                break;
            }
            created_writers++;
        }
    }

    for (int i = 0; i < created_readers; i++)
        pthread_join(reader_threads[i], NULL);
    for (int i = 0; i < created_writers; i++)
        pthread_join(writer_threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &run_end);
    run_ms = elapsed_ms(run_start, run_end);

    if (failed || created_readers != readers || created_writers != writers ||
        completed_readers != readers || completed_writers != writers ||
        shared_data != writers || reader_count != 0)
        failed = 1;

    if (show_output) {
        double average_writer_wait_ms = writer_wait_samples > 0
            ? total_writer_wait_ms / writer_wait_samples
            : 0.0;
        printf("Summary: readers=%d writers=%d final_data=%d "
               "average_writer_wait_ms=%.3f total_runtime_ms=%.3f\n",
               completed_readers, completed_writers, shared_data,
               average_writer_wait_ms, run_ms);
    }

    sem_destroy(&resource);
    pthread_mutex_destroy(&reader_mutex);
    pthread_mutex_destroy(&output_mutex);
    return !failed;
}

static int run_tests(void) {
    const struct { const char *name; int readers; int writers; } cases[] = {
        {"Readers only", 4, 0},
        {"Writers only", 0, 3},
        {"Mixed access", 3, 2},
        {"More readers", 8, 2},
        {"More writers", 2, 8},
        {"64-thread stress workload", 32, 32}
    };
    int failures = 0;
    int case_count = (int)(sizeof(cases) / sizeof(cases[0]));

    for (int i = 0; i < case_count; i++) {
        int pass = run_case(cases[i].readers, cases[i].writers, 0);
        printf("%s %s (readers=%d writers=%d expected_final=%d observed_final=%d)\n",
               pass ? "PASS" : "FAIL", cases[i].name,
               cases[i].readers, cases[i].writers, cases[i].writers, shared_data);
        if (!pass)
            failures++;
    }
    printf("%d failed checks\n", failures);
    return failures == 0 ? 0 : 1;
}

static int parse_count(const char *text, int *value) {
    char *end = NULL;
    long parsed;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno || end == text || *end != '\0' || parsed < 0 || parsed > MAX_THREADS)
        return 0;
    *value = (int)parsed;
    return 1;
}

int main(int argc, char **argv) {
    int readers = 3;
    int writers = 2;

    if (argc == 2 && strcmp(argv[1], "--test") == 0)
        return run_tests();
    if (argc == 3) {
        if (!parse_count(argv[1], &readers) || !parse_count(argv[2], &writers) ||
            readers + writers == 0) {
            fprintf(stderr, "Usage: %s [READERS WRITERS] | --test\n", argv[0]);
            fprintf(stderr, "Each count must be from 0 to %d; total must be positive.\n", MAX_THREADS);
            return 1;
        }
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [READERS WRITERS] | --test\n", argv[0]);
        return 1;
    }

    if (!run_case(readers, writers, 1)) {
        fprintf(stderr, "Simulation could not complete correctly.\n");
        return 1;
    }
    return 0;
}


