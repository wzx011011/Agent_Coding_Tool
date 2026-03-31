#include "framework/raw_terminal.h"

#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>

namespace act::framework
{

RawTerminal::RawTerminal()
{
    m_stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
    if (m_stdinHandle == INVALID_HANDLE_VALUE || m_stdinHandle == nullptr)
        return;

    if (!GetConsoleMode(m_stdinHandle, &m_originalMode))
        return;

    // Enable mouse input for wheel scrolling, disable line/echo/processed.
    // NOTE: Do NOT enable ENABLE_VIRTUAL_TERMINAL_INPUT — it converts arrow
    // keys into ANSI escape sequences (\x1b[A etc.) which swallows the
    // VK_UP/VK_DOWN events that we read via ReadConsoleInputW.
    DWORD newMode = m_originalMode;
    newMode |= ENABLE_MOUSE_INPUT;
    newMode &= ~static_cast<DWORD>(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);

    if (!SetConsoleMode(m_stdinHandle, newMode))
        return;

    m_valid = true;
}

RawTerminal::~RawTerminal()
{
    if (m_valid && m_stdinHandle != INVALID_HANDLE_VALUE && m_stdinHandle != nullptr)
    {
        SetConsoleMode(m_stdinHandle, m_originalMode);
    }
}

KeyPress RawTerminal::readKey()
{
    if (!m_valid)
        return {};

    INPUT_RECORD record;
    DWORD readCount = 0;

    while (true)
    {
        // Peek first — do NOT consume non-KEY_EVENT records (e.g. mouse scroll)
        if (!PeekConsoleInputW(m_stdinHandle, &record, 1, &readCount) || readCount == 0)
            return {};

        if (record.EventType != KEY_EVENT)
        {
            // Skip (consume) this non-key record so we don't loop forever
            ReadConsoleInputW(m_stdinHandle, &record, 1, &readCount);

            // For MOUSE_EVENT, also generate a synthetic key press so mouse wheel
            // can be used to navigate up/down
            if (record.EventType == MOUSE_EVENT)
            {
                const auto &me = record.Event.MouseEvent;
                if (me.dwEventFlags == MOUSE_WHEELED)
                {
                    // HIWORD of dwButtonState: positive = scroll up, negative = scroll down
                    short delta = static_cast<short>(HIWORD(me.dwButtonState));
                    if (delta > 0)
                        return {Key::Up};
                    else
                        return {Key::Down};
                }
            }

            continue;
        }

        // It's a KEY_EVENT — consume it
        ReadConsoleInputW(m_stdinHandle, &record, 1, &readCount);

        const auto &ke = record.Event.KeyEvent;
        if (!ke.bKeyDown)
            continue; // Ignore key-up events

        // Virtual key mapping
        switch (ke.wVirtualKeyCode)
        {
        case VK_UP:    return {Key::Up};
        case VK_DOWN:  return {Key::Down};
        case VK_RETURN: return {Key::Enter};
        case VK_ESCAPE: return {Key::Escape};
        case VK_TAB:   return {Key::Tab};
        default: break;
        }

        // Printable character
        if (ke.uChar.UnicodeChar != 0)
        {
            char32_t c = static_cast<char32_t>(ke.uChar.UnicodeChar);
            if (c >= 0x20) // Space and above
                return {Key::Unknown, c};
        }
    }
}

} // namespace act::framework
