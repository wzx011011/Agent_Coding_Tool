# Langfuse 使用指南（面向 ACT / Digital Twin 自我迭代闭环）

> 适用对象：`http://localhost:3000/project/digital-twin-project`
> 目标：把 Langfuse 从“看日志的地方”用成“观测、评估、回放、发布复盘”的统一工作台。

## 1. 先建立正确理解

结合 `agent-self-iteration-system.md`，Langfuse 在这套系统里不是单一工具，而是整个闭环的数据入口与验证面板。

它对应文档中的链路如下：

```text
真实使用数据 -> 问题识别 -> 样本沉淀 -> 分流干预 -> 离线验证 -> 小流量发布 -> 复盘归因
```

在 Langfuse 里，这条链路分别落到：

1. `Tracing` / `Observations`：看原始运行过程。
2. `Sessions` / `Users`：看跨 trace 的会话与用户维度问题。
3. `Scores` / `LLM-as-a-Judge` / `Human Annotation`：把“看起来有问题”变成结构化评估。
4. `Datasets`：把线上样本沉淀成可回放的验证集。
5. `Prompts` / `Playground`：管理候选 prompt 和实验。
6. `Dashboards`：长期看趋势和版本效果。

一句话原则：

`Tracing` 负责发现样本，`Scores` 负责定义质量，`Datasets` 负责沉淀验证资产，`Prompts/Playground` 负责做候选实验，`Dashboards` 负责复盘发布效果。

---

## 2. Langfuse 在你的三层系统里分别扮演什么角色

### 2.1 第零层：多用户洞察收集

这是 Langfuse 的核心职责。

你要用它回答四类问题：

1. 哪类任务经常失败。
2. 哪类任务成本过高。
3. 哪类任务延迟过高。
4. 哪类任务虽然没报错，但质量差。

对应页面主要是：

1. `Tracing`
2. `Observations`
3. `Sessions`
4. `Users`
5. `Dashboards`

### 2.2 方案一：自我迭代 Agent

Langfuse 在这一层的作用不是直接改 prompt，而是提供证据和验证集。

你要从线上 traces 里筛出这类样本：

1. 没有系统错误，但回答质量低。
2. 工具能执行，但选错工具或顺序不佳。
3. 某类任务表现波动大。
4. 某个 prompt 版本上线后，质量分下滑。

对应页面主要是：

1. `Tracing`
2. `Scores`
3. `Datasets`
4. `Prompts`
5. `Playground`
6. `LLM-as-a-Judge`

### 2.3 方案二：监督者 Agent

Langfuse 在这一层用来发现实现层缺陷和构建修复验证样本。

重点看这类模式：

1. tool 调用失败。
2. shell 超时。
3. 编码乱码。
4. 文件路径错误。
5. 某些 release 上线后错误率上升。

对应页面主要是：

1. `Tracing`
2. `Observations`
3. `Scores`
4. `Datasets`
5. `Dashboards`

---

## 3. 项目最少应该上报哪些字段

如果没有稳定的数据契约，Langfuse 最终只会变成漂亮的日志查看器。

建议最少上报这些字段。

### 3.1 Trace 级字段

每条 trace 最少要有：

| 字段          | 用途                            |
| ------------- | ------------------------------- |
| `trace_name`  | 按任务类型或链路分类            |
| `environment` | 区分 `dev` / `staging` / `prod` |
| `user_id`     | 用户分群与问题归因              |
| `session_id`  | 会话回放与跨 trace 聚合         |
| `version`     | 对应候选策略或客户端版本        |
| `release`     | 用于上线效果对比                |
| `tags`        | 快速过滤和人工标注              |
| `metadata`    | 业务字段、实验字段、上下文字段  |

### 3.2 Metadata 级字段

建议统一结构，不要自由发挥。

推荐字段：

```json
{
  "task_id": "...",
  "task_type": "rag_chat | local_bash | file_edit | retrieval_only",
  "channel": "cli | vscode | api",
  "model": "glm-5-turbo",
  "prompt_version": "prompt-v12",
  "policy_version": "policy-v4",
  "retrieval_enabled": true,
  "tool_chain": ["grep", "file_read", "shell_exec"],
  "candidate_version": "exp-2026-03-31-a",
  "tenant_id_hash": "...",
  "user_segment": "power_user | new_user | internal_test"
}
```

