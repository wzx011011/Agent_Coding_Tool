# ACT 项目评测与观测组合建议

> 适用范围：当前 ACT 仓库（P1 CLI-first runtime）
> 前提：当前项目不以 RAG 为核心，因此不把 Ragas 作为主组合的一部分。

## 1. 结论先行

对于当前 ACT 项目，最合适的组合不是单独依赖某一个平台或某一个评测库，而是采用下面这套分层组合：

1. `Langfuse`
   - 负责生产观测、trace/session 归因、样本沉淀、版本复盘。
2. `DeepEval`
   - 负责离线质量评测、LLM-as-a-judge、回归测试、CI 门禁。
3. `C++ 单元测试 / 集成测试 / CTest`
   - 负责运行时实现正确性、工具契约、状态机、权限与回归稳定性。
4. `ACT 自己的回归任务集 / EvalRunner`
   - 负责把真实任务转成项目内部长期基准集，连接 Langfuse 和本地测试体系。

一句话总结：

```text
Langfuse 看线上真实行为，DeepEval 测离线输出质量，CTest 保证实现正确性，ACT 自己的回归任务集负责把两边连起来。
```

这套组合是当前 ACT 项目最现实、最稳妥的方案。

---

## 2. 为什么当前项目不把 Ragas 作为主组合

Ragas 很适合 RAG、检索、groundedness、context precision/recall、tool-use with retrieval 这类场景。

但当前 ACT 的核心不是“检索质量”，而是：

1. AgentLoop 稳不稳定。
2. tool_use 循环是否正确。
3. 权限系统是否可控。
4. prompt / skill / command / subagent 是否能把任务做对。
5. shell、file、git、task graph、resume 等运行时能力是否可靠。

所以在当前阶段：

1. `Ragas` 不是最优先。
2. `DeepEval` 更适合当前 ACT，因为它更像 LLM 测试框架。
3. `Langfuse + DeepEval + CTest + 内部回归集` 更贴合 ACT 的系统性质。

如果未来 ACT 真正引入大规模知识库检索、multi-source retrieval、grounded answer generation，再考虑把 Ragas 加进来更合适。

---

## 3. 推荐组合的职责分工

## 3.1 Langfuse

在 ACT 里，Langfuse 的职责应该限定为生产观测与样本入口，而不是把它当成测试框架。

### 负责什么

1. 记录 trace、session、user、environment、release、version。
2. 记录 tool call、latency、token、cost、error、metadata。
3. 用 scores 和 annotation 标出线上问题样本。
4. 从线上样本沉淀 datasets。
5. 对比不同 release / version 的线上效果。

### 不负责什么

1. 不负责本地 CI 回归。
2. 不负责替代单元测试。
3. 不负责代码级正确性验证。
4. 不负责直接决定补丁是否可上线。

### 在 ACT 当前阶段最该重点采集的字段

1. `task_type`
2. `tool_chain`
3. `permission_mode`
4. `prompt_version`
5. `policy_version`
6. `session_id`
7. `release`
8. `version`
9. `task_result`
10. `error_code`

---

## 3.2 DeepEval

在 ACT 里，DeepEval 是最适合承担“离线质量评测层”的开源工具。

### 为什么适合 ACT

1. 它适合把 LLM 行为写成测试。
2. 它适合做 end-to-end 和 component-level eval。
3. 它有现成 judge-based metrics。
4. 它更容易放进 CI/CD，而不是只停留在 notebook 或手工分析。

### 在 ACT 中最适合用来测什么

1. 任务完成度
2. 输出正确性
3. 回复相关性
4. 幻觉风险
5. 多轮任务是否完成目标
6. tool-use 结果是否符合预期
7. prompt / skill 版本变更后是否退化

### 适合使用的指标类型

对于 ACT，优先考虑下面这些：

1. `Task Completion`
2. `GEval / Rubric-based correctness`
3. `Answer Relevancy`
4. `Hallucination`
5. `Conversation / Multi-turn metrics`
6. `Custom metric for tool-use success`

### 不建议拿它做什么

1. 不要让它替代生产观测。
2. 不要拿它替代系统层 C++ 回归测试。
3. 不要把所有失败都归因成 prompt 问题。

---

## 3.3 C++ 单元测试 / 集成测试 / CTest

