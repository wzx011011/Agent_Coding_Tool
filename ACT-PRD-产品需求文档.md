# ACT — PRD 产品需求文档

Runtime-first AI Coding Tool · 2026-03-23

---

## 一、项目概述

- **项目名称**：AI Coding Tool（简称 ACT）
- **一句话定位**：以独立 coding agent runtime 为内核、以 CLI 与 VS Code Extension 为第一阶段交付表面、以 Native GUI 为第二阶段差异化载体的 AI-native 开发环境。
- **核心卖点**：不是先做 IDE 壳子，而是先把任务闭环、技能加载、上下文压缩、子智能体和执行隔离做成可复用 runtime。

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

ACT 首先是一套面向真实软件工程任务的独立 coding agent runtime，其次才是承载这套 runtime 的产品表面。第一阶段以 CLI 与 VS Code Extension 交付，优先验证 Claude Code CLI 级别的任务闭环、Tool Runtime、权限确认、Diff 审核、可恢复执行和上下文管理；第二阶段再建设 Native GUI，作为低资源占用、高响应和桌面体验上的差异化载体。

产品基准拆分如下：**核心能力基准** 对标 Claude Code CLI，重点对齐任务闭环、Tool 调用、上下文裁剪、权限确认、Diff 审核和长任务可恢复；**第一阶段产品形态基准** 参考 VS Code 的成熟编辑器与扩展工作流；**第二阶段桌面形态基准** 参考 TRAE 与 VS Code 的信息架构，但不牺牲 runtime 独立性。

具备以下核心能力：

1. **AI 代码补全** — 编辑器内联 Ghost Text（类似 GitHub Copilot）
2. **AI 对话** — 在 CLI 和 VS Code 中支持多模型切换（OpenAI / Claude / GLM）
3. **Agent 模式** — AI 自主执行任务（读写文件、运行命令、Git 操作），需用户确认。采用 Agent Framework + Harness 分层架构，Tool 能力可插拔扩展
4. **代码理解** — Repo Map、AST 分析、智能上下文裁剪
5. **Diff 预览** — Agent 修改前展示 Diff，用户确认/拒绝

其中 runtime 的关键机制必须内建，而不是后补：

1. **稳定 Agent Loop** — 主循环保持小而稳定，高级能力通过机制叠加而不是不断改写 loop
2. **两层技能注入** — system prompt 仅保留技能摘要，完整技能内容按需以 tool_result 注入
3. **子智能体上下文隔离** — 大任务拆给 subagent 时使用独立消息空间，主会话只接收摘要结果
4. **三层上下文压缩** — 微压缩、自动压缩、手动压缩组合，避免长任务被历史消息拖垮
5. **任务图与执行通道分离** — TaskState 负责目标状态，Execution Lane / Worktree 负责隔离执行环境
6. **结构化运行时事件** — Tool、Permission、Task、Model 行为都进入统一事件流与审计链路

### 3.2 产品边界

- **不是纯补全插件**：目标不是依附现有 IDE 提供局部 AI 能力，而是提供完整开发环境
- **不是纯聊天助手**：目标不是回答问题，而是推进任务、调用工具、落地修改并接受用户确认
- **不是纯 Agent Framework**：目标不是只提供 SDK，而是将 runtime 以 CLI / VS Code Extension / Native GUI 形式交付给开发者
- **不是原生 GUI 优先**：路线顺序是先建立 runtime 闭环与 VS Code 内的产品验证，再把能力稳定投射到 Native GUI

### 3.3 目标用户

- 当前主要在 VS Code 中工作，但需要更强 Agent 任务闭环的开发者
- 对 Electron 内存占用不满，但愿意先接受 VS Code Extension 形态的开发者
- 追求长期原生体验和低资源占用，但接受 Native GUI 在第二阶段交付的开发者
- 需要本地运行、低资源占用的场景（嵌入式开发、远程开发）
- 偏好 CLI 工作流，但偶尔需要 GUI 的开发者

### 3.4 成功标准

