# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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

Windows 环境必须通过 `_build.bat` 构建（初始化 MSVC toolchain）。裸 `cmake` 命令从 Git Bash 会因缺少 MSVC 路径而失败。

| Command      | Script                  |
| ------------ | ----------------------- |
| Full build   | `_build.bat`            |
| Build only   | `_build.bat --no-test`  |
| Test only    | `_build.bat --test-only`|

从 Git Bash 调用时，必须将输出重定向到文件，不能用 pipe：

```bash
WIN_TMP=$(cygpath -w /tmp)
cmd.exe //c "$WIN_TMP/build.bat" > /tmp/out.txt 2>&1
cat /tmp/out.txt | tail -20
```

## Testing

GTest + `gtest_discover_tests()`，默认 10 秒超时。

| Command                              | Description                    |
| ------------------------------------ | ------------------------------ |
| `ctest --test-dir build -R TestSuite` | 运行单个测试套件               |
| `ctest --test-dir build -R TestSuite --output-on-failure` | 失败时输出详情 |
| `ctest --test-dir build -R TestSuite -V` | Verbose 模式                   |

测试文件路径模式：`tests/<module>/<Module>Test.cpp`

## Architecture

5 层分离架构，依赖方向单向向下：

```
Core → Infrastructure → Services → Harness → Framework → Presentation
```

| Layer            | Directory        | Purpose                                      |
| ---------------- | ---------------- | -------------------------------------------- |
| `act_core`       | `src/core`       | 类型定义、枚举、错误码（独立，无外部依赖）      |
| `infrastructure` | `src/infrastructure` | IFileSystem、IProcess 等平台抽象接口      |
| `services`       | `src/services`   | AIEngine、ConfigManager、ModelSwitcher        |
| `harness`        | `src/harness`    | ToolRegistry、PermissionManager、ContextManager、19 个 ITool 实现 |
| `framework`      | `src/framework`  | AgentLoop、CliRepl、SessionManager、SkillCatalog、TaskGraph |
| `presentation`   | `src/presentation` | TUI (ftxui)、Feishu channel（P2+ GUI 预留） |

关键概念：
- **AgentLoop** — 核心状态机：`Idle → Running ⇄ ToolRunning → Completed/Failed`。支持 tool_use 循环、权限审批、ask_user 暂停、context compaction
- **System Prompt 三层拼接** — `defaultBasePrompt()` (内置) + `.act/system_prompt.md` (项目) + `.act/skills/*.toml` (技能)，通过 `AgentLoop.setSystemPrompt()` 在首条用户消息前注入
- **TaskGraph** — DAG 任务图，Kahn 拓扑排序、cycle detection、JSON 持久化
- **SubagentManager** — 子 agent 数据层（Explore/Code 类型），执行层待实现

所有层 include 目录使用 `${PROJECT_SOURCE_DIR}/src`，跨层引用格式为 `#include "layer/file.h"`。

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

See @.claude/rules/build-rules.md for build commands, toolchain requirements, and target boundaries.
See @.claude/rules/code-quality.md for C++20/Qt6 conventions and anti-patterns.
See @.claude/rules/git-workflow.md for branch strategy and commit conventions.
See @.claude/rules/task-execution.md for closed-loop execution and retry policy.
