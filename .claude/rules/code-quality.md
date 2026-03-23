# Code Quality Rules

## C++20

- Use C++20 features: `std::format`, structured bindings, concepts, `<ranges>`, `std::span`.
- Prefer `constexpr` / `consteval` where possible.
- Use `std::string_view` for non-owning string parameters.
- Use `std::unique_ptr` / `std::shared_ptr` — no raw `new`/`delete` in application code.
- Prefer `enum class` over plain `enum`.
- Use `[[nodiscard]]` on functions whose return value must not be ignored.
- Mark single-argument constructors `explicit`.

## Qt6

- Widgets: PascalCase class names (e.g., `EditorPanel`, `ChatWidget`).
- Signals/Slots: camelCase with verb prefix (e.g., `streamTokenReceived`, `onTaskCompleted`).
- Use `Q_OBJECT` macro only in classes that need signals/slots.
- Prefer Qt signals/slots + `QThread` for async; avoid mixing with `std::thread`.
- Use `QString` at API boundaries; use `std::string_view` for internal string processing.
- Memory: parent-child ownership where possible; `std::unique_ptr` otherwise.

### Qt 6.10 Pitfalls

- **No implicit `const char*` to `QString` via `QJsonValueRef`**: Always use `QStringLiteral("key")` for `QJsonObject::operator[]` and `QJsonObject::contains()`. The bare `"key"` will fail to compile.
- **`Q_OBJECT` on pure virtual interfaces**: Causes LNK2001 if the interface has no signals/slots (moc can't generate symbols). Use `std::function` callbacks instead of QObject inheritance for pure interfaces.
- **`CMAKE_AUTOMOC ON`**: Required in root CMakeLists.txt for any class using `Q_OBJECT`. Without it, moc won't run and signals/slots won't work.
- **`QEventLoop` without `QCoreApplication`**: Will hang. Tests using code with `QEventLoop` need either a `QCoreApplication` fixture or test only pre-event-loop logic. Use `--timeout` on ctest as safety net.
- **GTest `operator<<` can't stream `QString`**: Use `.toStdString()` in GTest assertions: `ASSERT_TRUE(x) << qstr.toStdString();`

## Layer Boundaries

- P1 CLI is runtime-first, not GUI-first. Keep `QtWidgets`-based code out of CLI deliverables.
- Shared runtime interfaces must not expose `QWidget` or other GUI-only types.
- `QScintilla`, `QDockWidget`, `QMainWindow`, and `QTermWidget` stay in presentation-layer code only.
- CLI may use `QCoreApplication`, signals/slots, and other `QtCore` facilities for event-loop bridging.
- Prefer separating reusable runtime logic from surface adapters so CLI, GUI, and future VS Code integration can share the same core behavior.

## Anti-Patterns

- No `qDebug()` in production code — use spdlog.
- No commented-out code blocks — delete them.
- No magic numbers — extract to named constants or `constexpr`.
- No deeply nested conditionals — use early returns or extract to functions.
- No `using namespace std;` in headers.
- No raw pointer ownership — use smart pointers or Qt parent-child.

## Linting

### clang-tidy

- Run `clang-tidy -p build` before committing. Fix all errors.
- Project `.clang-tidy` config applies; do not override with inline `NOLINT` without a comment explaining why.

### clang-format

- Run `clang-format -i` on changed files before committing.
- Do not manually format code that clang-format handles.