| 指标            | 目标                                                             |
| --------------- | ---------------------------------------------------------------- |
| P1 交付形态     | CLI + VS Code Extension 形成最小可用产品闭环                     |
| 核心能力基准    | P1 形成可对标 Claude Code CLI 的 runtime 最小闭环                |
| 子任务隔离能力  | P2 前支持只读 Explore 子智能体，且主会话不被探索细节污染         |
| 上下文可持续性  | P2 前具备三层上下文压缩能力，长任务不因历史消息膨胀而失控        |
| 任务可恢复性    | P2 前具备 TaskState、Checkpoint、Resume 基础能力                 |
| 支持模型        | OpenAI / Claude / GLM 至少 3 家                                  |
| Tool 扩展       | 支持通过 ITool 接口自定义扩展                                    |
| Native GUI 节奏 | Native GUI 作为第二阶段 Beta，不阻塞 P1 / P2 的 runtime 能力验证 |

### 3.5 功能验收标准

| 能力       | 验收标准                                                                           |
| ---------- | ---------------------------------------------------------------------------------- |
| AI 对话    | 用户可在 CLI 和 VS Code Extension 中完成至少 3 轮连续对话，且支持中途切换模型      |
| Agent 模式 | Agent 在执行写文件、执行命令、网络访问前必须出现权限确认；被拒绝后可继续对话       |
| Diff 预览  | Agent 修改代码前必须展示 Diff，用户可明确 Accept / Reject                          |
| Repo Map   | 可在中型仓库中生成可用的代码结构摘要，并参与上下文裁剪                             |
| 多模型切换 | 同一任务流中可切换 Provider，失败时支持 fallback 到备用模型                        |
| Skills     | 系统提示中仅保留技能摘要，完整技能内容需按需注入，不允许把全部技能正文常驻系统提示 |
| Subagent   | 子智能体执行探索类任务后只返回摘要、结论和引用，不回灌完整探索对话                 |

### 3.6 非功能约束

- **工作区边界**：默认仅允许访问当前工作区，跨目录写入需额外确认
- **安全性**：危险命令、删除操作、网络请求必须显式确认，不允许静默执行
- **弱网体验**：模型请求失败时应返回结构化错误，并支持重试或 fallback
- **可恢复性**：长任务必须支持取消，用户拒绝权限后系统不能卡死或丢失上下文
- **跨平台**：Windows、Linux、macOS 的核心能力保持一致，平台差异仅体现在打包和系统集成层
- **Runtime First**：核心 runtime 必须可在 CLI 中独立工作，VS Code Extension 和 Native GUI 都只是能力载体，不得反向绑定核心逻辑
- **Surface Priority**：第一阶段优先保证 CLI 与 VS Code Extension 的体验闭环，Native GUI 不得阻塞 runtime 验证节奏
- **Context Discipline**：技能、工具结果、子任务摘要和历史消息都必须进入统一的上下文预算治理
- **Execution Isolation**：任务状态与执行目录必须拆分建模，为后续 Worktree / Execution Lane 并行隔离留出接口

---

## 四、为什么是现在

1. **2024-2025 AI Coding 工具爆发**，但全部扎堆 Web 栈，原生赛道无人
2. **LLM API 已标准化**（OpenAI/Anthropic/Google 协议趋同），底层调用不再困难
3. **开发者疲劳**：对"又一个 Electron 应用"产生疲劳，轻量原生工具有差异化空间

补充判断：AI Coding 产品正在从“代码补全工具”升级为“任务执行环境”。如果 ACT 先投入大量资源自研桌面 IDE 壳层，会被 GUI 工程吞掉验证周期；先把 runtime 做实、先在 VS Code 内验证产品闭环，再建设 Native GUI，才更符合当前阶段的资源效率。

---

## 五、典型用户 Journey

### Journey 1：CLI Agent 修复 Bug

```
用户 → aictl agent "main.cpp 第42行空指针崩溃，帮我修复"
  → AgentLoop 启动
  → [Read] FileReadTool 读取 main.cpp（自动放行）
  → [Read] GrepTool 搜索相关引用（自动放行）
  → Agent 分析后决定修改
  → [Write] FileEditTool 修改 main.cpp
    → PermissionManager → CLI 显示 Diff 并等待 Y/N
    → 用户输入 Y → 写入文件
  → [Exec] ShellExecTool 运行编译验证
    → PermissionManager → CLI 显示命令并等待 Y/N
    → 用户输入 Y → 执行编译
  → Agent 返回修复总结
```

**关键确认点**：写文件前展示 Diff、执行命令前展示命令内容，用户拒绝后 Agent 需继续推理。

### Journey 2：GUI 多轮对话 + 代码修改

