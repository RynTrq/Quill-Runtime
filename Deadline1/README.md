# Deadline1 Milestone

This directory preserves the first Quill Runtime milestone: the baseline work-stealing prototype used as the project's starting point.

## Contents

| File | Purpose |
| --- | --- |
| `quill-runtime.cpp` | Baseline runtime implementation. |
| `nqueens.cpp` | Benchmark used to validate recursive task execution. |
| `quill.h`, `quill-runtime.h` | Public and internal runtime headers for this milestone. |
| `Makefile` | Local build rules for this milestone. |

## Build

```sh
make clean
make
```

## Notes

- This is a historical snapshot kept for comparison with later milestones.
- The recommended implementation for current development is in `../Deadline3/upload`.
