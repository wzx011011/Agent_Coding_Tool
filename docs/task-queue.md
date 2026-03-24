# Task Queue — ACT Runtime-First Implementation Plan

## Metadata

- Created: 2026-03-23
- Source: ACT-PRD-产品需求文档.md + ACT-系统架构设计.md + ACT-开发计划与进度.md + ACT-技术选型报告.md
- Scope: P1a / P1b / P2 / P3 implementation planning
- Status: pending approval
- Completed: 26/28 + 6/16 (T11 skipped; T16 deferred; N1,N2,N7,N11,N12,N15 completed)

## Planning Notes

- 本队列基于当前已经更新后的 ACT 设计口径：`runtime-first + CLI / VS Code Extension 优先 + Native GUI 第二阶段`。
- 队列不再沿用旧的“先 CLI 闭环、后补几个 Tool”的窄 P1 拆法，而是按 ACT 现在明确的 runtime 机制栈组织：Stable Loop、Tool Dispatch、Skill Injection、Subagent Isolation、Context Compact、Task Graph、Execution Isolation。
- 每个任务都要求独立可验证；默认验证命令为：
  - `cmake --preset default`
  - `cmake --build build --config RelWithDebInfo`
  - `ctest --test-dir build --config RelWithDebInfo --output-on-failure`
- P4 `Multi-Agent / ExternalHarness / MCP / ACP / Plugin` 暂不进入本轮任务队列，避免把 implementation queue 拉成不可执行的长期愿景清单。
- P1 CLI 目标必须保持 headless：只允许 `QtCore`，禁止把 `QtWidgets`、`QScintilla`、`QTermWidget` 混入 runtime / CLI 目标。

## Tasks

### Batch 1: Runtime 核心与 CLI 闭环

| #   | ID  | Title                                                   | Scope       | Depends            | Status | Verification                            | Notes                                                                                                                                                       |
| --- | --- | ------------------------------------------------------- | ----------- | ------------------ | ------ | --------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------- |
| 1   | T1  | 初始化工程骨架与分层目标                                | infra       | -                  | [x]    | configure + build + smoke test pass     | 建立 `src/core`、`src/framework`、`src/harness`、`src/cli`、`src/presentation/vscode-protocol`、`tests/`、vcpkg manifest、CMake preset；CLI 仅链接 `QtCore` | commit: 68fcb49 |
| 2   | T2  | 定义核心类型、错误码与事件模型                          | backend     | T1                 | [x]    | build + unit tests pass                 | 包含 `ToolResult`、`ToolCall`、`LLMMessage`、`PermissionRequest`、`TaskState`、`RuntimeEvent`、结构化 error code                                            | commit: 59ba7da |
| 3   | T3  | 建立 IService / IInfrastructure / ITool 契约            | backend     | T1, T2             | [x]    | build + interface compile tests pass    | 固化层间接口，保证 runtime 与表面解耦                                                                                                                       | commit: 140ca9a |
| 4   | T4  | 实现 ConfigManager、AnthropicProvider 与 AIEngine       | backend     | T2, T3             | [x]    | build + provider tests pass             | 包含 SSE 流式、provider 注入、未配置 provider 错误、基础 fallback 接口                                                                                      | commit: 3a10e08 |
| 5   | T5  | 实现 ToolRegistry 与基础只读工具集                      | backend     | T2, T3             | [x]    | build + harness tests pass              | 实现 `FileReadTool`、`GrepTool`、`GlobTool`，完成 safe_path / 工作区边界校验                                                                                | commit: 3a10e08 |
| 6   | T6  | 实现权限系统与可写工具基线                              | integration | T2, T3, T5         | [x]    | build + tests pass                      | 实现 `PermissionManager`、`FileWriteTool`、`FileEditTool`、写入审批与工作区外写入拒绝                                                                       | commit: 2d12f77 |
| 7   | T7  | 实现 ShellExecTool 与 Shell 安全策略 v0                 | backend     | T2, T3, T6         | [x]    | build + tests pass                      | 覆盖超时、危险命令拦截、工作目录限制、allowlist / denylist 基线                                                                                             | commit: 2d12f77 |
| 8   | T8  | 实现 ContextManager 与三层压缩骨架                      | backend     | T2, T3, T4         | [x]    | build + tests pass                      | 先落 `estimateTokens`、窗口治理、Micro Compact / Auto Compact / Manual Compact 框架                                                                         | commit: 2d12f77 |
| 9   | T9  | 实现 AgentLoop、TaskState 与 Checkpoint 骨架            | integration | T4, T5, T6, T7, T8 | [x]    | build + framework tests pass            | 覆盖 tool_use 循环、权限拒绝后继续、结构化失败回传、基础 TaskState 流转                                                                                     | commit: ce4171e |
| 10  | T10 | 实现 CLI REPL 与 JSON Lines runtime 协议                | frontend    | T6, T7, T9         | [x]    | build + CLI e2e tests pass              | 同时支持人类可读 REPL 和机器可读 `--json` 事件流，为 VS Code Extension 提供协议面                              | commit: ce4171e |
| 11  | T11 | 实现 VS Code Extension MVP                              | frontend    | T10                | [-]    | build + extension smoke workflow pass   | **Skipped**: TypeScript/Node.js 技术栈，暂不纳入 C++ runtime 闭环；未来可作为独立前端项目对接 aictl JSON 协议   |
| 12  | T12 | 实现 SkillCatalog、SkillLoader 与 load_skill            | backend     | T3, T8, T9         | [x]    | build + tests pass                      | 实现两层技能注入：system prompt 摘要常驻，正文按需以 tool_result 注入；记录 Skill Trace                                                                     |
| 13  | T13 | 实现 SubagentManager 与 Explore / Code 子智能体         | integration | T9, T10, T12       | [x]    | build + tests pass                      | 子任务使用独立 `messages[]`，Explore 默认只读，主会话仅接收摘要与结构化结果                                                                                 |
| 14  | T14 | 实现 PatchTransaction、Git 只读工具与 RuntimeTraceStore | integration | T6, T7, T9, T10    | [x]    | build + tests pass                      | 落地 `GitStatusTool`、`GitDiffTool`、`PatchTransaction v0/v1`、`RuntimeEventLogger`、`RuntimeTraceStore v1`                                                 | commit: pending |
| 15  | T15 | 实现 Task Graph、Resume / Replay 与 Execution Lane      | backend     | T9, T13, T14       | [x]    | build + tests pass                      | 引入 `TaskStateStore`、依赖图、artifact 引用、background lane、worktree lane 抽象                                                                           |
| 16  | T16 | 接入 Native GUI Beta 与 P1-P3 回归评测                  | integration | T11, T14, T15      | [-]    | build + test + targeted regression pass | **Deferred**: P2+ GUI，requires QtWidgets；EvalRunner / 回归任务集拆分至 N4、N9 独立实现                                                               |

