# ACT — 技术选型报告

C++/Qt6 原生 AI IDE · 2026-03-23

> 本文档是决策记录，写完即归档。技术选型变更需说明理由。

技术路线总判断：ACT 的对外交付形态是 AI IDE，但技术内核是 coding agent runtime。因此选型优先级应围绕 runtime 的稳定执行、上下文质量、Tool 调用和跨表面复用，而不是单独优化某个 GUI 外壳。能力基准对标 Claude Code CLI，IDE 形态基准参考 TRAE 与 VS Code。

---

## 一、编程语言：C++20

| 项目           | 语言       | 优势                      | 劣势                    |
| -------------- | ---------- | ------------------------- | ----------------------- |
| **我们的方案** | **C++20**  | 原生性能、零 GC、内存可控 | 开发速度慢于脚本语言    |
| Claude Code    | TypeScript | 生态丰富、开发快          | Electron 内存大、速度慢 |
| Cursor         | TypeScript | 同上                      | 同上                    |
| Zed            | Rust       | 性能好、内存安全          | 学习曲线陡、Qt 生态缺失 |
| Aider          | Python     | 开发最快                  | 性能差、不能做 GUI IDE  |

**决策理由**：性能是核心卖点，C++ 是唯一选择。我们精通 C++ 是最大的可实施性保障。

---

## 二、GUI 框架：Qt6 QWidgets（非 QML）

| 维度           | QWidgets                     | QML                              |
| -------------- | ---------------------------- | -------------------------------- |
| 代码编辑器集成 | QScintilla 原生嵌入          | 需 QQuickWidget 桥接，有性能损耗 |
| 停靠分栏       | QDockWidget + QSplitter 原生 | 需手写或第三方组件               |
| 文本渲染       | QPainter 原生，无中间层      | 通过 Scene Graph，有 JS 开销     |
| 成熟 IDE 先例  | Qt Creator 就是纯 QWidgets   | 无主流 IDE 使用                  |
| 我们的经验     | 精通                         | 有了解但不精通                   |

**决策理由**：QScintilla + QWidgets 是 IDE 场景最成熟的组合，Qt Creator 本身就是最佳证明。

补充：我们选择 QWidgets，不是为了复刻传统 C++ IDE，而是为了在原生栈里承载接近 TRAE 与 VS Code 的高密度 IDE 信息架构，包括活动栏、侧边栏、右侧 Agent 面板、底部终端与输出区的稳定编排。

---

## 三、代码编辑器：QScintilla

| 项目           | 编辑器内核       | 语法高亮      | 代码折叠 | 性能         |
| -------------- | ---------------- | ------------- | -------- | ------------ |
| **我们的方案** | **QScintilla**   | **100+ 语言** | **原生** | **原生级**   |
| VS Code        | Monaco           | ✅            | ✅       | 好（吃内存） |
| Zed            | 自研 tree-sitter | ✅            | ✅       | 最好         |
| Qt Creator     | QScintilla       | ✅            | ✅       | 好           |

**决策理由**：QWidgets 生态最成熟的代码编辑器组件，直接嵌入不造轮子。

---

## 四、HTTP 客户端：cpp-httplib

| 项目           | HTTP 库            | 说明                        |
| -------------- | ------------------ | --------------------------- |
| **我们的方案** | **cpp-httplib**    | **Header-only，零外部依赖** |
| Claude Code    | Node.js 内置 fetch | Node.js 内置                |
| Aider          | httpx（Python）    | Python 生态                 |

**为什么不用 libcurl**：cpp-httplib 更轻量（单 header），只需 HTTP/SSE 的场景足够。

---

## 五、JSON 解析：nlohmann/json

C++ 生态事实标准 JSON 库，现代 API，Header-only。

---

## 六、构建系统：CMake + vcpkg

| 项目           | 构建系统         | 包管理       |
| -------------- | ---------------- | ------------ |
| **我们的方案** | **CMake**        | **vcpkg**    |
| Zed            | Cargo            | crates.io    |
| VS Code        | Electron Builder | npm          |
| Qt Creator     | CMake + Qbs      | 系统包管理器 |

**决策理由**：vcpkg 微软出品，Windows 体验最好，跨平台三端支持。QScintilla、libgit2、cmark 等一行命令安装。

---

## 七、LLM API + Agent Loop：自研

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

补充：对 ACT 来说，自研并不是为了“重复造编辑器轮子”，而是为了掌握 runtime 核心链路。如果 Framework、Harness、AIEngine 不能自洽，Qt GUI 再完整也只是一个没有护城河的壳。

能力对齐上，Claude Code CLI 是 runtime 行为基准，这意味着自研重点必须落在 AgentLoop、Tool Runtime、Permission、Context、Diff 审核和错误恢复，而不是优先追求 GUI 表层功能堆叠。