这部分是 ACT 当前阶段绝对不能弱化的底座。

因为 ACT 当前是 runtime-first 项目，很多核心风险都不在“回答像不像人”，而在：

1. 状态机是否正确。
2. 权限审批是否正确。
3. tool schema 和结果是否正确。
4. shell/file/git/task graph/resume 是否可靠。
5. 超时、编码、错误恢复是否稳定。

### 它负责验证什么

1. AgentLoop 状态流转
2. PermissionManager 行为
3. ToolRegistry / 各 Tool 的契约
4. PatchTransaction
5. EvalRunner
6. Resume / Replay / TaskGraph
7. JSON 协议 / CLI 行为

### 这部分为什么不能被 Langfuse 或 DeepEval 替代

因为这些问题本质上不是“输出质量问题”，而是“实现正确性问题”。

---

## 3.4 ACT 自己的回归任务集 / EvalRunner

这是整个组合里最关键的一层，因为它是桥梁。

Langfuse 能发现线上问题，DeepEval 能做离线质量评测，CTest 能做实现层回归，但如果没有 ACT 自己的长期回归任务集，这三者之间就断开了。

### 它应该承担的职责

1. 把真实任务沉淀成“长期基准任务集”。
2. 把线上失败样本转成离线可复放任务。
3. 把任务结果映射到主指标、护栏指标、运营指标。
4. 为 prompt / model / policy / tool / code 改动提供统一验证入口。

### 任务集建议分层

1. `golden_success`
   - 高价值成功任务，防止修一个问题坏一片。
2. `behavior_failures`
   - 行为层问题，主要供 DeepEval 使用。
3. `runtime_failures`
   - 实现层问题，主要供 CTest / 集成测试 / replay 使用。
4. `permission_cases`
   - 权限模式、审批路径、拒绝恢复。
5. `long_running_sessions`
   - 多轮任务、resume、background lane、subagent。

---

## 4. 最推荐的整体架构

建议把 ACT 当前的评测体系理解成四层：

```text
第 1 层：生产观测层
  Langfuse

第 2 层：离线质量评测层
  DeepEval

第 3 层：实现正确性验证层
  CTest / GTest / 集成测试

第 4 层：项目长期基准层
  ACT EvalRunner + Regression Task Sets
```

这四层对应的问题分别不同：

| 层               | 主要问题                   | 主要工具                |
| ---------------- | -------------------------- | ----------------------- |
| 生产观测层       | 用户真实任务发生了什么问题 | Langfuse                |
| 离线质量评测层   | 输出质量是否变好/变坏      | DeepEval                |
| 实现正确性验证层 | 代码逻辑是否正确           | CTest / GTest           |
| 项目长期基准层   | 改动是否破坏 ACT 核心能力  | EvalRunner / 回归任务集 |

---

## 5. 从用户行为到上线复盘的闭环

当前 ACT 最适合的完整闭环是：

```text
Langfuse 线上 trace
-> 筛出问题样本
-> 归类为 behavior / runtime / permission / session 问题
-> 沉淀到内部回归任务集
-> DeepEval 跑离线质量评测
-> CTest 跑实现层回归
-> 小流量上线
-> 再回 Langfuse 看 release/version 实际效果
```

### 5.1 如果发现行为层问题

例如：

1. prompt 跑偏
2. tool 选择不合理
3. 提前结束
4. 多轮任务中答非所问

处理流程：

1. Langfuse 标记问题 trace
2. 加入 `behavior_failures`
3. 用 DeepEval 跑 judge-based eval
4. prompt / skill / policy 改动后重新评估
5. 再上线看 Langfuse

### 5.2 如果发现实现层问题

例如：

1. shell timeout
2. file edit 错误
3. resume 异常
4. 编码乱码
5. 权限状态流转错误

处理流程：

1. Langfuse 标记问题 trace
2. 加入 `runtime_failures`
3. 先补 C++ 测试或 replay case
4. 修代码
5. 跑 CTest / 集成测试
6. 再上线看 Langfuse

---

## 6. ACT 当前阶段推荐指标体系

结合项目性质，当前 ACT 不应该照搬 RAG 项目指标，而应该用下面这套。

## 6.1 主指标

