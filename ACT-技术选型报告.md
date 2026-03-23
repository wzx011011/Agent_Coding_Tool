# ACT — 技术选型报告

Runtime-first AI Coding Tool · 2026-03-23

> 本文档是决策记录，写完即归档。技术选型变更需说明理由。

技术路线总判断：ACT 的对外交付形态不再是“原生 GUI 优先的 AI IDE”，而是“独立 coding agent runtime + 多表面交付”。第一阶段优先通过 CLI 与 VS Code Extension 验证核心能力和产品闭环，第二阶段再建设 Native GUI 作为差异化桌面载体。因此选型优先级应围绕 runtime 的稳定执行、上下文质量、Tool 调用、技能注入、执行隔离和跨表面复用，而不是单独优化某个 GUI 外壳。

本版技术路线不是把 learn-claude-code 的教学实现直接搬过来，而是消化为 ACT 自己的工程约束：保留其机制分层思路，替换其教学化的安全、持久化与权限实现。

---

## 一、吸收后的 runtime 设计原则

从 Claude Code 类产品和 learn-claude-code 这类教学逆向仓库中，ACT 明确吸收以下原则：

| 原则                         | ACT 的转化版本                                             |
| ---------------------------- | ---------------------------------------------------------- |
| loop 尽量稳定                | AgentLoop 保持小而稳定，高级能力通过独立机制叠加           |
| tool dispatch 是运行时骨架   | 所有 Tool、Skill、Permission、Trace 都走统一调度与审计入口 |
| 知识按需注入                 | Skill 摘要进 system prompt，正文按需以 tool_result 注入    |
| 子任务必须隔离上下文         | Subagent 不共享主 messages[]，仅返回摘要与结构化结果       |
| 上下文压缩要分层             | Micro / Auto / Manual Compact 三层并存                     |
| 任务状态和执行目录不是一回事 | Task Graph 与 Execution Lane / Worktree 解耦               |

不直接照搬的部分：

- 文件型 mailbox / JSONL 协议只作为教学参考，不视为生产最终方案
- 危险命令字符串拦截只作为 P1 安全兜底，不视为完整权限系统
- 纯文件 JSON 持久化可作为早期实现，但必须隐藏在 Store 接口之后

---

## 二、编程语言：C++20

| 项目           | 语言       | 优势                      | 劣势                    |
| -------------- | ---------- | ------------------------- | ----------------------- |
| **我们的方案** | **C++20**  | 原生性能、零 GC、内存可控 | 开发速度慢于脚本语言    |
| Claude Code    | TypeScript | 生态丰富、开发快          | Electron 内存大、速度慢 |
| Cursor         | TypeScript | 同上                      | 同上                    |
| Zed            | Rust       | 性能好、内存安全          | 学习曲线陡、Qt 生态缺失 |
| Aider          | Python     | 开发最快                  | 性能差、不能做 GUI IDE  |

**决策理由**：性能、资源占用和系统级控制力仍然重要，但 C++ 不再承担第一阶段整套产品壳层，而是集中承担 runtime core、PatchTransaction、TaskState、Trace、Security、Execution Isolation 等真正有壁垒的内核能力。我们精通 C++ 仍然是可实施性保障，但产品验证速度将通过 VS Code Extension 路线对冲。

---

## 三、第一阶段产品表面：CLI + VS Code Extension

| 方案                    | 优势                                           | 劣势                                       |
| ----------------------- | ---------------------------------------------- | ------------------------------------------ |
| CLI + VS Code Extension | 编辑器、终端、Diff、工作区能力成熟，验证速度快 | 受 extension API 约束                      |
| Code - OSS 深度改造     | 控制力更强                                     | 维护 fork 成本高，不适合作为第一步         |
| 纯原生 GUI 先行         | 差异化最强                                     | 会大量消耗 GUI 工程资源，拖慢 runtime 验证 |

**决策理由**：第一阶段优先采用 CLI + VS Code Extension，而不是直接深 fork Code - OSS，也不是让 Qt GUI 先行。目标是直接借用成熟编辑器、终端、Diff、工作区能力，把资源集中投入 runtime 深水区能力。

---

## 四、Native GUI 框架：Qt6 QWidgets（非 QML）

