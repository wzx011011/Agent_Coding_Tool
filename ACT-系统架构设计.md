# ACT — 系统架构设计

Runtime-first AI Coding Tool · 2026-03-23

## 一、设计理念：CLI 优先，多 Surface 渲染

对标 Claude Code 的架构模式：

|              | Claude Code             | 我们的方案                             |
| ------------ | ----------------------- | -------------------------------------- |
| 核心语言     | Node.js（TS）           | C++                                    |
| 核心接口     | CLI（claude 命令）      | CLI（aictl 命令）                      |
| 第一产品表面 | CLI / VS Code / Desktop | CLI + VS Code Extension                |
| 第二产品表面 | Desktop / 多端扩展      | Native GUI（第二阶段接入独立 runtime） |
| VS Code 集成 | TS 扩展 → spawn CLI     | TS 扩展 → spawn aictl                  |
| Desktop 性能 | Electron（重）          | Native GUI 保留为第二阶段差异化能力    |

核心思路：一个 runtime core，多个前端。CLI 与 VS Code Extension 是最先成熟的入口，Native GUI 在 runtime 稳定后再接入同一套能力内核。

产品定位上，ACT 对外呈现为 AI IDE；架构上，真正的核心是 Layer 2-4 组成的 runtime，而不是某一个具体 UI 壳层。

基准拆分上，Layer 2-4 的行为对齐对象是 Claude Code CLI；Layer 1 的第一阶段交互基准是 VS Code Extension 工作流，第二阶段桌面信息架构参考 TRAE 与 VS Code。前者决定 ACT 能不能完成真实工程任务，后者决定这些能力最终如何被用户消费。

---

## 二、系统架构总览（五层架构）

```
┌──────────────────────────────────────────────────────────┐
│           LAYER 1 · Presentation Layer（表现层）            │
│  CLI（aictl）· VS Code Extension（第一阶段）· Native GUI（第二阶段） │
├──────────────────────────────────────────────────────────┤
│           LAYER 2 · Agent Framework（Agent 框架层）         │
│   AgentLoop · AgentScheduler · ContextManager · Permission │
│            「决定做什么、按什么顺序」                         │
├─────────────────── ITool Interface（唯一桥梁）────────────┤
│           LAYER 3 · Agent Harness（Agent 执行层）            │
│     ToolRegistry · FileRead/Write/Edit · ShellExec · ...  │
│            「决定具体怎么执行」                               │
├──────────────────────────────────────────────────────────┤
│           LAYER 4 · Core Services（核心服务层）              │
│     AIEngine · ProjectManager · CodeAnalyzer · ConfigMgr  │
├──────────────────────────────────────────────────────────┤
│           LAYER 5 · Infrastructure（基础设施层）             │
│               fs · network · process · terminal            │
└──────────────────────────────────────────────────────────┘
```

### 架构演进：三层 → 五层

原三层架构的 Core Layer 是一个大杂烩，把调度、工具执行、AI 引擎全堆在一起。现在将它拆解为三个独立层：

- **Agent Framework（Layer 2）**：原来 Core 中的 AgentLoop、权限控制、上下文管理 → 抽取为独立的框架层
- **Agent Harness（Layer 3）**：原来 Core 中的 Tool Calling / 执行 → 抽取为独立的执行层
- **Core Services（Layer 4）**：原来 Core 中的 AIEngine、ProjectManager、CodeAnalyzer → 留为核心服务层

三者通过 **ITool 标准接口** 解耦，各自独立演进。

### Runtime 机制演进原则

ACT 吸收 Claude Code 类 runtime 的经验后，明确采用“稳定 loop + 机制外插”的演进方式：

- Agent Loop 尽量保持小而稳定，不因新增能力不断改写主循环
- 新能力作为独立运行时机制叠加到 loop 周围，而不是拼接为巨大的条件分支
- 运行时能力按六类机制持续演进：Tool Dispatch、Skill Injection、Subagent Isolation、Context Compact、Task Graph、Execution Isolation