### Batch 2: 真实 LLM API 接入（多 Provider 支持）

| #   | ID     | Title                                                   | Scope       | Depends                                  | Status | Verification                            | Notes                                                                                                                                                       |
| --- | ------ | ------------------------------------------------------- | ----------- | ---------------------------------------- | ------ | --------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------- |
| 17  | LLM-T1 | 链接 cpp-httplib 到基础设施层                            | infra       | -                                        | [x]    | build 通过                               | `find_package(httplib CONFIG REQUIRED)`，链接到 `act_infrastructure`                                                                                           |                |
| 18  | LLM-T2 | 实现 SSE 协议解析器                                     | infra       | LLM-T1                                   | [x]    | build + SseParserTest pass              | `feed(chunk)` 返回 `SseEvent` 列表，支持标准 SSE 和 Anthropic SSE 两种格式                                                                                    |                |
| 19  | LLM-T3 | 实现 HttpNetwork（cpp-httplib 封装）                    | infra       | LLM-T1                                   | [x]    | build + HttpNetworkTest pass            | HTTP POST + SSE 流式读取，代理、自定义 Headers、超时配置                                                                                                     |                |
| 20  | LLM-T4 | 扩展 LLMMessage 支持多 tool_call                         | backend     | -                                        | [x]    | build + 现有 test 全部通过               | `LLMMessage` 增加 `QList<ToolCall> toolCalls`，`toolCall` 向后兼容                                                                                             |                |
| 21  | LLM-T5 | 扩展 ConfigManager 添加 Provider 和网络配置              | backend     | -                                        | [x]    | build + ConfigManagerTest pass          | `provider`、`[network]` TOML 段（base_url、proxy），按 provider 聚合默认 base_url                                                                               |                |
| 22  | LLM-T6 | 抽取 LLMProvider 抽象基类                                | backend     | LLM-T4                                   | [x]    | build 通过                               | `chat()` / `stream()` / `cancel()` / `setToolDefinitions()` 统一接口                                                                                         |                |
| 23  | LLM-T7 | 实现 Anthropic 消息格式转换器                            | backend     | LLM-T2, LLM-T4                           | [x]    | build + AnthropicConverterTest pass     | `toRequest()` / `parseSseEvents()` / `toolToDefinition()`，`x-api-key` 认证                                                                                   |                |
| 24  | LLM-T8 | 实现 OpenAI 兼容消息格式转换器                           | backend     | LLM-T2, LLM-T4                           | [x]    | build + OpenAICompatConverterTest pass  | `toRequest()` / `parseSseEvents()` / `toolToDefinition()`，`Authorization: Bearer` 认证，GLM 适配                                                            |                |
| 25  | LLM-T9 | 替换 AnthropicProvider stub + 新建 OpenAICompatProvider   | backend     | LLM-T3, T5, T6, T7, T8                   | [x]    | build + test pass                       | 真实 HTTP/SSE 实现，AIEngine 根据 provider 选择对应实现，错误映射（401/429/timeout）                                                                            |                |
| 26  | LLM-T10| 实现流式输出 + 多 tool_call 分发                         | integration | LLM-T9                                   | [x]    | build + test pass                       | `streamTokenReceived` 信号，`IAIEngine::setToolDefinitions()`，AgentLoop 顺序分发所有 tool call                                                              |                |
| 27  | LLM-T11| 串联配置 → 网络 → Provider 全链路                       | integration | LLM-T5, LLM-T9                            | [x]    | build + test pass                       | AIEngine 从 ConfigManager 读取 provider/base_url/proxy/apiKey，main.cpp 注入 ToolRegistry 工具定义                                                           |                |
| 28  | LLM-T12| 端到端集成测试                                          | testing     | LLM-T10, LLM-T11                          | [x]    | build + skip (无 key) / pass (有 key)   | `ACT_ANTHROPIC_API_KEY` / `ACT_ZHIPU_API_KEY` 环境变量控制，无 key 时 GTEST_SKIP()                                                                            |                |