| 维度           | QWidgets                     | QML                              |
| -------------- | ---------------------------- | -------------------------------- |
| 代码编辑器集成 | QScintilla 原生嵌入          | 需 QQuickWidget 桥接，有性能损耗 |
| 停靠分栏       | QDockWidget + QSplitter 原生 | 需手写或第三方组件               |
| 文本渲染       | QPainter 原生，无中间层      | 通过 Scene Graph，有 JS 开销     |
| 成熟 IDE 先例  | Qt Creator 就是纯 QWidgets   | 无主流 IDE 使用                  |
| 我们的经验     | 精通                         | 有了解但不精通                   |

**决策理由**：QScintilla + QWidgets 是 Native GUI 第二阶段最成熟的组合，Qt Creator 本身就是最佳证明。

补充：Qt/QWidgets 不再承担第一阶段的产品验证任务，而是保留为第二阶段的桌面差异化方案，用来承载接近 TRAE 与 VS Code 的高密度 IDE 信息架构，包括活动栏、侧边栏、右侧 Agent 面板、底部终端与输出区的稳定编排。

---

## 五、代码编辑器：QScintilla

| 项目           | 编辑器内核       | 语法高亮      | 代码折叠 | 性能         |
| -------------- | ---------------- | ------------- | -------- | ------------ |
| **我们的方案** | **QScintilla**   | **100+ 语言** | **原生** | **原生级**   |
| VS Code        | Monaco           | ✅            | ✅       | 好（吃内存） |
| Zed            | 自研 tree-sitter | ✅            | ✅       | 最好         |
| Qt Creator     | QScintilla       | ✅            | ✅       | 好           |

**决策理由**：QWidgets 生态最成熟的代码编辑器组件，直接嵌入不造轮子。

---

## 六、HTTP 客户端：cpp-httplib

| 项目           | HTTP 库            | 说明                        |
| -------------- | ------------------ | --------------------------- |
| **我们的方案** | **cpp-httplib**    | **Header-only，零外部依赖** |
| Claude Code    | Node.js 内置 fetch | Node.js 内置                |
| Aider          | httpx（Python）    | Python 生态                 |

**为什么不用 libcurl**：cpp-httplib 更轻量（单 header），只需 HTTP/SSE 的场景足够。

**HTTP/2 升级路径**：cpp-httplib 不支持 HTTP/2。若后续 Provider（如 Google Gemini）要求 HTTP/2，备选方案为 libcurl（HTTP/2 + 多路复用）或 Boost.Beast（纯 C++ 方案）。届时通过 INetwork 接口替换，不影响上层逻辑。

---

## 七、JSON 解析：nlohmann/json

C++ 生态事实标准 JSON 库，现代 API，Header-only。

---

## 八、构建系统：CMake + vcpkg

| 项目           | 构建系统         | 包管理       |
| -------------- | ---------------- | ------------ |
| **我们的方案** | **CMake**        | **vcpkg**    |
| Zed            | Cargo            | crates.io    |
| VS Code        | Electron Builder | npm          |
| Qt Creator     | CMake + Qbs      | 系统包管理器 |

**决策理由**：vcpkg 微软出品，Windows 体验最好，跨平台三端支持。QScintilla、libgit2、cmark 等一行命令安装。

---

## 九、LLM API + Agent Loop：自研

| 项目           | LLM 调用                | Agent Loop          | 说明            |
| -------------- | ----------------------- | ------------------- | --------------- |
| **我们的方案** | **自研（cpp-httplib）** | **自研（~3000行）** | **多 Provider** |
| Claude Code    | Anthropic SDK（TS）     | 自研                | 闭源            |
| Cursor         | 自研                    | 自研                | 闭源            |
| Cline          | Vercel AI SDK           | 自研（~800行）      | 开源参考        |
| Aider          | 自研（Python）          | 自研（~2000行）     | 开源参考        |

**为什么自研**：

- C++ 没有 Agent Loop 框架（Python 有 LangChain，Rust 有 Rig，C++ 是荒漠）
- 没有 C++ 官方 LLM SDK
- 各家 API 协议大同小异，自研最简单
- 所有主流 AI IDE 的 LLM 层都是自研的