这意味着 ACT 的核心竞争力不是单个 Tool 或单个 UI，而是这些机制能否形成统一、可恢复、可观测、可扩展的 runtime。

### 各层职责

这五层里，Layer 1 决定产品如何被使用，Layer 2-4 决定产品是否真的具备 agentic coding 能力。因此 ACT 的路线不是“先做 IDE 再补 AI”，而是“先做 runtime，再把 runtime 投射到 IDE”。

**Presentation Layer（表现层）**

- 纯 UI 壳，不含业务逻辑
- 第一阶段前端：CLI（aictl）与 VS Code Extension
- 第二阶段前端：Native GUI
- VS Code Extension 通过 spawn `aictl` 复用 CLI 能力，而不是直连内部对象
- Native GUI 在第二阶段直接调用 Layer 2-4 暴露的运行时接口，但不得形成分叉实现
- 第一阶段优先复用 VS Code 的成熟编辑器与工作区能力，第二阶段再补原生桌面的信息架构差异化

**Agent Framework（框架层）**

- 负责任务编排、Agent 循环、上下文管理、权限决策、子任务隔离
- 不直接接触文件系统、终端、Git 等具体副作用

**Agent Harness（执行层）**

- 负责 Tool 注册、Tool 执行、权限分级、结果回传、技能注入、执行通道适配
- 通过 ITool 将具体副作用能力暴露给 Framework

**Core Services（核心服务层）**

- 负责模型调用、项目状态、代码分析、配置管理等基础服务
- 为 Framework 和 Harness 提供共享能力，但不承担 UI 逻辑

**Infrastructure Layer（基础设施层）**

- 文件系统、网络、进程管理、终端的抽象封装
- 为 Layer 2-4 提供统一的基础能力接口

### 依赖方向

- Presentation Layer 只能依赖 Layer 2-4 暴露的接口，不得下沉到基础设施细节
- Agent Framework 依赖 Harness 和 Core Services，不直接操作 Infrastructure
- Agent Harness 依赖 Core Services 和 Infrastructure，不负责调度策略
- Core Services 依赖 Infrastructure，不反向依赖 Presentation
- 整体依赖方向必须单向向下，避免 Qt GUI 反向侵入核心运行时

### 层间标准接口

ITool 定义了 Layer 2（Framework）与 Layer 3（Harness）之间的契约。为了让 Core Services 和 Infrastructure 也具备同等的可替换性，需要补充两组对称接口：

**IService 接口族（Layer 2/3 → Layer 4 的契约）**

| 接口            | 职责                                     |
| --------------- | ---------------------------------------- |
| IAIEngine       | 模型调用、流式补全、多轮对话与 Tool Call |
| IProjectManager | 工作区状态、文件列表、路径解析           |
| ICodeAnalyzer   | Repo Map 构建、AST 分析、上下文裁剪      |
| IConfigManager  | 模型/Key/Settings 读写                   |

所有 Core Service 必须面向接口编程，Framework 和 Harness 只依赖 `IService` 指针，不直接 `#include` 具体实现头文件。

**IInfrastructure 接口族（Layer 3/4 → Layer 5 的契约）**

| 接口        | 职责                                 |
| ----------- | ------------------------------------ |
| IFileSystem | 文件读写、目录遍历、路径规范化       |
| INetwork    | HTTP/SSE 请求、连接管理              |
| IProcess    | 子进程创建、输入输出、超时、信号控制 |
| ITerminal   | 终端复用、PTY 管理                   |

这组接口使得单元测试可以注入 Mock 实现，同时确保 Infrastructure 层的具体方案可替换（如将来替换 cpp-httplib）。

---

## 三、Agent Framework + Harness 架构设计

这一拆分不只是工程分层，也是产品战略分层：

- Presentation Layer 对应 ACT 的多表面产品形态，且具备明确的阶段优先级
- Agent Framework + Agent Harness + Core Services 对应 ACT 的 coding agent runtime 内核
- 多 Surface 复用同一 runtime，才能避免 GUI、CLI、Extension 各自长出一套逻辑

