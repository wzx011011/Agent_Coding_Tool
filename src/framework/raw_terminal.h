#pragma once

#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace act::framework
{

/// Keyboard key types recognized by raw terminal input.
enum class Key
{
    Up,
    Down,
    Enter,
    Escape,
    Tab,
    Unknown
};

/// A single key press event, optionally carrying a printable character.
struct KeyPress
{
    Key key = Key::Unknown;
    char32_t ch = 0;
};

/// RAII guard that switches the console to raw mode on construction
/// and restores the original mode on destruction.
///
/// Uses SetConsoleMode to disable line input, echo, and processed input;
/// reads via ReadConsoleInputW.
///
/// Usage:
///   RawTerminal raw;
///   KeyPress kp = raw.readKey();
///   // raw goes out of scope → original mode restored
class RawTerminal
{
public:
    RawTerminal();
    ~RawTerminal();

    RawTerminal(const RawTerminal &) = delete;
    RawTerminal &operator=(const RawTerminal &) = delete;

    /// Block until a key press is available and return it.
    [[nodiscard]] KeyPress readKey();

    /// Returns true if raw mode was successfully entered.
    [[nodiscard]] bool isValid() const { return m_valid; }

private:
    HANDLE m_stdinHandle = nullptr;
    DWORD m_originalMode = 0;
    bool m_valid = false;
};

} // namespace act::framework
