# ACT — 系统架构设计

C++/Qt6 原生 AI IDE · 2026-03-21

## 一、设计理念：CLI 优先，多 Surface 渲染

对标 Claude Code 的架构模式：

| | Claude Code | 我们的方案 |
|---|------------|-----------|
| 核心语言 | Node.js（TS） | C++ |
| 核心接口 | CLI（claude 命令） | CLI（aictl 命令） |
| 桌面 GUI | Electron 壳（调用 CLI） | Qt GUI（直接调用 Core） |
| VS Code 集成 | TS 扩展 → spawn CLI | TS 扩展 → spawn CLI |
| Desktop 性能 | Electron（重） | Qt 原生（轻） |

核心思路：一个 Core，多个前端。CLI 是核心，Qt GUI 和 VS Code 扩展都只是不同的"显示器"。

---

## 二、系统架构总览（五层架构）

```
┌──────────────────────────────────────────────────────────┐
│           LAYER 1 · Presentation Layer（表现层）            │
│     Qt GUI（桌面版）· CLI（aictl）· VS Code Extension      │
├──────────────────────────────────────────────────────────┤
│           LAYER 2 · Agent Framework（Agent 框架层）         │
│     AgentScheduler · ContextManager · PermissionManager   │
│            「决定做什么、按什么顺序」                         │
├─────────────────── ITool Interface（唯一桥梁）────────────┤
│           LAYER 3 · Agent Harness（Agent 执行层）            │
│     ToolRegistry · FileRead/Write/Edit · ShellExec · ...  │
│            「决定具体怎么执行」                               │
├──────────────────────────────────────────────────────────┤
│           LAYER 4 · Core Services（核心服务层）              │
│     AIEngine · ProjectManager · CodeAnalyzer · ConfigMgr  │
├──────────────────────────────────────────────────────────┤
│           LAYER 5 · Infrastructure（基础设施层）             │
│               fs · network · process · terminal            │
└──────────────────────────────────────────────────────────┘
```

### 架构演进：三层 → 五层

原三层架构的 Core Layer 是一个大杂烩，把调度、工具执行、AI 引擎全堆在一起。现在将它拆解为三个独立层：

- **Agent Framework（Layer 2）**：原来 Core 中的 AgentLoop、权限控制、上下文管理 → 抽取为独立的框架层
- **Agent Harness（Layer 3）**：原来 Core 中的 Tool Calling / 执行 → 抽取为独立的执行层
- **Core Services（Layer 4）**：原来 Core 中的 AIEngine、ProjectManager、CodeAnalyzer → 留为核心服务层

三者通过 **ITool 标准接口** 解耦，各自独立演进。

### 各层职责

**Presentation Layer（表现层）**
- 纯 UI 壳，不含业务逻辑
- CLI（aictl）、Qt GUI、VS Code Extension 三个前端
- 统一通过接口调用 Core Layer

**Core Layer → 三层拆分（见第三章详细说明）**
- 所有业务逻辑的唯一归属地
- AIEngine / ProjectManager / CodeAnalyzer / AgentLoop / ConfigManager 五大模块
- 不依赖任何 Qt Widget 头文件，只依赖 QtCore

**Infrastructure Layer（基础设施层）**
- 文件系统、网络、进程管理、终端的抽象封装
- 为 Core Layer 提供统一的基础能力接口

---

## 三、Agent Framework + Harness 架构设计

### 为什么 Framework + Harness 分离？

参考 Claude Code / OpenClaw / Cline 的行业最佳实践：

- **Framework** 决定"做什么、按什么顺序"：调度、编排、上下文管理、权限控制
- **Harness** 决定"具体怎么执行"：文件读写、终端命令、Git 操作、代码搜索
- 两者通过 **ITool 标准接口** 解耦，各自独立演进

### 关键组件

| 层 | 核心组件 | 职责 |
|---|---------|------|
| Agent Framework | AgentScheduler | Agent 循环、Tool Call 调度 |
| | ContextManager | 消息管理、窗口计算、自动压缩 |
| | PermissionManager | 权限检查、GUI弹窗/CLI确认注入 |
| Agent Harness | ToolRegistry | Tool 注册/发现/执行/权限检查 |
| | ITool 实现 | FileRead/Write/Edit, ShellExec, Glob, Grep 等 |
| Core Services | AIEngine | 多模型 LLM 抽象、Streaming、Fallback |
| | ProjectManager | 工作区、文件、Git 管理 |
| | CodeAnalyzer | Repo Map、tree-sitter AST |