补充：对 ACT 来说，自研并不是为了“重复造编辑器轮子”，而是为了掌握 runtime 核心链路。如果 Framework、Harness、AIEngine 不能自洽，那么无论 VS Code Extension 还是 Native GUI 都只是薄壳。

能力对齐上，Claude Code CLI 是 runtime 行为基准，这意味着自研重点必须落在 AgentLoop、Tool Runtime、Permission、Context、Diff 审核和错误恢复，而不是优先追求 GUI 表层功能堆叠。

**Agent Loop 工作量（按架构层分解）**：

| 架构层    | 模块                                        | 预估行数          |
| --------- | ------------------------------------------- | ----------------- |
| Framework | AgentLoop 核心循环                          | ~200-400（±100）  |
|           | AgentScheduler 调度器                       | ~300-500（±100）  |
|           | ContextManager 上下文管理                   | ~300-400（±50）   |
|           | PermissionManager 权限管理                  | ~200-300（±50）   |
| Harness   | ITool 接口 + ToolRegistry                   | ~300              |
|           | FileReadTool + FileWriteTool + FileEditTool | ~400-600（±100）  |
|           | ShellExecTool                               | ~200-300（±50）   |
|           | GlobTool + GrepTool                         | ~200              |
| Core      | AIEngine + ILLMProvider 抽象                | ~400-500（±50）   |
|           | AnthropicProvider（SSE 流式）               | ~300-400（±50）   |
|           | 错误处理 + 日志                             | ~150-250（±50）   |
| **合计**  |                                             | **~3000-4000 行** |

> 注：原预估 ~2850 行偏乐观，未充分计入错误处理、日志、重试逻辑和边界条件处理。参考 Cline ~800 行（TS）和 Aider ~2000 行（Python），C++ 实现含完善错误处理后 3000-4000 行更现实。

---

## 十、Runtime 机制选型

以下不是单一第三方库选型，而是 ACT 需要明确工程化落地的运行时机制：

### 10.1 Skill Loading：两层注入

- **保留的思路**：技能摘要常驻 system prompt，正文通过 `load_skill` 以 tool_result 按需注入
- **ACT 的转化**：SkillCatalog 提供目录索引，SkillLoader 负责正文解析和 Trace 记录
- **为什么必须这样做**：如果把 git、test、review、deploy 等技能正文全部常驻 system prompt，token 浪费会直接吞掉长任务预算

### 10.2 Subagent Isolation：独立消息空间

- **保留的思路**：子任务使用新 `messages[]`
- **ACT 的转化**：SubagentManager 统一管理角色、工具集、结果摘要和权限边界
- **工程约束**：Explore 子智能体默认只读；Code 子智能体才可获得写权限；主会话只接收摘要结果和引用，不接收原始全量对话

### 10.3 Context Compact：三层压缩

- **保留的思路**：微压缩、自动压缩、手动压缩三层并存
- **ACT 的转化**：ContextManager 负责预算判断，ContextCompactor 负责压缩执行，RuntimeTraceStore 保留归档引用
- **工程约束**：压缩不仅覆盖聊天文本，还要覆盖 ToolResult、Skill 正文、终端输出、Diff 摘要和权限结果

### 10.4 Task Graph：持久化任务骨架

- **保留的思路**：任务是 runtime 的协调骨架，不只是 UI 里的 todo 清单
- **ACT 的转化**：TaskStateStore 抽象持久化，P1 可落到文件，后续可升级到更强 Store 实现
- **工程约束**：任务必须记录状态、依赖、owner、checkpoint、artifacts、评测结果引用

### 10.5 Execution Isolation：执行通道而非简单目录

- **保留的思路**：工作区隔离和并行执行要进入 runtime，而不是只在 UI 里切标签页
- **ACT 的转化**：ExecutionLaneManager 统一抽象 foreground、background、worktree 三类执行通道
- **工程约束**：任务状态和执行目录解耦；Lane 生命周期、锁、回收和审计必须显式建模

---

## 十一、其他依赖

