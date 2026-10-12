#include <QGuiApplication>
#include <QIcon>
#include <QFont>
#include <QFontDatabase>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QLocale>
#include <QTranslator>
#include <QDir>
#include <QLockFile>
#include <QStandardPaths>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <memory>
#include "security/AppAccess.h"
#include "security/SystemLockMonitor.h"

#include "interface/FinanceController.h"
#include "ui/WindowScrollInput.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app/vexa.png")));
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    app.setDesktopFileName(QStringLiteral("vexa"));
#endif

    const int interfaceFontId = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/Commissioner.ttf"));
    const int brandFontId = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/BBHBartle-Regular.ttf"));
    if (interfaceFontId < 0 || brandFontId < 0) {
        qCritical() << "Не удалось загрузить встроенные шрифты приложения";
        return 1;
    }

    QFont interfaceFont = app.font();
    interfaceFont.setFamily(QStringLiteral("Commissioner"));
    app.setFont(interfaceFont);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Keep the existing application data directory; branding does not migrate user data.
    const auto dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataDirectory)) return 1;
    QLockFile installationLock(QDir(dataDirectory).filePath("vexa-session.lock"));
    installationLock.setStaleLockTime(0);
    if (!installationLock.tryLock()) {
        qWarning() << "Vexa is already running or its data directory is unavailable";
        return 1;
    }
    app.setQuitOnLastWindowClosed(false);
    AppAccess appAccess(dataDirectory);
    SystemLockMonitor systemLock;
    QObject::connect(&systemLock, &SystemLockMonitor::lockRequested, &appAccess, &AppAccess::systemLock);
    WindowScrollInput windowScrollInput;
    QTranslator translator;
    std::unique_ptr<FinanceController> financeController;
    std::unique_ptr<QQmlApplicationEngine> financeEngine;
    QQmlApplicationEngine accessEngine;
    accessEngine.rootContext()->setContextProperty("appAccess", &appAccess);
    QObject::connect(&accessEngine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    accessEngine.loadFromModule("MoneyTracker", "AccessWindow");
    if (accessEngine.rootObjects().isEmpty()) return 1;

    const auto applyUiLanguage = [&] {
        QCoreApplication::removeTranslator(&translator);
        const bool english = financeController && financeController->uiLanguage() == QStringLiteral("en");
        QLocale::setDefault(english ? QLocale(QLocale::English, QLocale::UnitedStates)
                                   : QLocale(QLocale::Russian, QLocale::Russia));
        if (english && translator.load(QStringLiteral(":/i18n/qml_en.qm")))
            QCoreApplication::installTranslator(&translator);
        accessEngine.retranslate();
        if (financeEngine) financeEngine->retranslate();
        if (financeController) financeController->retranslate();
    };
    const auto rebuildSession = [&] {
        // Destroy QML first so destruction handlers can still flush notes.
        financeEngine.reset();
        financeController.reset();
        if (appAccess.locked()) return;
        financeController = std::make_unique<FinanceController>();
        financeEngine = std::make_unique<QQmlApplicationEngine>();
        financeEngine->rootContext()->setContextProperty("appAccess", &appAccess);
        financeEngine->rootContext()->setContextProperty("windowScrollInput", &windowScrollInput);
        financeEngine->rootContext()->setContextProperty("financeController", financeController.get());
        QObject::connect(financeController.get(), &FinanceController::uiLanguageChanged,
                         financeEngine.get(), applyUiLanguage);
        applyUiLanguage();
        QObject::connect(financeEngine.get(), &QQmlApplicationEngine::objectCreationFailed,
                         &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        financeEngine->loadFromModule("MoneyTracker", "Mobile");
#else
        financeEngine->loadFromModule("MoneyTracker", "Desktop");
#endif
    };
    QObject::connect(&appAccess, &AppAccess::admissionChanged, &app, [&] {
        // Immediately withdraw the financial UI; teardown runs after the QML callback.
        if (appAccess.locked() && financeEngine) {
            for (auto* object : financeEngine->rootObjects()) {
                if (auto* window = qobject_cast<QQuickWindow*>(object)) {
                    window->setVisible(false);
                    window->contentItem()->setEnabled(false);
                }
            }
        }
        QTimer::singleShot(0, &app, rebuildSession);
    });
    appAccess.start();
    const int exitCode = app.exec();
    financeEngine.reset();
    financeController.reset();
    return exitCode;
}
