# Queue Update Suggestions

执行过程中不自动修改队列，但收集建议供用户确认。

## 原则

- **只读观察**：执行过程只更新任务状态和 Notes，不自动修改队列结构
- **延迟确认**：建议在批量模式结束后统一输出，由用户决定是否应用
- **显式批准**：所有建议需要用户显式确认才会写入队列

## 建议类型

| 标记 | 含义 | 触发条件 |
|------|------|---------|
| `[NEW]` | 发现新任务 | 实现中识别到未列出的必要工作 |
| `[MERGE]` | 合并建议 | 两个任务高度耦合，合并更高效 |
| `[SPLIT]` | 拆分建议 | 任务过于复杂，重试失败 |
| `[DEPENDS]` | 依赖调整 | 声明的依赖不正确或遗漏 |

### [NEW] 新增任务

**触发条件**：
- 实现任务时发现遗漏的依赖项或必要步骤
- 发现当前任务的前置条件未被队列覆盖
- 发现当前任务的后续工作需要补充

**格式**：
```
[NEW] <ID>: <Title> (<reason>)
```

**示例**：
```
[NEW] N30: Validate command input schema (prereq for N19)
[NEW] N31: Add error handling for invalid commands (found during N17)
```

### [MERGE] 合并任务

**触发条件**：
- 两个任务的实现代码高度重叠
- 一个任务的验收标准被另一个隐含包含
- 两个任务共享相同的文件修改

**格式**：
```
[MERGE] <ID1> + <ID2> → <NewTitle> (<reason>)
```

**示例**：
```
[MERGE] N17 + N18 → Unified permission system (shared logic in 80% of code)
```

### [SPLIT] 拆分任务

**触发条件**：
- 任务重试 3 次仍未通过
- 任务涉及 3 个以上不同的文件/模块
- 任务描述包含"且"、"然后"等多个动作

**格式**：
```
[SPLIT] <ID> → <ID>a/<ID>b (<reason>)
```

**示例**：
```
[SPLIT] N19 → N19a/N19b (scope too large: parsing + validation)
```

### [DEPENDS] 依赖调整

**触发条件**：
- 发现声明的依赖实际上不需要（任务可独立执行）
- 发现遗漏的依赖（任务执行时被阻塞）
- 发现依赖顺序不正确

**格式**：
```
[DEPENDS] <ID> depends on <DepID> (<reason>)
[DEPENDS] <ID> remove dependency on <DepID> (<reason>)
```

**示例**：
```
[DEPENDS] N22 depends on N17 (missing in queue)
[DEPENDS] N20 remove dependency on N15 (actually independent)
```

## 输出时机

**批量模式结束后**统一输出建议（如果有的话）。

单任务模式不输出建议，避免打断工作流。如需建议，用户可主动询问。

## 输出格式

```
Task Execution Complete
────────────────────────
Completed: 3/7 | Round 2

📋 Queue Suggestions (optional):
  [NEW] N30: Validate command input schema (prereq for N19)
  [SPLIT] N19 → N19a/N19b (scope too large)
  [DEPENDS] N22 depends on N17 (missing in queue)

Apply to task-queue.md? (y/n/edit)
```

## 交互方式

| 输入 | 行为 |
|------|------|
| `y` | 自动应用所有建议到 `docs/task-queue.md` |
| `n` | 忽略建议，队列保持不变 |
| `edit` | 打开 `docs/task-queue.md` 供手动编辑 |

## 与 /loop 模式的兼容

当与 `/loop` 配合时，建议输出后自动选择 `n`（忽略），因为：
- `/loop` 是无人监督模式，无法等待用户输入
- 建议会被记录在会话日志中，用户可稍后手动应用
- 下次手动运行 `/executing-tasks` 时可重新生成建议

## 收集机制

在执行每个任务时，agent 应记录观察到的建议：

```
_internal_suggestions:
  - type: NEW
    id: N30
    title: "Validate command input schema"
    reason: "prereq for N19"
```

批量模式结束时，汇总所有 agent 的 `_internal_suggestions` 后输出。
