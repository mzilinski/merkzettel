#pragma once

#include <QObject>

class KStatusNotifierItem;

namespace Merkzettel {

class TrayIcon : public QObject
{
    Q_OBJECT
public:
    explicit TrayIcon(QObject *parent = nullptr);

    void show();
    void setOpenCount(int count);

Q_SIGNALS:
    void activated();
    void syncRequested();
    void quitRequested();

private:
    KStatusNotifierItem *m_sni;
};

} // namespace Merkzettel
