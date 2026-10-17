#include <QtQuickTest/quicktest.h>
#include <QDir>
#include <QFontDatabase>
#include <QLocale>
#include <QQmlEngine>
#include <QQmlContext>

class UiTestSetup : public QObject
{
    Q_OBJECT
public slots:
    void applicationAvailable()
    {
        QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));
        const QDir fonts(QStringLiteral(QUICK_TEST_SOURCE_DIR "/../../assets/fonts/editorial"));
        for (const QString& name : fonts.entryList({QStringLiteral("*.ttf")}, QDir::Files))
            QFontDatabase::addApplicationFont(fonts.filePath(name));
    }
    void qmlEngineAvailable(QQmlEngine* engine)
    {
        engine->rootContext()->setContextProperty(QStringLiteral("uiCaptureDirectory"),
            qEnvironmentVariable("VEXA_UI_CAPTURE_DIR"));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(UiNavigationTest, UiTestSetup)
#include "UiNavigationTest.moc"
