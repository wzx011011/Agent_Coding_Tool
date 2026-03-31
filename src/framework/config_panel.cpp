#include "framework/config_panel.h"
#include "framework/raw_terminal.h"
#include "framework/terminal_style.h"
#include "services/config_manager.h"

#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>

namespace act::framework
{

// ---------------------------------------------------------------------------
// Terminal helpers
// ---------------------------------------------------------------------------

namespace
{

// Layout constants
constexpr int PANEL_WIDTH = 50;
constexpr int LABEL_WIDTH = 20;

/// Ensure VTP (Virtual Terminal Processing) is enabled on stdout.
/// This is required for ANSI escape sequences (colors, cursor movement, clear).
void ensureVtpEnabled()
{
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE || hStdOut == nullptr)
        return;

    DWORD mode = 0;
    if (!GetConsoleMode(hStdOut, &mode))
        return;

    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        return;

    SetConsoleMode(hStdOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

/// Switch to alternate screen buffer (like vim/less do).
/// Gives a clean canvas; original terminal content is preserved.
const QString ENTER_ALT_BUF = QStringLiteral("\x1b[?1049h");

/// Leave alternate screen buffer, restoring original terminal content.
const QString LEAVE_ALT_BUF = QStringLiteral("\x1b[?1049l");

/// Move cursor to home position (top-left).
const QString CURSOR_HOME = QStringLiteral("\x1b[H");

/// Reset all text attributes to default.
const QString RESET_ATTRS = QStringLiteral("\x1b[0m");

/// Show cursor.
const QString SHOW_CURSOR = QStringLiteral("\x1b[?25h");

/// Hide cursor.
const QString HIDE_CURSOR = QStringLiteral("\x1b[?25l");

/// Build the settings item list from ConfigManager.
QList<ConfigPanel::Item> buildItems(services::ConfigManager &config)
{
    QList<ConfigPanel::Item> items;

    // Provider (Enum)
    {
        ConfigPanel::Item item;
        item.label = QStringLiteral("Provider");
        item.configKey = QStringLiteral("provider");
        item.type = ConfigPanel::Item::Enum;
        item.options = {QStringLiteral("anthropic"), QStringLiteral("openai_compat")};
        QString current = config.provider();
        item.optionIndex = item.options.indexOf(current);
        if (item.optionIndex < 0)
            item.optionIndex = 0;
        item.value = item.options.at(item.optionIndex);
        items.append(item);
    }

    // Model (String — display only, edited via /model)
    {
        ConfigPanel::Item item;
        item.label = QStringLiteral("Model");
        item.configKey = QStringLiteral("model");
        item.type = ConfigPanel::Item::String;
        item.value = config.currentModel();
        items.append(item);
    }

    // Wire API (Enum)
    {
        ConfigPanel::Item item;
        item.label = QStringLiteral("Wire API");
        item.configKey = QStringLiteral("wireApi");
        item.type = ConfigPanel::Item::Enum;
        item.options = {QStringLiteral("chat_completions"), QStringLiteral("responses")};
        QString current = config.wireApi();
        item.optionIndex = item.options.indexOf(current);
        if (item.optionIndex < 0)
            item.optionIndex = 0;
        item.value = item.options.at(item.optionIndex);
        items.append(item);
    }

    // Base URL (String)
    {
        ConfigPanel::Item item;
        item.label = QStringLiteral("Base URL");
        item.configKey = QStringLiteral("baseUrl");
        item.type = ConfigPanel::Item::String;
        item.value = config.baseUrl();
        if (item.value.isEmpty())
            item.value = QStringLiteral("(default)");
        items.append(item);
    }

    // Proxy (String)
    {
        ConfigPanel::Item item;
        item.label = QStringLiteral("Proxy");
        item.configKey = QStringLiteral("proxy");
        item.type = ConfigPanel::Item::String;
        item.value = config.proxy();
        if (item.value.isEmpty())
            item.value = QStringLiteral("(none)");
        items.append(item);
    }

    // Active Profile (Enum)
    {
        ConfigPanel::Item item;
        item.label = QStringLiteral("Active Profile");
        item.configKey = QStringLiteral("activeProfile");
        item.type = ConfigPanel::Item::Enum;
        item.options = config.profileNames();
        item.options.prepend(QStringLiteral("(none)"));
        QString current = config.activeProfile();
        if (current.isEmpty())
            item.optionIndex = 0;
        else
        {
            item.optionIndex = item.options.indexOf(current);
            if (item.optionIndex < 0)
                item.optionIndex = 0;
        }
        item.value = item.options.at(item.optionIndex);
        items.append(item);
    }

    return items;
}

/// Render the panel to stdout.
void render(const QList<ConfigPanel::Item> &items, int selectedIndex)
{
    QString out;
    out += CURSOR_HOME;

    // Header
    out += TerminalStyle::bold(QStringLiteral("  Settings"))
         + TerminalStyle::dim(QString(PANEL_WIDTH, ' '))
         + TerminalStyle::dim(QStringLiteral("[Esc] close"))
         + QStringLiteral("\n");

    // Separator
    const QChar HORZ(0x2500);
    out += QStringLiteral("  ") + QString(PANEL_WIDTH, HORZ) + QStringLiteral("\n\n");

    // Items
    for (int i = 0; i < items.size(); ++i)
    {
        const auto &item = items.at(i);
        QString marker = QStringLiteral("  ");
        QString label = TerminalStyle::dim(item.label);

        QString value;
        if (item.type == ConfigPanel::Item::Enum)
        {
            if (i == selectedIndex)
                value = TerminalStyle::boldCyan(item.value);
            else
                value = item.value;
        }
        else if (item.type == ConfigPanel::Item::Boolean)
        {
            if (item.value == QStringLiteral("on") || item.value == QStringLiteral("true"))
                value = TerminalStyle::fgGreen(item.value);
            else
                value = TerminalStyle::fgGray(item.value);
        }
        else
        {
            value = item.value;
        }

        if (i == selectedIndex)
        {
            marker = TerminalStyle::boldCyan(QStringLiteral(">"));
            label = TerminalStyle::bold(item.label);
        }

        QString paddedLabel = label + QString(LABEL_WIDTH - item.label.length(), ' ');
        out += marker + QStringLiteral(" ") + paddedLabel + value + QStringLiteral("\n");
    }

    out += QStringLiteral("\n");

    // Footer separator
    out += QStringLiteral("  ") + QString(PANEL_WIDTH, HORZ) + QStringLiteral("\n");

    // Key hints
    out += TerminalStyle::dim(
        QStringLiteral("  up/down navigate  Enter toggle/select  Esc close"));
    // Reset attributes and clear any leftover lines from previous taller render
    out += RESET_ATTRS + QStringLiteral("\n\x1b[J");

    // Write to stdout
    fputs(out.toUtf8().constData(), stdout);
    fflush(stdout);
}

/// Apply an item's value back to ConfigManager.
void applyItem(services::ConfigManager &config, const ConfigPanel::Item &item)
{
    if (item.configKey == QLatin1String("provider"))
    {
        config.setProvider(item.value);
    }
    else if (item.configKey == QLatin1String("wireApi"))
    {
        config.setWireApi(item.value);
    }
    else if (item.configKey == QLatin1String("activeProfile"))
    {
        if (item.value == QLatin1String("(none)"))
            config.setActiveProfile(QString());
        else
            config.setActiveProfile(item.value);
    }
    // String items (model, baseUrl, proxy) are display-only in the panel
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// ConfigPanel::run
// ---------------------------------------------------------------------------

bool ConfigPanel::run(services::ConfigManager &config)
{
    // Check if stdin is a console (interactive terminal)
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode))
        return false;

    QList<Item> items = buildItems(config);
    int selectedIndex = 0;
    bool modified = false;

    RawTerminal raw;
    if (!raw.isValid())
        return false;

    // Ensure ANSI escape sequences work on stdout
    ensureVtpEnabled();

    // Enter alternate screen buffer (clean canvas, like vim/less)
    fputs(ENTER_ALT_BUF.toUtf8().constData(), stdout);
    fputs(HIDE_CURSOR.toUtf8().constData(), stdout);
    fflush(stdout);

    // Flush any leftover events in the input buffer to prevent spurious renders
    FlushConsoleInputBuffer(h);

    while (true)
    {
        render(items, selectedIndex);

        KeyPress kp = raw.readKey();

        switch (kp.key)
        {
        case Key::Up:
            if (selectedIndex > 0)
                --selectedIndex;
            break;

        case Key::Down:
            if (selectedIndex < items.size() - 1)
                ++selectedIndex;
            break;

        case Key::Enter:
        {
            auto &item = items[selectedIndex];
            if (item.type == Item::Enum)
            {
                // Cycle to next option
                item.optionIndex = (item.optionIndex + 1) % item.options.size();
                item.value = item.options.at(item.optionIndex);
                applyItem(config, item);
                modified = true;
            }
            break;
        }

        case Key::Escape:
            // Reset attributes, leave alternate buffer, restore cursor
            fputs(RESET_ATTRS.toUtf8().constData(), stdout);
            fputs(LEAVE_ALT_BUF.toUtf8().constData(), stdout);
            fflush(stdout);
            return modified;

        default:
            break;
        }
    }
}

} // namespace act::framework
