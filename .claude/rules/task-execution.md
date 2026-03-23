# Task Execution Rules

## Closed Loop

Every task execution must form a closed loop:

1. **Read** — Understand the task, read affected source files, identify acceptance criteria.
2. **Implement** — Make the minimal change that satisfies the task.
3. **Verify** — Run build and tests.
4. **Fix** — If verification fails, diagnose root cause and fix (max 3 retries).
5. **Commit** — Only after verification passes. Use conventional commit with task ID.

A task is NOT complete until build passes and tests pass.

## Retry Policy

- If build or tests fail, analyze the error and fix.
- Maximum 3 fix-and-retry cycles per task.
- After 3 failed retries, mark the task as `[!]` (blocked) and proceed to the next task.
- Blocked tasks require human review.

## Task Boundaries

- Each task must be independently verifiable (build + test).
- If a task cannot be verified independently, it should be split.
- Tasks should not have hidden dependencies — all dependencies must be declared in the task queue.

## Verification Requirements

- **Build**: Must pass without errors.
- **Tests**: Must not introduce new failures. Existing failures are acceptable if the task doesn't touch the related code.
- **Lint**: Should pass. Non-critical lint warnings are acceptable.
