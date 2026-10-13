#pragma once

#include <QObject>
#include <QPointer>
#include <QPointF>
#include <QCursor>
#include <QWindow>

// Observe input before popup and control filters consume it.
class WindowScrollInput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
public:
    explicit WindowScrollInput(QObject* parent = nullptr);
    ~WindowScrollInput() override;
    QWindow* window() const { return window_; }
    void setWindow(QWindow* window);
    bool active() const { return active_; }
    void setActive(bool active);
signals:
    void windowChanged();
    void activeChanged();
    void middlePressed(const QPointF& position);
    void pointerMoved(const QPointF& position);
    void cancelled();
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    QPointer<QWindow> window_;
    QCursor previousCursor_;
    bool active_ = false;
};