1. `task_completion`
2. `tool_use_success_rate`
3. `user_satisfaction`
4. `multi_turn_goal_completion`

## 6.2 护栏指标

1. `error_rate`
2. `unsafe_action_rate`
3. `rollback_rate`
4. `permission_violation_rate`
5. `unexpected_file_write_rate`

## 6.3 运营指标

1. `latency_ms`
2. `avg_iterations`
3. `tool_calls_per_task`
4. `cost_per_task`
5. `resume_success_rate`

### 哪些指标更适合放在哪

| 指标                        | 最适合在哪看        |
| --------------------------- | ------------------- |
| `task_completion`           | Langfuse + DeepEval |
| `tool_use_success_rate`     | Langfuse + 集成测试 |
| `user_satisfaction`         | Langfuse            |
| `error_rate`                | Langfuse + CTest    |
| `permission_violation_rate` | CTest + Langfuse    |
| `latency_ms`                | Langfuse            |
| `avg_iterations`            | Langfuse            |

---

## 7. 推荐的优先级，而不是一次性全上

当前项目最现实的落地顺序应该是：

## Phase A：先把底座打稳

1. 保持 CTest / GTest / 集成测试为第一优先级。
2. 扩展 EvalRunner 和回归任务集。
3. 用 Langfuse 把真实 trace、session、release、version 打起来。

## Phase B：引入 DeepEval 做行为层验证

1. 先挑 20 到 50 个高价值任务做 DeepEval dataset。
2. 先只做 2 到 4 个关键指标。
3. 把它接进 CI，作为非阻塞报告开始。

## Phase C：再做正式发布门禁

1. 让 DeepEval 和内部回归集成为候选版本门槛。
2. 让 Langfuse 的 release/version 对比成为上线复盘入口。
3. 让护栏指标恶化时阻止晋级。

---

## 8. 当前项目不推荐的组合

## 8.1 只用 Langfuse

问题：

1. 能看到问题，但很难形成 CI 回归。
2. 不适合本地开发阶段快速验证。
3. 容易停留在“看 trace”而不是“改进闭环”。

## 8.2 只用 DeepEval

问题：

1. 看不到真实线上分布。
2. 没有 release / session / environment 复盘能力。
3. 样本容易脱离真实用户任务。

## 8.3 只用通用 benchmark

例如只跑 MMLU、HellaSwag、TruthfulQA。

问题：

1. 这些更适合评模型本身，不适合评 ACT runtime。
2. 不能覆盖 shell、permissions、resume、tool-use 这些关键能力。
3. 无法替代项目内部长期基准集。

## 8.4 现在就上 Ragas 为主

问题：

1. 当前 ACT 不是 RAG-first。
2. 会把注意力带偏到 retrieval 指标。
3. 不如先把 Agent / runtime / tool-use 评测打牢。

---

## 9. 最终推荐

如果只输出一条最终建议，那就是：

```text
当前 ACT 项目最适合的组合是：
Langfuse + DeepEval + CTest/GTest + ACT 内部回归任务集 / EvalRunner
```

### 其中的角色分别是

1. `Langfuse`
   - 线上观测、样本沉淀、release 复盘
2. `DeepEval`
   - 离线质量评测、judge、CI 回归
3. `CTest / GTest`
   - 实现正确性、工具契约、状态机稳定性
4. `EvalRunner / Regression Task Sets`
   - 项目长期基准、桥接线上样本和本地验证

### 这是不是完美组合

不是，但它是当前 ACT 阶段最匹配、最稳妥、最能落地的组合。

因为 ACT 当前的核心问题不是“检索是否足够好”，而是：

1. agent 能不能把任务做完
2. tool_use 是否可靠
3. runtime 是否稳定
4. prompt / policy / code 改动后是否可验证

这套组合正好覆盖了这四件事。

---

## 10. 后续演进条件

只有当 ACT 后续真正引入下列能力时，再考虑把 Ragas 作为主组合补进来：

1. 内置知识库检索
2. retrieval-heavy workflow
3. grounded answer generation
4. retrieval ranking / chunk quality / context precision 成为核心问题

到那时，组合可以升级为：

```text
Langfuse + DeepEval + Ragas + CTest/GTest + ACT EvalRunner
```

但在当前阶段，不建议把 Ragas 放到核心路径里。