### 3.3 Score 级字段

你现在已经有部分分数，建议补成三层。

#### 主指标

1. `task_completion`
2. `answer_quality`
3. `user_satisfaction`
4. `goal_achievement_rate`

#### 护栏指标

1. `unsafe_action_rate`
2. `hallucination_rate`
3. `rollback_rate`
4. `error_rate`

#### 运营指标

1. `latency_ms`
2. `retrieval_latency_ms`
3. `tool_calls_per_task`
4. `total_tokens`
5. `cost_per_task`

---

## 4. 每个 Langfuse 模块该怎么用

## 4.1 Tracing

URL 形态：`/project/digital-twin-project/traces`

这是你每天用得最多的页面。

它的用途不是“逐条读输出”，而是做三件事：

1. 找异常样本。
2. 对样本做初步分类。
3. 把高价值样本送进 dataset 和 annotation 流程。

### 在 Tracing 页面应该固定做的动作

1. 先按时间窗口过滤：`6h`、`1d`、`7d`。
2. 再按 `environment` 过滤，避免混淆线上和实验数据。
3. 再按 `trace_name` 过滤，按链路切桶。
4. 用搜索框搜关键模式：
   - `timeout`
   - `Exit code 1`
   - `retrieval_status`
   - 某个 `task_id`
   - 某个 `session_id`
   - 某个模型名
5. 调整列，只保留当前任务相关列。

### 推荐保存的 Views

至少建这 6 个视图：

1. `RAG-低质量`
2. `RAG-高延迟`
3. `工具失败`
4. `高成本样本`
5. `低分未报错`
6. `待入基准集`

### 在 Trace 详情里重点看什么

1. `Timeline`
   - 判断慢在检索、生成还是工具调用。
2. `Preview`
   - 快速看输入输出是否跑偏。
3. `Log View`
   - 看完整结构、错误信息、metadata。
4. `Scores`
   - 看人工分与自动分是否一致。
5. `Annotate`
   - 对高价值问题做人工定性。
6. `Add to datasets`
   - 把高价值样本沉淀到离线集。

### Tracing 页上的分流原则

1. 没报错但质量差：优先走策略优化。
2. 报错、超时、乱码、状态错乱：优先走实现修复。
3. 单例问题先标注，不直接下结论。
4. 重复问题再进入候选改进和实验。

---

## 4.2 Observations

URL 形态：`/project/digital-twin-project/observations`

`Tracing` 看的是整条链路，`Observations` 更适合看链路中的单个节点。

适合排查：

1. 检索节点过慢。
2. 某个 tool 调用稳定失败。
3. 某个模型 generation 输出异常。
4. 某个 span 的 metadata 缺失。

### 应用方式

1. 按 observation type 切分：generation、span、event、tool 等。
2. 按 latency 排序，找最慢节点。
3. 按 error level 过滤，找最常失败节点。
4. 对高频失败 observation 打标签，作为实现层待修复证据。

如果 `Tracing` 是“案卷”，`Observations` 就是“案卷里的证据片段”。

---

## 4.3 Sessions

URL 形态：`/project/digital-twin-project/sessions`

这是你做多轮对话、长任务链、恢复/重试分析时最重要的页面。

文档里要求按 `session_id` 做聚合，这里就是落点。

### 适合回答的问题

1. 某个用户在一次会话中是不是频繁重试。
2. 某类任务是不是在多轮后才失败。
3. 某次发布后，会话级满意度是否下降。
4. 某类 session 是否经常出现“中途停止”“问错问题”“工具循环”。

### 正确用法

1. 确保每条 observation 都继承稳定的 `session_id`。
2. 对 session 做 session-level score，例如：
   - `conversation_quality`
   - `session_completion`
   - `user_feedback`
3. 对高价值 session 做 bookmark 或 comment。
4. 把典型 session 加入 dataset，作为多轮回放样本。

