# ACT — PRD 产品需求文档

Runtime-first AI Coding Tool · 2026-03-23

---

## 一、项目概述

- **项目名称**：AI Coding Tool（简称 ACT）
- **一句话定位**：以独立 coding agent runtime 为内核、以 CLI 与 VS Code Extension 为第一阶段交付表面、以原生 GUI 为第二阶段差异化载体的 AI-native 开发环境。
- **核心卖点**：先把可执行任务闭环做强，再决定表面形态，而不是先做一个 GUI 壳子。

---

## 二、现状与问题

当前主流 AI Coding 工具（Cursor、Windsurf、Claude Code）全部基于 Web 技术栈（Electron/TypeScript），带来了不可忽视的问题：

| 痛点           | 具体表现                                           | 影响                  |
| -------------- | -------------------------------------------------- | --------------------- |
| 内存占用高     | Cursor 空载 ~500MB，打开大项目后 1-2GB             | 低配机器卡顿          |
| 启动慢         | Electron 冷启动 2-5 秒                             | 丧失原生即开即用感    |
| 文件操作绕远路 | Web 技术无法直接调用系统 API，需 IPC 桥接          | 大文件/终端 IO 有延迟 |
| 资源焦虑       | IDE + 浏览器 + 终端 + AI 工具，Electron 再吃几个 G | 开发体验差            |

---

## 三、我们要做什么

### 3.1 产品定义

ACT 首先是一套面向真实软件工程任务的独立 coding agent runtime，其次才是承载这套 runtime 的产品表面。第一阶段以 CLI 与 VS Code Extension 为主交付，用最小成本验证 Claude Code CLI 风格的任务闭环、Tool Runtime、权限确认、Diff 审核、可恢复执行和评测体系；第二阶段再建设原生 GUI，作为性能、资源占用和桌面体验上的差异化载体。

产品基准拆分如下：**核心能力基准** 对标 Claude Code CLI，重点对齐任务闭环、Tool 调用、上下文裁剪、权限确认、Diff 审核和长任务可恢复；**第一阶段产品形态基准** 参考 VS Code 的成熟编辑器和扩展工作流；**第二阶段桌面形态基准** 参考 TRAE 与 VS Code 的信息架构，但不牺牲 runtime 独立性。

具备以下核心能力：

1. **AI 代码补全** — 编辑器内联 Ghost Text（类似 GitHub Copilot）
2. **AI 对话** — 在 CLI 和 VS Code 中支持多模型切换（OpenAI / Claude / GLM）
3. **Agent 模式** — AI 自主执行任务（读写文件、运行命令、Git 操作），需用户确认。采用 Agent Framework + Harness 分层架构，Tool 能力可插拔扩展
4. **代码理解** — Repo Map、AST 分析、智能上下文裁剪
5. **Diff 预览** — Agent 修改前展示 Diff，用户确认/拒绝

### 3.2 产品边界

- **不是纯补全插件**：目标不是依附现有 IDE 提供局部 AI 能力，而是提供完整开发环境
- **不是纯聊天助手**：目标不是回答问题，而是推进任务、调用工具、落地修改并接受用户确认
- **不是纯 Agent Framework**：目标不是只提供 SDK，而是将 runtime 以 CLI / VS Code Extension / Native GUI 形式交付给开发者
- **不是原生 GUI 优先**：路线顺序是先建立 runtime 闭环和 VS Code 内的产品验证，再把能力稳定投射到原生 GUI

### 3.3 目标用户

- 对 Electron 内存占用不满的开发者
- 当前主要在 VS Code 中工作，但需要更强 Agent 能力闭环的开发者
- 追求长期原生体验和低资源占用，但接受原生 GUI 在第二阶段交付的开发者
- 需要本地运行、低资源占用的场景（嵌入式开发、远程开发）
- 偏好 CLI 工作流，但偶尔需要 GUI 的开发者

### 3.4 成功标准

| 指标             | 目标                                                                 |
| ---------------- | -------------------------------------------------------------------- |
| P1 交付形态      | CLI + VS Code Extension 形成最小可用产品闭环                         |
| 核心能力基准     | P1 形成可对标 Claude Code CLI 的 runtime 最小闭环                    |
| 任务可恢复性     | P2 前具备 TaskState、Checkpoint、Resume 基础能力                     |
| 支持模型         | OpenAI / Claude / GLM 至少 3 家                                      |
| Tool 扩展        | 支持通过 ITool 接口自定义扩展                                        |
| 原生 GUI 交付节奏 | Native GUI 作为第二阶段 Beta，不阻塞 P1 / P2 的 runtime 能力验证     |

### 3.5 功能验收标准

| 能力       | 验收标准                                                                     |
| ---------- | ---------------------------------------------------------------------------- |
| AI 对话    | 用户可在 CLI 和 VS Code Extension 中完成至少 3 轮连续对话，且支持中途切换模型 |
| Agent 模式 | Agent 在执行写文件、执行命令、网络访问前必须出现权限确认；被拒绝后可继续对话 |
| Diff 预览  | Agent 修改代码前必须展示 Diff，用户可明确 Accept / Reject                    |
| Repo Map   | 可在中型仓库中生成可用的代码结构摘要，并参与上下文裁剪                       |
| 多模型切换 | 同一任务流中可切换 Provider，失败时支持 fallback 到备用模型                  |

