# ACT — 开发计划与进度

Runtime-first AI Coding Tool · 2026-03-23

---

## 一、五阶段路线

| Phase             | 时间    | 核心模块                                                    | 交付物                     | 对标水平                              |
| ----------------- | ------- | ----------------------------------------------------------- | -------------------------- | ------------------------------------- |
| P1a Runtime MVP   | 2 周    | AIEngine + 3 Tool + AgentLoop + CLI + VS Code Extension MVP | CLI / VS Code 最小闭环     | Claude Code CLI 最小内核              |
| P1b Runtime Core  | 2 周    | +3 Tool + TaskState + Patch + Shell 安全 + RuntimeEvent     | aictl agent 完整可用       | Claude Code CLI Core                  |
| P2 Runtime 强化   | 4-6 周  | Skill Loading + Subagent + Context Compact + Trace / Eval   | 长任务可持续推进的 runtime | Claude Code Runtime Productization    |
| P3 执行隔离与桌面 | 6-10 周 | Task Graph + Execution Lane + Repo Map + Native GUI Beta    | 可并行执行的 AI IDE Beta   | Claude Code Runtime + IDE Integration |
| P4 生态扩展       | 长期    | Multi-Agent / ExternalHarness + LSP + 插件系统              | 多表面统一 runtime 平台    | VS Code Ecosystem                     |

路线原则：先完成独立 runtime 与 VS Code Extension 的产品闭环，再补齐技能加载、上下文压缩、子任务隔离与执行通道等深水区能力，最后建设 Native GUI 并开放生态扩展。ACT 不是先做编辑器壳子，再把 Agent 能力补进去。

**P1 拆分理由**：原 P1 包含 28 个交付项，2-4 周内全部完成风险过高（R6）。拆分为 P1a（证明最小闭环）和 P1b（补齐能力），降低延期风险，尽早建立团队信心。

---

## 二、各阶段任务（按架构层分解）

### P1a Runtime MVP（2 周）

目标：证明 runtime 最小闭环成立。AgentLoop 能驱动 Tool 执行并返回结果，CLI 与 VS Code Extension MVP 都可完成一次完整 Agent 任务。

**🔵 Core Services 层**

- [ ] AIEngine 门面
- [ ] ILLMProvider 抽象接口
- [ ] AnthropicProvider（SSE 流式）
- [ ] ConfigManager（模型/Key 管理）

**🟠 Agent Harness 层**

- [ ] ITool 接口定义（itool.h）— Framework 与 Harness 的统一契约
- [ ] ToolRegistry — Tool 注册/发现/执行
- [ ] FileReadTool（支持行范围）
- [ ] FileWriteTool
- [ ] GrepTool（正则搜索）

**🟡 Agent Framework 层**

- [ ] AgentLoop — 单任务循环、Tool Call 决策、结果推进
- [ ] ContextManager — 消息管理 + 窗口计算
- [ ] PermissionManager — CLI Y/N 确认（注入式设计）

**🟢 Presentation Layer**

- [ ] CLI REPL 模式（GNU readline）
- [ ] CLI Permission Handler（Y/N 确认）
- [ ] Markdown 终端输出
- [ ] VS Code Extension MVP（TS → spawn aictl）
- [ ] VS Code Chat / Command / Diff 入口最小闭环

**🧪 测试**

- [ ] Harness 层单元测试（FileReadTool、FileWriteTool、GrepTool）
- [ ] Framework 层单元测试（AgentLoop、ContextManager）
- [ ] 端到端测试：aictl agent "读取 main.cpp 并解释"

**🏗️ CI**

- [ ] GitHub Actions 单平台 CI（Windows 编译 + 单元测试）
- [ ] vcpkg.json + baseline 提交锁定依赖

---

### P1b Runtime Core（2 周）

目标：在闭环成立的基础上补齐剩余 Tool、安全策略和运行时基础能力。

**🟡 Agent Framework 层**

- [ ] TaskState — 单任务运行状态模型（Running / WaitingApproval / ToolRunning / Cancelled / Failed / Completed）
- [ ] Checkpoint 基础结构 — 为长任务取消、失败恢复预留快照接口
- [ ] SkillCatalog 元信息索引 — 技能名称、描述、标签常驻 system prompt

