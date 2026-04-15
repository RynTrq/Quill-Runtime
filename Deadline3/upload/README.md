# Deadline3 Active Runtime

This directory contains the active Quill Runtime implementation: a portable C++11 work-stealing scheduler, benchmark programs, and regression tests.

## Commands

| Command | Description |
| --- | --- |
| `make` | Build the runtime object, shared library, N-Queens benchmark, iterative averaging benchmark, and smoke test. |
| `make test` | Run the runtime smoke suite with `QUILL_WORKERS=4`. |
| `make clean` | Remove generated objects, libraries, and binaries. |

## Files

| File | Description |
| --- | --- |
| `quill.h` | Public API for runtime lifecycle, finish/async, `parallel_for`, and portable allocation helpers. |
| `quill-runtime.cpp` | Scheduler implementation with local deques, stealing, finish tracking, and task exception propagation. |
| `nqueens.cpp` | Recursive benchmark with known-answer verification. |
| `iterative_averaging.cpp` | Parallel loop benchmark that accepts optional size and iteration arguments. |
| `tests/runtime_smoke.cpp` | Focused regression checks for core runtime behavior. |

## Examples

```sh
make clean && make
QUILL_WORKERS=4 ./runtime_smoke
QUILL_WORKERS=4 ./nqueens 8
QUILL_WORKERS=4 ./iterative_averaging 4096 4
```

## Runtime Contract

Call `quill::init_runtime()` before scheduling work or let the first runtime API initialize it lazily. Wrap task creation in `quill::start_finish()` and `quill::end_finish()` when the caller needs all spawned work to complete before continuing. Call `quill::finalize_runtime()` once at shutdown.

`quill::end_finish()` also rethrows the first exception captured from any scheduled task, so benchmark and application code can fail fast instead of silently dropping task failures.