**Agent Loop 工作量（按架构层分解）**：

| 架构层    | 模块                                        | 预估行数     |
| --------- | ------------------------------------------- | ------------ |
| Framework | AgentLoop 核心循环                          | ~100         |
|           | AgentScheduler 调度器                       | ~300         |
|           | ContextManager 上下文管理                   | ~300         |
|           | PermissionManager 权限管理                  | ~200         |
| Harness   | ITool 接口 + ToolRegistry                   | ~300         |
|           | FileReadTool + FileWriteTool + FileEditTool | ~400         |
|           | ShellExecTool                               | ~200         |
|           | GlobTool + GrepTool                         | ~200         |
| Core      | AIEngine + ILLMProvider 抽象                | ~400         |
|           | AnthropicProvider（SSE 流式）               | ~300         |
|           | 错误处理 + 日志                             | ~150         |
| **合计**  |                                             | **~2850 行** |

---

## 八、其他依赖

| 组件          | 选型             | 用途                | 对标                | 架构层         |
| ------------- | ---------------- | ------------------- | ------------------- | -------------- |
| 终端组件      | QTermWidget      | 底部终端面板        | VS Code 内置终端    | Presentation   |
| Markdown 渲染 | cmark / hoedown  | AI Chat 面板        | Claude Code Desktop | Presentation   |
| Git 集成      | libgit2          | gitStatus / gitDiff | Aider 用 subprocess | Harness (Tool) |
| Diff 解析     | diff-match-patch | Agent Diff 预览     | Cline 自研          | Harness (Tool) |
| AST 解析      | tree-sitter      | Repo Map            | Aider 同款          | Core           |
| CLI 交互      | GNU readline     | REPL 补全/历史      | Claude Code 同款    | Presentation   |
| LSP 客户端    | 自研（P4）       | clangd/pyright      | VS Code 同款        | Harness (Tool) |

### 关键依赖版本策略

- **Qt**：固定 Qt 6.x LTS 小版本，避免 Widgets、网络层和打包行为跨版本漂移
- **CMake**：明确最低版本，并与 CI 使用同一版本范围
- **vcpkg**：提交 `vcpkg.json` 和 baseline，锁定三平台依赖解析结果
- **QScintilla / tree-sitter / libgit2 / QTermWidget**：在文档和构建配置中写明已验证版本，避免“本机可用、CI 失败”

### 技术风险与备选方案

| 风险点                 | 当前选择          | 风险说明                                                | 备选方案                                       |
| ---------------------- | ----------------- | ------------------------------------------------------- | ---------------------------------------------- |
| Ghost Text / Diff 装饰 | QScintilla        | 需要验证对内联提示、复杂标记和并排 Diff 的支持深度      | 若能力不足，补充自绘层或替换部分编辑器交互实现 |
| LSP 客户端             | 自研（P4）        | LSP 协议复杂，完全自研成本高、联调周期长                | P4 优先封装成熟库，再按需要逐步替换            |
| SSE / 多 Provider      | cpp-httplib       | 需验证在弱网和长连接场景下的稳定性                      | 若稳定性不足，补充更成熟的 HTTP/SSE 方案       |
| Markdown 渲染          | cmark / hoedown   | 两套方案并列会增加维护分支                              | 尽快收敛为单一实现                             |
| GUI 信息架构           | QWidgets 自研布局 | 需在原生技术栈中实现接近 TRAE 与 VS Code 的高密度工作流 | 先固化面板组织和交互约束，再逐步优化视觉细节   |

### 尚需补充的工程能力

当前技术选型能够支撑 ACT 的 MVP 与第一代可用版本，但距离稳定对齐 Claude Code CLI 级别的 runtime 行为，还需要补足几类并非单一库即可解决的工程能力：

- **任务状态与恢复层**：需要为 TaskState、Checkpoint、Resume、Replay 设计独立的数据结构与存储策略，这不是 QWidgets、cpp-httplib 或 tree-sitter 能自动提供的能力
- **补丁事务层**：`diff-match-patch` 可用于展示 Diff，但多文件补丁的原子提交、预演、回滚和冲突处理仍需自研 PatchTransaction 机制
- **运行时观测层**：仅有日志不足以支撑 Agent 调优，还需要 Task Trace、Tool Span、Permission Audit、Provider 行为摘要与失败分类
- **评测体系**：需要独立维护标准任务集、回归任务集和人工验收标准，否则“核心能力对齐 Claude Code CLI”无法被验证
- **执行安全与隔离**：ShellExec、WebFetch、未来 ACP/MCP 调用都需要更细粒度的沙箱、环境变量隔离与策略控制，避免 runtime 能力增强后安全边界反而变弱

