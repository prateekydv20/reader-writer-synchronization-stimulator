
# Readers–Writers Synchronization Simulator

This project demonstrates the readers–writers synchronization problem in C using POSIX threads, a mutex, and a semaphore. Readers may share access to a common integer; writers update it with exclusive access.

## Build and run

Open a terminal in this folder on Linux or WSL with GCC installed:

    gcc -std=c11 -Wall -Wextra -pedantic -pthread readers_writers.c -o readers_writers
    ./readers_writers
    ./readers_writers --test
    ./readers_writers 4 2

The default run starts three readers and two writers. A custom run takes reader count followed by writer count. Each count can be from 0 to 64; the total must be positive. The test option runs six predefined cases.

The order of reader and writer messages varies with thread scheduling. The final shared value should equal the number of completed writer threads.

## How it works

`reader_count` is changed while holding `reader_mutex`. The first reader waits on the `resource` semaphore; later readers can join concurrently. The last reader posts the semaphore. A writer waits on the same semaphore, increments `shared_data`, and posts it when finished. This is the reader-priority solution for the project.

## Files

- `readers_writers.c` — C implementation and built-in tests.
- `TEST_CASES.md` — six scenarios, expectations, and replay commands.
- `results.txt` — supplied test log and outcome summary.
