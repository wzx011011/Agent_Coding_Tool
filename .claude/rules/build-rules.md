# Build Rules

## Build Commands

| Command      | Script                                                               |
| ------------ | -------------------------------------------------------------------- |
| Configure    | `cmake --preset default`                                             |
| Build        | `cmake --build build --config RelWithDebInfo`                        |
| Test         | `ctest --test-dir build --config RelWithDebInfo --output-on-failure` |
| Install deps | `vcpkg install` (auto via CMake manifest mode)                       |
| Lint         | `clang-tidy -p build src/**/*.cpp`                                   |

## Environment Requirements

| Tool             | Required Version                 | Notes                                                                                                        |
| ---------------- | -------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| CMake            | `>= 3.28`                        | Use the same version range locally and in CI. If lower, install/upgrade CMake before running presets.        |
| Ninja            | `>= 1.11`                        | Required by the default preset generator. If missing, install Ninja or update the preset deliberately.       |
| Qt               | `6.10.2`                         | Use a self-built or preinstalled Qt 6.10.2. P1 CLI links `QtCore` only. P2+ GUI targets may add `QtWidgets`. |
| Compiler (MSVC)  | `VS 2022 17.10+` / `MSVC 19.40+` | Default Windows target toolchain.                                                                            |
| Compiler (Clang) | `>= 17`                          | Supported alternative toolchain.                                                                             |
| Compiler (GCC)   | `>= 13`                          | Supported alternative toolchain.                                                                             |
| vcpkg            | Manifest mode + locked baseline  | `VCPKG_ROOT` must point to a valid vcpkg checkout.                                                           |
| Git              | `>= 2.44`                        | Required for baseline repo operations and future Git tools.                                                  |

If any tool version is below the requirement, stop and ask the user to install or upgrade that tool instead of weakening the project constraints.

Qt must be provided outside vcpkg. Set `CMAKE_PREFIX_PATH` or `Qt6_DIR` to the Qt 6.10.2 installation prefix before configuring the project.

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

- Use `ctest --test-dir build --config RelWithDebInfo --output-on-failure` for full suite.
- For single test binary: `ctest --test-dir build -R <test_name> --output-on-failure`.
- For verbose output: add `-V` flag.
- Test files follow pattern: `tests/<module>/<Module>Test.cpp`.
