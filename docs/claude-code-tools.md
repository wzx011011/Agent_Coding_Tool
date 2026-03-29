# Claude Code 内置工具完整清单

> 整理日期: 2026-03-28
> 来源: Claude Code 官方文档 (code.claude.com/docs)

## Agent 工具（26 个核心 + 3 个 MCP）

| 工具 | 说明 |
|:-----|:-----|
| **Agent** | 生成子 agent 处理复杂任务 |
| **AskUserQuestion** | 向用户提问（多选） |
| **Bash** | 执行 shell 命令 |
| **CronCreate / CronDelete / CronList** | 会话内定时任务 |
| **Edit** | 文件精确字符串替换 |
| **EnterPlanMode / ExitPlanMode** | 计划模式 |
| **EnterWorktree / ExitWorktree** | Git worktree 隔离 |
| **Glob** | 文件模式搜索 |
| **Grep** | 内容正则搜索（ripgrep） |
| **LSP** | 语言服务器代码智能 |
| **NotebookEdit** | 编辑 Jupyter notebook |
| **Read** | 读取文件/图片/PDF |
| **Skill** | 执行已注册的 skill |
| **TaskCreate / TaskGet / TaskList / TaskUpdate** | 任务管理 |
| **TaskOutput / TaskStop** | 任务输出与停止 |
| **TodoWrite** | 会话待办清单 |
| **ToolSearch** | 搜索延迟加载的工具 |
| **WebFetch** | 获取 URL 内容 |
| **WebSearch** | 网络搜索 |
| **Write** | 创建/覆盖文件 |
| **ListMcpResourcesTool / ReadMcpResourceTool** | MCP 资源访问 |

## 斜杠命令（~60 个）

### 会话管理

`/clear` `/compact` `/context` `/cost` `/copy` `/export` `/rename` `/resume` `/rewind` `/branch` `/exit`

### 配置

`/config` `/model` `/effort` `/permissions` `/sandbox` `/vim` `/theme` `/color` `/fast` `/memory` `/keybindings` `/terminal-setup` `/statusline`

### 集成

`/mcp` `/ide` `/init` `/login` `/logout` `/desktop` `/hooks` `/plugin` `/install-github-app` `/install-slack-app` `/remote-control`

### 开发

`/diff` `/plan` `/pr-comments` `/security-review` `/schedule` `/stats` `/insights` `/tasks` `/doctor` `/help` `/release-notes`

### 其他

`/btw` `/feedback` `/voice` `/mobile` `/passes` `/upgrade` `/extra-usage` `/usage` `/privacy-settings`

## 内置 Skills（5 个）

| Skill | 说明 |
|:------|:-----|
| `/batch` | 并行大规模变更 |
| `/claude-api` | API 参考文档 |
| `/debug` | 调试诊断 |
| `/loop` | 定时重复执行 |
| `/simplify` | 代码审查修复 |

## 常用快捷键

| 快捷键 | 功能 |
|:-------|:-----|
| `Ctrl+C` | 取消当前操作 |
| `Ctrl+D` | 退出 |
| `Ctrl+L` | 清屏 |
| `Ctrl+O` | 详细输出 |
| `Ctrl+R` | 搜索历史 |
| `Ctrl+V` | 粘贴图片 |
| `Ctrl+F` | 终止 agent |
| `Ctrl+B` | 后台任务 |
| `Ctrl+T` | 任务列表 |
| `Shift+Tab` | 切换权限 |
| `Alt+P` | 切换模型 |
| `Alt+T` | 扩展思考 |
| `\+Enter` | 换行 |
| `!` | Bash 模式 |
| `@` | 文件路径补全 |