### 你需要重点看什么

1. 同一个 session 下 trace 数量是否异常高。
2. 是否出现连续低分或连续超时。
3. 是否存在“单条 trace 还行，但整段会话失败”的情况。

这类问题只看单条 trace 往往看不出来。

---

## 4.4 Users

URL 形态：`/project/digital-twin-project/users`

这个页面不是做运营画像，而是做问题分群。

### 要回答的问题

1. 新用户和老用户的问题是否不同。
2. 某类用户是否更容易遇到 retrieval 质量差。
3. 某个租户是否经常报同类错误。
4. 某类用户是否成本异常高。

### 正确用法

1. `user_id` 必须稳定且脱敏。
2. 通过 metadata 或 tags 上报用户分群。
3. 对不同用户分群分别看质量、成本、延迟。
4. 不看总体平均值，避免掩盖局部退化。

这一步对应文档中的“不同用户群体应分别评估”。

---

## 4.5 Scores

URL 形态：`/project/digital-twin-project/scores`

这是从“观察现象”进入“结构化评估”的核心页面。

如果没有这个页面，所谓自我迭代只能停留在经验判断。

### Scores 的正确角色

1. 把人工判断变成可统计数据。
2. 把自动 judge 的结论变成可过滤维度。
3. 作为离线验证、canary 对比的共同指标。

### 推荐做法

1. 每个 score 都明确 `source`：
   - `human`
   - `rule`
   - `judge`
   - `api`
2. 人工分和自动分不要混成一个字段。
3. 同一类问题用固定命名。
4. 高风险指标做成护栏分，不参与“直接晋级”。

### 推荐字段命名

1. `answer_quality`
2. `retrieval_quality`
3. `tool_efficiency`
4. `task_completion`
5. `response_length`
6. `latency_score`
7. `safety_score`

### 什么时候看 Scores

1. 筛低分样本时。
2. 做新旧版本对比时。
3. 判断问题是行为层还是实现层时。
4. 判断某条建议是否值得进入 dataset 时。

---

## 4.6 LLM-as-a-Judge

URL 形态：`/project/digital-twin-project/evals`

这是把候选策略改动做自动化对照评估的核心能力。

它对应文档里的：

```text
候选生成 -> 离线回放 -> 护栏检查 -> canary
```

### 正确用途

1. 比较两个 prompt 版本。
2. 比较两个模型版本。
3. 比较两种检索策略。
4. 比较工具选择策略是否更稳。

### 不该怎么用

1. 不要把 LLM judge 当成最终真相。
2. 不要只看单一分数。
3. 不要跳过人工 spot check。
4. 不要只用近期样本做评估。

### 最佳实践

1. 先用 dataset 跑 judge。
2. 设定主指标和护栏指标。
3. 对高风险任务增加人工抽检。
4. judge 结论只作为候选证据，不能替代人工规则。

---

## 4.7 Human Annotation

URL 形态：`/project/digital-twin-project/annotation-queues`

这是机器分数和人工规则的交汇点。

### 最适合放进人工标注队列的样本

1. 自动分很低，但原因不明确。
2. 自动 judge 与人工直觉冲突。
3. 涉及安全、权限、越权风险。
4. 同类问题频发，需要统一口径。

### 标注时建议写的内容

1. `现象`
2. `怀疑根因`
3. `分流建议`
4. `是否进入 dataset`

### 人工与机器边界

1. 安全和合规判定以人工规则为准。
2. 机器判断可辅助排序，不可覆盖人工硬约束。
3. 人工标注结果要反哺 score 或 tags 命名规范。

---

## 4.8 Datasets

URL 形态：`/project/digital-twin-project/datasets`

这是把线上样本变成可重复验证资产的地方。

没有 dataset，你只能看线上问题，不能稳定复放和比较版本。

### Dataset 应该怎么分

至少分成下面几类：

1. `golden-success`
   - 高质量成功样本，防止新版本把原本做对的事情做坏。
2. `behavior-failures`
   - 行为层问题，供 prompt/策略优化。
3. `retrieval-failures`
   - 检索质量问题，供 RAG 迭代。
