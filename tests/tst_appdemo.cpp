// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later

// Smoke test of the real QML front end in demo mode: loads Main.qml from the
// source tree with the actual App controller, fails on any QML warning and
// drives the core flows through the controller API the UI uses.

#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTest>
#include <QTextDocument>

#include "app.h"
#include "models/tasklistsmodel.h"

using namespace Merkzettel;

namespace {
QStringList g_qmlWarnings;
QtMessageHandler g_previous = nullptr;

void collectQmlWarnings(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    // Only our own QML counts; Kirigami's internal messages (e.g. incubation
    // timing under sanitizers) are not ours to fix.
    const bool fromQml = msg.contains(QLatin1String(MERKZETTEL_QML_DIR "/"));
    // Known upstream issue: Kirigami's OverlaySheet template binds y to
    // implicitHeight and implicitHeight to y (templates/OverlaySheet.qml,
    // "parent.height - y"), which loops whenever the content is taller than the
    // window. Not caused by our QML; tracked for v0.4 (move to Kirigami.Dialog).
    const bool knownUpstream = msg.contains(QLatin1String("templates/OverlaySheet.qml"))
        && msg.contains(QLatin1String("Binding loop"));
    if (fromQml && !knownUpstream
        && (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg))
        g_qmlWarnings.append(msg);
    if (g_previous) g_previous(type, ctx, msg);
}

QStringList ids(const TasksModel *m)
{
    QStringList out;
    for (int i = 0; i < m->rowCount(); ++i)
        out.append(m->data(m->index(i, 0), TasksModel::IdRole).toString());
    return out;
}
} // namespace

class TestAppDemo : public QObject
{
    Q_OBJECT

    App *m_app = nullptr;
    QQmlApplicationEngine *m_engine = nullptr;

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        g_previous = qInstallMessageHandler(collectQmlWarnings);
        KLocalizedString::setApplicationDomain("merkzettel");
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));

        m_app = new App(this);
        m_app->setDemoMode(true);
        m_engine = new QQmlApplicationEngine(this);
        KLocalization::setupLocalizedContext(m_engine);
        m_engine->rootContext()->setContextProperty(QStringLiteral("app"), m_app);
        m_engine->rootContext()->setContextProperty(QStringLiteral("appVersion"),
                                                    QStringLiteral(MERKZETTEL_VERSION));
        m_engine->load(QUrl::fromLocalFile(QStringLiteral(MERKZETTEL_QML_DIR "/Main.qml")));
        QVERIFY(!m_engine->rootObjects().isEmpty());
        m_app->start();
        QTest::qWait(500);
        QVERIFY2(g_qmlWarnings.isEmpty(), qPrintable(g_qmlWarnings.join(QLatin1Char('\n'))));
    }

    void cleanup()
    {
        QVERIFY2(g_qmlWarnings.isEmpty(), qPrintable(g_qmlWarnings.join(QLatin1Char('\n'))));
    }

    void startsInTheDefaultList()
    {
        QCOMPARE(m_app->currentListId(), QStringLiteral("demo-tasks"));
        QVERIFY(m_app->loggedIn());
        QVERIFY(m_app->tasksModel()->rowCount() > 5);
        QCOMPARE(m_app->listsModel()->rowCount(), 5 + 5); // 5 smart lists + 5 demo lists
    }

    void smartListsFilter()
    {
        m_app->setCurrentListId(QString::fromLatin1(TaskListsModel::kVirtualMyDayId));
        QCOMPARE(m_app->tasksModel()->rowCount(), 3); // d2, d3, w1 are in the demo My Day
        m_app->setCurrentListId(QString::fromLatin1(TaskListsModel::kVirtualImportantId));
        QVERIFY(m_app->tasksModel()->rowCount() >= 3);
        QVERIFY(m_app->property("currentListVirtual").toBool());
        m_app->setCurrentListId(QStringLiteral("demo-tasks"));
        QVERIFY(!m_app->property("currentListVirtual").toBool());
    }

    void detailSheetOpensAndCloses()
    {
        m_app->openTaskDetails(0);
        QTest::qWait(300);
        QVERIFY(!m_app->detailTask().isEmpty());
        m_app->closeTaskDetails();
        QTest::qWait(200);
        QVERIFY(m_app->detailTask().isEmpty());
    }

    void deleteCanBeUndone()
    {
        const QStringList before = ids(m_app->tasksModel());
        const QString &victim = before.constFirst();
        m_app->deleteTasks({victim});
        QVERIFY(!ids(m_app->tasksModel()).contains(victim));
        m_app->undo();
        QCOMPARE(ids(m_app->tasksModel()), before);
    }

    void moveIsCommittedAfterTheUndoWindow()
    {
        const QString task = ids(m_app->tasksModel()).constFirst();
        m_app->moveTasks({task}, QStringLiteral("demo-work"));
        QVERIFY(!ids(m_app->tasksModel()).contains(task));
        QTest::qWait(6500); // undo window
        m_app->setCurrentListId(QStringLiteral("demo-work"));
        QVERIFY(ids(m_app->tasksModel()).contains(task));
        m_app->setCurrentListId(QStringLiteral("demo-tasks"));
    }

    void myDayToggle()
    {
        const QString task = ids(m_app->tasksModel()).constLast();
        const bool before = m_app->tasksModel()->isInMyDay(task);
        m_app->toggleMyDay({task});
        QCOMPARE(m_app->tasksModel()->isInMyDay(task), !before);
        m_app->toggleMyDay({task});
        QCOMPARE(m_app->tasksModel()->isInMyDay(task), before);
    }

    void demoChangesApplyLocally()
    {
        m_app->setCurrentListId(QStringLiteral("demo-tasks"));
        const int before = m_app->tasksModel()->rowCount();
        m_app->addTask(QStringLiteral("Smoke test task tomorrow"));
        QCOMPARE(m_app->tasksModel()->rowCount(), before + 1);
        QString id;
        for (int i = 0; i < m_app->tasksModel()->rowCount(); ++i) {
            const auto idx = m_app->tasksModel()->index(i, 0);
            if (m_app->tasksModel()->data(idx, TasksModel::TitleRole).toString()
                == QLatin1String("Smoke test task")) {
                id = m_app->tasksModel()->data(idx, TasksModel::IdRole).toString();
                QCOMPARE(m_app->tasksModel()
                             ->data(idx, TasksModel::DueDateRole)
                             .toDateTime()
                             .toLocalTime()
                             .date(),
                         QDate::currentDate().addDays(1));
            }
        }
        QVERIFY(!id.isEmpty());
        m_app->toggleImportance(id);
        m_app->completeTask(id);
        const auto task = m_app->tasksModel()->taskById(id);
        QVERIFY(task.isImportant());
        QVERIFY(task.completed());
    }

    void plainNeutralisesMarkup()
    {
        const QString s = App::plain(QStringLiteral("<b>x</b>"));
        QVERIFY(!Qt::mightBeRichText(s));
        QCOMPARE(s.size(), 10); // two zero-width spaces added, nothing removed
    }

    void cleanupTestCase()
    {
        delete m_engine;
        m_engine = nullptr;
        qInstallMessageHandler(g_previous);
    }
};

QTEST_MAIN(TestAppDemo)
#include "tst_appdemo.moc"