| 组件           | 选型             | 用途                | 对标                | 架构层         |
| -------------- | ---------------- | ------------------- | ------------------- | -------------- |
| 终端组件       | QTermWidget      | 底部终端面板        | VS Code 内置终端    | Presentation   |
| Markdown 渲染  | cmark-gfm        | AI Chat 面板        | Claude Code Desktop | Presentation   |
| Git 集成       | libgit2          | gitStatus / gitDiff | Aider 用 subprocess | Harness (Tool) |
| Diff 解析      | diff-match-patch | Agent Diff 预览     | Cline 自研          | Harness (Tool) |
| AST 解析       | tree-sitter      | Repo Map            | Aider 同款          | Core           |
| CLI 交互       | GNU readline     | REPL 补全/历史      | Claude Code 同款    | Presentation   |
| LSP 客户端     | 自研（P4）       | clangd/pyright      | VS Code 同款        | Harness (Tool) |
| Extension 宿主 | VS Code          | 第一阶段主产品表面  | VS Code Extension   | Presentation   |

### 关键依赖版本策略

- **Qt**：固定 **Qt 6.10.2**，由源码编译或预装分发提供，P1 CLI 仅链接 `QtCore`，P2 GUI 再引入 Widgets 栈，避免 Widgets、网络层和打包行为跨版本漂移
- **CMake**：明确最低版本，并与 CI 使用同一版本范围
- **vcpkg**：提交 `vcpkg.json` 和 baseline，锁定三平台依赖解析结果
- **QScintilla / tree-sitter / libgit2 / QTermWidget**：在文档和构建配置中写明已验证版本，避免“本机可用、CI 失败”

Qt 来源策略：**Qt 不通过 vcpkg 提供**。项目统一使用外部安装的 Qt 6.10.2（源码自编或官方安装包），通过 `CMAKE_PREFIX_PATH` / `Qt6_DIR` 接入 CMake，避免与 vcpkg 依赖图混用造成版本分叉。

建议采用以下版本矩阵作为 ACT 的工程基线：

| 工具 / 依赖   | 目标版本                   | 说明                                             |
| ------------- | -------------------------- | ------------------------------------------------ |
| Qt            | `6.10.2`                   | 外部提供；P1 仅 `QtCore`，P2+ 再增加 `QtWidgets` |
| CMake         | `>= 3.28`                  | 本地与 CI 统一；preset 和 toolchain 行为更稳定   |
| Ninja         | `>= 1.11`                  | 默认生成器                                       |
| MSVC          | `19.40+`（VS 2022 17.10+） | Windows 主力编译器                               |
| Clang         | `>= 17`                    | 可选编译器                                       |
| GCC           | `>= 13`                    | Linux 可选编译器                                 |
| vcpkg         | baseline 锁定              | 管理非 Qt 依赖；Qt 由外部安装提供                |
| GTest / GMock | 跟随 vcpkg baseline        | 测试框架                                         |
| spdlog        | 跟随 vcpkg baseline        | 日志框架                                         |
| toml++        | 跟随 vcpkg baseline        | 配置解析                                         |
| cpp-httplib   | 跟随 vcpkg baseline        | HTTP / SSE                                       |

规则：若本机版本低于上述要求，不降低项目基线；直接提示安装或升级相应工具。

### 技术风险与备选方案

| 风险点                 | 当前选择           | 风险说明                                                  | 备选方案                                       |
| ---------------------- | ------------------ | --------------------------------------------------------- | ---------------------------------------------- |
| Ghost Text / Diff 装饰 | QScintilla         | 需要验证对内联提示、复杂标记和并排 Diff 的支持深度        | 若能力不足，补充自绘层或替换部分编辑器交互实现 |
| LSP 客户端             | 自研（P4）         | LSP 协议复杂，完全自研成本高、联调周期长                  | P4 优先封装成熟库，再按需要逐步替换            |
| SSE / 多 Provider      | cpp-httplib        | 需验证在弱网和长连接场景下的稳定性                        | 若稳定性不足，补充更成熟的 HTTP/SSE 方案       |
| Markdown 渲染          | cmark-gfm          | 两套方案已收敛为 cmark-gfm（GFM 扩展，支持表格/任务列表） | hoedown 已停止维护（最后提交 2015），不再考虑  |
| GUI 信息架构           | QWidgets 自研布局  | 需在原生技术栈中实现接近 TRAE 与 VS Code 的高密度工作流   | 先固化面板组织和交互约束，再逐步优化视觉细节   |
| VS Code Extension API  | 第一阶段主产品表面 | 某些深层交互可能受 extension API 约束                     | 若关键体验受限，再评估 Code - OSS 定制发行版   |

