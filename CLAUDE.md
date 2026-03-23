# ACT (AI Coding Tool) Claude Code Instructions

This repository uses Claude Code for assisted development with autonomous task execution.

## Project Skills

- `/planning-requirements <需求>` — 拆分需求为结构化任务队列
- `/executing-tasks` — 从队列执行任务。追加 `all` 为批量模式。
- `/reviewing-code` — 只读代码质量分析
- `/adapting-project-structure` — 重新适配 .claude/ 结构（技术栈变更后）

## Development Workflow

### Full Pipeline
1. `/planning-requirements <需求>` — 拆分需求
2. 审批任务队列
3. `/executing-tasks all` — 闭环执行
4. `/loop 5m /executing-tasks all` — 无人监督持续执行

### Quick Tasks
- `/executing-tasks` — 每次执行一个任务
- `/executing-tasks T3` — 执行指定任务

## Build

**Build:** `cmake --build build --config RelWithDebInfo`
**Dev:**   `cmake --preset default`
**Test:**  `ctest --test-dir build --config RelWithDebInfo --output-on-failure`
**Lint:**  `clang-tidy -p build src/**/*.cpp`

## Toolchain Baseline

- CMake `>= 3.28`
- Ninja `>= 1.11`
- Qt `6.10.2` provided externally (`QtCore` for P1 CLI, `QtWidgets` from P2+)
- MSVC `19.40+` / VS 2022 `17.10+`
- Clang `>= 17` or GCC `>= 13`
- `VCPKG_ROOT` must point to a valid vcpkg checkout
- `CMAKE_PREFIX_PATH` or `Qt6_DIR` must point to the Qt 6.10.2 install prefix

If a local toolchain is below these requirements, install or upgrade it. Do not lower project version constraints to match the machine.

## Engineering Constraints

- P1 deliverable is a CLI-first runtime. Do not introduce `QtWidgets`, `QScintilla`, or `QTermWidget` into CLI targets.
- `QtCore` is allowed in CLI/runtime code for signals, threads, and event-loop integration.
- Keep GUI code isolated to presentation-layer targets planned for P2+.
- Shared runtime interfaces must stay reusable across CLI, Qt GUI, and future VS Code integration.

## Rules

See @.claude/rules/build-rules.md for build commands and constraints.
See @.claude/rules/code-quality.md for code style conventions.
See @.claude/rules/git-workflow.md for git conventions.
See @.claude/rules/task-execution.md for closed-loop execution rules.
