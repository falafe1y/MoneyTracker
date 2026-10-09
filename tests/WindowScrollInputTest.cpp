#include "../ui/WindowScrollInput.h"
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtTest>

class CountingWindow : public QWindow {
public:
    int presses = 0, moves = 0, wheels = 0;
    bool event(QEvent* event) override {
        if (event->type() == QEvent::MouseButtonPress) ++presses;
        if (event->type() == QEvent::MouseMove) ++moves;
        if (event->type() == QEvent::Wheel) ++wheels;
        return QWindow::event(event);
    }
};
class WindowScrollInputTest : public QObject {
    Q_OBJECT
private slots:
    void clicksMovementAndCancellation();
    void wheelAndWindowChange();
};
namespace {
void press(QWindow& window, Qt::MouseButton button) {
    QMouseEvent event(QEvent::MouseButtonPress,QPointF(12,34),QPointF(12,34),button,button,Qt::NoModifier);
    QCoreApplication::sendEvent(&window,&event);
}
void connectInput(WindowScrollInput& input) {
    QObject::connect(&input,&WindowScrollInput::middlePressed,&input,[&input] { input.setActive(true); });
    QObject::connect(&input,&WindowScrollInput::cancelled,&input,[&input] { input.setActive(false); });
}
}
void WindowScrollInputTest::clicksMovementAndCancellation() {
    CountingWindow window; window.setCursor(Qt::CrossCursor);
    WindowScrollInput input; input.setWindow(&window);
    QSignalSpy starts(&input,&WindowScrollInput::middlePressed),moves(&input,&WindowScrollInput::pointerMoved),stops(&input,&WindowScrollInput::cancelled);
    press(window,Qt::LeftButton);QCOMPARE(window.presses,1);QCOMPARE(starts.size(),0);
    // An unhandled middle click must keep its normal meaning.
    press(window,Qt::MiddleButton);QCOMPARE(window.presses,2);QCOMPARE(starts.size(),1);
    connectInput(input);press(window,Qt::MiddleButton);
    QVERIFY(input.active());QCOMPARE(window.presses,2);QCOMPARE(window.cursor().shape(),Qt::SizeAllCursor);
    QCOMPARE(starts.last().first().toPointF(),QPointF(12,34));
    QMouseEvent move(QEvent::MouseMove,QPointF(40,90),QPointF(40,90),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&window,&move);QCOMPARE(moves.size(),1);QCOMPARE(window.moves,0);
    QCOMPARE(moves.first().first().toPointF(),QPointF(40,90));
    QKeyEvent overrideEvent(QEvent::ShortcutOverride,Qt::Key_Escape,Qt::NoModifier);
    QCoreApplication::sendEvent(&window,&overrideEvent);QVERIFY(overrideEvent.isAccepted());QVERIFY(input.active());
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
    QCoreApplication::sendEvent(&window,&escape);QVERIFY(!input.active());QCOMPARE(stops.size(),1);
    QCOMPARE(window.cursor().shape(),Qt::CrossCursor);
    press(window,Qt::MiddleButton);press(window,Qt::LeftButton);QVERIFY(!input.active());QCOMPARE(window.presses,2);
}
void WindowScrollInputTest::wheelAndWindowChange() {
    CountingWindow first,second;WindowScrollInput input;input.setWindow(&first);connectInput(input);
    press(first,Qt::MiddleButton);
    QWheelEvent wheel(QPointF(12,34),QPointF(12,34),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QCoreApplication::sendEvent(&first,&wheel);QVERIFY(!input.active());QCOMPARE(first.wheels,1);
    press(first,Qt::MiddleButton);QEvent deactivate(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(&first,&deactivate);QVERIFY(!input.active());
    input.setWindow(&second);const auto oldPresses=first.presses;press(first,Qt::MiddleButton);
    QCOMPARE(first.presses,oldPresses+1);QVERIFY(!input.active());
    press(second,Qt::MiddleButton);QVERIFY(input.active());input.setWindow(nullptr);QVERIFY(!input.active());
}
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);WindowScrollInputTest test;return QTest::qExec(&test,argc,argv);
}
#include "WindowScrollInputTest.moc"
