# Stop Conditions

批量模式（`$ARGUMENTS` 包含 `all`）在以下条件自动停止。

## 硬性停止（立即停止，所有模式生效）

1. **全部完成** — 所有任务状态为 `[x]`
2. **全部阻塞** — 所有可执行任务状态为 `[!]`
3. **循环依赖** — 拓扑排序检测到循环
4. **队列损坏** — `docs/task-queue.md` 不存在或格式错误

## 软性停止（当前轮完成后停止，仅非 /loop 模式）

5. **上下文压力** — autocompact 已触发 2+ 次
6. **连续失败** — 同一轮中 2+ 个任务被标记为 `[!]`
7. **检查点** — 已连续完成 5 个任务，输出检查点报告

## /loop 模式

当与 `/loop` 配合时，每次迭代是新会话，软性停止 5-7 不适用。
仅硬性停止 1-4 生效。这确保 `/loop` 能跨轮次持续推进。

## 停止输出

```
Session Complete
────────────────
Completed: 7/10 tasks
Blocked: 1 (T8: circular import in auth module)
Remaining: 2 (T9, T10 — blocked by T8)

Next steps:
  1. Fix T8 circular import manually
  2. Run /executing-tasks all to continue
```
