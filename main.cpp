#include <QGuiApplication>
#include <QFont>
#include <QFontDatabase>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QLocale>
#include <QTranslator>

#include "interface/FinanceController.h"
#include "ui/WindowScrollInput.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

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

    WindowScrollInput windowScrollInput;
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("windowScrollInput", &windowScrollInput);
    FinanceController financeController;
    QTranslator translator;

    const auto applyUiLanguage = [&]()
    {
        QCoreApplication::removeTranslator(&translator);

        const bool english = financeController.uiLanguage() == QStringLiteral("en");
        QLocale::setDefault(english ? QLocale(QLocale::English, QLocale::UnitedStates)
                                    : QLocale(QLocale::Russian, QLocale::Russia));
        if (english) {
            const QString translationPath = QStringLiteral(":/i18n/qml_en.qm");
            if (translator.load(translationPath)) {
                QCoreApplication::installTranslator(&translator);
            }
        }

        engine.retranslate();
        financeController.retranslate();
    };

    applyUiLanguage();
    QObject::connect(
        &financeController,
        &FinanceController::uiLanguageChanged,
        &engine,
        applyUiLanguage
        );

    engine.rootContext()->setContextProperty(
        "financeController",
        &financeController
        );

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []()
        {
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
        );

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    engine.loadFromModule("MoneyTracker", "Mobile");
#else
    engine.loadFromModule("MoneyTracker", "Desktop");
#endif

    return app.exec();
}
