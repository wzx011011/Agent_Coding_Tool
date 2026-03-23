# Task Queue Format

任务队列存储在 `docs/task-queue.md`，是跨会话的持久状态。

## 模板

```markdown
# Task Queue — {feature name}

## Metadata
- Created: {date}
- Source: {original requirement text}
- Status: pending / in-progress / completed
- Completed: {n}/{total}

## Tasks

| # | ID | Title | Scope | Depends | Status | Verification | Notes |
|---|-----|-------|-------|---------|--------|-------------|-------|
| 1 | T1 | {title} | {scope} | - | [ ] | {how to verify} | |
| 2 | T2 | {title} | {scope} | T1 | [ ] | {how to verify} | |
| 3 | T3 | {title} | {scope} | T1, T2 | [ ] | {how to verify} | |
```

## Status Legend

- `[x]` completed — build pass + test pass + committed
- `[-]` in-progress — started but not yet verified
- `[!]` blocked — failed after 3 retries, needs human review
- `[ ]` pending — not started

## Scope Values

| Value | Meaning |
|-------|---------|
| `backend` | Server-side code (API, DB, services) |
| `frontend` | Client-side code (UI, components, pages) |
| `integration` | Connecting frontend and backend |
| `testing` | Unit tests, integration tests, e2e tests |
| `infra` | Configuration, CI/CD, deployment |
| `docs` | Documentation |

## Verification Examples

- `build + test pass`
- `build + test pass + typecheck`
- `build + test pass + manual: login flow works`
- `build + test pass + e2e: auth.spec.ts green`
- `lint + typecheck + test pass`

## Updating Rules

- Only mark `[x]` after verification passes AND commit succeeds
- Update Notes column with commit hash when completed
- Update Status column to `[!]` with failure reason when blocked
- Update Metadata Completed count after each status change
