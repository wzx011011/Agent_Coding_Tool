# ACT — PRD 产品需求文档

C++/Qt6 原生 AI IDE · 2026-03-23

---

## 一、项目概述

- **项目名称**：AI Coding Tool（简称 ACT）
- **一句话定位**：以 coding agent runtime 为内核、以原生桌面 IDE 为主要载体的 AI-native 开发环境。
- **核心卖点**：轻量、原生、快，同时具备可执行任务闭环的 Agent 能力。

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

ACT 对外是一款原生 C++/Qt 的轻量 AI IDE，对内是一套面向真实软件工程任务的 coding agent runtime。IDE 是当前最重要的交付形态，但产品护城河不在编辑器外壳，而在任务编排、上下文管理、Tool 执行、权限控制和可恢复执行闭环。

产品基准拆分如下：**核心能力基准** 对标 Claude Code CLI，重点对齐任务闭环、Tool 调用、上下文裁剪、权限确认、Diff 审核和长任务可恢复；**IDE 形态基准** 参考 TRAE 和 VS Code，重点吸收其活动栏与侧边栏组织、中心编辑区、右侧 Agent 面板、底部终端与输出区、多面板联动等优秀设计。

具备以下核心能力：

1. **AI 代码补全** — 编辑器内联 Ghost Text（类似 GitHub Copilot）
2. **AI 对话** — 右侧 Chat Panel，支持多模型切换（OpenAI / Claude / GLM）
3. **Agent 模式** — AI 自主执行任务（读写文件、运行命令、Git 操作），需用户确认。采用 Agent Framework + Harness 分层架构，Tool 能力可插拔扩展
4. **代码理解** — Repo Map、AST 分析、智能上下文裁剪
5. **Diff 预览** — Agent 修改前展示 Diff，用户确认/拒绝

### 3.2 产品边界

- **不是纯补全插件**：目标不是依附现有 IDE 提供局部 AI 能力，而是提供完整开发环境
- **不是纯聊天助手**：目标不是回答问题，而是推进任务、调用工具、落地修改并接受用户确认
- **不是纯 Agent Framework**：目标不是只提供 SDK，而是将 runtime 以 IDE / CLI / Extension 形式交付给开发者
- **不是先做编辑器再塞 AI**：路线顺序是先建立 runtime 闭环，再把能力稳定投射到 GUI、CLI 和后续多表面

### 3.3 目标用户

- 对 Electron 内存占用不满的开发者
- 追求启动速度和原生体验的 C++ 开发者
- 需要本地运行、低资源占用的场景（嵌入式开发、远程开发）
- 偏好 CLI 工作流，但偶尔需要 GUI 的开发者

### 3.4 成功标准

| 指标             | 目标                                                        |
| ---------------- | ----------------------------------------------------------- |
| 空载内存         | < 100MB（Cursor 的 1/5）                                    |
| 冷启动时间       | < 500ms（Cursor 的 1/10）                                   |
| 客户端处理延迟   | < 50ms（从收到首 token 到渲染）                              |
| 端到端补全延迟   | < 2s（含网络，首 token，取决于 Provider）                    |
| 支持模型         | OpenAI / Claude / GLM 至少 3 家                             |
| Tool 扩展        | 支持通过 ITool 接口自定义扩展                               |
| 核心能力基准     | P1a 形成可对标 Claude Code CLI 的 CLI-first runtime 最小闭环 |

### 3.5 功能验收标准

| 能力       | 验收标准                                                                     |
| ---------- | ---------------------------------------------------------------------------- |
| AI 对话    | 用户可在 GUI 和 CLI 中完成至少 3 轮连续对话，且支持中途切换模型              |
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
- **Headless First**：核心 runtime 必须可在 CLI 中独立工作，GUI 只是能力载体，不得反向绑定核心逻辑

---

## 四、为什么是现在

1. **2024-2025 AI Coding 工具爆发**，但全部扎堆 Web 栈，原生赛道无人
2. **LLM API 已标准化**（OpenAI/Anthropic/Google 协议趋同），底层调用不再困难
3. **开发者疲劳**：对"又一个 Electron 应用"产生疲劳，轻量原生工具有差异化空间

补充判断：AI Coding 产品正在从“代码补全工具”升级为“任务执行环境”。如果 ACT 只做一个更轻的编辑器，很容易落入同质化竞争；只有把 runtime 能力做实，AI IDE 这个产品形态才成立。

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

| 项目        | Stars | 语言       | GUI      | 借鉴点                                                      |
| ----------- | ----- | ---------- | -------- | ----------------------------------------------------------- |
| Claude Code | -     | TypeScript | Electron | CLI 优先架构、Agent Loop、Tool 系统、Framework/Harness 分离 |
| OpenClaw    | -     | TypeScript | 多端     | Agent 编排、ACP 外部 Agent 驱动、权限管理                   |
| Cline       | 47k   | TypeScript | VS Code  | Agent Loop + Permission + Diff（开源）                      |
| TRAE        | -     | -          | 自研     | AI IDE 信息架构、Agent 面板、任务流与上下文可见性           |
| VS Code     | -     | TypeScript | Electron | 活动栏、侧边栏、底部面板、扩展生态与开发者工作流            |
| Aider       | 40k   | Python     | CLI      | Repo Map、tree-sitter                                       |
| Zed         | 51k   | Rust       | 自研     | 原生性能标杆                                                |
| Lapce       | 42k   | Rust       | Floem    | 插件系统                                                    |
| Qt Creator  | -     | C++        | QWidgets | IDE 布局、LSP 集成                                          |

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

整理：小欧 🦊 · 2026-03-23
