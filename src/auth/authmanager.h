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

    bool isAuthenticated() const;
    QString accessToken() const;

    void requestFreshToken();

Q_SIGNALS:
    void authenticated();
    void authError(const QString &message);
    void loggedOut();
    void accessTokenChanged();

private:
    void configureFlow();
    void onGranted();
    void onError(const QString &error, const QString &description, const QUrl &uri);
    void persistRefreshToken();

    QNetworkAccessManager *m_nam;
    QOAuth2AuthorizationCodeFlow *m_flow;
    QOAuthHttpServerReplyHandler *m_handler;
    TokenStore *m_store;
    QDateTime m_expiresAt;
    bool m_authenticated = false;
};

} // namespace Merkzettel
