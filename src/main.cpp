#include <QApplication>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KAboutData>
#include <KDBusService>
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
// Builds before the app-ID change ran with organizationName "KDE", which put
// the cache under ~/.local/share/KDE/merkzettel and ~/.cache/KDE/merkzettel and
// the share-explainer flag into ~/.config/KDE/merkzettel.conf. Move them once.
void migrateLegacyLocations()
{
    const struct {
        QStandardPaths::StandardLocation generic;
        QStandardPaths::StandardLocation app;
    } dirs[] = {
        {QStandardPaths::GenericDataLocation, QStandardPaths::AppDataLocation},
        {QStandardPaths::GenericCacheLocation, QStandardPaths::CacheLocation},
    };
    for (const auto &d : dirs) {
        const QString legacy = QStandardPaths::writableLocation(d.generic)
                             + QStringLiteral("/KDE/merkzettel");
        const QString current = QStandardPaths::writableLocation(d.app);
        if (QFileInfo::exists(legacy) && !QFileInfo::exists(current)) {
            QDir().mkpath(QFileInfo(current).absolutePath());
            QDir().rename(legacy, current);
        }
    }

    const QString legacyConf = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                             + QStringLiteral("/KDE/merkzettel.conf");
    if (QFileInfo::exists(legacyConf)) {
        QSettings legacy(legacyConf, QSettings::IniFormat);
        if (legacy.value(QStringLiteral("Share/explainerSeen")).toBool()) {
            KConfigGroup group = KSharedConfig::openConfig()->group(QStringLiteral("Share"));
            group.writeEntry("explainerSeen", true);
            group.sync();
        }
        QFile::remove(legacyConf);
    }
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // No organization name: data goes to ~/.local/share/merkzettel and
    // ~/.cache/merkzettel. The domain makes KDBusService register
    // io.github.mzilinski.merkzettel, matching the app ID (D1).
    QApplication::setOrganizationDomain(QStringLiteral("mzilinski.github.io"));
    QApplication::setApplicationName(QStringLiteral("merkzettel"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.mzilinski.merkzettel")));
    migrateLegacyLocations();
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
    about.setLicense(KAboutLicense::GPL_V3, KAboutLicense::OrLaterVersions);
    about.setCopyrightStatement(QStringLiteral("© 2026 Malte Zilinski"));
    about.setHomepage(QStringLiteral("https://mzilinski.github.io/merkzettel/"));
    about.setBugAddress(QByteArrayLiteral("https://github.com/mzilinski/merkzettel/issues"));
    about.setOrganizationDomain(QByteArrayLiteral("mzilinski.github.io"));
    about.setDesktopFileName(QStringLiteral("io.github.mzilinski.merkzettel"));
    about.addAuthor(QStringLiteral("Malte Zilinski"), i18n("Author"),
                    QStringLiteral("malte@zilinski.eu"));
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

    // Single-instance: a second `merkzettel` invocation hands off to the
    // running process via D-Bus instead of spawning another tray icon, sync
    // loop, and SQLite writer (the cache + delta-link + token refresh would
    // race). The running instance raises its window in response.
    KDBusService service(KDBusService::Unique);

    Merkzettel::App controller;
    controller.setStartMinimized(parser.isSet(trayOption));
    controller.setDemoMode(parser.isSet(demoOption));

    QObject::connect(&service, &KDBusService::activateRequested,
                     &controller, [&controller](const QStringList &, const QString &) {
                         controller.activateWindow();
                     });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("appVersion"),
                                             QStringLiteral(MERKZETTEL_VERSION));

    engine.loadFromModule(QStringLiteral("io.github.mzilinski.merkzettel"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    controller.start();

    return app.exec();
}
