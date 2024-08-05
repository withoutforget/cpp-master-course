## Unbounded lock-free stack

## Params

**THREAD_COUNT** — count threads for consuming and producing\
**ITER_COUNT** — iter count for all threads combined\

Works only since C++20, because of specialization `std::atomic<std::shared_ptr<T>>;` which solves ABA & reclamation problem.