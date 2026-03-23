# Build Rules

## Build Commands

| Command      | Script                                                               |
| ------------ | -------------------------------------------------------------------- |
| Full build   | `_build.bat`                                                         |
| Build only   | `_build.bat --no-test`                                               |
| Test only    | `_build.bat --test-only`                                             |
| Configure    | `_build.bat --configure`                                             |
| Lint         | `clang-tidy -p build src/**/*.cpp`                                   |

On Windows, **always use `_build.bat`** — it initializes the MSVC toolchain (`vcvarsall.bat`), sets `VCPKG_ROOT`, `Qt6.10` PATH, and proxy. Bare `cmake` commands from Git Bash will fail because MSVC include/library paths are not available.

### Arguments

| Argument        | Description                              |
| --------------- | ---------------------------------------- |
| (none)          | Configure + Build + Test                 |
| `--no-test`     | Configure + Build (skip tests)           |
| `--test-only`   | Run tests only (skip configure + build)  |
| `--configure`   | Configure only                           |
| `--build`       | Build only (skip configure)              |
| `--timeout N`   | Per-test timeout in seconds (default 10) |

### Running from Git Bash

When running `_build.bat` from Git Bash, redirect output to a file to avoid pipe-blocking:

```bash
# WRONG — pipe blocks until process exits, which may hang forever
_build.bat 2>&1 | tail -20

# RIGHT — redirect to file, then read
WIN_TMP=$(cygpath -w /tmp)
cmd.exe //c "$WIN_TMP/build.bat" > /tmp/out.txt 2>&1
cat /tmp/out.txt | tail -20
```

## Environment Requirements

| Tool             | Required Version                   | Installed                    | Notes                                                                                                        |
| ---------------- | ---------------------------------- | ---------------------------- | ------------------------------------------------------------------------------------------------------------ |
| CMake            | `>= 3.28`                          | 4.2.3 (VS 18 Insiders)      | Use the same version range locally and in CI. If lower, install/upgrade CMake before running presets.        |
| Ninja            | `>= 1.11`                          | 1.12.1                      | Required by the default preset generator. If missing, install Ninja or update the preset deliberately.       |
| Qt               | `6.10.2`                           | 6.10.2 @ `E:\Qt6.10`       | P1 CLI links `QtCore` only. P2+ GUI targets may add `QtWidgets`. Provided outside vcpkg.                   |
| Compiler (MSVC)  | `VS 2022 17.10+` / `MSVC 19.40+`  | VS 18 Community (MSVC 19.50)| Default Windows target toolchain. Located at `C:\Program Files\Microsoft Visual Studio\18\Community`.        |
| Compiler (Clang) | `>= 17`                            | —                            | Supported alternative toolchain.                                                                             |
| Compiler (GCC)   | `>= 13`                            | —                            | Supported alternative toolchain.                                                                             |
| vcpkg            | Manifest mode + locked baseline    | Shallow clone @ `E:\vcpkg`  | `VCPKG_ROOT` must point to a valid vcpkg checkout.                                                           |
| Git              | `>= 2.44`                          | —                            | Required for baseline repo operations and future Git tools.                                                  |

If any tool version is below the requirement, stop and ask the user to install or upgrade that tool instead of weakening the project constraints.

Qt must be provided outside vcpkg. The build script adds `E:\Qt6.10\bin` to PATH for DLL resolution.

## Prohibited

- Do not create alternative build scripts or Makefiles.
- Do not use `--force` or skip dependency checks.
- Do not modify `vcpkg.json` baseline without a corresponding dependency change.
- Do not bypass CMake presets with manual `-D` flags unless debugging.

## Target Boundaries

- P1 CLI targets must remain headless: allow `QtCore`, but do not link `QtWidgets`, `QScintilla`, or `QTermWidget`.
- Shared runtime targets (`core`, `framework`, `harness`, `cli`) must not depend on GUI-only libraries.
- GUI dependencies belong to separate presentation targets introduced in P2+.
- If a feature needs both CLI and GUI, implement the logic in shared runtime code and keep GUI adaptation in the presentation layer.

## Testing

### GTest + GMock

- All test binaries use `gtest_discover_tests()` with a 10-second timeout (configurable via `ACT_TEST_TIMEOUT` cache variable).
- Test names follow `TestSuite.TestName` format (e.g. `ToolRegistryTest.RegisterAndRetrieveTool`). Use `ctest -R TestSuite` to filter.
- For full suite: `_build.bat --test-only`
- For single test binary: `ctest --test-dir build -R TestSuite --output-on-failure`
- For verbose output: add `-V` flag.
- Test files follow pattern: `tests/<module>/<Module>Test.cpp`.

### Known Pitfall: QEventLoop in Tests

Tests that exercise code using `QEventLoop` (e.g. `ShellExecTool::execute()`) will **hang** if the test process has no `QCoreApplication`. The 10-second timeout prevents infinite blocking. For such tests, either:
- Test only the pre-QEventLoop logic (security checks, validation)
- Create a `QCoreApplication` in a test fixture and defer mock callbacks via `QTimer::singleShot`
