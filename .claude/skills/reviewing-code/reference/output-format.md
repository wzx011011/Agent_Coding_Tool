# Review Output Format

## Review Report

```
Code Review: {scope}
Files reviewed: {n}
Date: {date}

Summary: {overall quality assessment, 1-2 sentences}

Issues Found
────────────
| # | Severity | File:Line | Issue | Suggestion |
|---|----------|-----------|-------|------------|
| 1 | error    | src/api/auth.ts:42 | Unhandled promise rejection | Wrap in try/catch |
| 2 | warn     | src/components/Form.tsx:15 | Missing error boundary | Add ErrorBoundary wrapper |

Patterns Observed
─────────────────
+ Positive: consistent naming conventions
+ Positive: proper TypeScript types on exports
- Concern: several files use console.log instead of logger
- Concern: inconsistent error handling patterns

Recommendations
───────────────
1. [Priority] Fix unhandled promise rejections in API layer
2. Replace console.log with structured logger
3. Add error boundaries for form components
```

## Severity Levels

| Level | Meaning | Action |
|-------|---------|--------|
| `error` | Must fix — bugs, security, broken functionality | Fix before merging |
| `warn` | Should fix — quality, maintainability, performance | Fix soon |
| `info` | Consider — style, convention, minor improvements | Optional |