4. `tool-failures`
   - 工具或执行链失败样本，供实现层修复。
5. `safety-review`
   - 需要安全复核的样本。

### 什么样本值得加入 dataset

1. 可复现。
2. 代表性强。
3. 问题边界清晰。
4. 能对应某一类改动验证。

### 不值得加入 dataset 的样本

1. 噪音太大。
2. 无法解释。
3. 纯偶发基础设施抖动。
4. 与任何改动方向都不相关。

---

## 4.9 Prompts

URL 形态：`/project/digital-twin-project/prompts`

这部分要严格对应文档中的“Machine Policy Layer”。

不要把 prompt 管理当成单文件文本管理，而要把它看成版本化策略对象。

### 应该管理什么

1. 系统 prompt
2. 任务模板
3. few-shot 示例
4. 输出约束模板
5. 检索提示模板
6. 工具调用指导模板

### 最佳实践

1. 每个 prompt 版本都绑定版本号。
2. 每个版本都写清用途和变更原因。
3. 每个版本都尽量绑定一批 dataset 做验证。
4. 不要直接覆盖默认版本，先走 candidate。

---

## 4.10 Playground

URL 形态：`/project/digital-twin-project/playground`

这是做候选 prompt 快速试验的地方。

### 正确用途

1. 快速试 prompt 版本差异。
2. 验证某个样本在新策略下是否改善。
3. 对比模型、prompt、参数配置。
4. 在少量样本上做人工探索。

### 不该用它代替什么

1. 不代替 dataset 实验。
2. 不代替线上 canary。
3. 不代替 session 级回放。

Playground 适合“快速试”，不适合“正式证明”。

---

## 4.11 Dashboards

URL 形态：`/project/digital-twin-project/dashboards`

这是周度复盘和发布归因的总览页面。

### 至少应该建的仪表盘

1. 质量总览
   - `task_completion`
   - `answer_quality`
   - `retrieval_quality`
2. 运行健康
   - `error_rate`
   - `timeout_count`
   - `rollback_rate`
3. 效率成本
   - `latency_ms`
   - `retrieval_latency_ms`
   - `total_tokens`
   - `cost_per_task`
4. 发布效果
   - 按 `release` / `version` 对比上述指标

### 正确用法

1. 每周固定复盘，而不是出问题才看。
2. 每次候选发布都要做 release 对比。
3. 同时看主指标和护栏指标。
4. 分环境、分用户群体看，而不是只看总体。

---

## 5. 标签、评论、命名规范

如果不定规范，Langfuse 很快会变成“每个人都有自己的叫法”。

### 5.1 标签规范

建议使用三段式：

```text
layer:category:detail
```

示例：

1. `behavior:tool-selection:wrong-tool`
2. `behavior:completion:stopped-early`
3. `retrieval:quality:low-recall`
4. `retrieval:latency:slow-search`
5. `impl:shell:timeout`
6. `impl:encoding:garbled-output`
7. `eval:golden:keep`
8. `risk:security:manual-review`

### 5.2 评论模板

每条高价值问题样本统一写 4 行：

```text
现象：
怀疑原因：
建议分流：
是否进入 dataset：
```

### 5.3 Dataset 命名规范

1. `golden-success-v1`
2. `behavior-failures-v1`
3. `retrieval-failures-v1`
4. `tool-failures-v1`
5. `safety-review-v1`

### 5.4 Prompt / Candidate 命名规范

1. `prompt-main-v12`
2. `prompt-main-exp-2026-03-31-a`
3. `retrieval-policy-v4`
4. `tool-routing-exp-2026-03-31-b`

---

## 6. 日常工作流：每天怎么用 Langfuse

## 6.1 每日巡检

建议每天 15 分钟固定看四类问题：

1. 慢：按 `Latency` 排序。
2. 贵：按 `Total Cost` / `total_tokens` 排序。
3. 差：按低质量分过滤。
4. 错：按 error level、timeout、Exit code 过滤。

### 每日巡检动作

