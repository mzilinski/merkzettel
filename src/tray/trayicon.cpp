#include "trayicon.h"

#include <KStatusNotifierItem>
#include <KLocalizedString>
#include <QMenu>
#include <QAction>
#include <QIcon>

namespace Merkzettel {

TrayIcon::TrayIcon(QObject *parent)
    : QObject(parent)
    , m_sni(new KStatusNotifierItem(QStringLiteral("merkzettel"), this))
{
    m_sni->setTitle(QStringLiteral("Merkzettel"));
    m_sni->setIconByName(QStringLiteral("view-pim-tasks"));
    m_sni->setAttentionIconByName(QStringLiteral("emblem-important"));
    m_sni->setStatus(KStatusNotifierItem::Active);
    m_sni->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_sni->setStandardActionsEnabled(false);
    m_sni->setToolTip(QStringLiteral("merkzettel"),
                      i18n("Merkzettel"),
                      i18n("Microsoft To Do"));

    auto *menu = new QMenu();
    auto *openAction = menu->addAction(QIcon::fromTheme(QStringLiteral("view-list-details")),
                                       i18n("Show/hide window"));
    auto *syncAction = menu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                                       i18n("Synchronize"));
    menu->addSeparator();
    auto *quitAction = menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")),
                                       i18n("Quit"));

    connect(openAction, &QAction::triggered, this, &TrayIcon::activated);
    connect(syncAction, &QAction::triggered, this, &TrayIcon::syncRequested);
    connect(quitAction, &QAction::triggered, this, &TrayIcon::quitRequested);

    m_sni->setContextMenu(menu);

    connect(m_sni, &KStatusNotifierItem::activateRequested,
            this, [this](bool, const QPoint &) { Q_EMIT activated(); });
}

void TrayIcon::show()
{
    m_sni->setStatus(KStatusNotifierItem::Active);
}

void TrayIcon::setOpenCount(int count)
{
    if (count > 0) {
        m_sni->setToolTip(QStringLiteral("merkzettel"),
                          i18n("Merkzettel"),
                          i18np("%1 open task due today", "%1 open tasks due today", count));
        m_sni->setOverlayIconByName(QStringLiteral("emblem-important"));
    } else {
        m_sni->setToolTip(QStringLiteral("merkzettel"),
                          i18n("Merkzettel"),
                          i18n("No tasks due"));
        m_sni->setOverlayIconByName(QString());
    }
}

} // namespace Merkzettel
