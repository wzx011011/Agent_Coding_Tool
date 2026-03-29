#include "tui_app.h"

#include <QJsonDocument>
#include <algorithm>
#include <chrono>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <thread>

namespace act::cli {

namespace {

ftxui::Color messageColor(act::framework::SessionMessageKind kind) {
    switch (kind) {
    case act::framework::SessionMessageKind::User:
        return ftxui::Color::CyanLight;
    case act::framework::SessionMessageKind::Assistant:
        return ftxui::Color::White;
    case act::framework::SessionMessageKind::System:
        return ftxui::Color::GrayLight;
    case act::framework::SessionMessageKind::Tool:
        return ftxui::Color::YellowLight;
    case act::framework::SessionMessageKind::Error:
        return ftxui::Color::RedLight;
    }

    return ftxui::Color::White;
}

ftxui::Element renderMessage(const act::framework::SessionMessage &message) {
    using namespace ftxui;

    Elements lines;
    lines.push_back(text(message.title.toStdString()) | bold | color(messageColor(message.kind)));

    if (message.content.trimmed().isEmpty())
        lines.push_back(text(" "));
    else
        lines.push_back(paragraph(message.content.toStdString()) | color(Color::GrayLight));

    return vbox(std::move(lines));
}

QString latestActivity(const QStringList &entries) {
    if (entries.isEmpty())
        return QStringLiteral("No recent activity");

    return entries.constLast();
}

ftxui::Element renderHeaderCard(const act::framework::InteractiveSessionState &state) {
    using namespace ftxui;

    Elements leftColumn;
    leftColumn.push_back(text("Welcome back") | bold | color(Color::White));
    leftColumn.push_back(text("ACT TUI preview") | color(Color::YellowLight));
    leftColumn.push_back(text("Runtime-first frontend") | dim);
    leftColumn.push_back(text(state.status().toStdString()) |
                         color(state.isBusy() ? Color::YellowLight : Color::GreenLight));

    Elements rightColumn;
    rightColumn.push_back(text("Tips") | bold | color(Color::YellowLight));
    rightColumn.push_back(paragraph("Enter to submit, Esc to exit, /reset to clear the session."));
    rightColumn.push_back(separator());
    rightColumn.push_back(text("Recent activity") | bold | color(Color::YellowLight));
    rightColumn.push_back(paragraph(latestActivity(state.activityLog()).toStdString()) | dim);

    return hbox({
               vbox(std::move(leftColumn)) | flex,
               separator(),
               vbox(std::move(rightColumn)) | flex,
           }) |
           border;
}

ftxui::Element renderTranscript(const act::framework::InteractiveSessionState &state) {
    using namespace ftxui;

    Elements transcriptElements;
    if (state.messages().isEmpty()) {
        transcriptElements.push_back(text("No messages yet") | dim);
    } else {
        for (int index = 0; index < state.messages().size(); ++index) {
            transcriptElements.push_back(renderMessage(state.messages().at(index)));
            if (index + 1 < state.messages().size())
                transcriptElements.push_back(separator());
        }
    }

    return vbox(std::move(transcriptElements)) | yframe | vscroll_indicator | flex;
}

ftxui::Element renderFooter(const act::framework::InteractiveSessionState &state, const ftxui::Component &input) {
    using namespace ftxui;

    const auto statusColor = state.isBusy() ? Color::YellowLight : Color::GrayLight;

    return vbox({
        separator(),
        input->Render(),
        hbox({
            text("Enter submit  Esc exit  /reset clear  y/n permission") | dim,
            filler(),
            text(state.isBusy() ? "running" : "idle") | color(statusColor),
        }),
    });
}

} // namespace

TuiApp::TuiApp(act::services::AIEngine &engine, act::harness::ToolRegistry &tools,
               act::harness::PermissionManager &permissions, act::harness::ContextManager &context,
               const QString &systemPrompt)
    : m_controller(engine, tools, permissions, context) {
    m_controller.setSystemPrompt(systemPrompt);
}

TuiApp::~TuiApp() {
}

act::framework::InteractiveSessionState TuiApp::snapshotState() const {
    return m_controller.snapshotState();
}

int TuiApp::run() {
    using namespace ftxui;
    using namespace std::chrono_literals;

    ScreenInteractive screen = ScreenInteractive::TerminalOutput();
    std::string inputBuffer;

    std::jthread refreshThread([&screen](std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            std::this_thread::sleep_for(50ms);
            screen.PostEvent(Event::Custom);
        }
    });

    auto stateChangedConnection =
        QObject::connect(&m_controller, &act::framework::InteractiveSessionController::stateChanged,
                         [&screen] { screen.PostEvent(Event::Custom); });

    InputOption inputOptions;
    inputOptions.placeholder = "Describe the task and press Enter";
    Component input = Input(&inputBuffer, inputOptions);

    auto renderer = Renderer(input, [&] {
        const auto state = snapshotState();

        Element root = vbox({
                           renderHeaderCard(state),
                           separator(),
                           renderTranscript(state),
                           renderFooter(state, input),
                       }) |
                       flex;

        if (state.isBusy()) {
            root = vbox({
                       root,
                       separator(),
                       text("Running...") | color(Color::YellowLight) | bold,
                   }) |
                   flex;
        }

        if (state.permissionPrompt().active) {
            auto modal = vbox({
                             text("Permission Request") | bold | color(Color::YellowLight),
                             separator(),
                             text(state.permissionPrompt().toolName.toStdString()),
                             text(state.permissionPrompt().level.toStdString()) | dim,
                             paragraph(state.permissionPrompt().description.toStdString()),
                             separator(),
                             text("Press y to approve or n to deny") | dim,
                         }) |
                         border | bgcolor(Color::Black) | size(WIDTH, LESS_THAN, 72);
            root = dbox({root, vbox({filler(), hbox({filler(), modal, filler()}), filler()})});
        }

        return root;
    });

    auto app = CatchEvent(renderer, [&](Event event) {
        const auto state = snapshotState();

        if (event == Event::Custom)
            return true;

        if (state.permissionPrompt().active) {
            if (event == Event::Character('y') || event == Event::Character('Y')) {
                m_controller.approvePermission();
                return true;
            }
            if (event == Event::Character('n') || event == Event::Character('N') || event == Event::Escape) {
                m_controller.denyPermission();
                return true;
            }
        }

        if (event == Event::Return) {
            QString message = QString::fromUtf8(inputBuffer);
            if (message == QStringLiteral("/reset")) {
                m_controller.resetConversation();
                inputBuffer.clear();
                return true;
            }

            if (!state.isBusy() && !message.trimmed().isEmpty()) {
                m_controller.submitInput(message);
                inputBuffer.clear();
                return true;
            }
        }

        if (event == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }

        return false;
    });

    screen.Loop(app);
    QObject::disconnect(stateChangedConnection);
    return 0;
}

} // namespace act::cli