**🟠 Agent Harness 层**

- [ ] FileEditTool（精确替换）
- [ ] ShellExecTool（含超时控制）
- [ ] GlobTool（文件模式匹配）
- [ ] PatchTransaction v0 — 单文件修改预览、确认、提交链路
- [ ] Shell 安全策略 v0 — 工作目录限制、危险命令拦截、基础 allowlist / denylist
- [ ] load_skill Tool — 完整技能正文以 tool_result 按需注入

**🔵 Core Services 层**

- [ ] RuntimeEventLogger v0 — Task / Tool / Permission / Provider 结构化事件日志（基于 spdlog）

**🧪 测试**

- [ ] Harness 层单元测试（FileEditTool、ShellExecTool、GlobTool）
- [ ] Harness 层单元测试（ToolRegistry）
- [ ] 回归任务集 v0：读取文件、搜索代码、编辑单文件、执行命令、权限拒绝后继续推进

---

### P2 Runtime 强化（4-6 周）

目标：把 runtime 从“能跑”提升到“能长时间稳定推进真实任务”。重点补齐技能加载、子智能体隔离、三层上下文压缩、结构化 Trace 与基础评测。

**🟡 Agent Framework 层**

- [ ] SubagentManager — Explore / Code 子智能体角色与独立 messages[]
- [ ] ContextManager 增强：Micro Compact / Auto Compact / Manual Compact
- [ ] ResumeTask — 从 Checkpoint 恢复中断任务

**🟠 Agent Harness 层**

- [ ] GitStatusTool
- [ ] GitDiffTool
- [ ] DiffViewTool（修改预览）
- [ ] PatchTransaction v1 — 多文件预览、Accept / Reject、部分失败处理
- [ ] SkillLoader v1 — 支持多技能叠加与 Skill Trace

**🟢 Presentation Layer（CLI / VS Code）**

- [ ] VS Code 任务状态区（显示 Running / Waiting / Failed / Completed）
- [ ] Tool / Permission 事件流面板（最小可观测性 UI）
- [ ] 技能调用、子任务摘要和 compact 状态的表面可视化

**🔵 Core Services 层**

- [ ] RuntimeTraceStore v1 — 按任务聚合 Tool、Permission、Provider 事件
- [ ] EvalRunner v0 — 可执行基础回归任务集并记录通过率

---

### P3 执行隔离与桌面（6-10 周）

目标：把“能推进任务”提升为“能并行推进任务且不互相污染”，同时接入 Native GUI 作为第二阶段桌面载体。

**🟡 Agent Framework 层**

- [ ] AgentScheduler — 并行任务编排
- [ ] AgentScheduler — 串行流水线
- [ ] Task Replay / Resume v1 — 多步任务恢复与复盘
- [ ] Task Graph v1 — 任务依赖、blockedBy、owner、artifact 引用

**🟠 Agent Harness 层**

- [ ] GitCommitTool
- [ ] RepoMapTool（tree-sitter）
- [ ] PatchTransaction v2 — 多文件原子提交、回滚、冲突处理
- [ ] 执行隔离策略 v1 — 环境变量隔离、工作区外写入控制、网络访问分级
- [ ] ExecutionLane / WorktreeManager v1 — 任务与执行目录解耦

**🟢 Presentation Layer（Native GUI）**

- [ ] Qt GUI 主窗口（QMainWindow + QDockWidget）
- [ ] 活动栏 + 侧边栏布局（参考 VS Code 的导航心智）
- [ ] AI Chat Panel（cmark-gfm Markdown 渲染）
- [ ] 右侧 Agent 面板（参考 TRAE 的任务流与上下文可见性）
- [ ] QScintilla 编辑器集成
- [ ] PermissionDialog（图形化权限确认弹窗）
- [ ] DiffWidget（并排 Diff 预览，Accept/Reject）
- [ ] 底部终端（QTermWidget）
- [ ] 底部终端 / 输出 / 诊断联动（参考 VS Code 的面板组织）
- [ ] 文件浏览器 + 搜索

**🔵 Core Services 层**

