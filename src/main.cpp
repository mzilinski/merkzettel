#include <QApplication>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>
#include <KAboutData>
#include <KLocalizedContext>
#include <KLocalizedString>

#include "app.h"

namespace {
// Eagerly install Qt-style (qsTr) translations for Kirigami so the first
// render shows the localized strings instead of the English source — see
// the "Close Sidebar" flicker. KLocalizedString does not handle qsTr().
void installKirigamiTranslator(QApplication *app)
{
    auto *tr = new QTranslator(app);
    const QString locale = QLocale().name();
    const QString lang = locale.section(QLatin1Char('_'), 0, 0);
    const QStringList candidates = {
        QStringLiteral("/usr/share/locale/%1/LC_MESSAGES/").arg(locale),
        QStringLiteral("/usr/share/locale/%1/LC_MESSAGES/").arg(lang),
        QStringLiteral("/usr/local/share/locale/%1/LC_MESSAGES/").arg(locale),
        QStringLiteral("/usr/local/share/locale/%1/LC_MESSAGES/").arg(lang),
    };
    for (const QString &dir : candidates) {
        if (tr->load(QStringLiteral("libkirigami6_qt"), dir)) {
            QApplication::installTranslator(tr);
            return;
        }
    }
    tr->deleteLater();
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("KDE"));
    QApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    QApplication::setApplicationName(QStringLiteral("merkzettel"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("merkzettel")));
    QApplication::setQuitOnLastWindowClosed(false);

    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));

    installKirigamiTranslator(&app);

    KLocalizedString::setApplicationDomain("merkzettel");
#ifdef MERKZETTEL_BUILD_LOCALE_DIR
    // Pick up translations from the build dir when running uninstalled.
    KLocalizedString::addDomainLocaleDir("merkzettel",
                                         QStringLiteral(MERKZETTEL_BUILD_LOCALE_DIR));
#endif

    KAboutData about(QStringLiteral("merkzettel"),
                     i18n("Merkzettel"),
                     QStringLiteral(MERKZETTEL_VERSION),
                     i18n("Microsoft To Do client for KDE"),
                     KAboutLicense::GPL_V3);
    about.setHomepage(QStringLiteral("https://invent.kde.org/"));
    KAboutData::setApplicationData(about);

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    QCommandLineOption trayOption(
        {QStringLiteral("t"), QStringLiteral("tray")},
        i18n("Start hidden in the system tray"));
    QCommandLineOption demoOption(
        QStringLiteral("demo"),
        i18n("Run with built-in demo data, no sign-in or network access"));
    parser.addOption(trayOption);
    parser.addOption(demoOption);
    parser.process(app);
    about.processCommandLine(&parser);

    Merkzettel::App controller;
    controller.setStartMinimized(parser.isSet(trayOption));
    controller.setDemoMode(parser.isSet(demoOption));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("appVersion"),
                                             QStringLiteral(MERKZETTEL_VERSION));

    engine.loadFromModule(QStringLiteral("org.kde.merkzettel"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    controller.start();

    return app.exec();
}
