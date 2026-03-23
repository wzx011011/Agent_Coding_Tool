# Task Queue — ACT CLI Runtime MVP (P1a + P1b)

## Metadata
- Created: 2026-03-23
- Source: ACT-开发计划与进度.md（P1a 最小闭环 + P1b 能力补齐）
- Status: pending
- Completed: 0/14

## Planning Notes

- 本队列只覆盖可直接进入实现的 P1a / P1b，P2-P4 保留在路线图中，待 P1 稳定后再单独拆分。
- 每个任务必须满足独立可验证；验证默认基于 `cmake --preset default`、`cmake --build build --config RelWithDebInfo`、`ctest --test-dir build --config RelWithDebInfo --output-on-failure`。
- 任务命名按“先搭骨架，再接 Provider，再接 Tool，再闭环，再补齐安全与回归”的顺序组织。

## Tasks

| # | ID | Title | Scope | Depends | Status | Verification | Notes |
|---|----|-------|-------|---------|--------|--------------|-------|
| 1 | T1 | 初始化 CMake 工程骨架与目录结构 | infra | - | [-] | configure + build + test pass | 建立 `src/`、`tests/`、`app/cli`、vcpkg manifest、CMake presets、headless CLI target（仅 QtCore）、preflight 检查、README 启动说明 |
| 2 | T2 | 建立核心类型与服务接口 | backend | T1 | [ ] | build pass + unit: core types compile coverage | 包含 `ToolResult`、`LLMMessage`、`TaskState`、`PermissionRequest`、`ILLMProvider`、`IAIEngine` |
| 3 | T3 | 实现 ConfigManager 与 AnthropicProvider 流式调用 | backend | T1, T2 | [ ] | build + test pass + mock/provider stream test green | 含 API Key 读取、SSE 流解析、结构化错误返回 |
| 4 | T4 | 实现 AIEngine 门面与 Provider 切换 | backend | T2, T3 | [ ] | build + test pass | 验证 provider 注入、透传 streaming、未设置 provider 错误 |
| 5 | T5 | 定义 ITool 契约并实现 ToolRegistry | backend | T1, T2 | [ ] | build + test pass | 完成 Tool 注册、查找、重复注册保护、执行入口 |
| 6 | T6 | 实现 FileReadTool 与 GrepTool | backend | T5 | [ ] | build + test pass | 覆盖行范围读取、正则搜索、工作区边界校验 |
| 7 | T7 | 实现 FileWriteTool 与 PermissionManager | integration | T2, T5 | [ ] | build + test pass | 覆盖写权限审批、目录自动创建、工作区外写入拒绝 |
| 8 | T8 | 实现 ContextManager 与 token 估算策略 | backend | T2 | [ ] | build + test pass | 实现窗口估算、截断策略、系统消息保留 |
| 9 | T9 | 实现 AgentLoop 最小单任务闭环 | integration | T4, T5, T6, T7, T8 | [ ] | build + test pass + unit: agent loop scenarios green | 覆盖 tool call、权限拒绝后继续、provider 错误回传 |
| 10 | T10 | 实现 CLI REPL、权限确认与 Markdown 终端渲染 | frontend | T7, T9 | [ ] | build + manual: CLI single task works + no QtWidgets linkage | 提供 `aictl agent` 与交互式确认链路，保持 CLI headless（禁止 QtWidgets/QScintilla/QTermWidget） |
| 11 | T11 | 补齐 P1a 测试与 Windows 单平台 CI | testing | T6, T7, T8, T9, T10 | [ ] | build + test pass + CI workflow green | 含 vcpkg baseline 锁定、Harness/Framework 单测、P1a e2e |
| 12 | T12 | 实现 TaskState、Checkpoint 骨架与 RuntimeEventLogger | backend | T9 | [ ] | build + test pass | 为长任务恢复、事件记录、状态可观测性预留稳定接口 |
| 13 | T13 | 实现 FileEditTool、GlobTool、ShellExecTool 与 Shell 安全策略 | backend | T5, T7, T12 | [ ] | build + test pass | 覆盖超时、危险命令拦截、工作目录限制、精确替换 |
| 14 | T14 | 实现 PatchTransaction v0 与 P1b 回归任务集 | testing | T10, T11, T12, T13 | [ ] | build + test pass + regression suite green | 覆盖读取、搜索、编辑、命令执行、权限拒绝后继续推进 |

## Dependency Waves

| Wave | Tasks |
|------|-------|
| 1 | T1 |
| 2 | T2, T5 |
| 3 | T3, T6, T7, T8 |
| 4 | T4, T9 |
| 5 | T10, T12 |
| 6 | T11, T13 |
| 7 | T14 |

## Approval Checklist

- [ ] 任务范围聚焦 P1a / P1b，无混入 P2 GUI 交付
- [ ] 每个任务都能独立 build + test 验证
- [ ] 依赖关系已显式声明，无隐式前置条件
- [ ] 队列顺序符合“先闭环、再补能力、最后补回归”的策略

## Deferred Phases

- P2：Qt GUI、Diff 预览、终端面板、任务状态 UI
- P3：AgentScheduler、RepoMap、多 Provider、Fallback、Failure Classifier
- P4：VS Code Extension、MCP/ACP、插件系统、三平台发布