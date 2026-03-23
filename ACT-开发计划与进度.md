# ACT — 开发计划与进度

Runtime-first AI Coding Tool · 2026-03-23

---

## 一、四阶段路线

| Phase         | 时间    | 核心模块                              | 交付物                     | 对标水平                              |
| ------------- | ------- | ------------------------------------- | -------------------------- | ------------------------------------- |
| P1 Runtime MVP | 2-4 周  | Framework 骨架 + Harness 6 Tool + CLI + VS Code Extension | VS Code 中可用的 agent MVP | Claude Code CLI Core + VS Code Workflow |
| P2 Runtime 强化 | 4-6 周  | TaskState / Trace / Eval / PatchTransaction + Extension 深化 | VS Code 内完整任务闭环 | Claude Code Runtime Productization |
| P3 Native GUI Beta | 6-10 周 | Native GUI 接入独立 runtime + Repo Map + Git Tool | 原生 GUI Beta | TRAE + VS Code Desktop Architecture |
| P4 多表面统一 | 长期    | ExternalHarness + LSP + 插件系统 + 多表面统一 | CLI / VS Code / Native GUI 一体化 | VS Code Ecosystem + Runtime Platform |

路线原则：先完成独立 runtime 与 VS Code Extension 的产品闭环，再强化 runtime 深水区能力，最后建设 Native GUI 并开放生态扩展。ACT 不是原生 GUI 优先，而是 runtime 优先、VS Code 表面优先、Native GUI 第二阶段。

---

## 二、各阶段任务（按架构层分解）

### P1 Runtime MVP（2-4 周）

目标：先证明 runtime 内核成立，并通过 CLI 与 VS Code Extension 形成最小可用产品闭环，确保 AgentLoop、Tool Runtime、权限确认、结构化错误和 Diff 审核在第一阶段就能被用户直接消费。

**🟡 Agent Framework 层**

- [ ] AgentLoop — 单任务循环、Tool Call 决策、结果推进
- [ ] ContextManager — 消息管理 + 窗口计算
- [ ] PermissionManager — CLI Y/N 确认（注入式设计）
- [ ] TaskState — 单任务运行状态模型（Running / WaitingApproval / ToolRunning / Cancelled / Failed / Completed）
- [ ] Checkpoint 基础结构 — 为长任务取消、失败恢复预留快照接口

**🟠 Agent Harness 层**

- [ ] ITool 接口定义（itool.h）— Framework 与 Harness 的统一契约
- [ ] ToolRegistry — Tool 注册/发现/执行
- [ ] FileReadTool（支持行范围）
- [ ] FileWriteTool
- [ ] FileEditTool（精确替换）
- [ ] ShellExecTool（含超时控制）
- [ ] GlobTool（文件模式匹配）
- [ ] GrepTool（正则搜索）
- [ ] PatchTransaction v0 — 单文件修改预览、确认、提交链路
- [ ] Shell 安全策略 v0 — 工作目录限制、危险命令拦截、基础 allowlist / denylist

**🔵 Core Services 层**

- [ ] AIEngine 门面
- [ ] ILLMProvider 抽象接口
- [ ] AnthropicProvider（SSE 流式）
- [ ] ConfigManager（模型/Key 管理）
- [ ] RuntimeEventLogger v0 — Task / Tool / Permission / Provider 结构化事件日志

**🟢 Presentation Layer**

- [ ] CLI REPL 模式（GNU readline）
- [ ] CLI Permission Handler（Y/N 确认）
- [ ] Markdown 终端输出
- [ ] VS Code Extension MVP（TS → spawn aictl）
- [ ] VS Code Chat / Command / Diff 审核最小闭环

**🧪 测试**

- [ ] Harness 层单元测试（6 个 Tool）
- [ ] Framework 层单元测试（AgentLoop、ContextManager）
- [ ] Harness 层单元测试（ToolRegistry）
- [ ] 端到端测试：aictl agent "读取 main.cpp 并解释"
- [ ] 回归任务集 v0：读取文件、搜索代码、编辑单文件、执行命令、权限拒绝后继续推进

---

### P2 Runtime 强化（4-6 周）

目标：继续强化 runtime 深水区能力，并在 VS Code Extension 内把任务状态、事件流、补丁预演、回归评测等能力产品化，形成第一阶段真正可持续迭代的交付表面。

**🟡 Agent Framework 层**

- [ ] ContextManager 增强：自动压缩策略
- [ ] ResumeTask — 从 Checkpoint 恢复中断任务