### 核心机制栈

吸收 learn-claude-code 这类逆向教学仓库之后，ACT 将运行时机制明确拆为以下七个可独立演进的子系统：

| 机制                | ACT 组件                               | 作用                                       |
| ------------------- | -------------------------------------- | ------------------------------------------ |
| Stable Loop         | AgentLoop                              | 保持主循环稳定，所有高级能力围绕它叠加     |
| Tool Dispatch       | ToolRegistry + ITool                   | 统一工具定义、执行、权限和审计入口         |
| Skill Injection     | SkillCatalog + SkillLoader             | 两层技能注入，避免 system prompt 膨胀      |
| Subagent Isolation  | SubagentManager                        | 子任务使用独立消息空间，主上下文只接收摘要 |
| Context Compact     | ContextManager + ContextCompactor      | 微压缩、自动压缩、手动压缩                 |
| Task Graph          | TaskManager + TaskStateStore           | 目标状态、依赖和恢复点的持久化             |
| Execution Isolation | ExecutionLaneManager / WorktreeManager | 执行目录、后台任务、并行通道与任务状态解耦 |

### 为什么 Framework + Harness 分离？

参考 Claude Code / OpenClaw / Cline 的行业最佳实践：

- **Framework** 决定"做什么、按什么顺序"：调度、编排、上下文管理、权限控制
- **Harness** 决定"具体怎么执行"：文件读写、终端命令、Git 操作、代码搜索
- 两者通过 **ITool 标准接口** 解耦，各自独立演进

### 关键组件

| 层              | 核心组件          | 职责                                          |
| --------------- | ----------------- | --------------------------------------------- |
| Agent Framework | AgentLoop         | 单轮推理、Tool Call 决策、任务推进            |
|                 | AgentScheduler    | 多任务编排、串并行调度、Worker 协作           |
|                 | ContextManager    | 消息管理、窗口计算、自动压缩                  |
|                 | SubagentManager   | 生成只读/可写子智能体并隔离上下文             |
|                 | PermissionManager | 权限检查、GUI弹窗/CLI确认注入                 |
| Agent Harness   | ToolRegistry      | Tool 注册/发现/执行/权限检查                  |
|                 | ITool 实现        | FileRead/Write/Edit, ShellExec, Glob, Grep 等 |
|                 | SkillCatalog      | 技能目录扫描、摘要暴露、正文按需注入          |
|                 | ExecutionLane     | CLI、Background、Worktree 等执行通道适配      |
| Core Services   | AIEngine          | 多模型 LLM 抽象、Streaming、Fallback          |
|                 | ProjectManager    | 工作区、文件、Git 管理                        |
|                 | CodeAnalyzer      | Repo Map、tree-sitter AST                     |
|                 | TaskStateStore    | TaskState、Checkpoint、Resume 元数据持久化    |

### ITool 接口（Framework 和 Harness 的唯一桥梁）

所有 Tool 必须实现此接口：

- `name()` / `description()` / `schema()` — 元信息（供 LLM 生成 Tool Call）
- `execute(params) → ToolResult` — 实际执行
- `permissionLevel() → Read | Write | Exec | Network | Destructive`

### SkillLoader：两层技能注入

ACT 不把所有 workflow 说明、代码规范、工具约束全文放进 system prompt，而采用两层注入：

- **Layer 1**：系统提示中仅常驻技能名称、简介、标签和适用条件
- **Layer 2**：模型需要时调用 `load_skill`，将完整技能正文以 `tool_result` 注入当前上下文

设计目的：

- 避免 system prompt 被大量低命中率知识膨胀
- 让技能像 Tool 一样进入统一审计和 Trace 链路
- 支持多个技能叠加，但只让真正被调用的内容消耗 token

SkillLoader 属于 Harness 层，因为它本质上是“向模型注入外部知识的执行机制”，而不是调度器。

