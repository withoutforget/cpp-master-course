## Thread safe mutex queue

## Params

**THREAD_COUNT** — count threads for consuming and producing
**ITER_COUNT** — iter count for all threads combined
**QUEUE_WAIT** — using `ts_queue<T>::wait_and_pop` or `ts_queue<T>::try_pop`