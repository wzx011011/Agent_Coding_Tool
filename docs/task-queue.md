# Task Queue — ACT Runtime-First Implementation Plan

## Metadata
- Created: 2026-03-23
- Source: ACT-PRD-产品需求文档.md + ACT-系统架构设计.md + ACT-开发计划与进度.md + ACT-技术选型报告.md
- Scope: P1a / P1b / P2 / P3 implementation planning
- Status: pending approval
- Completed: 9/16

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

| # | ID  | Title | Scope | Depends | Status | Verification | Notes |
|---|-----|-------|-------|---------|--------|--------------|-------|
| 1 | T1 | 初始化工程骨架与分层目标 | infra | - | [x] | configure + build + smoke test pass | 建立 `src/core`、`src/framework`、`src/harness`、`src/cli`、`src/presentation/vscode-protocol`、`tests/`、vcpkg manifest、CMake preset；CLI 仅链接 `QtCore` | commit: 68fcb49 |
| 2 | T2 | 定义核心类型、错误码与事件模型 | backend | T1 | [x] | build + unit tests pass | 包含 `ToolResult`、`ToolCall`、`LLMMessage`、`PermissionRequest`、`TaskState`、`RuntimeEvent`、结构化 error code | commit: 59ba7da |
| 3 | T3 | 建立 IService / IInfrastructure / ITool 契约 | backend | T1, T2 | [x] | build + interface compile tests pass | 固化层间接口，保证 runtime 与表面解耦 | commit: 140ca9a |
| 4 | T4 | 实现 ConfigManager、AnthropicProvider 与 AIEngine | backend | T2, T3 | [x] | build + provider tests pass | 包含 SSE 流式、provider 注入、未配置 provider 错误、基础 fallback 接口 | commit: 3a10e08 |
| 5 | T5 | 实现 ToolRegistry 与基础只读工具集 | backend | T2, T3 | [x] | build + harness tests pass | 实现 `FileReadTool`、`GrepTool`、`GlobTool`，完成 safe_path / 工作区边界校验 | commit: 3a10e08 |
| 6 | T6 | 实现权限系统与可写工具基线 | integration | T2, T3, T5 | [x] | build + tests pass | 实现 `PermissionManager`、`FileWriteTool`、`FileEditTool`、写入审批与工作区外写入拒绝 | commit: 2d12f77 |
| 7 | T7 | 实现 ShellExecTool 与 Shell 安全策略 v0 | backend | T2, T3, T6 | [x] | build + tests pass | 覆盖超时、危险命令拦截、工作目录限制、allowlist / denylist 基线 | commit: 2d12f77 |
| 8 | T8 | 实现 ContextManager 与三层压缩骨架 | backend | T2, T3, T4 | [x] | build + tests pass | 先落 `estimateTokens`、窗口治理、Micro Compact / Auto Compact / Manual Compact 框架 | commit: 2d12f77 |
| 9 | T9 | 实现 AgentLoop、TaskState 与 Checkpoint 骨架 | integration | T4, T5, T6, T7, T8 | [x] | build + framework tests pass | 覆盖 tool_use 循环、权限拒绝后继续、结构化失败回传、基础 TaskState 流转 | commit: ce4171e |
| 10 | T10 | 实现 CLI REPL 与 JSON Lines runtime 协议 | frontend | T6, T7, T9 | [ ] | build + CLI e2e tests pass | 同时支持人类可读 REPL 和机器可读 `--json` 事件流，为 VS Code Extension 提供协议面 |
| 11 | T11 | 实现 VS Code Extension MVP | frontend | T10 | [ ] | build + extension smoke workflow pass | 完成 `spawn aictl`、chat 入口、命令入口、权限响应回传、最小 diff 审核闭环 |
| 12 | T12 | 实现 SkillCatalog、SkillLoader 与 load_skill | backend | T3, T8, T9 | [ ] | build + tests pass | 实现两层技能注入：system prompt 摘要常驻，正文按需以 tool_result 注入；记录 Skill Trace |
| 13 | T13 | 实现 SubagentManager 与 Explore / Code 子智能体 | integration | T9, T10, T12 | [ ] | build + tests pass | 子任务使用独立 `messages[]`，Explore 默认只读，主会话仅接收摘要与结构化结果 |
| 14 | T14 | 实现 PatchTransaction、Git 只读工具与 RuntimeTraceStore | integration | T6, T7, T9, T10 | [ ] | build + tests pass | 落地 `GitStatusTool`、`GitDiffTool`、`PatchTransaction v0/v1`、`RuntimeEventLogger`、`RuntimeTraceStore v1` |
| 15 | T15 | 实现 Task Graph、Resume / Replay 与 Execution Lane | backend | T9, T13, T14 | [ ] | build + tests pass | 引入 `TaskStateStore`、依赖图、artifact 引用、background lane、worktree lane 抽象 |
| 16 | T16 | 接入 Native GUI Beta 与 P1-P3 回归评测 | integration | T11, T14, T15 | [ ] | build + test + targeted regression pass | 接入 Qt GUI 壳、任务状态与事件流面板、DiffWidget、EvalRunner v0/v1、关键回归任务集 |

## Dependency Waves

| Wave | Tasks |
|------|-------|
| 1 | T1 |
| 2 | T2, T3 |
| 3 | T4, T5 |
| 4 | T6, T7, T8 |
| 5 | T9 |
| 6 | T10 |
| 7 | T11, T12 |
| 8 | T13, T14 |
| 9 | T15 |
| 10 | T16 |

## Round Estimate

- 任务数：16
- 预估执行轮次：10 轮
- 并行潜力：Wave 3、Wave 4、Wave 7、Wave 8 可局部并行
- 风险最高阶段：T13（Subagent）、T15（Task Graph + Execution Lane）、T16（Native GUI + Regression）

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