### Subagent：上下文隔离而非消息堆叠

ACT 的子智能体不是主会话里再塞一轮 prompt，而是独立 runtime 会话：

- 主 Agent 通过 `run_subagent` / `task` 触发子任务
- 子智能体获得独立 `messages[]`、受限工具集和角色提示
- 子智能体完成后只返回摘要、结论、引用位置和结构化结果
- 主会话禁止直接吞入子会话完整历史，避免上下文污染

第一阶段至少支持两类子智能体：

- `Explore`：只读工具集，用于搜索、阅读、定位信息
- `Code`：完整工具集，用于实现类子任务

后续可扩展到 `Review`、`Test`、`Plan` 等角色。

### 权限等级

| 等级        | 含义            | 行为           |
| ----------- | --------------- | -------------- |
| Read        | 读取文件/搜索   | 安全，自动放行 |
| Write       | 写入/创建文件   | 需确认         |
| Exec        | 执行 shell 命令 | 需确认         |
| Network     | 网络请求        | 需确认         |
| Destructive | 删除/强制操作   | 严格确认       |

### 权限注入设计

GUI 和 CLI 共用同一套权限逻辑，仅表现层不同：

- **GUI**：弹出确认对话框，显示命令详情
- **CLI**：终端输出 `[y/N]` 等待用户确认

### 安全边界

- Read 级 Tool 默认放行，但仍受工作区边界约束
- Write / Exec / Network 必须在 Tool 执行前经过 PermissionManager
- Destructive 级操作默认二次确认，并要求展示明确影响范围
- ShellExecTool 必须支持危险命令拦截、超时控制、工作目录限制
- 系统需支持只读模式，在该模式下拒绝所有 Write / Exec / Destructive 请求

### 错误恢复策略

- Tool 执行失败时，Harness 返回结构化错误，Framework 决定是否重试、降级或终止
- LLM 请求失败时，AIEngine 可按配置触发重试或 Fallback Provider
- 用户拒绝权限时，AgentLoop 应将拒绝结果作为显式上下文继续推理，而不是静默失败
- 长任务必须支持取消，避免 GUI 和 CLI 被阻塞

### Context Compact：三层压缩策略

ACT 明确采用三层上下文压缩，而不是只在 token 超限时做一次摘要：

1. **Micro Compact**：把较旧的 ToolResult、权限结果、技能正文替换为短占位符或摘要引用
2. **Auto Compact**：达到阈值后，归档完整会话片段并生成阶段摘要，替换中旧消息
3. **Manual Compact**：模型或用户显式触发 compact，对当前长任务进行主动收缩

压缩的目标不是单纯“省 token”，而是维持任务推进的因果链：决策、拒绝、失败、补丁确认和技能加载都必须以可复盘的形式保留下来。

### Task Graph 与 Execution Lane 解耦

ACT 不将任务和执行目录混为一个概念，而是拆成两个正交模型：

- **Task Graph**：记录任务目标、状态、依赖、责任人、Checkpoint、评测结果
- **Execution Lane**：记录任务在哪个执行通道中运行，如当前工作区、后台通道或独立 worktree

这样设计的收益：

- 任务状态可以持久化和恢复，而不依赖具体目录是否仍存在
- 并行任务可以共享同一任务系统，但各自拥有独立执行空间
- Background Run、Worktree Run、GUI 触发执行都能进入同一 runtime 观察链路

### 运行时事件总线（RuntimeEventBus）

文档提到"GUI 用 signal/slot，CLI 用回调"，但 Layer 2-4 之间的异步通信机制需要形式化定义。引入轻量 RuntimeEventBus，统一所有运行时事件的分发：

**核心事件类型**

| 事件                  | 生产者            | 消费者                     |
| --------------------- | ----------------- | -------------------------- |
| TaskStateChanged      | AgentLoop         | Presentation（状态区）     |
| ToolExecutionProgress | ToolRegistry      | Presentation（事件流）     |
| StreamToken           | AIEngine          | ChatPanel / CLI 输出       |
| PermissionRequested   | PermissionManager | GUI弹窗 / CLI Y/N          |
| PermissionResolved    | Presentation      | PermissionManager          |
| ErrorOccurred         | 任意层            | Presentation + EventLogger |

