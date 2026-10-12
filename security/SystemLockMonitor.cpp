#include "SystemLockMonitor.h"
#include <QCoreApplication>
#include <QEvent>
#include <QWindow>
#include <QDebug>
#ifdef Q_OS_WIN
#include <windows.h>
#include <wtsapi32.h>
#endif
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

SystemLockMonitor::SystemLockMonitor(QObject* parent) : QObject(parent)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
    QCoreApplication::instance()->installEventFilter(this);
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    auto bus = QDBusConnection::systemBus();
    if (!bus.connect("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager",
                     "PrepareForSleep", this, SLOT(prepareForSleep(bool))))
        qWarning() << "Vexa: logind sleep notifications unavailable";
    QDBusInterface manager("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", bus);
    auto* watcher = new QDBusPendingCallWatcher(manager.asyncCall("GetSessionByPID", uint(QCoreApplication::applicationPid())), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, bus, watcher]() mutable {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (!reply.isError()) {
            const auto path = reply.value().path();
            bus.connect("org.freedesktop.login1", path, "org.freedesktop.login1.Session", "Lock", this, SLOT(sessionLocked()));
            bus.connect("org.freedesktop.login1", path, "org.freedesktop.DBus.Properties", "PropertiesChanged",
                        this, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
        } else qWarning() << "Vexa: logind session notifications unavailable";
        watcher->deleteLater();
    });
    acquireInhibitor();
#endif
}
SystemLockMonitor::~SystemLockMonitor()
{
    QCoreApplication::instance()->removeNativeEventFilter(this);
}
void SystemLockMonitor::acquireInhibitor()
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QDBusInterface manager("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", QDBusConnection::systemBus());
    auto* watcher = new QDBusPendingCallWatcher(manager.asyncCall("Inhibit", "sleep", "Vexa", "Lock application", "delay"), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        QDBusPendingReply<QDBusUnixFileDescriptor> reply = *watcher;
        if (!reply.isError()) inhibitor_ = reply.value();
        watcher->deleteLater();
    });
#endif
}
void SystemLockMonitor::prepareForSleep(bool sleeping)
{
    emit lockRequested();
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (sleeping) inhibitor_ = QDBusUnixFileDescriptor();
    else acquireInhibitor();
#else
    Q_UNUSED(sleeping);
#endif
}
void SystemLockMonitor::sessionLocked() { emit lockRequested(); }
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
void SystemLockMonitor::propertiesChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated)
{
    Q_UNUSED(invalidated);
    if (interface == "org.freedesktop.login1.Session" && changed.value("LockedHint").toBool()) emit lockRequested();
}
#endif
bool SystemLockMonitor::eventFilter(QObject* object, QEvent* event)
{
#ifdef Q_OS_WIN
    if (event->type() == QEvent::Show) {
        if (auto* window = qobject_cast<QWindow*>(object))
            WTSRegisterSessionNotification(reinterpret_cast<HWND>(window->winId()), NOTIFY_FOR_THIS_SESSION);
    }
#endif
    return QObject::eventFilter(object, event);
}
bool SystemLockMonitor::nativeEventFilter(const QByteArray& type, void* message, qintptr* result)
{
    Q_UNUSED(type); Q_UNUSED(result);
#ifdef Q_OS_WIN
    const auto* event = static_cast<MSG*>(message);
    if ((event->message == WM_WTSSESSION_CHANGE && event->wParam == WTS_SESSION_LOCK)
        || (event->message == WM_POWERBROADCAST
            && (event->wParam == PBT_APMSUSPEND || event->wParam == PBT_APMRESUMEAUTOMATIC
                || event->wParam == PBT_APMRESUMESUSPEND))) emit lockRequested();
#else
    Q_UNUSED(message);
#endif
    return false;
}
