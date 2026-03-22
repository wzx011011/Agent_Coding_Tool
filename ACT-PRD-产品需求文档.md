# ACT — PRD 产品需求文档

C++/Qt6 原生 AI IDE · 2026-03-21

---

## 一、项目概述

- **项目名称**：AI Coding Tool（简称 ACT）
- **一句话定位**：Cursor 的 AI 能力 + Sublime Text 的速度 + VS Code 的生态，全部用 C++/Qt 原生实现。
- **核心卖点**：轻量、原生、快。

---

## 二、现状与问题

当前主流 AI Coding 工具（Cursor、Windsurf、Claude Code）全部基于 Web 技术栈（Electron/TypeScript），带来了不可忽视的问题：

| 痛点 | 具体表现 | 影响 |
|------|---------|------|
| 内存占用高 | Cursor 空载 ~500MB，打开大项目后 1-2GB | 低配机器卡顿 |
| 启动慢 | Electron 冷启动 2-5 秒 | 丧失原生即开即用感 |
| 文件操作绕远路 | Web 技术无法直接调用系统 API，需 IPC 桥接 | 大文件/终端 IO 有延迟 |
| 资源焦虑 | IDE + 浏览器 + 终端 + AI 工具，Electron 再吃几个 G | 开发体验差 |

---

## 三、我们要做什么

### 3.1 产品定义

一个原生 C++/Qt 的轻量 AI IDE，具备以下核心能力：

1. **AI 代码补全** — 编辑器内联 Ghost Text（类似 GitHub Copilot）
2. **AI 对话** — 右侧 Chat Panel，支持多模型切换（OpenAI / Claude / GLM）
3. **Agent 模式** — AI 自主执行任务（读写文件、运行命令、Git 操作），需用户确认。采用 Agent Framework + Harness 分层架构，Tool 能力可插拔扩展
4. **代码理解** — Repo Map、AST 分析、智能上下文裁剪
5. **Diff 预览** — Agent 修改前展示 Diff，用户确认/拒绝

### 3.2 目标用户

- 对 Electron 内存占用不满的开发者
- 追求启动速度和原生体验的 C++ 开发者
- 需要本地运行、低资源占用的场景（嵌入式开发、远程开发）
- 偏好 CLI 工作流，但偶尔需要 GUI 的开发者

### 3.3 成功标准

| 指标 | 目标 |
|------|------|
| 空载内存 | < 100MB（Cursor 的 1/5） |
| 冷启动时间 | < 500ms（Cursor 的 1/10） |
| 基础补全延迟 | < 2s（首 token） |
| 支持模型 | OpenAI / Claude / GLM 至少 3 家 |
| Tool 扩展 | 支持通过 ITool 接口自定义扩展 |

---

## 四、为什么是现在

1. **2024-2025 AI Coding 工具爆发**，但全部扎堆 Web 栈，原生赛道无人
2. **LLM API 已标准化**（OpenAI/Anthropic/Google 协议趋同），底层调用不再困难
3. **开发者疲劳**：对"又一个 Electron 应用"产生疲劳，轻量原生工具有差异化空间

---

## 五、参考项目

| 项目 | Stars | 语言 | GUI | 借鉴点 |
|------|-------|------|-----|--------|
| Claude Code | - | TypeScript | Electron | CLI 优先架构、Agent Loop、Tool 系统、Framework/Harness 分离 |
| OpenClaw | - | TypeScript | 多端 | Agent 编排、ACP 外部 Agent 驱动、权限管理 |
| Cline | 47k | TypeScript | VS Code | Agent Loop + Permission + Diff（开源） |
| Aider | 40k | Python | CLI | Repo Map、tree-sitter |
| Zed | 51k | Rust | 自研 | 原生性能标杆 |
| Lapce | 42k | Rust | Floem | 插件系统 |
| Qt Creator | - | C++ | QWidgets | IDE 布局、LSP 集成 |

---

## 六、相关文档

- 技术选型报告：[ACT — 技术选型报告](https://feishu.cn/docx/Pq5ldy0fOoya9Fx6tslc5ChVnPc)
- 系统架构设计：[ACT — 系统架构设计](https://feishu.cn/docx/SUB2di5RJoT89BxwV2ucfPYqnnd)
- 开发计划与进度：[ACT — 开发计划与进度](https://feishu.cn/docx/YmOidD7nMoUVDdxwcmfcTHGFnme)
- 前期调研报告：https://feishu.cn/docx/JO5mdZRupovzPIxLzhHcqpOtnbh
- 项目进度表：https://gcnaf7ct1kpv.feishu.cn/base/WDPPbs2mKakUpzsndzmcr1Qvncf

---

整理：小欧 🦊 · 2026-03-21