**设计约束**

- EventBus 属于 Infrastructure 层，Layer 2-4 均可发布和订阅
- Presentation 层只订阅事件，不反向推送状态到 runtime
- RuntimeEventLogger（持久化侧）和 RuntimeEventBus（实时分发侧）共享同一套事件类型定义
- 事件传递必须异步非阻塞，避免 Tool 执行被 UI 渲染阻塞

### 并发访问模型

P3 引入 AgentScheduler 支持并行 worker 编排后，多个 Agent/Worker 可能同时读写同一组文件。需要在 Harness 层引入 FileLockManager 协调并发访问：

- **读操作**：共享锁，多 Agent 可并行读取同一文件
- **写操作**：排他锁，同一时刻只允许一个 Agent 写入特定文件
- **策略选择**：P1 采用乐观锁（提交时检测冲突），P3 可升级为悲观锁（写前加锁）
- **与 PatchTransaction 集成**：PatchTransaction v2 的原子提交必须在持有所有目标文件写锁的前提下执行
- **死锁预防**：按文件路径字典序加锁，避免循环等待

### 当前架构缺口清单

当前五层架构已经足以支撑 ACT 的第一代产品，但若目标是稳定逼近 Claude Code CLI 的核心能力，还需补齐以下运行时能力：

- **任务状态持久化**：为 AgentLoop、AgentScheduler 和长任务引入 TaskState、Checkpoint、Resume 机制，避免任务取消、进程退出或 Provider 失败后只能从头开始
- **补丁事务模型**：在 FileWrite / FileEdit / Diff 之上补一层 PatchTransaction，支持多文件变更预览、部分失败处理、回滚和最终提交
- **运行时可观测性**：除统一日志外，还需要 Task Trace、Tool Call Trace、Permission Audit、Model Request 摘要和失败归因，保证问题可定位、任务可复盘
- **评测与对标机制**：为“对标 Claude Code CLI”建立标准任务集、回归测试集和人工验收口径，否则无法判断 runtime 是否真的在变强
- **执行安全策略细化**：在现有权限等级之上继续细化 shell allowlist / denylist、环境变量隔离、网络访问分级和工作区外写入策略

### 开发阶段

| Phase | 时间   | 核心交付                                           |
| ----- | ------ | -------------------------------------------------- |
| P1    | 2-4周  | 独立 runtime + CLI + VS Code Extension MVP         |
| P2    | 4-6周  | Skill / Subagent / Context Compact / Runtime Trace |
| P3    | 6-10周 | Task Graph + Execution Lane + Native GUI Beta      |
| P4    | 长期   | 多Agent协作 + ExternalHarness + LSP + 插件系统     |

---

## 四、Layer 2-4 设计原则

- **只依赖 QtCore**：不 include 任何 Qt Widget 头文件
- **异步 everywhere**：GUI 用 signal/slot，CLI 用回调
- **权限注入**：GUI 弹窗确认 / CLI Y/N 确认
- **统一日志**：可输出到 GUI 或终端
- **可测试**：脱离 UI 单独跑单元测试
- **单向依赖**：Framework 不直接碰基础设施，Harness 不承载调度策略
- **失败可恢复**：超时、拒绝、网络中断都必须有结构化返回
- **界面与内核解耦**：Presentation 只负责显示和交互编排，不承载 AgentLoop、Tool Runtime 和权限决策本体
- **缓存意识**：AST / Repo Map / 文件内容 / Token 计算等高开销结果需缓存，基于文件修改时间戳 + hash 失效，在 Core Services 或 Infrastructure 层引入 CacheManager

---

## 五、核心运行时职责拆分

### 5.1 Agent Framework

