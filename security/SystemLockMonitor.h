#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QDBusUnixFileDescriptor>
#include <QVariantMap>
#endif
class SystemLockMonitor final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit SystemLockMonitor(QObject* parent = nullptr);
    ~SystemLockMonitor() override;
    bool eventFilter(QObject* object, QEvent* event) override;
    bool nativeEventFilter(const QByteArray& type, void* message, qintptr* result) override;
signals:
    void lockRequested();
private slots:
    void prepareForSleep(bool sleeping);
    void sessionLocked();
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    void propertiesChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated);
#endif
private:
    void acquireInhibitor();
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QDBusUnixFileDescriptor inhibitor_;
#endif
};