- [ ] CodeAnalyzer（tree-sitter AST）
- [ ] 多 Provider 支持（OpenAI / Claude / GLM）
- [ ] Fallback 链（主模型失败 → 备用模型）
- [ ] EvalRunner v1 — 标准任务集、回归任务集、人工验收记录
- [ ] Failure Classifier — 将失败归因到模型、工具、权限、上下文或基础设施

---

### P4 生态扩展（长期）

目标：让 ACT 从已验证的 runtime 演进为可接入多 Agent 协作、外部 Harness、IDE 和插件生态的统一运行时平台。

**🟡 Agent Framework 层**

- [ ] 嵌套编排（main → orchestrator → workers）
- [ ] Team Protocol — 子智能体协作协议、计划审批、任务认领

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

- [ ] GitHub Actions CI 扩展三平台（Linux / macOS）
- [ ] 自动发布（.exe / .AppImage / .dmg）

---

## 三、跨平台策略

- **开发环境**：选最舒服的系统开发，不限制
- **跨平台**：GitHub Actions CI 三平台编译
- **发布**：Windows .exe + Linux .AppImage + macOS .dmg

---

## 四、阶段验收口径

### P1a 验收

- `aictl agent "读取 main.cpp 并解释"` 可稳定跑通
- VS Code Extension 中可触发同一任务并完成最小闭环
- FileReadTool、FileWriteTool、GrepTool 均有单元测试
- CLI 权限确认可用，用户拒绝后 AgentLoop 可继续推理
- CLI-first runtime 可独立跑通，不依赖任何 GUI 组件
- GitHub Actions 单平台 CI 可自动编译并运行单元测试

### P1b 验收

- 6 个基础 Tool 均有单元测试
- 超时控制、结构化错误返回可用
- 至少具备单任务 TaskState、基础 Checkpoint 接口和单文件 PatchTransaction
- Shell 安全策略 v0 可拦截危险命令并限制工作目录
- 回归任务集 v0 可稳定执行，并覆盖权限拒绝、命令失败和单文件修改场景
- 技能元信息常驻 system prompt，技能正文通过 load_skill 按需注入

### P2 验收

- Explore 子智能体可独立执行只读任务，并仅向主会话返回摘要
- 三层上下文压缩可在长任务中持续工作，不破坏任务因果链
- VS Code / CLI 可展示任务状态、Tool 事件、权限事件和技能调用事件
- GitStatusTool / GitDiffTool 能在 VS Code 或 CLI 中展示结果
- 中断任务可从 Checkpoint 恢复，至少支持单任务恢复到最近一次确认点

### P3 验收

- AgentScheduler 可支持串行流水线与并行 worker 两种编排方式
- Repo Map 和上下文裁剪能在中型仓库中可用
- 多 Provider 和 Fallback 链完成联调
- 可在真实仓库中完成接近 Claude Code CLI 水平的多步任务推进与审阅闭环
- 多文件补丁支持预演、原子提交与失败回滚
- 标准任务集和回归任务集可持续运行，能区分模型问题、工具问题和权限问题
- Task Graph 与 Execution Lane 可独立演进，worktree 切换不破坏任务状态
- Native GUI 在不分叉 runtime 的前提下接入，并完成与 VS Code 相同的基础任务闭环

### P4 验收

- 外部 Harness、LSP、插件系统形成稳定扩展点
- 三平台 CI、打包和版本发布流程跑通
- VS Code Extension 与原生 GUI 共用同一套 runtime 能力，不形成分叉实现
- ACP / MCP / Plugin 调用进入统一权限与审计体系，不形成新的安全盲区
- Multi-Agent 协作协议在统一 Task Graph 与 Event Trace 中可观察、可审计、可恢复

---

## 五、运行时补强优先级

### 必须进入 P1a

- AIEngine + AnthropicProvider + SSE 流式
- ITool + ToolRegistry + 3 核心 Tool
- AgentLoop + ContextManager + PermissionManager
- CLI REPL + Permission Handler
- GitHub Actions 单平台 CI

### 必须进入 P1b

- TaskState 基础模型
- Checkpoint 接口预留
- PatchTransaction v0
- Shell 安全策略 v0
- RuntimeEventLogger v0
- 回归任务集 v0
- load_skill Tool + SkillCatalog 摘要注入