1. 打开 `Tracing`。
2. 切 `Past 1 day`。
3. 依次检查保存好的 6 个视图。
4. 对高价值异常打标签和评论。
5. 把可复用样本加入 dataset。

## 6.2 每周复盘

每周至少做一次发布复盘。

### 复盘动作

1. 打开 `Dashboards` 看总体趋势。
2. 对比不同 `release` / `version`。
3. 检查护栏指标是否恶化。
4. 从 traces 抽 10 到 20 个代表性样本做人审。
5. 更新 dataset 和 annotation 队列。

## 6.3 每次做策略改动前

1. 从 `Tracing` 或 `Sessions` 挑选候选问题样本。
2. 加标签。
3. 加入 dataset。
4. 用 `LLM-as-a-Judge` 或 Playground 做初步比较。
5. 只有收益明确时才进 canary。

## 6.4 每次做实现修复前

1. 先看 `Tracing` 和 `Observations` 是否为重复问题。
2. 评论里写清根因猜测。
3. 单独建 `tool-failures` 或 `impl-failures` dataset。
4. 修复后必须回放这批样本。

---

## 7. 用 Langfuse 支撑文档中的验证与发布机制

文档里要求：

```text
洞察发现 -> 生成候选改动 -> 离线回放 -> 护栏检查 -> canary -> 复盘 -> 全量/回滚
```

在 Langfuse 里的落法是：

1. 洞察发现
   - `Tracing` / `Observations` / `Sessions`
2. 候选改动
   - `Prompts` / 外部策略配置系统
3. 离线回放
   - `Datasets` + `LLM-as-a-Judge`
4. 护栏检查
   - `Scores` + `Dashboards`
5. canary
   - 新 `release` / `version` 上线后继续看 traces
6. 复盘
   - `Dashboards` + 样本抽检
7. 全量或回滚
   - 根据主指标和护栏指标做决定

### 绝对不要省略的三件事

1. `release` / `version` 上报
2. dataset 沉淀
3. score 命名统一

缺其中任何一项，Langfuse 都很难真正支撑可验证迭代。

---

## 8. 你当前这个项目建议立刻落实的配置

结合目前 `digital-twin-project` 页面和已有 traces，建议优先补齐这些内容。

### 第一优先级

1. 把 `environment` 正式拆成 `dev` / `staging` / `prod`。
2. 为所有 trace 稳定上报 `session_id`、`user_id`、`release`、`version`。
3. 统一 metadata 结构。
4. 统一 score 命名。

### 第二优先级

1. 建 6 个 saved views。
2. 建 5 个基础 datasets。
3. 建 4 类 dashboard。
4. 建 annotation 规范。

### 第三优先级

1. 对 prompt/策略候选版本接入 Langfuse Prompts。
2. 对关键样本接入 LLM-as-a-Judge。
3. 对 release 做固定复盘模板。

---

## 9. 最终工作分工建议

为了让 Langfuse 真正服务自我迭代系统，建议团队内部分工明确。

### 开发负责

1. 正确埋点。
2. 上报 metadata、scores、release、version。
3. 修实现层问题。

### 算法或提示工程负责

1. 维护 prompt 版本。
2. 维护 behavior dataset。
3. 运行 judge 和候选比较。

### 产品或运营负责

1. 看 sessions 和 users 趋势。
2. 协助人工标注。
3. 判断哪些问题值得优先修。

### 共同负责

1. 统一标签规范。
2. 统一 score 语义。
3. 统一 release 复盘口径。

---

## 10. 最简落地版本

如果你现在只想先把 Langfuse 用起来，不追求一步到位，最低可执行方案是：

1. 在 `Tracing` 中按环境、trace 名、延迟、质量分巡检。
2. 对问题 trace 打标签和评论。
3. 把高价值样本加入 `behavior-failures` 和 `tool-failures` 两个 dataset。
4. 用 `Scores` 统一看 `retrieval_quality`、`answer_quality`、`total_tokens`。
5. 每周在 `Dashboards` 里按 `release` 做一次对比。

做到这一步，你的 Langfuse 就已经不再只是日志系统，而是文档中“可验证、可回滚、可持续优化”闭环的基础设施。
