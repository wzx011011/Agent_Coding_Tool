---
name: adapting-project-structure
description: "Adapting the .claude/ project template to the current project's tech stack and conventions. Triggers on first-time project setup or when the tech stack changes significantly."
---

# Project Structure Adaptation

将模板中的通用 `.claude/` 结构适配为当前项目的专用配置。替代 `/init`，功能是其超集。

## 目标

1. 检测项目技术栈
2. 裁剪模板文件（保留相关段，删除无关段）
3. 填入检测到的具体值（包管理器命令、框架约定等）
4. 生成适配后的 `CLAUDE.md`（合并已有 `/init` 产物）

## 检测项

| 项目 | 信号 |
|------|------|
| 包管理器 | `pnpm-lock.yaml` / `yarn.lock` / `package-lock.json` |
| 框架 | `package.json` dependencies（react, vue, next, nuxt, angular, svelte） |
| TypeScript | `tsconfig.json` |
| 构建工具 | `vite.config.*` / `next.config.*` / `webpack.config.*` |
| 测试框架 | `package.json` devDependencies（vitest, jest, playwright, cypress） |
| Lint 工具 | eslint config files / `biome.json` |
| Monorepo | `turbo.json` / `nx.json` / `pnpm-workspace.yaml` / `lerna.json` |

## 工作流

1. **检测** — 读取上述文件，构建项目画像
2. **适配 rules/build-rules.md**
   - 替换包管理器命令（Install/Build/Dev/Test/Lint/CI）
   - 保留检测到的测试框架段，删除其他测试段
   - 如非 monorepo，删除 Monorepo 段
3. **适配 rules/code-quality.md**
   - 保留 TypeScript 段（如检测到）
   - 保留检测到的框架段（React / Vue / Angular / Svelte / Next.js / Nuxt），删除其他框架段
   - 保留 Anti-Patterns 段
   - 保留检测到的 Lint 段，删除其他 Lint 段
4. **不动以下文件**（通用，无需适配）
   - `rules/task-execution.md`
   - `rules/git-workflow.md`
   - `agents/task-worker.md`
   - 所有 `skills/`
5. **生成 CLAUDE.md**
   - 如果已存在 `CLAUDE.md`（来自 `/init`）：读取并保留项目概述段，追加 skill 引用和 build 段
   - 如果不存在：从模板 `{placeholders}` 生成完整 `CLAUDE.md`，填入检测到的项目名和命令
6. **报告** — 输出检测结果摘要和适配清单

## 适配原则

- **保留**：与检测到的技术栈匹配的内容段
- **删除**：不匹配的框架/工具内容段
- **替换**：包管理器命令、具体工具名称
- **不动**：通用规则、agent 定义、skill 文件