### 最迟 P2 补齐

- ResumeTask
- PatchTransaction v1
- 任务状态区与事件流面板
- RuntimeTraceStore v1
- EvalRunner v0
- SubagentManager v1
- 三层 Context Compact

### 最迟 P3 补齐

- Task Replay / Resume v1
- PatchTransaction v2
- 执行隔离策略 v1
- EvalRunner v1
- Failure Classifier
- ExecutionLane / WorktreeManager v1

---

## 六、核心模块验收标准

每个模块验收分两部分：**接口合约**（输入/输出）和**必测场景**列表。单元测试必须覆盖所有"必测场景"才算该模块通过 P1 Gate。

### 6.1 P1a 模块

#### AnthropicProvider

| 项目     | 内容                                                                 |
| -------- | -------------------------------------------------------------------- |
| 接口     | `chat(messages, toolSchemas) → [streamToken signal...] → LLMMessage` |
| 输出类型 | `LLMMessage { role, content, toolCall? }`                            |

必测场景：

| #   | 输入                              | 期望输出                                                    |
| --- | --------------------------------- | ----------------------------------------------------------- |
| 1   | 正常对话请求                      | emit 多个 streamToken + 最终完整 LLMMessage                 |
| 2   | 含 Tool Call 的响应               | LLMMessage.toolCall 有效，名称/参数可解析                   |
| 3   | 401 Invalid API Key               | 立即返回 `errorCode: AUTH_ERROR`                            |
| 4   | 网络超时 (> configurable_timeout) | `errorCode: PROVIDER_TIMEOUT`，已收 token 不丢失            |
| 5   | Rate limit (429)                  | 等 retry-after → 重试一次；再失败 → `errorCode: RATE_LIMIT` |

#### AIEngine

| 项目 | 内容                                                                           |
| ---- | ------------------------------------------------------------------------------ |
| 接口 | `setProvider(name)` / `chat(messages)` / `requestCompletion(prefix) → QString` |

必测场景：

| #   | 输入                                             | 期望输出                                 |
| --- | ------------------------------------------------ | ---------------------------------------- |
| 1   | setProvider("anthropic") → setProvider("openai") | 后续 chat 调用目标切换为 OpenAI Provider |
| 2   | chat() 正常                                      | streaming 信号透传到调用方               |
| 3   | Provider 未设置时调用 chat()                     | `errorCode: NO_PROVIDER`                 |

#### ContextManager

| 项目 | 内容                                                                                   |
| ---- | -------------------------------------------------------------------------------------- |
| 接口 | `buildContext(messages, maxTokens) → QList<LLMMessage>` / `estimateTokens(msgs) → int` |

必测场景：

| #   | 条件                | 期望行为                                     |
| --- | ------------------- | -------------------------------------------- |
| 1   | 总量 < 80% 窗口     | 原样返回，不修改任何消息                     |
| 2   | 总量 > 80% 窗口     | Truncate：系统消息保留，最旧消息先丢弃       |
| 3   | 单条消息 > 50% 窗口 | 该消息内容截断至 40% 窗口大小                |
| 4   | estimateTokens 精度 | 与实际 token 数误差 < ±25%（chars/3.5 基线） |

#### PermissionManager

| 项目 | 内容                                                              |
| ---- | ----------------------------------------------------------------- |
| 接口 | `checkPermission(PermissionRequest) → bool`（确认函数由构造注入） |

必测场景：

| #   | 条件                              | 期望行为                                   |
| --- | --------------------------------- | ------------------------------------------ |
| 1   | Read 级请求                       | 直接 true，不调用确认函数                  |
| 2   | Write 级，mock 返回 true          | true                                       |
| 3   | Write 级，mock 返回 false         | false，不执行后续操作                      |
| 4   | Destructive 级                    | 确认函数被调用两次（二次确认）             |
| 5   | 只读模式下 Write/Exec/Destructive | 直接 false，不调用确认函数                 |
| 6   | 工作区外路径（任意级别）          | 直接 false，`errorCode: OUTSIDE_WORKSPACE` |

