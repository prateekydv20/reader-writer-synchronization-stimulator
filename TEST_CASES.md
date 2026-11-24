# Test Cases

Each test starts fresh. For every successful run, each thread should complete once, `reader_count` should return to zero, and final shared data should equal the number of writers. `--test` runs all six cases automatically and returns a nonzero status if a check fails.

Build first as described in `README.md`, then run:

```sh
./readers_writers --test
```

## 1. Readers only

Readers **4** · Writers **0**

Checks that readers can complete when no writer is present. Expected final data: **0**.

Replay: `./readers_writers 4 0`

## 2. Writers only

Readers **0** · Writers **3**

Checks that writers serialize their updates. Expected final data: **3**.

Replay: `./readers_writers 0 3`

## 3. Mixed access

Readers **3** · Writers **2**

The default-sized case. Checks that all five threads finish and the two writes are reflected in the shared value. Expected final data: **2**.

Replay: `./readers_writers 3 2`

## 4. More readers

Readers **8** · Writers **2**

Checks a reader-heavy workload. Expected final data: **2**.

Replay: `./readers_writers 8 2`

## 5. More writers

Readers **2** · Writers **8**

Checks a writer-heavy workload. Expected final data: **8**.

Replay: `./readers_writers 2 8`

## 6. Maximum supported threads

Readers **32** · Writers **32**

Checks the configured limit of 64 total threads and that all 32 increments are retained. Expected final data: **32**.

Replay: `./readers_writers 32 32`

## What these tests establish

The cases check completion counts and the final value, which can reveal lost updates or threads that fail to complete. They do not prove fairness or rule out starvation under an endless workload. This implementation gives readers priority, so a waiting writer can be delayed if readers keep arriving.
