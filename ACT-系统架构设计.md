# ACT — 系统架构设计

Runtime-first AI Coding Tool · 2026-03-23

## 一、设计理念：CLI 优先，多 Surface 渲染

对标 Claude Code 的架构模式：

|              | Claude Code                  | 我们的方案                                  |
| ------------ | ---------------------------- | ------------------------------------------- |
| 核心语言     | Node.js（TS）                | C++                                         |
| 核心接口     | CLI（claude 命令）           | CLI（aictl 命令）                           |
| 第一产品表面 | CLI / VS Code / Desktop      | CLI + VS Code Extension                     |
| 第二产品表面 | Desktop / 多端同步扩展       | Native GUI（第二阶段接入独立 runtime）      |
| VS Code 集成 | TS 扩展 → spawn CLI          | TS 扩展 → spawn aictl                       |
| Desktop 性能 | Electron（重）               | Native GUI 保留为第二阶段差异化能力         |

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

- 负责任务编排、Agent 循环、上下文管理、权限决策
- 不直接接触文件系统、终端、Git 等具体副作用

**Agent Harness（执行层）**

- 负责 Tool 注册、Tool 执行、权限分级、结果回传
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

---

## 三、Agent Framework + Harness 架构设计

这一拆分不只是工程分层，也是产品战略分层：

- Presentation Layer 对应 ACT 的多表面产品形态，且具备明确的阶段优先级
- Agent Framework + Agent Harness + Core Services 对应 ACT 的 coding agent runtime 内核
- 多 Surface 复用同一 runtime，才能避免 GUI、CLI、Extension 各自长出一套逻辑

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
|                 | PermissionManager | 权限检查、GUI弹窗/CLI确认注入                 |
| Agent Harness   | ToolRegistry      | Tool 注册/发现/执行/权限检查                  |
|                 | ITool 实现        | FileRead/Write/Edit, ShellExec, Glob, Grep 等 |
| Core Services   | AIEngine          | 多模型 LLM 抽象、Streaming、Fallback          |
|                 | ProjectManager    | 工作区、文件、Git 管理                        |
|                 | CodeAnalyzer      | Repo Map、tree-sitter AST                     |

### ITool 接口（Framework 和 Harness 的唯一桥梁）

所有 Tool 必须实现此接口：

- `name()` / `description()` / `schema()` — 元信息（供 LLM 生成 Tool Call）
- `execute(params) → ToolResult` — 实际执行
- `permissionLevel() → Read | Write | Exec | Network | Destructive`

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

### 当前架构缺口清单

当前五层架构已经足以支撑 ACT 的第一代产品，但若目标是稳定逼近 Claude Code CLI 的核心能力，还需补齐以下运行时能力：

- **任务状态持久化**：为 AgentLoop、AgentScheduler 和长任务引入 TaskState、Checkpoint、Resume 机制，避免任务取消、进程退出或 Provider 失败后只能从头开始
- **补丁事务模型**：在 FileWrite / FileEdit / Diff 之上补一层 PatchTransaction，支持多文件变更预览、部分失败处理、回滚和最终提交
- **运行时可观测性**：除统一日志外，还需要 Task Trace、Tool Call Trace、Permission Audit、Model Request 摘要和失败归因，保证问题可定位、任务可复盘
- **评测与对标机制**：为“对标 Claude Code CLI”建立标准任务集、回归测试集和人工验收口径，否则无法判断 runtime 是否真的在变强
- **执行安全策略细化**：在现有权限等级之上继续细化 shell allowlist / denylist、环境变量隔离、网络访问分级和工作区外写入策略

### 开发阶段

| Phase | 时间   | 核心交付                                 |
| ----- | ------ | ---------------------------------------- |
| P1    | 2-4周  | 独立 runtime + CLI + VS Code Extension MVP |
| P2    | 4-6周  | runtime 强化 + VS Code 深度产品化         |
| P3    | 6-10周 | Native GUI Beta + 多Agent编排 + RepoMap   |
| P4    | 长期   | 多表面统一 + ExternalHarness + LSP + 插件系统 |

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

---

## 五、核心运行时职责拆分

### 5.1 Agent Framework

