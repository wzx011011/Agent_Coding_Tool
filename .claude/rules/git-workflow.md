# Git Workflow Rules

## Branch Strategy

- `main` — stable, deployable branch.
- `feat/<description>` — new features.
- `fix/<description>` — bug fixes.
- `refactor/<description>` — code refactoring.

## Commit Conventions

- Use conventional commits: `feat:`, `fix:`, `refactor:`, `docs:`, `test:`, `chore:`.
- Include task ID when executing from task queue: `feat(T5): add login page UI`.
- Keep commits small and focused — one logical change per commit.
- Do not commit generated files, lock file changes without package.json changes, or secrets.

## Pull Requests

- PR title should summarize the change in one line.
- PR description should explain *why*, not just *what*.
- Do not force-push to main.

## Prohibited

- No `git push --force` to shared branches.
- No committing `.env`, credentials, or API keys.
- No large binary files without Git LFS.
