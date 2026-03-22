# ACT — 技术选型报告

C++/Qt6 原生 AI IDE · 2026-03-21

> 本文档是决策记录，写完即归档。技术选型变更需说明理由。

---

## 一、编程语言：C++20

| 项目 | 语言 | 优势 | 劣势 |
|------|------|------|------|
| **我们的方案** | **C++20** | 原生性能、零 GC、内存可控 | 开发速度慢于脚本语言 |
| Claude Code | TypeScript | 生态丰富、开发快 | Electron 内存大、速度慢 |
| Cursor | TypeScript | 同上 | 同上 |
| Zed | Rust | 性能好、内存安全 | 学习曲线陡、Qt 生态缺失 |
| Aider | Python | 开发最快 | 性能差、不能做 GUI IDE |

**决策理由**：性能是核心卖点，C++ 是唯一选择。我们精通 C++ 是最大的可实施性保障。

---

## 二、GUI 框架：Qt6 QWidgets（非 QML）

| 维度 | QWidgets | QML |
|------|----------|-----|
| 代码编辑器集成 | QScintilla 原生嵌入 | 需 QQuickWidget 桥接，有性能损耗 |
| 停靠分栏 | QDockWidget + QSplitter 原生 | 需手写或第三方组件 |
| 文本渲染 | QPainter 原生，无中间层 | 通过 Scene Graph，有 JS 开销 |
| 成熟 IDE 先例 | Qt Creator 就是纯 QWidgets | 无主流 IDE 使用 |
| 我们的经验 | 精通 | 有了解但不精通 |

**决策理由**：QScintilla + QWidgets 是 IDE 场景最成熟的组合，Qt Creator 本身就是最佳证明。

---

## 三、代码编辑器：QScintilla

| 项目 | 编辑器内核 | 语法高亮 | 代码折叠 | 性能 |
|------|-----------|---------|---------|------|
| **我们的方案** | **QScintilla** | **100+ 语言** | **原生** | **原生级** |
| VS Code | Monaco | ✅ | ✅ | 好（吃内存） |
| Zed | 自研 tree-sitter | ✅ | ✅ | 最好 |
| Qt Creator | QScintilla | ✅ | ✅ | 好 |

**决策理由**：QWidgets 生态最成熟的代码编辑器组件，直接嵌入不造轮子。

---

## 四、HTTP 客户端：cpp-httplib

| 项目 | HTTP 库 | 说明 |
|------|---------|------|
| **我们的方案** | **cpp-httplib** | **Header-only，零外部依赖** |
| Claude Code | Node.js 内置 fetch | Node.js 内置 |
| Aider | httpx（Python） | Python 生态 |

**为什么不用 libcurl**：cpp-httplib 更轻量（单 header），只需 HTTP/SSE 的场景足够。

---

## 五、JSON 解析：nlohmann/json

C++ 生态事实标准 JSON 库，现代 API，Header-only。

---

## 六、构建系统：CMake + vcpkg

| 项目 | 构建系统 | 包管理 |
|------|---------|--------|
| **我们的方案** | **CMake** | **vcpkg** |
| Zed | Cargo | crates.io |
| VS Code | Electron Builder | npm |
| Qt Creator | CMake + Qbs | 系统包管理器 |

**决策理由**：vcpkg 微软出品，Windows 体验最好，跨平台三端支持。QScintilla、libgit2、cmark 等一行命令安装。

---

## 七、LLM API + Agent Loop：自研

| 项目 | LLM 调用 | Agent Loop | 说明 |
|------|---------|-----------|------|
| **我们的方案** | **自研（cpp-httplib）** | **自研（~3000行）** | **多 Provider** |
| Claude Code | Anthropic SDK（TS） | 自研 | 闭源 |
| Cursor | 自研 | 自研 | 闭源 |
| Cline | Vercel AI SDK | 自研（~800行） | 开源参考 |
| Aider | 自研（Python） | 自研（~2000行） | 开源参考 |

**为什么自研**：

- C++ 没有 Agent Loop 框架（Python 有 LangChain，Rust 有 Rig，C++ 是荒漠）
- 没有 C++ 官方 LLM SDK
- 各家 API 协议大同小异，自研最简单
- 所有主流 AI IDE 的 LLM 层都是自研的

**Agent Loop 工作量（按架构层分解）**：

| 架构层 | 模块 | 预估行数 |
|--------|------|---------|
| Framework | AgentLoop 核心循环 | ~100 |
| | AgentScheduler 调度器 | ~300 |
| | ContextManager 上下文管理 | ~300 |
| | PermissionManager 权限管理 | ~200 |
| Harness | ITool 接口 + ToolRegistry | ~300 |
| | FileReadTool + FileWriteTool + FileEditTool | ~400 |
| | ShellExecTool | ~200 |
| | GlobTool + GrepTool | ~200 |
| Core | AIEngine + ILLMProvider 抽象 | ~400 |
| | AnthropicProvider（SSE 流式） | ~300 |
| | 错误处理 + 日志 | ~150 |
| **合计** | | **~2850 行** |

---

## 八、其他依赖

| 组件 | 选型 | 用途 | 对标 | 架构层 |
|------|------|------|------|--------|
| 终端组件 | QTermWidget | 底部终端面板 | VS Code 内置终端 | Presentation |
| Markdown 渲染 | cmark / hoedown | AI Chat 面板 | Claude Code Desktop | Presentation |
| Git 集成 | libgit2 | gitStatus / gitDiff | Aider 用 subprocess | Harness (Tool) |
| Diff 解析 | diff-match-patch | Agent Diff 预览 | Cline 自研 | Harness (Tool) |
| AST 解析 | tree-sitter | Repo Map | Aider 同款 | Core |
| CLI 交互 | GNU readline | REPL 补全/历史 | Claude Code 同款 | Presentation |
| LSP 客户端 | 自研（P4） | clangd/pyright | VS Code 同款 | Harness (Tool) |

---

## 九、技术栈完整清单

| 架构层 | 技术 | 说明 |
|--------|------|------|
| 语言 | C++20 | 核心优势，性能+精通 |
| Framework | 自研 | AgentScheduler + AgentLoop + ContextManager + PermissionManager |
| Harness | 自研 | ITool 接口 + ToolRegistry + 16 个内置 Tool |
| Core | 自研 | AIEngine + ProjectManager + CodeAnalyzer + ConfigManager |
| GUI 框架 | Qt6 QWidgets | 非 QML |
| 代码编辑器 | QScintilla | 语法高亮/折叠 |
| HTTP 客户端 | cpp-httplib | Header-only |
| JSON 解析 | nlohmann/json | 现代 C++ JSON 库 |
| 构建系统 | CMake + vcpkg | 跨平台包管理 |
| AST 解析 | tree-sitter | Repo Map |
| LSP 客户端 | 自研（P4） | clangd/pyright |
| 终端 | QTermWidget | 底部终端 |
| Markdown | cmark / hoedown | Chat Panel |
| Git | libgit2 | gitStatus/gitDiff |
| Diff | diff-match-patch | Diff 预览 |
| CLI | GNU readline | REPL |

---

## 十、相关文档

- 产品需求文档：ACT — PRD 产品需求文档
- 系统架构设计：ACT — 系统架构设计
- 开发计划与进度：ACT — 开发计划与进度

---

整理：小欧 🦊 · 2026-03-21
