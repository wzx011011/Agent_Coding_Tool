# 从项目专属到通用模板：Claude Code `.claude/` 架构演进设计记录

> 记录 2026-03-16 从 3DPrinter_Qt6 项目专属架构到通用自主开发模板的完整探索过程。

## 起点问题

3DPrinter_Qt6 项目经过多轮优化，建立了三层 `.claude/` 架构（rules + agents + skills），有效解决了源码迁移中的上下文膨胀和流程规范问题。但这套架构是**项目专属的**——rules 里写死了 Qt6/C++ 的构建命令，skills 里硬编码了 CrealityPrint 的迁移工作流。

**核心问题：如何将这套经过验证的架构模式迁移到新项目？**

## 探索路径

### Phase 1：评估当前架构的自迭代能力

**问题：** 当前结构是"完美"的吗？

**发现：**
- 当前架构没有自动反馈闭环——`.claude/` 是静态配置，不会自我评估
- 可以建立半自动评估机制：MEMORY.md 中记录评估清单，每次迁移会话后由人工触发填写
- 可观测指标：Agent 自动委派命中率、reference 文件实际读取率、rules 违规次数

**结论：** 架构本身是好的，但缺少自我迭代的机制。通过 MEMORY.md 评估日志可以半自动地持续改进。

### Phase 2：迁移方案的两种路径

**方案 A：纯脚本（`claude-init.js`）**

```
检测项目文件 → JS 正则匹配 → 模板字符串拼接 → 写入文件
```

**方案 B：模板仓库 + Skill**

```
模板仓库（完整 .md） → Skill 读取 → LLM 分析项目 → 智能适配 → 写入文件
```

**对比决策：**

| 维度 | 纯脚本 | 模板 + Skill |
|------|--------|-------------|
| 模板维护 | 改 JS 字符串（反人类） | 直接改 .md（所见即所得） |
| 适配质量 | 正则匹配，死板 | LLM 理解上下文，灵活 |
| 运行依赖 | 只需 Node.js | 需要 Claude Code |
| 一致性 | 确定性输出 | 非确定性但更智能 |
| 边缘情况 | 检测不到就生成通用内容 | LLM 可推理和提问 |

**关键洞察：** 纯脚本的痛点不是功能不够，而是**模板嵌在 JS 字符串里**——维护体验极差。模板 + Skill 把"内容"和"逻辑"分离了。

**先实现了方案 A 作为验证：** 创建了 `claude-init.js`，支持 8 项自动检测（包管理器、框架、TS、构建工具、测试、Lint、Monorepo、任务追踪），验证了检测逻辑的正确性。随后确认方案 B 是最终方向。

### Phase 3：与 `/init` 命令的关系

**问题：** 模板 Skill 和 Claude Code 内置的 `/init` 命令如何共存？

**分析：** `/init` 只做一件事——分析项目生成 `CLAUDE.md`。它不生成 rules/agents/skills。

**决策：** 适配 Skill **吸收 `/init` 的能力**，不冲突也不需要分两步执行：

- 全新项目：Skill 直接生成完整 `CLAUDE.md`（等同 `/init`）+ 适配全部文件
- 已执行 `/init`：Skill 读取已有 `CLAUDE.md`，合并而非覆盖
- 已有自定义 `CLAUDE.md`：Skill 不覆盖，仅追加缺失段

### Phase 4：从"适配工具"到"自主开发流水线"

**转折点：** 用户提出更大的目标——不只是迁移 `.claude/` 结构，而是要建立一个**完整的自主开发流程**：

```
用户需求 → 拆分任务 → 逐步推进 → 闭环验证 → 完成需求
                                    ↑
                              可无人监督自我迭代
```

这要求架构支持三个新能力：
1. **需求拆分**（`planning-requirements` Skill）
2. **依赖感知的任务执行**（拓扑排序 + 并行 agent）
3. **自我迭代**（`/loop` 驱动）

### Phase 5：与 `/batch` 命令的关系

**问题：** 设计的执行引擎和 `/batch` 能否完美适配？

**分析：** `/batch` 已有的能力：并行 agent spawn、worktree 隔离、进度跟踪。
缺失的能力：持久化状态、任务依赖、闭环重试、自我迭代。

**决策：** 执行引擎**复用 `/batch` 的底层机制**（Agent tool + isolation + background），但**不调用 `/batch` 命令**（它会触发 Plan Mode 中断自主流程）。而是在 Skill 内部实现相同模式，加上 `/batch` 缺失的三块：

```
/batch 的能力（复用）        +    流水线补充的能力（新增）
─────────────────────             ─────────────────────────
并行 agent spawn                   持久化 task-queue.md
worktree 隔离                      拓扑排序依赖调度
进度表跟踪                         闭环 verify → fix → retry
                                   /loop 自我迭代
```

### Phase 6：模板内容设计——"完整但待裁剪"

**关键设计决策：** 模板文件不是用占位符（`{{PM}}`），而是写**完整的全框架覆盖内容**，由 LLM 在适配时智能裁剪。

