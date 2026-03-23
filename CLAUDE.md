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

Windows 环境必须通过 `_build.bat` 构建（初始化 MSVC toolchain）。

| Command      | Script                  |
| ------------ | ----------------------- |
| Full build   | `_build.bat`            |
| Build only   | `_build.bat --no-test`  |
| Test only    | `_build.bat --test-only`|

从 Git Bash 调用时，必须将输出重定向到文件，不能用 pipe：

```bash
WIN_TMP=$(cygpath -w /tmp)
cmd.exe //c "E:\\ai\\Agent_Coding_Tool\\_build.bat" > /tmp/build.txt 2>&1
cat /tmp/build.txt | tail -20
```

## Toolchain Baseline

- CMake `>= 3.28` (installed: 4.2.3)
- Ninja `>= 1.11` (installed: 1.12.1)
- Qt `6.10.2` at `E:\Qt6.10` (`QtCore` for P1 CLI, `QtWidgets` from P2+)
- MSVC `19.40+` / VS 2022 `17.10+` (installed: VS 18 Community, MSVC 19.50)
- vcpkg manifest mode, shallow clone at `E:\vcpkg`
- Git `>= 2.44`

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