### 3.6 非功能约束

- **工作区边界**：默认仅允许访问当前工作区，跨目录写入需额外确认
- **安全性**：危险命令、删除操作、网络请求必须显式确认，不允许静默执行
- **弱网体验**：模型请求失败时应返回结构化错误，并支持重试或 fallback
- **可恢复性**：长任务必须支持取消，用户拒绝权限后系统不能卡死或丢失上下文
- **跨平台**：Windows、Linux、macOS 的核心能力保持一致，平台差异仅体现在打包和系统集成层
- **Runtime First**：核心 runtime 必须可在 CLI 中独立工作，VS Code Extension 和 Native GUI 都只是能力载体，不得反向绑定核心逻辑
- **Surface Priority**：第一阶段优先保证 VS Code Extension 与 CLI 的体验闭环，原生 GUI 不得阻塞 runtime 验证节奏

---

## 四、为什么是现在

1. **2024-2025 AI Coding 工具爆发**，但全部扎堆 Web 栈，原生赛道无人
2. **LLM API 已标准化**（OpenAI/Anthropic/Google 协议趋同），底层调用不再困难
3. **开发者疲劳**：对"又一个 Electron 应用"产生疲劳，轻量原生工具有差异化空间

补充判断：AI Coding 产品正在从“代码补全工具”升级为“任务执行环境”。如果 ACT 先投入大量资源自研桌面 IDE 壳层，会被 GUI 工程吞掉验证周期；先把 runtime 做实、先在 VS Code 内验证产品闭环，再建设原生 GUI，才更符合当前阶段的资源效率。

---

## 五、参考项目

| 项目        | Stars | 语言       | GUI      | 借鉴点                                                      |
| ----------- | ----- | ---------- | -------- | ----------------------------------------------------------- |
| Claude Code | -     | TypeScript | Electron | CLI 优先架构、Agent Loop、Tool 系统、Framework/Harness 分离 |
| VS Code Extension | - | TypeScript | VS Code | 第一阶段主交付表面、成熟编辑器能力、扩展工作流 |
| Code - OSS | - | TypeScript | Electron | 可选第二步壳层方案，但当前优先 extension 而非深 fork |
| OpenClaw    | -     | TypeScript | 多端     | Agent 编排、ACP 外部 Agent 驱动、权限管理                   |
| Cline       | 47k   | TypeScript | VS Code  | Agent Loop + Permission + Diff（开源）                      |
| TRAE        | -     | -          | 自研     | AI IDE 信息架构、Agent 面板、任务流与上下文可见性           |
| VS Code     | -     | TypeScript | Electron | 活动栏、侧边栏、底部面板、扩展生态与开发者工作流            |
| Aider       | 40k   | Python     | CLI      | Repo Map、tree-sitter                                       |
| Zed         | 51k   | Rust       | 自研     | 原生性能标杆                                                |
| Lapce       | 42k   | Rust       | Floem    | 插件系统                                                    |
| Qt Creator  | -     | C++        | QWidgets | IDE 布局、LSP 集成                                          |

---

## 六、相关文档

- [ACT — PRD 产品需求文档](./ACT-PRD-%E4%BA%A7%E5%93%81%E9%9C%80%E6%B1%82%E6%96%87%E6%A1%A3.md)
- [ACT — 技术选型报告](./ACT-%E6%8A%80%E6%9C%AF%E9%80%89%E5%9E%8B%E6%8A%A5%E5%91%8A.md)
- [ACT — 系统架构设计](./ACT-%E7%B3%BB%E7%BB%9F%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.md)
- [ACT — 开发计划与进度](./ACT-%E5%BC%80%E5%8F%91%E8%AE%A1%E5%88%92%E4%B8%8E%E8%BF%9B%E5%BA%A6.md)

## 七、术语表

| 术语               | 定义                                                   |
| ------------------ | ------------------------------------------------------ |
| ACT                | AI Coding Tool，本文档描述的 runtime-first AI Coding 项目 |
| Presentation Layer | 表现层，包含 CLI、VS Code Extension、Native GUI 等交互入口 |
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

## 八、风险矩阵

| 风险ID | 风险描述                                               | 概率 | 影响 | 应对策略                                                      |
| ------ | ------------------------------------------------------ | ---- | ---- | ------------------------------------------------------------- |
| R1     | QScintilla 对 Ghost Text、复杂标记和 Diff 装饰支持不足 | 中   | 高   | 在 P2 前完成编辑器能力验证，不足时补自绘层或调整交互方案      |
| R2     | 自研 LSP Client 成本过高，导致 P4 范围失控             | 中   | 高   | P4 优先封装成熟库，保留渐进替换空间                           |
| R3     | cpp-httplib 在弱网、SSE、长连接下稳定性不足            | 中   | 中   | 提前做 Provider 联调和断网压测，必要时替换 HTTP/SSE 方案      |
| R4     | 权限确认、危险命令拦截和只读模式定义不完整             | 中   | 高   | 在 Framework/Harness 设计阶段固化权限等级、确认流程和拒绝路径 |
| R5     | 三平台依赖版本漂移，导致本地与 CI 结果不一致           | 高   | 中   | 锁定 Qt、CMake、vcpkg baseline 及关键三方库版本               |

---

整理：小欧 🦊 · 2026-03-23
