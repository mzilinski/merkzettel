#pragma once

#include <QObject>
#include <QString>
#include <QJsonValue>
#include <QJsonObject>
#include <QUrlQuery>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace Merkzettel {

class AuthManager;

using GraphCallback = std::function<void(const QJsonValue &result, const QString &error)>;

class GraphClient : public QObject
{
    Q_OBJECT
public:
    explicit GraphClient(AuthManager *auth, QObject *parent = nullptr);

    void get(const QString &path, const QUrlQuery &query, GraphCallback cb);
    void post(const QString &path, const QJsonObject &body, GraphCallback cb);
    void patch(const QString &path, const QJsonObject &body, GraphCallback cb);
    void del(const QString &path, GraphCallback cb);

    // GET against an absolute URL. Use this to follow @odata.nextLink and
    // @odata.deltaLink, which Graph returns as fully-qualified URLs that
    // already encode any cursor/token.
    void getAbsolute(const QString &absoluteUrl, GraphCallback cb);

private:
    enum class Method { Get, Post, Patch, Delete };
    void send(Method method, const QString &path, const QUrlQuery &query,
              const QJsonObject &body, GraphCallback cb, int retries = 1);
    void sendUrl(Method method, const QUrl &url, const QJsonObject &body,
                 GraphCallback cb, int retries = 1);
    void handleReply(QNetworkReply *reply, GraphCallback cb,
                     Method method, const QUrl &url,
                     const QJsonObject &body, int retries);

    AuthManager *m_auth;
    QNetworkAccessManager *m_nam;
};

} // namespace Merkzettel