| 组件           | 关键方法        | 功能                          | 对标                  |
| -------------- | --------------- | ----------------------------- | --------------------- |
| AgentLoop      | executeTask     | 驱动单任务 Agent 循环         | Claude Code agentic   |
|                | planNextStep    | 决定回复还是调用 Tool         | Cline Tool loop       |
| AgentScheduler | submitTask      | 多任务入口                    | OpenClaw orchestrator |
|                | runPipeline     | 串行流水线 / 并行 worker 编排 | 多 Agent 协作         |
| ContextManager | buildContext    | 上下文拼装与裁剪              | smart context         |
|                | compressHistory | 长会话压缩（见下方压缩策略）  | context compaction    |

**ContextManager 压缩策略**

`compressHistory` 支持三种压缩模式，按成本从低到高依次尝试：

1. **截断模式（Truncate）**：丢弃最早的 N 条消息，保留系统提示和最近上下文。零额外开销，适用于对话历史不重要的场景。
2. **摘要模式（Summarize）**：调用 LLM 对前半段对话生成 1-2 段摘要，替换原始消息。消耗一次模型请求，适用于长任务需保留关键决策的场景。
3. **混合模式（Hybrid）**：对最早 60% 截断，对中间 30% 摘要，保留最近 10% 原始消息。平衡成本与信息保留。

默认使用混合模式。压缩触发阈值为当前 token 窗口的 80%，压缩后目标为窗口的 50%。
| PermissionManager | checkPermission | 权限判断 | Claude Code 确认 |
| | requestApproval | 注入 GUI/CLI 确认 | permission prompt |

### 5.2 Agent Harness

| 组件                                        | 关键方法     | 功能                         | 对标                 |
| ------------------------------------------- | ------------ | ---------------------------- | -------------------- |
| ToolRegistry                                | registerTool | Tool 注册和发现              | Cline Tool 系统      |
|                                             | executeTool  | 执行 Tool 并回传结果         | Tool runtime         |
| FileReadTool / FileWriteTool / FileEditTool | execute      | 文件读写与精确编辑           | Read/Write/Edit Tool |
| ShellExecTool                               | execute      | 命令执行、超时控制、输出收集 | shell tool           |
| GitStatusTool / GitDiffTool                 | execute      | Git 状态和 Diff 查询         | Aider git            |

### 5.3 Core Services

| 组件           | 关键方法                | 功能                         | 对标                 |
| -------------- | ----------------------- | ---------------------------- | -------------------- |
| AIEngine       | setProvider             | 切换模型                     | Claude Code --model  |
|                | requestCompletion       | 流式补全                     | Claude Code Tab 补全 |
|                | chat                    | 多轮对话与 Tool Calling 请求 | Claude Code chat     |
| ProjectManager | openProject             | 打开项目和工作区状态维护     | claude 命令          |
|                | listFiles / resolvePath | 项目路径与文件能力封装       | workspace service    |
| CodeAnalyzer   | buildRepoMap            | 代码结构图                   | Aider repo_map       |
|                | getContextForQuery      | 智能裁剪上下文               | smart context        |
| ConfigManager  | currentModel / setModel | 模型管理                     | /model 命令          |
|                | apiKey / settings       | API Key 和本地配置           | config               |

---

## 六、第二阶段 Native GUI 布局

```
┌──────────────────────────────────────────────────┐
│  菜单栏 · 工具栏 · 模型选择                        │
├──────────┬───────────────────────┬───────────────┤
│ Left Dock│     Central Widget     │  Right Dock   │
│          │                        │               │
│ File     │  QScintilla 编辑器      │  AI Chat      │
│ Explorer │  + Ghost Text（⚠ 需P2前验证） │  Panel   │
│          │                        │  (Markdown)   │
│ Search   │  Diff 预览              │  模型切换      │
│ Symbol   │  确认/拒绝              │               │
│ Outline  │                        │               │
├──────────┴───────────────────────┴───────────────┤
│  Bottom Dock: QTermWidget 终端 + 诊断 + 输出      │
└──────────────────────────────────────────────────┘
```