### 尚需补充的工程能力

当前技术选型能够支撑 ACT 的 MVP 与第一代可用版本，但距离稳定对齐 Claude Code CLI 级别的 runtime 行为，还需要补足几类并非单一库即可解决的工程能力：

- **任务状态与恢复层**：需要为 TaskState、Checkpoint、Resume、Replay 设计独立的数据结构与存储策略，这不是 QWidgets、cpp-httplib 或 tree-sitter 能自动提供的能力
- **补丁事务层**：`diff-match-patch` 可用于展示 Diff，但多文件补丁的原子提交、预演、回滚和冲突处理仍需自研 PatchTransaction 机制
- **运行时观测层**：仅有日志不足以支撑 Agent 调优，还需要 Task Trace、Tool Span、Permission Audit、Provider 行为摘要与失败分类
- **评测体系**：需要独立维护标准任务集、回归任务集和人工验收标准，否则“核心能力对齐 Claude Code CLI”无法被验证
- **执行安全与隔离**：ShellExec、WebFetch、未来 ACP/MCP 调用都需要更细粒度的沙箱、环境变量隔离与策略控制，避免 runtime 能力增强后安全边界反而变弱
- **技能与知识注入层**：SkillCatalog、SkillLoader、load_skill Trace、技能冲突处理与技能缓存都需要独立设计
- **子任务隔离层**：SubagentManager 需要管理独立上下文、工具边界、结果摘要和失败回传，不能只把“再起一个 prompt”当成子智能体

这些能力不要求在 P1 一次做完，但必须在 P1-P3 阶段逐步前置定义，否则架构虽然成立，runtime 的稳定性和可维护性会在后期迅速失控。

### 测试、日志、配置与性能分析选型

P1 即要求单元测试和端到端测试，以下选型需在开工前确定：

| 组件         | 选型                | 备选            | 决策理由                                                                  |
| ------------ | ------------------- | --------------- | ------------------------------------------------------------------------- |
| 单元测试框架 | Google Test + GMock | Catch2          | vcpkg 一行安装，C++ 生态事实标准；GMock 支持 IService/ITool Mock          |
| 日志框架     | spdlog              | Qt qDebug       | Header-only、异步写、格式化强；与 RuntimeEventLogger 对接方便             |
| 配置文件格式 | TOML（toml++）      | JSON（已有）    | 对人类更友好；Claude Code 用 JSON，但 ACT 面向开发者的本地配置更适合 TOML |
| 性能分析     | Tracy Profiler      | Valgrind / perf | 支持实时帧分析，适合 GUI + 长任务场景；Header-only 集成                   |

**spdlog 集成说明**：

- 与 RuntimeEventLogger 共享日志 sink，结构化事件同时输出到文件和 EventBus
- 日志级别：`trace`（Tool 执行细节）、`debug`（AgentLoop 决策）、`info`（任务状态）、`warn`（权限拒绝）、`error`（失败）
- GUI 运行时日志输出到 `.act/logs/`，CLI 默认 stderr

**toml++ 集成说明**：

- 用户配置文件：`~/.act/config.toml`（模型、API Key 引用、默认行为）
- 项目配置文件：`.act/project.toml`（工作区设置、Tool 白名单）
- toml++ 是 Header-only，MIT License，活跃维护（2024-2025 持续更新）

---

### C++ 的边界定义

在新路线下，C++ 不再负责第一阶段整套 IDE 产品壳，而是聚焦在以下必须坚持的内核能力：

- AgentLoop / AgentScheduler / ContextManager / PermissionManager
- Tool Runtime / PatchTransaction / TaskState / Checkpoint / Resume
- SkillLoader / SubagentManager / ContextCompactor
- RuntimeEventLogger / RuntimeTraceStore / Failure Classifier / EvalRunner
- Shell / Web / MCP / ACP 的执行安全与隔离策略
- ExecutionLane / WorktreeManager / LockManager