#### AgentLoop

| 项目     | 内容                                                                                                                   |
| -------- | ---------------------------------------------------------------------------------------------------------------------- |
| 接口     | `executeTask(taskDesc, workspacePath)` → 通过信号输出事件流                                                            |
| 输出信号 | `streamToken` / `toolCallStarted` / `toolCallCompleted` / `permissionRequested` / `taskStateChanged` / `errorOccurred` |

必测场景：

| #   | 场景                        | 期望信号序列                                                                                          |
| --- | --------------------------- | ----------------------------------------------------------------------------------------------------- |
| 1   | 无 Tool Call 任务（"你好"） | streamToken × N → taskCompleted，无 toolCallStarted                                                   |
| 2   | 单 Tool（FileRead）任务     | toolCallStarted → toolCallCompleted → streamToken → taskCompleted                                     |
| 3   | Tool Chain（Read → Edit）   | toolCallStarted(read) → toolCallCompleted → toolCallStarted(edit) → toolCallCompleted → taskCompleted |
| 4   | Write 权限被拒              | permissionRequested → 拒绝 → agent 继续推理（不卡死）                                                 |
| 5   | Provider 超时               | taskStateChanged(Failed) + errorOccurred("PROVIDER_TIMEOUT", …)                                       |
| 6   | LLM 返回不存在的 Tool 名    | Tool 错误作为 tool result 回传 → agent 继续推理                                                       |

#### ITool / ToolRegistry

| 项目 | 内容                                                                                         |
| ---- | -------------------------------------------------------------------------------------------- |
| 接口 | `registerTool(ITool*)` / `executeTool(name, params) → ToolResult` / `getTool(name) → ITool*` |

必测场景：

| #   | 场景                             | 期望结果                                                |
| --- | -------------------------------- | ------------------------------------------------------- |
| 1   | 注册后检索                       | getTool(name) 返回注册的实例                            |
| 2   | 重复注册同名 Tool                | 抛出异常或返回错误                                      |
| 3   | executeTool 已注册 Tool          | 调用 execute()，结果透传                                |
| 4   | executeTool 未注册 Tool          | `ToolResult{success:false, errorCode:"TOOL_NOT_FOUND"}` |
| 5   | executeTool Write 级，权限未批准 | 不调用 execute()，返回权限拒绝错误                      |

#### FileReadTool

| 项目 | 内容                                                   |
| ---- | ------------------------------------------------------ |
| 接口 | `execute({path, start_line?, end_line?}) → ToolResult` |

必测场景：

| #   | 输入                      | 期望结果                                |
| --- | ------------------------- | --------------------------------------- |
| 1   | 正常文件路径              | success:true, output 为文件完整内容     |
| 2   | 带行范围 start=10, end=20 | 仅返回第 10–20 行                       |
| 3   | 不存在的文件              | `errorCode: FILE_NOT_FOUND`             |
| 4   | 工作区外路径              | `errorCode: OUTSIDE_WORKSPACE`          |
| 5   | 二进制文件（非 UTF-8）    | `errorCode: BINARY_FILE`                |
| 6   | 超大文件（> 10MB）        | 返回前 500 行 + metadata.truncated:true |

#### FileWriteTool

| 项目 | 内容                                    |
| ---- | --------------------------------------- |
| 接口 | `execute({path, content}) → ToolResult` |

必测场景：

| #   | 输入         | 期望结果                              |
| --- | ------------ | ------------------------------------- |
| 1   | 正常写入     | success:true，文件内容与 content 一致 |
| 2   | 父目录不存在 | 自动创建父目录后写入                  |
| 3   | 工作区外路径 | `errorCode: OUTSIDE_WORKSPACE`        |
| 4   | 系统无写权限 | `errorCode: PERMISSION_DENIED`        |

#### GrepTool

| 项目 | 内容                                                 |
| ---- | ---------------------------------------------------- |
| 接口 | `execute({pattern, path?, recursive?}) → ToolResult` |

必测场景：