布局原则：该布局仅在第二阶段 Native GUI 接入时生效。第一阶段优先依托 VS Code 的成熟编辑器、终端、侧边栏与面板系统；第二阶段的 Native GUI 仍需保持左侧导航、中央编辑与 Diff、右侧 Agent 面板、底部终端输出的工作流，但必须复用同一套 runtime，而不是自带独立逻辑。

---

## 七、CLI 命令

| 命令              | 功能        | 对标 Claude Code |
| ----------------- | ----------- | ---------------- |
| aictl             | 交互式 REPL | claude           |
| aictl completions | 流式补全    | Tab 补全         |
| aictl agent       | Agent 模式  | agentic 模式     |
| aictl diff        | Diff 预览   | diff 展示        |
| aictl config      | 配置管理    | claude config    |
| aictl repo-map    | Repo Map    | context 展示     |

---

## 八、安全架构

对于一个可以执行 shell 命令、读写文件、访问网络的 Agent 工具，安全设计是一等公民。本节将分散在各处的安全约束统一为独立架构章节。

### API Key 存储策略

| 平台    | 首选方案                   | 降级方案         |
| ------- | -------------------------- | ---------------- |
| Windows | Windows Credential Manager | 加密本地配置文件 |
| macOS   | Keychain                   | 加密本地配置文件 |
| Linux   | libsecret / GNOME Keyring  | 加密本地配置文件 |

- 禁止明文存储 API Key
- 配置文件中只保存加密后的密文，运行时解密到内存
- 环境变量注入的 API Key 不落盘

### 工作区沙箱

- 默认仅允许访问当前工作区目录（`cwd`）
- 跨目录读取需 Read 级确认，跨目录写入需 Write 级确认
- 系统关键路径（`/etc`、`C:\Windows`、用户主目录下的 `.ssh`）列入全局禁写名单
- 工作区边界由 ProjectManager 维护，Tool 执行前由 PermissionManager 校验路径合法性

### Shell 命令安全

| 策略           | 说明                                                         |
| -------------- | ------------------------------------------------------------ | ---------- |
| 危险命令黑名单 | `rm -rf /`、`format`、`mkfs`、`dd if=`、`:(){ :              | :& };:` 等 |
| 安全命令白名单 | `ls`、`cat`、`grep`、`find`、`git status`、`git diff` 等     |
| 工作目录限制   | ShellExecTool 强制在工作区目录内执行，禁止 `cd /` 逃逸       |
| 超时控制       | 默认 30s 超时，用户可配置上限 300s                           |
| 输出截断       | 单次命令输出上限 100KB，超出截断并提示                       |
| 环境变量隔离   | 子进程仅继承白名单环境变量，API Key 等敏感变量不泄露给 shell |

### 网络访问分级

| 等级     | 说明                                               | 行为     |
| -------- | -------------------------------------------------- | -------- |
| Provider | 访问已配置的 LLM API Endpoint（OpenAI/Claude/GLM） | 自动放行 |
| Internal | 访问 localhost / 127.0.0.1                         | 自动放行 |
| External | 访问任意外部 URL（WebFetchTool）                   | 需确认   |
| Blocked  | SSRF 高危目标（云元数据 169.254.169.254 等）       | 强制拦截 |

### 会话数据安全

- 对话历史、TaskState、Checkpoint 存储在工作区 `.act/` 目录下
- 包含 API Key 的消息在持久化前脱敏（替换为 `***`）
- `.act/` 目录自动加入 `.gitignore`，避免意外提交

### P4 外部调用信任边界

- ACP/MCP 外部 Server 调用必须经过 PermissionManager，等级至少为 Network
- 外部 Tool 返回结果视为不可信输入，禁止直接拼接为 shell 命令或文件路径
- 外部 Harness 注册需显式用户授权，运行时权限不高于内置 Tool

---

## 九、相关文档