### ITool 接口（Framework 和 Harness 的唯一桥梁）

所有 Tool 必须实现此接口：

- `name()` / `description()` / `schema()` — 元信息（供 LLM 生成 Tool Call）
- `execute(params) → ToolResult` — 实际执行
- `permissionLevel() → Read | Write | Exec | Network | Destructive`

### 权限等级

| 等级 | 含义 | 行为 |
|------|------|------|
| Read | 读取文件/搜索 | 安全，自动放行 |
| Write | 写入/创建文件 | 需确认 |
| Exec | 执行 shell 命令 | 需确认 |
| Network | 网络请求 | 需确认 |
| Destructive | 删除/强制操作 | 严格确认 |

### 权限注入设计

GUI 和 CLI 共用同一套权限逻辑，仅表现层不同：

- **GUI**：弹出确认对话框，显示命令详情
- **CLI**：终端输出 `[y/N]` 等待用户确认

### 开发阶段

| Phase | 时间 | 核心交付 |
|-------|------|---------|
| P1 | 2-4周 | ITool + ToolRegistry + 6个基础Tool + CLI |
| P2 | 4-6周 | Qt GUI + ChatPanel + Diff预览 |
| P3 | 6-10周 | 多Agent编排 + RepoMap + Git Tool |
| P4 | 长期 | ExternalHarness + LSP + 插件系统 |

---

## 四、Core Layer 设计原则

- **只依赖 QtCore**：不 include 任何 Qt Widget 头文件
- **异步 everywhere**：GUI 用 signal/slot，CLI 用回调
- **权限注入**：GUI 弹窗确认 / CLI Y/N 确认
- **统一日志**：可输出到 GUI 或终端
- **可测试**：脱离 UI 单独跑单元测试

---

## 五、Core Layer 五大能力

| 模块 | 方法 | 功能 | 对标 |
|------|------|------|------|
| AIEngine | setProvider | 切换模型 | Claude Code --model |
| | requestCompletion | 流式补全 | Claude Code Tab 补全 |
| | chat | 多轮对话 | Claude Code 对话 |
| | agentLoop | Agent 工具调用 | Claude Code agentic |
| ProjectManager | openProject | 打开项目 | claude 命令 |
| | readFile/writeFile | 文件读写 | Cline Read/Write Tool |
| | gitStatus/gitDiff | Git 操作 | Aider git |
| CodeAnalyzer | buildRepoMap | 代码结构图 | Aider repo_map |
| | getContextForQuery | 智能裁剪上下文 | Cline smart context |
| AgentLoop | execute | 执行任务 | Claude Code agentic |
| | registerTool | 注册工具 | Cline Tool 系统 |
| | setPermissionHandler | 权限回调 | Claude Code 确认 |
| ConfigManager | currentModel/setModel | 模型管理 | /model 命令 |
| | apiKey | API Key | claude config |

---

## 六、GUI 布局

```
┌──────────────────────────────────────────────────┐
│  菜单栏 · 工具栏 · 模型选择                        │
├──────────┬───────────────────────┬───────────────┤
│ Left Dock│     Central Widget     │  Right Dock   │
│          │                        │               │
│ File     │  QScintilla 编辑器      │  AI Chat      │
│ Explorer │  + Ghost Text          │  Panel        │
│          │                        │  (Markdown)   │
│ Search   │  Diff 预览              │  模型切换      │
│ Symbol   │  确认/拒绝              │               │
│ Outline  │                        │               │
├──────────┴───────────────────────┴───────────────┤
│  Bottom Dock: QTermWidget 终端 + 诊断 + 输出      │
└──────────────────────────────────────────────────┘
```

---

## 七、CLI 命令

| 命令 | 功能 | 对标 Claude Code |
|------|------|-----------------|
| aictl | 交互式 REPL | claude |
| aictl completions | 流式补全 | Tab 补全 |
| aictl agent | Agent 模式 | agentic 模式 |
| aictl diff | Diff 预览 | diff 展示 |
| aictl config | 配置管理 | claude config |
| aictl repo-map | Repo Map | context 展示 |

---

## 八、相关文档

- 产品需求文档：ACT — PRD 产品需求文档
- 技术选型报告：ACT — 技术选型报告
- 开发计划与进度：ACT — 开发计划与进度

---

整理：小欧 🦊 · 2026-03-21