| 组件              | 关键方法        | 功能                          | 对标                  |
| ----------------- | --------------- | ----------------------------- | --------------------- |
| AgentLoop         | executeTask     | 驱动单任务 Agent 循环         | Claude Code agentic   |
|                   | planNextStep    | 决定回复还是调用 Tool         | Cline Tool loop       |
| AgentScheduler    | submitTask      | 多任务入口                    | OpenClaw orchestrator |
|                   | runPipeline     | 串行流水线 / 并行 worker 编排 | 多 Agent 协作         |
| ContextManager    | buildContext    | 上下文拼装与裁剪              | smart context         |
|                   | compressHistory | 长会话压缩                    | context compaction    |
| PermissionManager | checkPermission | 权限判断                      | Claude Code 确认      |
|                   | requestApproval | 注入 GUI/CLI 确认             | permission prompt     |

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
│ Explorer │  + Ghost Text          │  Panel        │
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

## 八、相关文档

- [ACT — PRD 产品需求文档](./ACT-PRD-%E4%BA%A7%E5%93%81%E9%9C%80%E6%B1%82%E6%96%87%E6%A1%A3.md)
- [ACT — 技术选型报告](./ACT-%E6%8A%80%E6%9C%AF%E9%80%89%E5%9E%8B%E6%8A%A5%E5%91%8A.md)
- [ACT — 系统架构设计](./ACT-%E7%B3%BB%E7%BB%9F%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.md)
- [ACT — 开发计划与进度](./ACT-%E5%BC%80%E5%8F%91%E8%AE%A1%E5%88%92%E4%B8%8E%E8%BF%9B%E5%BA%A6.md)

## 九、术语表

| 术语               | 定义                                                   |
| ------------------ | ------------------------------------------------------ |
| ACT                | AI Coding Tool，本文档描述的 C++/Qt6 原生 AI IDE 项目  |
| Presentation Layer | 表现层，包含 CLI、Qt GUI、VS Code Extension 等交互入口 |
| Agent Framework    | 决定任务如何拆解、推进和确认的框架层                   |
| Agent Harness      | 承载 Tool 注册、执行和权限分级的执行层                 |
| Core Services      | 提供模型调用、项目状态、代码分析、配置管理的核心服务层 |
| Infrastructure     | 文件系统、网络、进程、终端等基础设施抽象层             |
| AgentLoop          | 单任务 Agent 循环，负责在回复与 Tool Call 之间做决策   |
| AgentScheduler     | 多任务调度器，负责串行流水线和并行 worker 编排         |
| ITool              | Framework 与 Harness 之间的标准 Tool 接口              |
| Provider           | 大模型服务提供方，例如 OpenAI、Claude、GLM             |
| Repo Map           | 面向仓库结构理解的代码摘要与上下文索引                 |
| Diff 预览          | Agent 落地修改前向用户展示的变更对比视图               |
| Fallback           | 主模型失败后切换到备用模型的降级机制                   |

## 十、风险矩阵

| 风险ID | 风险描述                                               | 概率 | 影响 | 应对策略                                                      |
| ------ | ------------------------------------------------------ | ---- | ---- | ------------------------------------------------------------- |
| R1     | QScintilla 对 Ghost Text、复杂标记和 Diff 装饰支持不足 | 中   | 高   | 在 P2 前完成编辑器能力验证，不足时补自绘层或调整交互方案      |
| R2     | 自研 LSP Client 成本过高，导致 P4 范围失控             | 中   | 高   | P4 优先封装成熟库，保留渐进替换空间                           |
| R3     | cpp-httplib 在弱网、SSE、长连接下稳定性不足            | 中   | 中   | 提前做 Provider 联调和断网压测，必要时替换 HTTP/SSE 方案      |
| R4     | 权限确认、危险命令拦截和只读模式定义不完整             | 中   | 高   | 在 Framework/Harness 设计阶段固化权限等级、确认流程和拒绝路径 |
| R5     | 三平台依赖版本漂移，导致本地与 CI 结果不一致           | 高   | 中   | 锁定 Qt、CMake、vcpkg baseline 及关键三方库版本               |

---

整理：小欧 🦊 · 2026-03-23
