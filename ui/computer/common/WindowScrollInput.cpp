#include "WindowScrollInput.h"

#include <QKeyEvent>
#include <QMouseEvent>

WindowScrollInput::WindowScrollInput(QObject* parent) : QObject(parent) {}
WindowScrollInput::~WindowScrollInput() { setWindow(nullptr); }

void WindowScrollInput::setWindow(QWindow* window)
{
    if (window_ == window) return;
    setActive(false);
    if (window_) window_->removeEventFilter(this);
    window_ = window;
    if (window_) window_->installEventFilter(this);
    emit windowChanged();
}

void WindowScrollInput::setActive(bool active)
{
    if (active_ == active) return;
    active_ = active;
    if (window_) {
        if (active_) { previousCursor_ = window_->cursor(); window_->setCursor(Qt::SizeAllCursor); }
        else window_->setCursor(previousCursor_);
    }
    emit activeChanged();
}

bool WindowScrollInput::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != window_) return false;
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        if (active_) { emit cancelled(); return true; }
        if (const auto mouse = static_cast<QMouseEvent*>(event); mouse->button() == Qt::MiddleButton) {
            emit middlePressed(mouse->position());
            return active_; // Leave clicks outside scrollable areas untouched.
        }
        break;
    case QEvent::MouseMove:
        if (active_) { emit pointerMoved(static_cast<QMouseEvent*>(event)->position()); return true; }
        break;
    case QEvent::MouseButtonRelease:
        if (active_ && static_cast<QMouseEvent*>(event)->button() == Qt::MiddleButton) return true;
        break;
    case QEvent::ShortcutOverride:
        if (active_ && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            event->accept(); return true;
        }
        break;
    case QEvent::KeyPress:
        if (active_ && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            emit cancelled(); return true;
        }
        break;
    case QEvent::Wheel:
    case QEvent::WindowDeactivate:
    case QEvent::Hide:
        if (active_) emit cancelled();
        break;
    default: break;
    }
    return false;
}
