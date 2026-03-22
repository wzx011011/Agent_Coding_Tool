# ACT — 开发计划与进度

C++/Qt6 原生 AI IDE · 2026-03-21

---

## 一、四阶段路线

| Phase | 时间 | 核心模块 | 交付物 | 对标水平 |
|-------|------|---------|--------|---------|
| P1 基础框架 | 2-4 周 | Framework 骨架 + Harness 6 Tool + CLI | aictl agent "修复bug" 可用 | 早期 Aider |
| P2 智能对话 | 4-6 周 | Qt GUI + Chat Panel + Diff 预览 | GUI Beta | Claude Code CLI |
| P3 Agent 智能 | 6-10 周 | 多 Agent 编排 + Repo Map + Git Tool | 可用 IDE | Cline / Cursor |
| P4 生态扩展 | 长期 | ExternalHarness + LSP + 插件系统 | 完整 IDE | VS Code + Cursor |

---

## 二、各阶段任务（按架构层分解）

### P1 基础框架（2-4 周）

**🟡 Agent Framework 层**
- [ ] ITool 接口定义（itool.h）— 所有 Tool 的契约
- [ ] ToolRegistry — Tool 注册/发现/执行
- [ ] ContextManager — 消息管理 + 窗口计算
- [ ] PermissionManager — CLI Y/N 确认（注入式设计）
- [ ] AgentLoop — while 循环 + Tool Calling

**🟠 Agent Harness 层**
- [ ] FileReadTool（支持行范围）
- [ ] FileWriteTool
- [ ] FileEditTool（精确替换）
- [ ] ShellExecTool（含超时控制）
- [ ] GlobTool（文件模式匹配）
- [ ] GrepTool（正则搜索）

**🔵 Core Services 层**
- [ ] AIEngine 门面
- [ ] ILLMProvider 抽象接口
- [ ] AnthropicProvider（SSE 流式）
- [ ] ConfigManager（模型/Key 管理）

**🟢 Presentation Layer**
- [ ] CLI REPL 模式（GNU readline）
- [ ] CLI Permission Handler（Y/N 确认）
- [ ] Markdown 终端输出

**🧪 测试**
- [ ] Harness 层单元测试（6 个 Tool）
- [ ] Framework 层单元测试（ToolRegistry、ContextManager）
- [ ] 端到端测试：aictl agent "读取 main.cpp 并解释"

---

### P2 智能对话（4-6 周）

**🟡 Agent Framework 层**
- [ ] ContextManager 增强：自动压缩策略

**🟠 Agent Harness 层**
- [ ] GitStatusTool
- [ ] GitDiffTool
- [ ] DiffViewTool（修改预览）

**🟢 Presentation Layer（GUI）**
- [ ] Qt GUI 主窗口（QMainWindow + QDockWidget）
- [ ] AI Chat Panel（cmark Markdown 渲染）
- [ ] QScintilla 编辑器集成
- [ ] PermissionDialog（图形化权限确认弹窗）
- [ ] DiffWidget（并排 Diff 预览，Accept/Reject）
- [ ] 底部终端（QTermWidget）
- [ ] 文件浏览器 + 搜索

---

### P3 Agent 智能（6-10 周）

**🟡 Agent Framework 层**
- [ ] AgentScheduler 并行任务编排
- [ ] AgentScheduler 串行流水线

**🟠 Agent Harness 层**
- [ ] GitCommitTool
- [ ] RepoMapTool（tree-sitter）
- [ ] LspTool（clangd / pyright）

**🔵 Core Services 层**
- [ ] CodeAnalyzer（tree-sitter AST）
- [ ] 多 Provider 支持（OpenAI / Claude / GLM）
- [ ] Fallback 链（主模型失败 → 备用模型）

---

### P4 生态扩展（长期）

**🟡 Agent Framework 层**
- [ ] 嵌套编排（main → orchestrator → workers）

**🟠 Agent Harness 层**
- [ ] WebFetchTool
- [ ] WebSearchTool
- [ ] ExternalHarnessTool
- [ ] ACP Client（调用 Claude Code / Codex）
- [ ] MCP Client（调用外部 MCP Server）
- [ ] PluginLoader（动态 .so/.dll 加载）

**🟢 Presentation Layer**
- [ ] VS Code Extension（TS → spawn aictl）

**DevOps**
- [ ] GitHub Actions CI（Linux / Windows / macOS）
- [ ] 自动发布（.exe / .AppImage / .dmg）

---

## 三、跨平台策略

- **开发环境**：选最舒服的系统开发，不限制
- **跨平台**：GitHub Actions CI 三平台编译
- **发布**：Windows .exe + Linux .AppImage + macOS .dmg

---

## 四、进度追踪

项目进度表：https://gcnaf7ct1kpv.feishu.cn/base/WDPPbs2mKakUpzsndzmcr1Qvncf

⚡ 当前进度：尚未开始（等大老板说"开工"）

---

## 五、相关文档

- 产品需求文档：ACT — PRD 产品需求文档
- 技术选型报告：ACT — 技术选型报告
- 系统架构设计：ACT — 系统架构设计

---

整理：小欧 🦊 · 2026-03-21
