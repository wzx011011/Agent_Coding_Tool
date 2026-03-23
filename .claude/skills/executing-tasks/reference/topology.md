# Topology-Based Task Scheduling

任务队列中的任务存在依赖关系，需要拓扑排序决定执行顺序。

## 算法

1. 从 `docs/task-queue.md` 读取所有任务的 Depends 列表
2. 构建依赖图（邻接表）
3. 计算每个节点的入度
4. Kahn's algorithm 拓扑排序
5. 将排序结果分层：同一层的任务可以并行执行

## 示例

```
任务队列:
  T1: Depends = []
  T2: Depends = [T1]
  T3: Depends = [T1]
  T4: Depends = []
  T5: Depends = [T2, T3]

拓扑排序:
  Layer 0: [T1, T4]     ← 无依赖，可并行
  Layer 1: [T2, T3]     ← 依赖 Layer 0 完成，可并行
  Layer 2: [T5]         ← 依赖 Layer 1 完成，串行
```

## 执行策略

| 场景 | 策略 |
|------|------|
| 单任务 | 主线程直接执行闭环 |
| 同层 2-3 个任务 | 各委派一个 `task-worker` agent，并行 |
| 同层 > 3 个任务 | 委派 agent，但限制同时运行数量为 3 |
| 跨层 | 串行：上一层全部 `[x]` 后才开始下一层 |

## 并行委派规则

1. 每个 agent 拿到完全自包含的 prompt：任务描述、验收标准、相关文件列表
2. 每个 agent 在独立 worktree 中执行
3. 同层 agent 全部完成后，验证所有结果，更新队列
4. 如有 agent 失败，分析原因后决定：修复重试 或 标记 `[!]` 继续

## 循环检测

如果拓扑排序检测到循环依赖，立即停止并报告：

```
循环依赖检测到: T2 → T3 → T2
请修复 docs/task-queue.md 中的依赖关系后重新执行。
```
