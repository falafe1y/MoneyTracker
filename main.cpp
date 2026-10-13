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
#include <QProcess>
#include "storage/DatabaseProtection.h"
#include <memory>
#include "security/AppAccess.h"
#include "security/SystemLockMonitor.h"

#include "interface/FinanceController.h"
#include "ui/computer/common/WindowScrollInput.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral(VEXA_APP_VERSION));
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
    DatabaseProtection databaseProtection(QDir(dataDirectory).filePath("moneytracker.sqlite3"));
    databaseProtection.setBackupSchemaValidator(FinanceRepository::migrateBackup);
    SystemLockMonitor systemLock;
    QObject::connect(&systemLock, &SystemLockMonitor::lockRequested, &appAccess, &AppAccess::systemLock);
    WindowScrollInput windowScrollInput;
    QTranslator translator;
    std::unique_ptr<FinanceController> financeController;
    std::unique_ptr<QQmlApplicationEngine> financeEngine;
    QQmlApplicationEngine databaseEngine;
    databaseEngine.rootContext()->setContextProperty("appAccess", &appAccess);
    databaseEngine.rootContext()->setContextProperty("databaseProtection", &databaseProtection);
    databaseEngine.loadFromModule("MoneyTracker", "DatabaseWindow");
    if (databaseEngine.rootObjects().isEmpty()) return 1;
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
        databaseEngine.retranslate();
        if (financeEngine) financeEngine->retranslate();
        if (financeController) financeController->retranslate();
    };
    const auto closeFinance = [&] {
        financeEngine.reset();
        financeController.reset();
    };
    const auto hideFinance = [&] {
        if (!financeEngine) return;
        for (auto* object : financeEngine->rootObjects()) {
            if (auto* window = qobject_cast<QQuickWindow*>(object)) {
                window->setVisible(false);
                window->contentItem()->setEnabled(false);
            }
        }
    };
    const auto openFinance = [&] {
        // Destroy QML first so destruction handlers can still flush notes.
        financeEngine.reset();
        financeController.reset();
        if (appAccess.locked() || !databaseProtection.ready() || databaseProtection.busy()) return;
        const auto access = databaseProtection.access();
        financeController = std::make_unique<FinanceController>(nullptr, access.path, access.key);
        if (!financeController->storageReady()) {
            const auto error = financeController->storageError();
            financeController.reset();
            databaseProtection.storageFailed(error);
            return;
        }
        financeEngine = std::make_unique<QQmlApplicationEngine>();
        financeEngine->rootContext()->setContextProperty("appAccess", &appAccess);
        financeEngine->rootContext()->setContextProperty("databaseProtection", &databaseProtection);
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
    QObject::connect(&databaseProtection, &DatabaseProtection::available, &app, openFinance);
    QObject::connect(&databaseProtection, &DatabaseProtection::maintenanceRequested, &app, [&](bool encrypted) {
        hideFinance();
        QTimer::singleShot(0, &app, [&, encrypted] {
            closeFinance();
            databaseProtection.beginChange(encrypted);
        });
    });
    QObject::connect(&databaseProtection, &DatabaseProtection::restoreRequested, &app, [&] {
        hideFinance();
        QTimer::singleShot(0, &app, [&] {
            closeFinance();
            databaseProtection.beginRestore();
        });
    });
    QObject::connect(&appAccess, &AppAccess::admissionChanged, &app, [&] {
        databaseProtection.setAdmitted(!appAccess.locked());
        if (appAccess.locked()) hideFinance();
        QTimer::singleShot(0, &app, [&] {
            closeFinance();
            databaseProtection.releaseSession();
            if (!appAccess.locked()) databaseProtection.prepare();
        });
    });
    appAccess.start();
    for (;;) {
        const int exitCode = app.exec();
        closeFinance();
        if (exitCode != 42) return exitCode;
        // The converted database is closed and durable before starting a new process.
        installationLock.unlock();
        if (QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1)))
            return 0;
        if (!installationLock.tryLock()) return 1;
        databaseProtection.storageFailed(QStringLiteral("Не удалось перезапустить Vexa. Повторите попытку или откройте приложение вручную."));
    }
}