### Batch 3: P1a/P1b 补齐

| #   | ID  | Title                                                   | Scope       | Depends            | Status | Verification                            | Notes                                                                                                                                                       |
| --- | --- | ------------------------------------------------------- | ----------- | ------------------ | ------ | --------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------- |
| 29  | N1  | 实现 GitHub Actions CI（Windows 编译 + 测试）           | infra       | -                  | [x]    | CI workflow 触发后 build + test 全绿   | `.github/workflows/ci.yml`，MSVC + vcpkg + Qt6，cache vcpkg packages                                                                                       | commit: 74b2cbc |
| 30  | N2  | 实现 Markdown 终端输出                                   | frontend    | T10                | [x]    | build + test pass                      | MarkdownFormatter: 代码块边框、标题下划线、粗体大写、列表项目符、行内代码括号、水平线；集成到 CliRepl human 模式                         | commit: 73d58aa |
| 31  | N3  | 实现 PatchTransaction v0（单文件修改预览/确认）          | integration | T6                 | [ ]    | build + test pass                      | 文件修改前生成 diff 预览，用户确认后提交；为 v1 多文件批量修改打基础                                                                                   |
| 32  | N4  | 实现回归任务集 v0                                         | testing     | T10                | [ ]    | build + test pass                      | 自动化回归测试用例：读取文件、搜索、编辑、执行命令、权限拒绝；为 EvalRunner 提供输入                                                               |
| 33  | N5  | 实现端到端 CLI 测试                                       | testing     | T10, LLM-T12       | [ ]    | build + test pass (无 key 时 GTEST_SKIP) | `aictl` 实际调用的集成测试（可用 mock LLM），覆盖完整 REPL 输入→工具调用→输出闭环                                                             |

### Batch 4: P2 核心能力

| #   | ID  | Title                                                   | Scope       | Depends            | Status | Verification                            | Notes                                                                                                                                                       |
| --- | --- | ------------------------------------------------------- | ----------- | ------------------ | ------ | --------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------- |
| 34  | N6  | 实现 ContextManager 三层压缩                             | backend     | T8                 | [ ]    | build + test pass                      | auto-compact 自动触发 + manual-compact 用户触发；当前仅有 microCompact，补齐完整三层（micro / auto / manual）                                         |
| 35  | N7  | 实现 DiffViewTool（修改预览）                             | backend     | T14                | [x]    | build + test pass                      | CLI diff 输出工具，支持 staged/unstaged/all 模式 + stat_only；unified diff 格式，change statistics                                            | commit: 74b2cbc |
| 36  | N8  | 实现 PatchTransaction v1（多文件预览/部分失败）           | integration | N3, N7             | [ ]    | build + test pass                      | 多文件批量修改预览、Accept/Reject 逐文件确认、部分失败回滚处理                                                                                       |
| 37  | N9  | 实现 EvalRunner v0                                        | testing     | N4                 | [ ]    | build + test pass                      | 执行回归任务集并记录通过率，输出结构化 JSON 报告（pass/fail/skip/timeout）                                                                           |
| 38  | N10 | 实现 ResumeTask（Checkpoint 恢复）                        | integration | T9                 | [ ]    | build + test pass                      | 从 Checkpoint 恢复中断任务，支持单任务恢复到最近确认点；利用现有 Checkpoint 骨架                                                                   |

### Batch 5: P3 基础

