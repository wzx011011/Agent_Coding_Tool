#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

#include "core/runtime_version.h"

int main(int argc, char *argv[])
{
  QCoreApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("aictl"));
  app.setApplicationVersion(act::core::runtimeVersion());

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("ACT CLI runtime bootstrap"));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument(QStringLiteral("task"), QStringLiteral("Task description for the agent runtime."));
  parser.process(app);

  QTextStream out(stdout);
  out << "ACT CLI bootstrap is ready. Runtime version: " << app.applicationVersion() << Qt::endl;
  return 0;
}