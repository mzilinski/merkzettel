#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>

class QOAuth2AuthorizationCodeFlow;
class QOAuthHttpServerReplyHandler;
class QNetworkAccessManager;

namespace Merkzettel {

class TokenStore;

class AuthManager : public QObject
{
    Q_OBJECT
public:
    explicit AuthManager(QObject *parent = nullptr);
    ~AuthManager() override;

    void start();
    void login();
    void logout();

    // A valid access token is available (requests can go out).
    bool isAuthenticated() const;
    // A refresh token is stored: the user is signed in even while offline,
    // the UI shows cached data and sync resumes once a refresh succeeds.
    bool hasSession() const;
    QString accessToken() const;

    void requestFreshToken();

Q_SIGNALS:
    void authenticated();
    void authError(const QString &message);
    void loggedOut();
    void accessTokenChanged();
    // Refresh token loaded from the keychain at startup (before any network).
    void sessionRestored();
    // A token refresh failed. sessionEnded: the refresh token was rejected
    // (revoked/expired) and has been deleted; otherwise it was a transient
    // (network/server) error and a later retry may succeed.
    void refreshFailed(const QString &message, bool sessionEnded);

private:
    void configureFlow();
    void onGranted();
    void onError(const QString &error, const QString &description, const QUrl &uri);
    void persistRefreshToken();

    QNetworkAccessManager *m_nam;
    QOAuth2AuthorizationCodeFlow *m_flow;
    QOAuthHttpServerReplyHandler *m_handler;
    TokenStore *m_store;
    void endSession(const QString &message);

    QDateTime m_expiresAt;
    bool m_authenticated = false;
    bool m_refreshing = false;
    bool m_sessionJustEnded = false;
};

} // namespace Merkzettel
