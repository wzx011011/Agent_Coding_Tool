#include "framework/interactive_session_controller.h"
#include "harness/context_manager.h"
#include "harness/interfaces.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "services/config_manager.h"
#include <QTemporaryDir>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

using namespace act::core;
using namespace act::framework;
using namespace act::harness;

namespace {

class FakeStreamingAIEngine : public act::services::AIEngine {
  public:
    explicit FakeStreamingAIEngine(act::services::ConfigManager &config) : act::services::AIEngine(config) {}

    QList<LLMMessage> responseQueue;
    QStringList streamedTokens;
    QString errorCode = QStringLiteral("INTERNAL_ERROR");
    int cancelCount = 0;

    void chat(const QList<LLMMessage> & /*messages*/, std::function<void(LLMMessage)> onMessage,
              std::function<void()> onComplete, std::function<void(QString, QString)> onError) override {
        if (!responseQueue.isEmpty()) {
            for (const auto &token : streamedTokens)
                emit streamTokenReceived(token);
            streamedTokens.clear();
            onMessage(responseQueue.takeFirst());
            onComplete();
            return;
        }

        onError(errorCode, QStringLiteral("mock error"));
    }

    void cancel() override { ++cancelCount; }
};

class ExecEchoTool : public ITool {
  public:
    [[nodiscard]] QString name() const override { return QStringLiteral("exec_echo"); }

    [[nodiscard]] QString description() const override { return QStringLiteral("Echo with exec permission"); }

    [[nodiscard]] QJsonObject schema() const override { return {}; }

    ToolResult execute(const QJsonObject &params) override {
        return ToolResult::ok(QStringLiteral("Executed: %1").arg(params.value(QStringLiteral("text")).toString()));
    }

    [[nodiscard]] PermissionLevel permissionLevel() const override { return PermissionLevel::Exec; }

    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

LLMMessage finalResponse(const QString &text) {
    LLMMessage msg;
    msg.role = MessageRole::Assistant;
    msg.content = text;
    return msg;
}

LLMMessage toolCallResponse(const QString &toolName, const QJsonObject &params) {
    LLMMessage msg;
    msg.role = MessageRole::Assistant;
    msg.content = QStringLiteral("Working on it");
    msg.toolCall.id = QStringLiteral("call_123");
    msg.toolCall.name = toolName;
    msg.toolCall.params = params;
    return msg;
}

bool waitForCondition(const std::function<bool()> &predicate, int timeoutMs = 1000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return predicate();
}

} // namespace

class InteractiveSessionControllerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        tempDir = std::make_unique<QTemporaryDir>();
        config = std::make_unique<act::services::ConfigManager>(tempDir->path() + QStringLiteral("/settings.json"));
        engine = std::make_unique<FakeStreamingAIEngine>(*config);
        tools = std::make_unique<ToolRegistry>();
        permissions = std::make_unique<PermissionManager>();
        context = std::make_unique<ContextManager>();
        controller = std::make_unique<InteractiveSessionController>(*engine, *tools, *permissions, *context,
                                                                    InteractiveSessionExecutionMode::Inline);
    }

    void TearDown() override {
        controller.reset();
        context.reset();
        permissions.reset();
        tools.reset();
        engine.reset();
        config.reset();
        tempDir.reset();
    }

    std::unique_ptr<QTemporaryDir> tempDir;
    std::unique_ptr<act::services::ConfigManager> config;
    std::unique_ptr<FakeStreamingAIEngine> engine;
    std::unique_ptr<ToolRegistry> tools;
    std::unique_ptr<PermissionManager> permissions;
    std::unique_ptr<ContextManager> context;
    std::unique_ptr<InteractiveSessionController> controller;
};

TEST_F(InteractiveSessionControllerTest, SubmitInputStreamsAndFinalizesAssistantMessage) {
    engine->streamedTokens = {QStringLiteral("Hello"), QStringLiteral(", world")};
    engine->responseQueue.append(finalResponse(QStringLiteral("Hello, world")));

    controller->submitInput(QStringLiteral("Say hello"));

    ASSERT_TRUE(waitForCondition([this] { return !controller->isBusy(); }));

    const auto state = controller->snapshotState();
    ASSERT_EQ(state.messages().size(), 2);
    EXPECT_EQ(state.messages().at(0).kind, SessionMessageKind::User);
    EXPECT_EQ(state.messages().at(1).kind, SessionMessageKind::Assistant);
    EXPECT_EQ(state.messages().at(1).content, QStringLiteral("Hello, world"));
    EXPECT_EQ(state.status(), QStringLiteral("Completed"));
}

TEST_F(InteractiveSessionControllerTest, AsyncThreadedModeCompletesTurn) {
    auto asyncEngine = std::make_unique<act::services::AIEngine>(*config);
    controller = std::make_unique<InteractiveSessionController>(*asyncEngine, *tools, *permissions, *context,
                                                                InteractiveSessionExecutionMode::AsyncThreaded);

    controller->submitInput(QStringLiteral("Say hello"));

    ASSERT_TRUE(waitForCondition([this] { return !controller->isBusy(); }));

    const auto state = controller->snapshotState();
    ASSERT_FALSE(state.messages().isEmpty());
    EXPECT_EQ(state.messages().front().kind, SessionMessageKind::User);
    EXPECT_EQ(state.status(), QStringLiteral("Failed"));

    controller.reset();
}

TEST_F(InteractiveSessionControllerTest, PermissionPromptCanBeApprovedAndCompletesTurn) {
    tools->registerTool(std::make_unique<ExecEchoTool>());

    QJsonObject params;
    params[QStringLiteral("text")] = QStringLiteral("run");
    engine->responseQueue.append(toolCallResponse(QStringLiteral("exec_echo"), params));
    engine->responseQueue.append(finalResponse(QStringLiteral("Done")));

    std::jthread submitter([this] { controller->submitInput(QStringLiteral("Run command")); });

    ASSERT_TRUE(waitForCondition([this] { return controller->snapshotState().permissionPrompt().active; }));
    controller->approvePermission();
    ASSERT_TRUE(waitForCondition([this] { return !controller->isBusy(); }));

    const auto state = controller->snapshotState();
    EXPECT_FALSE(state.permissionPrompt().active);
    EXPECT_EQ(state.status(), QStringLiteral("Completed"));
    EXPECT_TRUE(state.activityLog().contains(QStringLiteral("Turn completed")));
}

TEST_F(InteractiveSessionControllerTest, ResetConversationClearsStateAndAddsSystemNotice) {
    engine->responseQueue.append(finalResponse(QStringLiteral("Ack")));

    controller->submitInput(QStringLiteral("Hello"));
    ASSERT_TRUE(waitForCondition([this] { return !controller->isBusy(); }));

    controller->resetConversation();

    const auto state = controller->snapshotState();
    ASSERT_EQ(state.messages().size(), 1);
    EXPECT_EQ(state.messages().front().kind, SessionMessageKind::System);
    EXPECT_EQ(state.messages().front().content, QStringLiteral("Conversation reset."));
    EXPECT_EQ(state.status(), QStringLiteral("Idle"));
}