| #   | ID  | Title                                                   | Scope       | Depends            | Status | Verification                            | Notes                                                                                                                                                       |
| --- | --- | ------------------------------------------------------- | ----------- | ------------------ | ------ | --------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------- |
| 39  | N11 | 实现 GitCommitTool                                        | backend     | T14                | [x]    | build + test pass                      | 调用 `git add` + `git commit -m`，返回 commit hash；conventional commit 格式校验（warning only）                                                    | commit: 74b2cbc |
| 40  | N12 | 实现 Fallback 链（主模型→备用模型）                       | backend     | LLM-T9             | [x]    | build + test pass                      | AIEngine::tryStreamWithProvider 递归重试；ConfigManager 支持 [network].fallback_providers TOML 数组；fallbackTriggered 信号              | commit: 73d58aa |
| 41  | N13 | 实现 AgentScheduler v0（串行流水线）                      | integration | T15                | [ ]    | build + test pass                      | 串行执行多个 Task Graph，支持依赖阻塞与按序调度；为并行调度（P4）打基础                                                                             |
| 42  | N14 | 实现 ExecutionLane / WorktreeManager v1                   | integration | T15                | [ ]    | build + test pass                      | 任务与执行目录解耦，git worktree 隔离；基于现有 worktree lane 抽象实现                                                                                 |
| 43  | N15 | 实现 RepoMapTool（基于文件树）                            | backend     | T5                 | [x]    | build + test pass                      | 基于 IFileSystem.listFiles 递归构建文件树，显示项目结构、文件/目录计数、git branch；线程安全                                                      | commit: 74b2cbc |

## Dependency Waves

### Batch 1 & 2 (T1~T16, LLM-T1~T12)

| Wave | Tasks      |
| ---- | ---------- |
| 1    | T1         |
| 2    | T2, T3     |
| 3    | T4, T5     |
| 4    | T6, T7, T8 |
| 5    | T9         |
| 6    | T10        |
| 7    | T11, T12   |
| 8    | T13, T14   |
| 9    | T15        |
| 10   | T16        |

### Batch 3: P1a/P1b 补齐

| Wave | Tasks                                    |
| ---- | ---------------------------------------- |
| B3-1 | N1 (CI, 无依赖)                          |
| B3-2 | N2, N3, N4 (依赖已有 T6/T10)             |
| B3-3 | N5 (依赖 N4 + LLM-T12)                   |

### Batch 4: P2 核心

| Wave | Tasks                                    |
| ---- | ---------------------------------------- |
| B4-1 | N6, N7, N10 (依赖已有 T8/T14/T9)         |
| B4-2 | N8 (依赖 N3 + N7)                        |
| B4-3 | N9 (依赖 N4)                             |

### Batch 5: P3 基础

| Wave | Tasks                                    |
| ---- | ---------------------------------------- |
| B5-1 | N11, N12, N15 (依赖已有 T14/LLM-T9/T5)   |
| B5-2 | N13, N14 (依赖已有 T15)                  |

## Round Estimate

### Batch 1 & 2 (已完成)

- 任务数：28
- 实际执行轮次：已完成

### Batch 3: P1a/P1b 补齐

- 任务数：5
- 预估执行轮次：3 轮
- 并行潜力：B3-2 中 N2、N3、N4 可并行

### Batch 4: P2 核心

- 任务数：5
- 预估执行轮次：3 轮
- 并行潜力：B4-1 中 N6、N7、N10 可并行

### Batch 5: P3 基础

- 任务数：5
- 预估执行轮次：2 轮
- 并行潜力：B5-1 中 N11、N12、N15 可并行；B5-2 中 N13、N14 可并行

### 总计

- 新增任务数：15
- 预估执行轮次：8 轮（Batch 3~5 可流水线重叠）
- 风险最高阶段：N8（PatchTransaction v1 多文件逻辑）、N13（AgentScheduler 依赖图调度）

## Approval Checklist

- [ ] 任务队列已对齐当前 runtime-first 设计，而不是旧版原生 GUI 优先路线
- [ ] 每个任务都能独立 build + test 验证
- [ ] 依赖关系已显式声明，无隐式前置条件
- [ ] P1 CLI 保持 headless，不混入 GUI 依赖
- [ ] 技能加载、子智能体隔离、上下文压缩、任务图与执行通道都已进入实现队列

## Deferred Scope

- P4 `Multi-Agent / Team Protocol / ExternalHarness / ACP / MCP / PluginLoader`
- 三平台完整发布流水线
- Native GUI 的高级交互打磨和视觉优化
- 更完整的权限治理、信任策略和组织级策略中心
- VS Code Extension MVP (T11)：TypeScript/Node.js 技术栈，需独立前端项目
- RepoMapTool 完整版：基于 tree-sitter 的语义级代码索引（当前 N15 仅文件树级别）
- AgentScheduler 并行调度：当前 N13 仅串行流水线
- ExecutionLane 完整隔离：当前 N14 为 worktree v1 基础版
