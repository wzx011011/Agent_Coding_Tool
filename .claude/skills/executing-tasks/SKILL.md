---
name: executing-tasks
description: "Executing tasks from the task queue with closed-loop verification, dependency-aware scheduling, and parallel agent delegation. Triggers when the user asks to execute tasks, continue implementation, or run tasks in batch mode."
---

# Task Execution Engine

从任务队列中读取任务，按依赖顺序执行，每个任务闭环验证。

遵循 `.claude/rules/task-execution.md` 中的闭环规则，不在此重复。

## 参数

- `$ARGUMENTS` 为空：**单任务模式** — 执行下一个可执行任务后停止
- `$ARGUMENTS` 包含 `all`：**批量模式** — 持续执行直到遇到停止条件
- `$ARGUMENTS` 包含任务 ID（如 `T3`）：**定向模式** — 执行指定任务

## 前置条件

- `docs/task-queue.md` 必须存在
- 如果不存在，提示用户先执行 `/planning-requirements <需求>`

## 工作流

### 1. 读取任务队列

读取 `docs/task-queue.md`，解析所有任务及其状态和依赖。

### 2. 选择可执行任务

- 找到所有 `[ ]`（pending）状态的任务
- 筛选出依赖已全部满足的任务（所有 Depends 中的任务状态为 `[x]`）
- 如果无可用任务但队列未完成，检查是否有 `[!]`（blocked）任务需要报告

### 3. 拓扑分组

将可执行任务按依赖关系分组：
- 同一组的任务之间无依赖，可以并行
- 组间按依赖顺序串行

详见 `reference/topology.md`。

### 4. 执行任务

**单任务 / 串行任务：**
- 主线程直接执行闭环（read → implement → verify → fix → commit）

**并行任务组（2+ 个独立任务）：**
- 每个任务委派给 `task-worker` agent
- 使用 `isolation: "worktree"` 和 `run_in_background: true`
- 所有 agent 在同一消息中 spawn（最大化并行）

### 5. 验证与更新

- 验证标准和构建命令遵循 `.claude/rules/build-rules.md` 和 `.claude/rules/task-execution.md`
- 更新 `docs/task-queue.md` 中的任务状态和 Notes
- 展示进度表

### 6. 下一轮 / 停止

- **批量模式**: 回到步骤 2，继续下一轮
- **单任务模式**: 停止并报告
- **定向模式**: 执行完指定任务后停止

停止条件详见 `reference/stop-conditions.md`。

## 进度报告

每轮执行后渲染：

```
Task Execution Progress
───────────────────────
| # | ID  | Title | Status | Commit  |
|---|-----|-------|--------|---------|
| 1 | T1  | ...   | [x]    | abc123  |
| 2 | T2  | ...   | [x]    | def456  |
| 3 | T3  | ...   | [-]    | —       |

Progress: 2/4 completed | Round 2 of 3
```

## 与 /loop 配合

```bash
/loop 5m /executing-tasks all
```

每次 loop 迭代是新会话，从 `docs/task-queue.md` 恢复状态。仅硬性停止条件生效（详见 stop-conditions.md）。
