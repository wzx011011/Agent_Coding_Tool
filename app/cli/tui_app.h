#pragma once

#include "framework/interactive_session_controller.h"
#include "framework/interactive_session_state.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "services/ai_engine.h"
#include <QObject>

namespace act::cli {

class TuiApp {
  public:
    TuiApp(act::services::AIEngine &engine, act::harness::ToolRegistry &tools,
           act::harness::PermissionManager &permissions, act::harness::ContextManager &context);
    ~TuiApp();

    int run();

  private:
    [[nodiscard]] act::framework::InteractiveSessionState snapshotState() const;

    act::framework::InteractiveSessionController m_controller;
};

} // namespace act::cli