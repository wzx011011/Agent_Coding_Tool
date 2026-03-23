---
name: reviewing-code
description: "Reviewing code quality, patterns, and potential issues without making changes. Triggers when the user asks for a code review, quality analysis, or architectural assessment."
---

# Code Review

对代码进行只读质量分析，不修改任何文件。

## 参数

- `$ARGUMENTS` 指定审查范围：文件路径、目录、组件名、或 `all`
- 为空时对最近变更的文件进行审查（`git diff` / `git log`）

## 工作流

1. 确定审查范围
2. 阅读相关源码和规则文件
3. 对照 `.claude/rules/code-quality.md` 检查合规性
4. 如有任务队列，检查任务闭环质量（commit 对应、验证通过）
5. 识别问题并按严重性分类
6. 输出审查报告

## Review Checklist

- [ ] 类型安全（TypeScript strict / 正确类型）
- [ ] 命名规范（组件 PascalCase、工具 camelCase）
- [ ] Lint 规则合规
- [ ] 无 magic numbers / 无 console.log / 无注释代码
- [ ] 错误处理完整（无 unhandled rejection）
- [ ] 性能（不必要的重渲染、内存泄漏风险）
- [ ] 安全（XSS、注入、敏感数据泄露）

## 输出

格式参照 `reference/output-format.md`。