以下能力应尽量配置化、协议化或放在 TS 扩展层承接，避免高变化逻辑全部沉入 C++ 编译期实现：

- Prompt Policy
- Provider Profile 与模型切换策略
- Skill 命令入口与表面展示
- VS Code 侧边栏、任务入口、命令面板、Diff 审核流
- 评测任务集配置和产品层交互编排

---

## 十二、技术栈完整清单

| 架构层          | 技术              | 说明                                                            |
| --------------- | ----------------- | --------------------------------------------------------------- |
| 语言            | C++20             | 核心优势，性能+精通                                             |
| Framework       | 自研              | AgentScheduler + AgentLoop + ContextManager + PermissionManager |
| Harness         | 自研              | ITool 接口 + ToolRegistry + SkillLoader + ExecutionLane         |
| Core            | 自研              | AIEngine + ProjectManager + CodeAnalyzer + TaskStateStore       |
| GUI 宿主        | VS Code Extension | 第一阶段主产品表面                                              |
| Native GUI 框架 | Qt6 QWidgets      | 第二阶段 Native GUI，非 QML                                     |
| 代码编辑器      | QScintilla        | 第二阶段原生编辑器                                              |
| HTTP 客户端     | cpp-httplib       | Header-only                                                     |
| JSON 解析       | nlohmann/json     | 现代 C++ JSON 库                                                |
| 构建系统        | CMake + vcpkg     | 跨平台包管理                                                    |
| AST 解析        | tree-sitter       | Repo Map                                                        |
| LSP 客户端      | 自研（P4）        | clangd/pyright                                                  |
| 终端            | QTermWidget       | 底部终端                                                        |
| Markdown        | cmark-gfm         | Chat Panel（GFM 扩展）                                          |
| Git             | libgit2           | gitStatus/gitDiff                                               |
| Diff            | diff-match-patch  | Diff 预览                                                       |
| CLI             | GNU readline      | REPL                                                            |
| 单元测试        | GTest + GMock     | 单元测试 + Mock                                                 |
| 日志            | spdlog            | 异步结构化日志                                                  |
| 配置格式        | toml++            | 用户/项目配置文件                                               |
| 性能分析        | Tracy Profiler    | 实时帧分析（开发期）                                            |

---

## 十三、开发就绪性前置决策

以下六项决策缺少任意一项，P1a 无法正常启动或会在开发中引发返工。

### 10.1 多表面设计与 CLI 协议

ACT 对标 Claude Code 的三表面策略——CLI、桌面 App、VS Code Extension——架构上已覆盖（Presentation Layer），但 CLI 的机器可读输出协议必须在 P1a 就定义好，否则 P4 的 VS Code Extension 无法基于 CLI 构建，且会被迫回头改 CLI 接口。

| 表面              | 实现               | 与 runtime 连接方式                  | 阶段 |
| ----------------- | ------------------ | ------------------------------------ | ---- |
| CLI (`aictl`)     | C++ + GNU readline | 直接调用 Layer 2-4 C++ API           | P1a  |
| 桌面 App (Qt GUI) | C++ + Qt6 QWidgets | 直接调用 Layer 2-4 C++ API           | P2   |
| VS Code Extension | TypeScript         | spawn `aictl --json`，JSON-Lines IPC | P4   |

**CLI 双输出模式**

`aictl` 必须支持两种模式，通过 `--json` 标志区分：

- **Interactive 模式**（默认）：彩色 Markdown 渲染、人类可读 Y/N 确认
- **JSON-Lines 模式**（`--json`）：每行一个 JSON 事件，供 VS Code Extension / CI 脚本消费

JSON-Lines 基础事件格式（P1a 需实现的子集）：

```json
{"type":"stream_token","token":"Hello"}
{"type":"tool_call","tool":"FileReadTool","params":{"path":"main.cpp"}}
{"type":"permission_request","id":"req_001","level":"Write","description":"覆写 main.cpp"}
{"type":"permission_response","id":"req_001","approved":true}
{"type":"task_state","state":"Completed","summary":"修复完成"}
{"type":"error","code":"PROVIDER_TIMEOUT","message":"..."}
```