```
用户 → 打开项目 → 右侧 Chat Panel 输入 "重构 UserService 的登录方法"
  → AgentLoop 启动，任务状态区显示 Running
  → Agent 使用 FileReadTool 读取相关文件 → 事件流面板显示 Tool 执行
  → Agent 回复分析结论 → Chat Panel 渲染 Markdown
  → 用户追问 "同时处理一下异常情况"
  → Agent 决定多文件修改
    → PermissionDialog 弹窗显示所有修改的 Diff
    → 用户在 DiffWidget 中逐文件 Accept / Reject
  → Agent 根据接受的修改继续推进
  → 任务状态区显示 Completed
```

**关键确认点**：多文件修改通过 DiffWidget 逐文件审核，拒绝部分修改后 Agent 继续处理。

### Journey 3：Agent 执行失败与恢复

```
用户 → aictl agent "给所有公开方法添加单元测试"
  → AgentLoop 启动，开始多步任务
  → 步骤 1-3 正常完成，Checkpoint 自动保存
  → 步骤 4：ShellExecTool 执行测试命令
    → 用户拒绝权限 → Agent 将拒绝作为上下文继续推理
    → Agent 改为只生成测试文件，不执行
  → 步骤 5：AIEngine 模型请求超时
    → Fallback → 切换到备用模型继续
  → 步骤 6：用户主动取消任务（Ctrl+C）
    → TaskState → Cancelled，Checkpoint 已保存
  → 用户稍后执行 aictl resume
    → 从步骤 6 的 Checkpoint 恢复，继续推进
```

**关键确认点**：权限拒绝后不卡死、模型失败自动 Fallback、取消后可恢复。

---

## 六、参考项目

| 项目              | Stars | 语言        | GUI      | 借鉴点                                                                 |
| ----------------- | ----- | ----------- | -------- | ---------------------------------------------------------------------- |
| Claude Code       | -     | TypeScript  | Electron | CLI 优先架构、Agent Loop、Tool 系统、Framework/Harness 分离            |
| learn-claude-code | 36.7k | Python / TS | Web 教学 | 两层技能注入、子智能体隔离、三层上下文压缩、任务图与 worktree 教学拆解 |
| OpenClaw          | -     | TypeScript  | 多端     | Agent 编排、ACP 外部 Agent 驱动、权限管理                              |
| Cline             | 47k   | TypeScript  | VS Code  | Agent Loop + Permission + Diff（开源）                                 |
| TRAE              | -     | -           | 自研     | AI IDE 信息架构、Agent 面板、任务流与上下文可见性                      |
| VS Code           | -     | TypeScript  | Electron | 活动栏、侧边栏、底部面板、扩展生态与开发者工作流                       |
| Aider             | 40k   | Python      | CLI      | Repo Map、tree-sitter                                                  |
| Zed               | 51k   | Rust        | 自研     | 原生性能标杆                                                           |
| Lapce             | 42k   | Rust        | Floem    | 插件系统                                                               |
| Qt Creator        | -     | C++         | QWidgets | IDE 布局、LSP 集成                                                     |

---

## 七、相关文档

- [ACT — PRD 产品需求文档](./ACT-PRD-%E4%BA%A7%E5%93%81%E9%9C%80%E6%B1%82%E6%96%87%E6%A1%A3.md)
- [ACT — 技术选型报告](./ACT-%E6%8A%80%E6%9C%AF%E9%80%89%E5%9E%8B%E6%8A%A5%E5%91%8A.md)
- [ACT — 系统架构设计](./ACT-%E7%B3%BB%E7%BB%9F%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.md)
- [ACT — 开发计划与进度](./ACT-%E5%BC%80%E5%8F%91%E8%AE%A1%E5%88%92%E4%B8%8E%E8%BF%9B%E5%BA%A6.md)
- [ACT — 术语表](./ACT-%E6%9C%AF%E8%AF%AD%E8%A1%A8.md)
- [ACT — 风险矩阵](./ACT-%E9%A3%8E%E9%99%A9%E7%9F%A9%E9%98%B5.md)

## 八、术语表

> 完整术语表维护于 [ACT — 术语表](./ACT-%E6%9C%AF%E8%AF%AD%E8%A1%A8.md)，此处不再重复。

## 九、风险矩阵

> 完整风险矩阵维护于 [ACT — 风险矩阵](./ACT-%E9%A3%8E%E9%99%A9%E7%9F%A9%E9%98%B5.md)，此处不再重复。

---

整理：小欧 · 2026-03-23