**🟠 Agent Harness 层**

- [ ] GitStatusTool
- [ ] GitDiffTool
- [ ] DiffViewTool（修改预览）
- [ ] PatchTransaction v1 — 多文件预览、Accept / Reject、部分失败处理

**🟢 Presentation Layer（VS Code Extension）**

- [ ] Chat View / Side Panel 产品化
- [ ] 命令面板与任务入口
- [ ] VS Code Diff 审核流接入 runtime
- [ ] 任务状态区（显示 Running / Waiting / Failed / Completed）
- [ ] Tool / Permission 事件流面板（最小可观测性 UI）
- [ ] 工作区上下文选择与文件引用体验

**🔵 Core Services 层**

- [ ] RuntimeTraceStore v1 — 按任务聚合 Tool、Permission、Provider 事件
- [ ] EvalRunner v0 — 可执行基础回归任务集并记录通过率

---

### P3 Native GUI Beta（6-10 周）

目标：在 runtime 已经被 CLI 与 VS Code 验证后，接入 Native GUI 作为第二阶段差异化载体，同时补齐 Repo 理解、Git 闭环和多任务编排。

**🟡 Agent Framework 层**

- [ ] AgentScheduler — 并行任务编排
- [ ] AgentScheduler — 串行流水线
- [ ] Task Replay / Resume v1 — 多步任务恢复与复盘

**🟠 Agent Harness 层**

- [ ] GitCommitTool
- [ ] RepoMapTool（tree-sitter）
- [ ] PatchTransaction v2 — 多文件原子提交、回滚、冲突处理
- [ ] 执行隔离策略 v1 — 环境变量隔离、工作区外写入控制、网络访问分级

**🟢 Presentation Layer（Native GUI）**

- [ ] Qt GUI 主窗口（QMainWindow + QDockWidget）
- [ ] 活动栏 + 侧边栏布局（参考 VS Code 的导航心智）
- [ ] AI Chat Panel（cmark Markdown 渲染）
- [ ] 右侧 Agent 面板（参考 TRAE 的任务流与上下文可见性）
- [ ] QScintilla 编辑器集成
- [ ] PermissionDialog（图形化权限确认弹窗）
- [ ] DiffWidget（并排 Diff 预览，Accept/Reject）
- [ ] 底部终端（QTermWidget）
- [ ] 底部终端 / 输出 / 诊断联动（参考 VS Code 的面板组织）

**🔵 Core Services 层**

- [ ] CodeAnalyzer（tree-sitter AST）
- [ ] 多 Provider 支持（OpenAI / Claude / GLM）
- [ ] Fallback 链（主模型失败 → 备用模型）
- [ ] EvalRunner v1 — 标准任务集、回归任务集、人工验收记录
- [ ] Failure Classifier — 将失败归因到模型、工具、权限、上下文或基础设施

---

### P4 多表面统一（长期）

目标：让 ACT 从“已验证的 runtime + 多个产品表面”演进为可接入外部 Harness、IDE 和插件生态的统一运行时平台。

**🟡 Agent Framework 层**

- [ ] 嵌套编排（main → orchestrator → workers）

**🟠 Agent Harness 层**

- [ ] WebFetchTool
- [ ] WebSearchTool
- [ ] ExternalHarnessTool
- [ ] ACP Client（调用 Claude Code / Codex）
- [ ] MCP Client（调用外部 MCP Server）
- [ ] LspTool（clangd / pyright）
- [ ] PluginLoader（动态 .so/.dll 加载）
- [ ] ACP / MCP 权限与审计策略

**🟢 Presentation Layer**

- [ ] VS Code Extension / Native GUI / CLI 多表面统一能力面

**DevOps**

- [ ] GitHub Actions CI（Linux / Windows / macOS）
- [ ] 自动发布（.exe / .AppImage / .dmg）

---

## 三、跨平台策略

- **开发环境**：选最舒服的系统开发，不限制
- **跨平台**：GitHub Actions CI 三平台编译
- **发布**：Windows .exe + Linux .AppImage + macOS .dmg

---

## 四、阶段验收口径

### P1 验收

- `aictl agent "读取 main.cpp 并解释"` 可稳定跑通
- VS Code Extension 中可触发同一任务并完成最小闭环
- 6 个基础 Tool 均有单元测试
- CLI 权限确认、超时控制、结构化错误返回可用
- runtime 可独立跑通，不依赖任何 GUI 组件
- 至少具备单任务 TaskState、基础 Checkpoint 接口和单文件 PatchTransaction
- 回归任务集 v0 可稳定执行，并覆盖权限拒绝、命令失败和单文件修改场景