VS Code Extension 通过 aictl 的 stdin 发送权限确认（`{"type":"approve","id":"req_001","approved":true}`），aictl stdout 持续输出事件流。

### 10.2 核心类型定义（`src/types.h`）

以下类型是跨层接口合约的基础，**必须在 P1a 第一个 commit 中提交为 `src/types.h`**，所有模块接口头文件 `#include "types.h"`。

```cpp
// 权限等级
enum class PermissionLevel { Read, Write, Exec, Network, Destructive };

// Tool 执行结果
struct ToolResult {
    bool        success;
    QString     output;      // 成功时的输出内容
    QString     error;       // 失败时的错误描述
    QString     errorCode;   // 结构化错误码，如 "FILE_NOT_FOUND"
    QJsonObject metadata;    // Tool 特有附加数据（diff 内容、行号、exit_code 等）
};

// LLM 对话消息
struct LLMMessage {
    QString     role;        // "system" | "user" | "assistant" | "tool"
    QString     content;
    QString     toolCallId;  // 仅 role=tool 时填写
    QJsonObject toolCall;    // 仅 role=assistant 且含 Tool Call 时填写
};

// Tool Call（从 LLM 响应解析）
struct ToolCall {
    QString     id;
    QString     name;
    QJsonObject params;
};

// 任务状态
enum class TaskState {
    Running, ToolRunning, WaitingApproval, Paused, Cancelled, Failed, Completed
};

// 权限请求
struct PermissionRequest {
    QString         id;
    PermissionLevel level;
    QString         toolName;
    QString         description;  // 展示给用户的说明
    QJsonObject     params;       // 相关 Tool 参数（用于 Diff 展示等）
};
```

### 10.3 异步模型决策

**结论：统一使用 Qt 信号/槽 + QThread，不引入 `std::future` 或 C++20 coroutine。**

| 方案                 | 优点                                       | 缺点                           | 结论     |
| -------------------- | ------------------------------------------ | ------------------------------ | -------- |
| Qt 信号/槽 + QThread | 与 Qt GUI 天然集成，CLI 可通过事件循环桥接 | 需继承 QObject                 | **采用** |
| `std::future`        | 标准库，无外部依赖                         | 与 Qt 信号不兼容，跨线程易死锁 | 不采用   |
| C++20 coroutine      | 零开销、现代                               | Qt 无标准集成方案，维护成本高  | 不采用   |

**线程模型**：

| 线程           | 内容                   | 通信方式                               |
| -------------- | ---------------------- | -------------------------------------- |
| 主线程         | UI 渲染 / CLI 事件循环 | `QCoreApplication::exec()`             |
| AgentLoop 线程 | 推理循环               | 独立 QThread，通过信号向主线程报告     |
| Tool 线程池    | 每次 Tool 执行         | `QThreadPool` worker，结果通过信号回传 |
| AIEngine 线程  | HTTP/SSE 请求          | 独立 QThread，token 逐个 emit          |

**AgentLoop 向 Presentation 层暴露的核心信号**：

```cpp
signals:
    void streamToken(const QString& token);
    void toolCallStarted(const QString& toolName, const QJsonObject& params);
    void permissionRequested(const PermissionRequest& request);
    void taskStateChanged(TaskState newState);
    void taskCompleted(const QString& summary);
    void errorOccurred(const QString& code, const QString& message);
```

CLI 的 `CliHandler` 连接这些信号，将事件转为终端输出（Interactive 模式）或 JSON-Lines（`--json` 模式）。GUI 的各 Panel 直接连接对应信号。

### 10.4 Token 计数策略

| 阶段        | 方案                 | 精度             | 理由                                 |
| ----------- | -------------------- | ---------------- | ------------------------------------ |
| P1a         | `chars / 3.5` 估算   | ±20%             | 零成本，足够触发压缩时机判断         |
| P1b         | `chars / 3`（保守）  | ±25%（不超窗口） | 降低超窗口风险                       |
| P3+（可选） | 各 Provider 精确实现 | 精确             | ILLMProvider 接口加 estimateTokens() |

