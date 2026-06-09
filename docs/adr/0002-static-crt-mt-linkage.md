# ADR-0002: Static CRT (/MT) for zero-dependency distribution

**Status:** Accepted
**Date:** 2026-06-09

## Context

IndexCard is a free utility intended for non-technical Windows users. The original build used `/MD` (dynamic MSVC runtime linkage), which requires `vcruntime140.dll`, `msvcp140.dll`, and related DLLs to be present on the target machine. These DLLs are absent on a freshly imaged Windows 10 machine without Visual Studio or another MSVC-linked app installed. A user who downloaded IndexCard without those DLLs would see a "DLL not found" error with no clear remedy.

## Decision

Add `set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")` to `CMakeLists.txt`. This switches all configurations to `/MT` (Release) and `/MTd` (Debug). Verified post-build with `dumpbin /imports`: zero `vcruntime`, `msvcp`, or `ucrtbase` imports in the release binary.

## Consequences

- Release binary grows from ~150 KB to 385 KB. Acceptable for a desktop utility; the Inno Setup installer compresses this to ~200 KB internally.
- The exe runs on any Windows 10+ machine with no prerequisites.
- `/MT` and `/MD` must not be mixed across translation units or libraries. IndexCard has no external C++ library dependencies, so this constraint is trivially satisfied.
- Future library dependencies, if any, must also use `/MT` linkage. This will be a constraint to revisit if a third-party library is ever added.
