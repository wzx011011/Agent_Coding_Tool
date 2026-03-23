# Closed-Loop Execution

每个任务必须执行完整的闭环，不允许半完成状态。

## 闭环步骤

```
READ → IMPLEMENT → VERIFY → (FIX → VERIFY) × N → COMMIT
```

### READ
- 读取任务描述和验收标准
- 读取 `docs/task-queue.md` 中依赖任务的产物（commit、修改文件）
- 读取受影响的源文件
- 确认修改范围和现有模式

### IMPLEMENT
- 实现最小、可验证的变更
- 不引入任务范围外的改动
- 不在一个任务中混合多个不相关的变更

### VERIFY
- 运行 build（参照 `.claude/rules/build-rules.md`）
- 运行 test
- 如有 lint，运行 lint

### FIX（仅在 VERIFY 失败时）
- 分析失败原因，定位根因（不是表面修复）
- 修复并重新 VERIFY
- 最多重试 3 次

### COMMIT
- 仅在 VERIFY 通过后执行
- 格式: `{type}({task_id}): {description}`
- 示例: `feat(T3): add login API with JWT auth`

## 重试策略

| 重试次数 | 行为 |
|---------|------|
| 1 | 读取错误日志，修复根因，重新 VERIFY |
| 2 | 如果同样的错误持续，换一种方法 |
| 3 | 最后一次尝试，如仍失败标记为 `[!]` |
| > 3 | 标记 `[!]` blocked，记录失败原因，跳过 |

## 禁止行为

- 不允许 VERIFY 失败后直接 COMMIT
- 不允许跳过 VERIFY 步骤
- 不允许标记 `[x]` 但 build 或 test 失败
- 不允许一个任务中混合不相关的变更
- 不允许修改任务范围外的文件（配置、其他模块）除非任务明确要求
