# Deadline2 Milestone

This directory preserves the second project milestone for Quill Runtime. It represents the intermediate scheduler design explored before the final production-focused implementation in `Deadline3/upload`.

## Contents

| File | Purpose |
| --- | --- |
| `quill-runtime.cpp` | Main Deadline2 runtime implementation. |
| `nqueens.cpp` | Benchmark used to exercise recursive task scheduling. |
| `quill.h`, `quill-runtime.h` | Public and internal runtime headers for this milestone. |
| `copy.cpp`, `quill-runtime2.cpp`, `quill3.cpp` | Archived experimental variants retained for reference. |
| `Makefile` | Local build rules for this milestone. |

## Build

```sh
make clean
make
```

## Notes

- This folder is intentionally kept as a historical milestone rather than the recommended runtime for new work.
- The maintained, portable implementation is in `../Deadline3/upload`.