| #   | 输入             | 期望结果                                  |
| --- | ---------------- | ----------------------------------------- |
| 1   | 有效正则，有匹配 | output 为 `文件名:行号:内容` 格式逐行列出 |
| 2   | 有效正则，无匹配 | success:true, output 为空字符串           |
| 3   | 非法正则         | `errorCode: INVALID_PATTERN`              |
| 4   | path 为工作区外  | `errorCode: OUTSIDE_WORKSPACE`            |

---

### 6.2 P1b 模块

#### FileEditTool

| 项目 | 内容                                                   |
| ---- | ------------------------------------------------------ |
| 接口 | `execute({path, old_string, new_string}) → ToolResult` |

必测场景：

| #   | 输入                | 期望结果                                 |
| --- | ------------------- | ---------------------------------------- |
| 1   | old_string 唯一匹配 | success:true，替换完成，metadata 含 diff |
| 2   | old_string 不存在   | `errorCode: STRING_NOT_FOUND`            |
| 3   | old_string 多处匹配 | `errorCode: AMBIGUOUS_MATCH`，不执行替换 |
| 4   | 工作区外路径        | `errorCode: OUTSIDE_WORKSPACE`           |

#### ShellExecTool

| 项目 | 内容                                                  |
| ---- | ----------------------------------------------------- |
| 接口 | `execute({command, timeout?, workdir?}) → ToolResult` |

必测场景：

| #   | 输入                          | 期望结果                                              |
| --- | ----------------------------- | ----------------------------------------------------- |
| 1   | `echo hello`                  | success:true, output 含 stdout "hello"                |
| 2   | 退出码非零（`exit 1`）        | success:false, metadata.exit_code:1, output 含 stderr |
| 3   | 命令超时（执行 > timeout 秒） | 进程终止，`errorCode: TIMEOUT`，output 含已有输出     |
| 4   | 黑名单命令（`rm -rf /`）      | 不执行，`errorCode: COMMAND_BLOCKED`                  |
| 5   | workdir 外的 cd               | P1b 软限制（记录警告）；P3 升级为硬隔离               |

#### GlobTool

| 项目 | 内容                                         |
| ---- | -------------------------------------------- |
| 接口 | `execute({pattern, basePath?}) → ToolResult` |

必测场景：

| #   | 输入                | 期望结果                           |
| --- | ------------------- | ---------------------------------- |
| 1   | `**/*.cpp`          | 返回工作区内所有 .cpp 文件路径列表 |
| 2   | 无匹配 pattern      | success:true, output 为空列表      |
| 3   | basePath 为工作区外 | `errorCode: OUTSIDE_WORKSPACE`     |

---

## 七、进度追踪

项目进度表：https://gcnaf7ct1kpv.feishu.cn/base/WDPPbs2mKakUpzsndzmcr1Qvncf

⚡ 当前进度：尚未开始（等大老板说"开工"）

---

## 八、相关文档

- [ACT — PRD 产品需求文档](./ACT-PRD-%E4%BA%A7%E5%93%81%E9%9C%80%E6%B1%82%E6%96%87%E6%A1%A3.md)
- [ACT — 技术选型报告](./ACT-%E6%8A%80%E6%9C%AF%E9%80%89%E5%9E%8B%E6%8A%A5%E5%91%8A.md)
- [ACT — 系统架构设计](./ACT-%E7%B3%BB%E7%BB%9F%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.md)
- [ACT — 开发计划与进度](./ACT-%E5%BC%80%E5%8F%91%E8%AE%A1%E5%88%92%E4%B8%8E%E8%BF%9B%E5%BA%A6.md)
- [ACT — 术语表](./ACT-%E6%9C%AF%E8%AF%AD%E8%A1%A8.md)
- [ACT — 风险矩阵](./ACT-%E9%A3%8E%E9%99%A9%E7%9F%A9%E9%98%B5.md)

## 九、术语表

> 完整术语表维护于 [ACT — 术语表](./ACT-%E6%9C%AF%E8%AF%AD%E8%A1%A8.md)，此处不再重复。

## 十、风险矩阵

> 完整风险矩阵维护于 [ACT — 风险矩阵](./ACT-%E9%A3%8E%E9%99%A9%E7%9F%A9%E9%98%B5.md)，此处不再重复。

---

整理：小欧 🦊 · 2026-03-23