在 `ILLMProvider` 接口中预留 `virtual int estimateTokens(const QList<LLMMessage>&) const`，P1a 基类默认实现 `chars/3.5`，后续可按 Provider 覆盖。

### 10.5 Windows 终端方案

QTermWidget 在 Windows 无原生 PTY 支持，需分阶段处理：

| 阶段 | 方案                             | 说明                                               |
| ---- | -------------------------------- | -------------------------------------------------- |
| P1a  | `CreateProcess` + 匿名管道       | 仅 ShellExecTool 命令执行，无交互式终端 UI         |
| P2   | **ConPTY**（主）+ winpty（降级） | 底部交互式终端 Panel，ConPTY 最低要求 Win 10 1809+ |

ConPTY 是微软官方 API（`CreatePseudoConsole`），Windows Terminal 和 VS Code 内置终端均基于此。P2 的底部终端 Panel 在 Windows 上需用 ConPTY 后端替换 QTermWidget，或封装为独立 `WinTermWidget`。

### 10.6 项目目录结构

```
act/
├── CMakeLists.txt
├── vcpkg.json
├── src/
│   ├── types.h                      # P1a 第一个 commit：全局类型合约
│   ├── core/                        # Layer 4: Core Services
│   │   ├── ai_engine.h/.cpp
│   │   ├── providers/
│   │   │   ├── iprovider.h          # ILLMProvider 接口
│   │   │   └── anthropic.h/.cpp
│   │   ├── config_manager.h/.cpp
│   │   └── project_manager.h/.cpp
│   ├── framework/                   # Layer 2: Agent Framework
│   │   ├── agent_loop.h/.cpp
│   │   ├── context_manager.h/.cpp
│   │   └── permission_manager.h/.cpp
│   ├── harness/                     # Layer 3: Agent Harness
│   │   ├── itool.h                  # ITool 接口
│   │   ├── tool_registry.h/.cpp
│   │   └── tools/
│   │       ├── file_read_tool.h/.cpp
│   │       ├── file_write_tool.h/.cpp
│   │       ├── grep_tool.h/.cpp
│   │       └── shell_exec_tool.h/.cpp
│   ├── infrastructure/              # Layer 5: Infrastructure
│   │   ├── event_bus.h/.cpp         # RuntimeEventBus
│   │   └── logger.h/.cpp            # spdlog 封装
│   └── presentation/
│       ├── cli/
│       │   ├── main.cpp             # aictl 入口
│       │   └── cli_handler.h/.cpp   # REPL + Signal → 终端输出
│       └── gui/                     # P2 起
│           └── main_window.h/.cpp
└── tests/
    ├── harness/
    │   ├── test_file_read_tool.cpp
    │   └── test_tool_registry.cpp
    └── framework/
        └── test_agent_loop.cpp
```

---

## 十一、相关文档

- [ACT — PRD 产品需求文档](./ACT-PRD-%E4%BA%A7%E5%93%81%E9%9C%80%E6%B1%82%E6%96%87%E6%A1%A3.md)
- [ACT — 技术选型报告](./ACT-%E6%8A%80%E6%9C%AF%E9%80%89%E5%9E%8B%E6%8A%A5%E5%91%8A.md)
- [ACT — 系统架构设计](./ACT-%E7%B3%BB%E7%BB%9F%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.md)
- [ACT — 开发计划与进度](./ACT-%E5%BC%80%E5%8F%91%E8%AE%A1%E5%88%92%E4%B8%8E%E8%BF%9B%E5%BA%A6.md)
- [ACT — 术语表](./ACT-%E6%9C%AF%E8%AF%AD%E8%A1%A8.md)
- [ACT — 风险矩阵](./ACT-%E9%A3%8E%E9%99%A9%E7%9F%A9%E9%98%B5.md)

## 十二、术语表

> 完整术语表维护于 [ACT — 术语表](./ACT-%E6%9C%AF%E8%AF%AD%E8%A1%A8.md)，此处不再重复。

## 十三、风险矩阵

> 完整风险矩阵维护于 [ACT — 风险矩阵](./ACT-%E9%A3%8E%E9%99%A9%E7%9F%A9%E9%98%B5.md)，此处不再重复。

---

整理：小欧 🦊 · 2026-03-23
