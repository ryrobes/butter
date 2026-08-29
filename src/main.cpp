#include "BtrfsBackend.h"
#include "HomeScanner.h"
#include "HomeShadow.h"
#include "ThemeBridge.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>

int main(int argc, char *argv[]) {
  qputenv("QT_NO_XDG_DESKTOP_PORTAL", QByteArrayLiteral("1"));

  QSurfaceFormat format = QSurfaceFormat::defaultFormat();
  format.setAlphaBufferSize(8);
  QSurfaceFormat::setDefaultFormat(format);

  QGuiApplication::setDesktopFileName(QStringLiteral("org.omarchy.butter"));
  QGuiApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("butter"));
  app.setApplicationDisplayName(QStringLiteral("Butter"));
  app.setApplicationVersion(QStringLiteral(BUTTER_VERSION));
  app.setOrganizationName(QStringLiteral("omarchy"));
  app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Butter/assets/butter.svg")));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Your filesystem, explained."));
  parser.addHelpOption();
  parser.addVersionOption();
  const QCommandLineOption screenshotOption(
      QStringLiteral("screenshot"),
      QStringLiteral("Render one frame to a PNG and exit."),
      QStringLiteral("path"));
  parser.addOption(screenshotOption);
  parser.process(app);

  ThemeBridge theme;
  BtrfsBackend filesystem;
  HomeScanner homeSpace;
  HomeShadow homeShadow;

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("appTheme"), &theme);
  engine.rootContext()->setContextProperty(QStringLiteral("filesystem"),
                                           &filesystem);
  engine.rootContext()->setContextProperty(QStringLiteral("homeSpace"),
                                           &homeSpace);
  engine.rootContext()->setContextProperty(QStringLiteral("homeShadow"),
                                           &homeShadow);
  engine.loadFromModule(QStringLiteral("Butter"), QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty())
    return 1;

  if (parser.isSet(screenshotOption)) {
    const QString path = parser.value(screenshotOption);
    QTimer::singleShot(450, &app, [&engine, &app, path] {
      auto *window =
          qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
      if (!window || !window->grabWindow().save(path)) {
        app.exit(2);
        return;
      }
      app.quit();
    });
  }
  return app.exec();
}
