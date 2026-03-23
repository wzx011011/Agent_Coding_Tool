# ACT — 术语表

Runtime-first AI Coding Tool · 2026-03-23

> 本文件是项目统一术语表，各文档引用此处定义。术语变更请在此文件修改，不要在各文档中分别维护。

| 术语               | 定义                                                                 |
| ------------------ | -------------------------------------------------------------------- |
| ACT                | AI Coding Tool，本项目的 runtime-first AI Coding 运行时与多表面产品  |
| Presentation Layer | 表现层，包含 CLI、VS Code Extension、Native GUI 等交互入口           |
| Agent Framework    | 决定任务如何拆解、推进和确认的框架层（Layer 2）                      |
| Agent Harness      | 承载 Tool 注册、执行和权限分级的执行层（Layer 3）                    |
| Core Services      | 提供模型调用、项目状态、代码分析、配置管理的核心服务层（Layer 4）    |
| Infrastructure     | 文件系统、网络、进程、终端等基础设施抽象层（Layer 5）                |
| AgentLoop          | 单任务 Agent 循环，负责在回复与 Tool Call 之间做决策                 |
| AgentScheduler     | 多任务调度器，负责串行流水线和并行 worker 编排                       |
| SubagentManager    | 子智能体管理器，负责独立上下文、工具边界和结果摘要回传               |
| ITool              | Framework 与 Harness 之间的标准 Tool 接口                            |
| IService           | Core Services 层向 Framework/Harness 暴露的标准服务接口族            |
| IInfrastructure    | Infrastructure 层向上层暴露的标准基础设施接口族                      |
| RuntimeEventBus    | 运行时事件实时分发总线，Layer 2-4 发布、Presentation 订阅            |
| RuntimeEventLogger | 运行时事件持久化记录器，与 EventBus 共享事件类型                     |
| SkillCatalog       | 技能目录索引，向 system prompt 暴露技能摘要与标签                    |
| SkillLoader        | 按需读取并注入完整 SKILL 正文的机制，正文通过 tool_result 进入上下文 |
| ContextCompactor   | 上下文压缩器，负责 Micro / Auto / Manual Compact                     |
| FileLockManager    | Harness 层文件并发访问协调器，提供共享锁和排他锁                     |
| CacheManager       | 高开销计算结果的缓存管理器，基于时间戳+hash 失效                     |
| PatchTransaction   | 多文件补丁事务模型，支持预览、确认、提交和回滚                       |
| Provider           | 大模型服务提供方，例如 OpenAI、Claude、GLM                           |
| Repo Map           | 面向仓库结构理解的代码摘要与上下文索引                               |
| Diff 预览          | Agent 落地修改前向用户展示的变更对比视图                             |
| Fallback           | 主模型失败后切换到备用模型的降级机制                                 |
| TaskState          | 单任务运行状态模型（Running / WaitingApproval / Cancelled 等）       |
| Checkpoint         | 长任务执行快照，用于取消/失败后恢复                                  |
| Task Graph         | 带依赖关系的任务状态模型，记录 blockedBy、owner、artifacts 等        |
| Execution Lane     | 任务执行通道模型，描述 foreground / background / worktree 等运行空间 |

---

## 相关文档

- [ACT — PRD 产品需求文档](./ACT-PRD-%E4%BA%A7%E5%93%81%E9%9C%80%E6%B1%82%E6%96%87%E6%A1%A3.md)
- [ACT — 技术选型报告](./ACT-%E6%8A%80%E6%9C%AF%E9%80%89%E5%9E%8B%E6%8A%A5%E5%91%8A.md)
- [ACT — 系统架构设计](./ACT-%E7%B3%BB%E7%BB%9F%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.md)
- [ACT — 开发计划与进度](./ACT-%E5%BC%80%E5%8F%91%E8%AE%A1%E5%88%92%E4%B8%8E%E8%BF%9B%E5%BA%A6.md)
- [ACT — 风险矩阵](./ACT-%E9%A3%8E%E9%99%A9%E7%9F%A9%E9%98%B5.md)

---

整理：小欧 · 2026-03-23
