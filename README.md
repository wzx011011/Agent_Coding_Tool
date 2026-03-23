# ACT

ACT 是一个 C++20 / Qt6 原生 AI IDE，当前仓库处于 P1 CLI-first runtime 阶段。

## Prerequisites: Windows Environment Setup

| Tool | Required Version | Notes |
|------|------------------|-------|
| CMake | `>= 3.28` | 必须在 PATH 中 |
| Ninja | `>= 1.11` | 默认生成器 |
| Qt | `6.10.2` | 外部提供，不通过 vcpkg 安装 |
| MSVC | `VS 2022 17.10+` / `MSVC 19.40+` | 默认 Windows 编译器 |
| vcpkg | manifest mode | `VCPKG_ROOT` 必须指向有效 checkout |

## Bootstrap

```powershell
# 1. Set vcpkg location
$env:VCPKG_ROOT = "C:\path\to\vcpkg"

# 2. Point CMake to Qt 6.10.2
$env:CMAKE_PREFIX_PATH = "C:\path\to\Qt\6.10.2\msvc2022_64"
# or
$env:Qt6_DIR = "C:\path\to\Qt\6.10.2\msvc2022_64"

# 3. Configure
cmake --preset default

# 4. Build
cmake --build build --config RelWithDebInfo

# 5. Test
ctest --test-dir build --config RelWithDebInfo --output-on-failure
```

P1 交付物是 headless CLI runtime。CLI target 只允许链接 `QtCore`，不得引入 `QtWidgets`、`QScintilla` 或 `QTermWidget`。

## Development Workflow

### Step 1: Plan
```
/planning-requirements 实现用户登录注册功能，包含 JWT 认证和权限管理
```
分析代码库，生成 `docs/task-queue.md`。

### Step 2: Execute
```
/executing-tasks all
```
按依赖顺序执行，每个任务闭环验证。

### Step 3: Autonomous Mode
```
/loop 5m /executing-tasks all
```
每 5 分钟自动推进一轮，直到全部完成。无需人工监督。

## Architecture

```
.claude/
├── rules/                              # Auto-loaded every session
│   ├── build-rules.md                      # Build commands (adapt to PM)
│   ├── code-quality.md                     # Framework conventions (trim to match)
│   ├── git-workflow.md                     # Git conventions (generic)
│   └── task-execution.md                   # Closed-loop rules (generic)
├── agents/
│   └── task-worker.md                      # Isolated task executor (generic)
└── skills/
    ├── adapting-project-structure/            # Init: detect & adapt
    ├── planning-requirements/                 # Decompose requirements
    │   └── reference/queue-format.md
    ├── executing-tasks/                       # Execute with closed-loop
    │   └── reference/
    │       ├── closed-loop.md
    │       ├── topology.md
    │       └── stop-conditions.md
    └── reviewing-code/                        # Read-only code analysis
        └── reference/output-format.md
```

## Three-Layer Design

| Layer | Purpose | When Loaded |
|-------|---------|-------------|
| `rules/` | Global constraints (what NOT to do) | Every session (auto) |
| `agents/` | Runtime constraints (tools, isolation, turns) | When delegated |
| `skills/` | User workflows (what to do) | When invoked |

## Key Concepts

- **Persistent State**: `docs/task-queue.md` survives sessions and compaction
- **Closed Loop**: Every task must pass build + test before marked complete
- **Dependency-Aware**: Topology-sorted scheduling, parallel where possible
- **Self-Iterating**: `/loop` integration for autonomous execution
- **Progressive Disclosure**: Reference files loaded on demand, not every session

## Adaptation

`/adapting-project-structure` auto-detects:
- Package manager (npm / pnpm / yarn)
- Framework (React / Vue / Angular / Svelte / Next / Nuxt)
- TypeScript, build tool, testing, linting, monorepo

And adapts template files accordingly (keeps relevant sections, removes others).