- [ACT — PRD 产品需求文档](./ACT-PRD-%E4%BA%A7%E5%93%81%E9%9C%80%E6%B1%82%E6%96%87%E6%A1%A3.md)
- [ACT — 技术选型报告](./ACT-%E6%8A%80%E6%9C%AF%E9%80%89%E5%9E%8B%E6%8A%A5%E5%91%8A.md)
- [ACT — 系统架构设计](./ACT-%E7%B3%BB%E7%BB%9F%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.md)
- [ACT — 开发计划与进度](./ACT-%E5%BC%80%E5%8F%91%E8%AE%A1%E5%88%92%E4%B8%8E%E8%BF%9B%E5%BA%A6.md)
- [ACT — 术语表](./ACT-%E6%9C%AF%E8%AF%AD%E8%A1%A8.md)
- [ACT — 风险矩阵](./ACT-%E9%A3%8E%E9%99%A9%E7%9F%A9%E9%98%B5.md)

## 十、术语表

> 完整术语表维护于 [ACT — 术语表](./ACT-%E6%9C%AF%E8%AF%AD%E8%A1%A8.md)，此处仅列本文档新增术语。

| 术语            | 定义                                                      |
| --------------- | --------------------------------------------------------- |
| IService        | Core Services 层向 Framework/Harness 暴露的标准服务接口族 |
| IInfrastructure | Infrastructure 层向上层暴露的标准基础设施接口族           |
| RuntimeEventBus | 运行时事件实时分发总线，Layer 2-4 发布、Presentation 订阅 |
| FileLockManager | Harness 层文件并发访问协调器，提供共享锁和排他锁          |
| CacheManager    | 高开销计算结果的缓存管理器，基于时间戳+hash失效           |

## 十一、风险矩阵

> 完整风险矩阵维护于 [ACT — 风险矩阵](./ACT-%E9%A3%8E%E9%99%A9%E7%9F%A9%E9%98%B5.md)，此处保留本文档相关风险。

| 风险ID | 风险描述                                               | 概率 | 影响 | 应对策略                                                      |
| ------ | ------------------------------------------------------ | ---- | ---- | ------------------------------------------------------------- |
| R1     | QScintilla 对 Ghost Text、复杂标记和 Diff 装饰支持不足 | 中   | 高   | 在 P2 前完成编辑器能力验证，不足时补自绘层或调整交互方案      |
| R2     | 自研 LSP Client 成本过高，导致 P4 范围失控             | 中   | 高   | P4 优先封装成熟库，保留渐进替换空间                           |
| R3     | cpp-httplib 在弱网、SSE、长连接下稳定性不足            | 中   | 中   | 提前做 Provider 联调和断网压测，必要时替换 HTTP/SSE 方案      |
| R4     | 权限确认、危险命令拦截和只读模式定义不完整             | 中   | 高   | 在 Framework/Harness 设计阶段固化权限等级、确认流程和拒绝路径 |
| R5     | 三平台依赖版本漂移，导致本地与 CI 结果不一致           | 高   | 中   | 锁定 Qt、CMake、vcpkg baseline 及关键三方库版本               |
| R6     | P1 范围过大导致延期，影响团队信心                      | 高   | 高   | 拆分 P1a/P1b，先证明最小闭环再补齐能力                        |
| R7     | 层间通信未形式化，P2 加 GUI 时大量返工                 | 中   | 高   | P1 定义 RuntimeEventBus 接口和 IService 接口族                |
| R8     | 无 CI 导致三平台问题积累到 P4 才发现                   | 高   | 中   | P1 搭建单平台 CI，P2 扩展三平台                               |
| R9     | 并发 Agent 文件冲突导致数据丢失                        | 低   | 高   | P1 预留 FileLockManager 接口，P3 实现完整并发控制             |
| R10    | API Key 明文存储导致安全事故                           | 中   | 高   | P1 确定平台 Keychain 存储方案，禁止明文落盘                   |

---

整理：小欧 🦊 · 2026-03-23