这些能力不要求在 P1 一次做完，但必须在 P1-P3 阶段逐步前置定义，否则架构虽然成立，runtime 的稳定性和可维护性会在后期迅速失控。

---

## 九、技术栈完整清单

| 架构层      | 技术             | 说明                                                            |
| ----------- | ---------------- | --------------------------------------------------------------- |
| 语言        | C++20            | 核心优势，性能+精通                                             |
| Framework   | 自研             | AgentScheduler + AgentLoop + ContextManager + PermissionManager |
| Harness     | 自研             | ITool 接口 + ToolRegistry + 16 个内置 Tool                      |
| Core        | 自研             | AIEngine + ProjectManager + CodeAnalyzer + ConfigManager        |
| GUI 框架    | Qt6 QWidgets     | 非 QML                                                          |
| 代码编辑器  | QScintilla       | 语法高亮/折叠                                                   |
| HTTP 客户端 | cpp-httplib      | Header-only                                                     |
| JSON 解析   | nlohmann/json    | 现代 C++ JSON 库                                                |
| 构建系统    | CMake + vcpkg    | 跨平台包管理                                                    |
| AST 解析    | tree-sitter      | Repo Map                                                        |
| LSP 客户端  | 自研（P4）       | clangd/pyright                                                  |
| 终端        | QTermWidget      | 底部终端                                                        |
| Markdown    | cmark / hoedown  | Chat Panel                                                      |
| Git         | libgit2          | gitStatus/gitDiff                                               |
| Diff        | diff-match-patch | Diff 预览                                                       |
| CLI         | GNU readline     | REPL                                                            |

---

## 十、相关文档

- [ACT — PRD 产品需求文档](./ACT-PRD-%E4%BA%A7%E5%93%81%E9%9C%80%E6%B1%82%E6%96%87%E6%A1%A3.md)
- [ACT — 技术选型报告](./ACT-%E6%8A%80%E6%9C%AF%E9%80%89%E5%9E%8B%E6%8A%A5%E5%91%8A.md)
- [ACT — 系统架构设计](./ACT-%E7%B3%BB%E7%BB%9F%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.md)
- [ACT — 开发计划与进度](./ACT-%E5%BC%80%E5%8F%91%E8%AE%A1%E5%88%92%E4%B8%8E%E8%BF%9B%E5%BA%A6.md)

## 十一、术语表

| 术语               | 定义                                                   |
| ------------------ | ------------------------------------------------------ |
| ACT                | AI Coding Tool，本文档描述的 C++/Qt6 原生 AI IDE 项目  |
| Presentation Layer | 表现层，包含 CLI、Qt GUI、VS Code Extension 等交互入口 |
| Agent Framework    | 决定任务如何拆解、推进和确认的框架层                   |
| Agent Harness      | 承载 Tool 注册、执行和权限分级的执行层                 |
| Core Services      | 提供模型调用、项目状态、代码分析、配置管理的核心服务层 |
| Infrastructure     | 文件系统、网络、进程、终端等基础设施抽象层             |
| AgentLoop          | 单任务 Agent 循环，负责在回复与 Tool Call 之间做决策   |
| AgentScheduler     | 多任务调度器，负责串行流水线和并行 worker 编排         |
| ITool              | Framework 与 Harness 之间的标准 Tool 接口              |
| Provider           | 大模型服务提供方，例如 OpenAI、Claude、GLM             |
| Repo Map           | 面向仓库结构理解的代码摘要与上下文索引                 |
| Diff 预览          | Agent 落地修改前向用户展示的变更对比视图               |
| Fallback           | 主模型失败后切换到备用模型的降级机制                   |

## 十二、风险矩阵

| 风险ID | 风险描述                                               | 概率 | 影响 | 应对策略                                                      |
| ------ | ------------------------------------------------------ | ---- | ---- | ------------------------------------------------------------- |
| R1     | QScintilla 对 Ghost Text、复杂标记和 Diff 装饰支持不足 | 中   | 高   | 在 P2 前完成编辑器能力验证，不足时补自绘层或调整交互方案      |
| R2     | 自研 LSP Client 成本过高，导致 P4 范围失控             | 中   | 高   | P4 优先封装成熟库，保留渐进替换空间                           |
| R3     | cpp-httplib 在弱网、SSE、长连接下稳定性不足            | 中   | 中   | 提前做 Provider 联调和断网压测，必要时替换 HTTP/SSE 方案      |
| R4     | 权限确认、危险命令拦截和只读模式定义不完整             | 中   | 高   | 在 Framework/Harness 设计阶段固化权限等级、确认流程和拒绝路径 |
| R5     | 三平台依赖版本漂移，导致本地与 CI 结果不一致           | 高   | 中   | 锁定 Qt、CMake、vcpkg baseline 及关键三方库版本               |

---

整理：小欧 🦊 · 2026-03-23