### P2 验收

- VS Code Extension 内可完成对话、Diff 预览、权限确认闭环
- GitStatusTool / GitDiffTool 能在 VS Code 中展示结果
- Chat、编辑器、终端、侧栏任务区形成稳定联动
- VS Code 表面可展示任务状态、Tool 事件和权限事件，用户能看清 runtime 当前在做什么
- 中断任务可从 Checkpoint 恢复，至少支持单任务恢复到最近一次确认点

### P3 验收

- AgentScheduler 可支持串行流水线与并行 worker 两种编排方式
- Repo Map 和上下文裁剪能在中型仓库中可用
- 多 Provider 和 Fallback 链完成联调
- 可在真实仓库中完成接近 Claude Code CLI 水平的多步任务推进与审阅闭环
- 多文件补丁支持预演、原子提交与失败回滚
- 标准任务集和回归任务集可持续运行，能区分模型问题、工具问题和权限问题
- Native GUI 在不分叉 runtime 的前提下接入，并完成与 VS Code 相同的基础任务闭环

### P4 验收

- 外部 Harness、LSP、插件系统形成稳定扩展点
- 三平台 CI、打包和版本发布流程跑通
- VS Code Extension 与 Native GUI 共用同一套 runtime 能力，不形成分叉实现
- ACP / MCP / Plugin 调用进入统一权限与审计体系，不形成新的安全盲区

---

## 五、运行时补强优先级

### 必须进入 P1

- TaskState 基础模型
- Checkpoint 接口预留
- PatchTransaction v0
- Shell 安全策略 v0
- RuntimeEventLogger v0
- 回归任务集 v0
- VS Code Extension MVP

### 最迟 P2 补齐

- ResumeTask
- PatchTransaction v1
- 任务状态区与事件流面板
- RuntimeTraceStore v1
- EvalRunner v0
- VS Code 表面内的任务状态与事件流体验

### 最迟 P3 补齐

- Task Replay / Resume v1
- PatchTransaction v2
- 执行隔离策略 v1
- EvalRunner v1
- Failure Classifier

---

## 六、进度追踪

项目进度表：https://gcnaf7ct1kpv.feishu.cn/base/WDPPbs2mKakUpzsndzmcr1Qvncf

⚡ 当前进度：尚未开始（等大老板说"开工"）

---

## 七、相关文档

- [ACT — PRD 产品需求文档](./ACT-PRD-%E4%BA%A7%E5%93%81%E9%9C%80%E6%B1%82%E6%96%87%E6%A1%A3.md)
- [ACT — 技术选型报告](./ACT-%E6%8A%80%E6%9C%AF%E9%80%89%E5%9E%8B%E6%8A%A5%E5%91%8A.md)
- [ACT — 系统架构设计](./ACT-%E7%B3%BB%E7%BB%9F%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.md)
- [ACT — 开发计划与进度](./ACT-%E5%BC%80%E5%8F%91%E8%AE%A1%E5%88%92%E4%B8%8E%E8%BF%9B%E5%BA%A6.md)

## 八、术语表

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

## 九、风险矩阵

| 风险ID | 风险描述                                               | 概率 | 影响 | 应对策略                                                      |
| ------ | ------------------------------------------------------ | ---- | ---- | ------------------------------------------------------------- |
| R1     | QScintilla 对 Ghost Text、复杂标记和 Diff 装饰支持不足 | 中   | 高   | 在 P2 前完成编辑器能力验证，不足时补自绘层或调整交互方案      |
| R2     | 自研 LSP Client 成本过高，导致 P4 范围失控             | 中   | 高   | P4 优先封装成熟库，保留渐进替换空间                           |
| R3     | cpp-httplib 在弱网、SSE、长连接下稳定性不足            | 中   | 中   | 提前做 Provider 联调和断网压测，必要时替换 HTTP/SSE 方案      |
| R4     | 权限确认、危险命令拦截和只读模式定义不完整             | 中   | 高   | 在 Framework/Harness 设计阶段固化权限等级、确认流程和拒绝路径 |
| R5     | 三平台依赖版本漂移，导致本地与 CI 结果不一致           | 高   | 中   | 锁定 Qt、CMake、vcpkg baseline 及关键三方库版本               |

---

整理：小欧 🦊 · 2026-03-23