```markdown
<!-- 模板原文（code-quality.md）-->
## React         ← 保留（如检测到 React）
## Vue 3         ← 保留（如检测到 Vue）
## Angular       ← 保留（如检测到 Angular）
## Svelte        ← 保留（如检测到 Svelte）
```

适配 Skill 的指令是"保留匹配的，删除其他的"，而不是"替换占位符"。这比正则替换灵活得多，能处理边缘情况。

### Phase 7：闭环执行设计

**核心原则：** 每个任务必须形成可验证的闭环。

```
READ → IMPLEMENT → VERIFY → (FIX → VERIFY) × 3 → COMMIT
```

**重试策略：**
- 最多 3 次 fix-and-retry
- 超过 3 次标记为 `[!]` blocked，跳过继续
- 与 `/loop` 配合时，`[!]` 任务留在队列中等待人工介入

**持久化设计的关键作用：** `docs/task-queue.md` 是文件而非内存状态，使得：
- `/loop` 每次新会话从文件恢复状态
- 会话 compaction 不丢失进度
- 多人会话可以共享同一个任务队列

## 最终架构

### 目录结构

```
claude-project-template/
├── CLAUDE.md                              ← {placeholders} 待适配
├── README.md
└── .claude/
    ├── rules/                              ← 自动加载（每会话）
    │   ├── build-rules.md                  ← 待适配：PM 命令、测试段
    │   ├── code-quality.md                 ← 待裁剪：全框架覆盖
    │   ├── git-workflow.md                 ← 通用
    │   └── task-execution.md               ← 通用：闭环规则
    ├── agents/                             ← 委派时加载
    │   └── task-worker.md                  ← 通用：闭环 agent
    └── skills/                             ← 用户调用时加载
        ├── adapting-project-structure/      ← 初始化适配
        ├── planning-requirements/           ← 需求拆分
        │   └── reference/queue-format.md
        ├── executing-tasks/                 ← 执行引擎
        │   └── reference/
        │       ├── closed-loop.md
        │       ├── topology.md
        │       └── stop-conditions.md
        └── reviewing-code/                  ← 代码审查
            └── reference/output-format.md
```

### 三层分离

| 层 | 职责 | 加载时机 |
|----|------|---------|
| `rules/` | 全局约束（不该做什么） | 每会话自动加载 |
| `agents/` | 运行时约束（工具、隔离、轮次） | 委派时加载 |
| `skills/` | 用户工作流（该做什么） | 调用时加载 |

### 四个 Skill 的协作

```
/planning-requirements <需求>
         ↓
   docs/task-queue.md
         ↓
/executing-tasks all  ←── /loop 5m（可选，无人监督）
         ↓
   代码变更 + PR
         ↓
/reviewing-code        ←── 可穿插在任意阶段
```

### 用户使用流程

```bash
# 1. 初始化（一次性）
cp -r claude-project-template/.claude ./my-project/
cd my-project && claude
/adapting-project-structure

# 2. 日常开发
/planning-requirements 实现用户管理模块
# 审批任务队列...
/executing-tasks all                    # 手动推进
/loop 5m /executing-tasks all          # 无人监督
```

## 设计决策汇总

| 决策点 | 选项 | 最终选择 | 理由 |
|--------|------|---------|------|
| 迁移方式 | 纯脚本 vs 模板+Skill | 模板+Skill | 模板维护体验好，LLM 适配更灵活 |
| 模板语法 | 占位符 `{{}}` vs 完整内容待裁剪 | 完整待裁剪 | LLM 理解力比正则强，边缘情况处理更好 |
| 与 `/init` 关系 | 替代 vs 共存 | 吸收替代 | 功能是超集，无冲突 |
| 与 `/batch` 关系 | 调用 vs 复用机制 | 复用底层机制 | `/batch` 会触发 Plan Mode 中断自主流程 |
| 任务持久化 | 内存 vs 文件 | 文件（task-queue.md） | 支持 `/loop` 跨会话恢复 |
| 闭环重试 | 无限重试 vs 有限重试 | 最多 3 次 | 避免死循环，blocked 任务留给人 |
| 自我迭代 | 无 vs `/loop` | `/loop` | 用户一行命令启动，队列驱动自动停止 |

## 从中提炼的方法论

1. **内容与逻辑分离** — 模板是纯内容（.md），适配逻辑由 LLM 执行，不在内容中嵌入代码逻辑
2. **渐进式信息披露** — rules 自动加载（核心约束），reference 按需加载（详细规范）
3. **持久化优于内存** — 任务队列是文件，不依赖会话上下文，天然支持跨会话和 `/loop`
4. **超集替代而非并存** — 当新能力完全覆盖旧能力时，吸收替代比并存更简洁
5. **复用机制而非调用命令** — `/batch` 的 Plan Mode 会中断自主流程，但它的底层机制（Agent + worktree + background）可以直接复用
