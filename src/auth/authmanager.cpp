#include "authmanager.h"
#include "tokenstore.h"

#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <KLocalizedString>

namespace Merkzettel {

namespace {
constexpr auto kAuthority = "https://login.microsoftonline.com";
constexpr auto kRedirectPath = "/callback";
constexpr quint16 kCallbackPort = 53682;  // must match Azure App Registration redirect URI

QSet<QByteArray> defaultScopes()
{
    return {
        QByteArrayLiteral("openid"),
        QByteArrayLiteral("profile"),
        QByteArrayLiteral("Tasks.ReadWrite"),
        QByteArrayLiteral("User.Read"),
        QByteArrayLiteral("offline_access"),
    };
}
} // namespace

AuthManager::AuthManager(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_flow(new QOAuth2AuthorizationCodeFlow(this))
    , m_handler(nullptr)
    , m_store(new TokenStore(this))
{
    configureFlow();

    connect(m_store, &TokenStore::tokenRead, this, [this](const QString &refresh) {
        m_flow->setRefreshToken(refresh);
        requestFreshToken();
    });
    connect(m_store, &TokenStore::tokenMissing, this, [this] {
        Q_EMIT loggedOut();
    });
    connect(m_store, &TokenStore::error, this, [this](const QString &msg) {
        Q_EMIT authError(msg);
    });
}

AuthManager::~AuthManager() = default;

void AuthManager::configureFlow()
{
    const QString clientId = QString::fromLatin1(MERKZETTEL_CLIENT_ID);
    const QString tenant = QString::fromLatin1(MERKZETTEL_TENANT);
    const QString base = QStringLiteral("%1/%2").arg(QString::fromLatin1(kAuthority), tenant);

    m_flow->setNetworkAccessManager(m_nam);
    m_flow->setClientIdentifier(clientId);
    m_flow->setAuthorizationUrl(QUrl(base + QStringLiteral("/oauth2/v2.0/authorize")));
    m_flow->setTokenUrl(QUrl(base + QStringLiteral("/oauth2/v2.0/token")));
    m_flow->setRequestedScopeTokens(defaultScopes());

    // Qt 6.5+ adds PKCE (S256) automatically. Only push the prompt parameter ourselves.
    m_flow->setModifyParametersFunction(
        [](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant> *params) {
            if (stage == QAbstractOAuth::Stage::RequestingAuthorization) {
                params->insert(QStringLiteral("prompt"), QStringLiteral("select_account"));
            }
        });

    connect(m_flow, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser,
            this, [](const QUrl &url) { QDesktopServices::openUrl(url); });

    connect(m_flow, &QOAuth2AuthorizationCodeFlow::granted, this, &AuthManager::onGranted);
    connect(m_flow, &QAbstractOAuth::requestFailed, this, [this](QAbstractOAuth::Error err) {
        QString name;
        switch (err) {
        case QAbstractOAuth::Error::NetworkError: name = i18n("Network error"); break;
        case QAbstractOAuth::Error::ServerError: name = i18n("Server error"); break;
        case QAbstractOAuth::Error::OAuthTokenNotFoundError: name = i18n("Token not received"); break;
        case QAbstractOAuth::Error::OAuthTokenSecretNotFoundError: name = i18n("Token secret missing"); break;
        case QAbstractOAuth::Error::OAuthCallbackNotVerified: name = i18n("Callback not verified"); break;
        default: name = i18n("Unknown error %1", int(err));
        }
        Q_EMIT authError(name);
    });
}

void AuthManager::start()
{
    m_store->readRefreshToken();
}

void AuthManager::login()
{
    const QString clientId = QString::fromLatin1(MERKZETTEL_CLIENT_ID);
    if (clientId.isEmpty()) {
        Q_EMIT authError(i18n("MERKZETTEL_CLIENT_ID not set — see docs/azure-setup.md"));
        return;
    }

    if (!m_handler) {
        m_handler = new QOAuthHttpServerReplyHandler(this);
        m_handler->setCallbackHost(QStringLiteral("localhost"));
        m_handler->setCallbackPath(QString::fromLatin1(kRedirectPath));
    }
    if (m_handler->isListening() && m_handler->port() != kCallbackPort) {
        m_handler->close();  // wrong port from previous attempt
    }
    if (!m_handler->isListening()) {
        if (!m_handler->listen(QHostAddress::LocalHost, kCallbackPort)) {
            Q_EMIT authError(i18n("Port %1 in use — close other instances or programs", kCallbackPort));
            return;
        }
    }
    qInfo() << "[Auth] Handler listening:" << m_handler->isListening()
            << "port:" << m_handler->port()
            << "callback URL:" << m_handler->callback();
    m_flow->setReplyHandler(m_handler);
    m_flow->grant();
}

void AuthManager::logout()
{
    m_authenticated = false;
    m_flow->setToken(QString());
    m_flow->setRefreshToken(QString());
    m_store->clear();
    Q_EMIT loggedOut();
}

bool AuthManager::isAuthenticated() const { return m_authenticated && !m_flow->token().isEmpty(); }

QString AuthManager::accessToken() const { return m_flow->token(); }

void AuthManager::requestFreshToken()
{
    if (m_flow->refreshToken().isEmpty()) {
        Q_EMIT loggedOut();
        return;
    }
    m_flow->refreshTokens();
}

void AuthManager::onGranted()
{
    m_authenticated = true;
    if (m_flow->expirationAt().isValid()) {
        m_expiresAt = m_flow->expirationAt();
    }
    persistRefreshToken();
    Q_EMIT accessTokenChanged();
    Q_EMIT authenticated();
}

void AuthManager::onError(const QString &error, const QString &description, const QUrl &)
{
    Q_EMIT authError(description.isEmpty() ? error : description);
}

void AuthManager::persistRefreshToken()
{
    const QString rt = m_flow->refreshToken();
    if (!rt.isEmpty()) {
        m_store->writeRefreshToken(rt);
    }
}

} // namespace Merkzettel
