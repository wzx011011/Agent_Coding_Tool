---
name: planning-requirements
description: "Breaking down user requirements into a structured, executable task queue with dependencies. Triggers when the user describes a feature or requirement that needs to be decomposed into implementation tasks."
---

# Requirement Planning

将用户需求拆分为结构化、可执行的任务队列。

## 参数

- `$ARGUMENTS`: 用户需求的自然语言描述（必需）

## 工作流

1. **理解需求**
   - 解析用户需求，明确功能范围和验收标准
   - 如果需求模糊，先在代码库中探索现有相关实现

2. **探索代码库**
   - 识别需求涉及的模块、文件、组件
   - 了解现有架构和约定（参照 `.claude/rules/code-quality.md`）
   - 确认技术可行性

3. **拆分任务**
   - 将需求分解为 5-20 个独立、可验证的任务单元
   - 每个任务必须满足：
     - **独立可验证**: build + test 能通过
     - **边界清晰**: 明确的输入/输出/修改范围
     - **粒度适中**: 不要太大（一整天做不完），不要太小（改一行代码）
   - 标注任务间依赖关系

4. **确定任务属性**
   - **Scope**: backend / frontend / integration / testing / infra / docs
   - **Depends**: 依赖的任务 ID 列表
   - **Verification**: 如何验证此任务完成

5. **生成任务队列**
   - 写入 `docs/task-queue.md`
   - 格式参照 `reference/queue-format.md`

6. **呈现给用户审批**
   - 展示任务队列总览（任务数、依赖关系、预估轮次）
   - 等待用户确认或修改

## 约束

- 每个任务必须有明确的验收标准
- 任务之间不要有隐式依赖 — 所有依赖必须在 Depends 列表中声明
- 不要创建无法独立验证的任务 — 如果需要，拆分或合并
- 合理任务数量：太少（< 3）说明粒度太粗，太多（> 25）说明过度拆分

## 输出

任务队列写入 `docs/task-queue.md`，展示给用户审批。
用户确认后，使用 `/executing-tasks` 开始执行。
