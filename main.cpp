#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "interface/FinanceController.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;

    FinanceController financeController;

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

    engine.loadFromModule("MoneyTracker", "Main");

    return app.exec();
}