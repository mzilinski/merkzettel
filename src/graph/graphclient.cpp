#include "graphclient.h"
#include "auth/authmanager.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

namespace Merkzettel {

namespace {
constexpr auto kGraphBase = "https://graph.microsoft.com/v1.0";
}

GraphClient::GraphClient(AuthManager *auth, QObject *parent)
    : QObject(parent), m_auth(auth), m_nam(new QNetworkAccessManager(this))
{
}

void GraphClient::get(const QString &path, const QUrlQuery &query, GraphCallback cb)
{
    send(Method::Get, path, query, {}, std::move(cb));
}

void GraphClient::post(const QString &path, const QJsonObject &body, GraphCallback cb)
{
    send(Method::Post, path, {}, body, std::move(cb));
}

void GraphClient::patch(const QString &path, const QJsonObject &body, GraphCallback cb)
{
    send(Method::Patch, path, {}, body, std::move(cb));
}

void GraphClient::del(const QString &path, GraphCallback cb)
{
    send(Method::Delete, path, {}, {}, std::move(cb));
}

void GraphClient::getAbsolute(const QString &absoluteUrl, GraphCallback cb)
{
    sendUrl(Method::Get, QUrl(absoluteUrl), {}, std::move(cb));
}

void GraphClient::send(Method method, const QString &path, const QUrlQuery &query,
                       const QJsonObject &body, GraphCallback cb, int retries)
{
    QUrl url(QString::fromLatin1(kGraphBase) + path);
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    sendUrl(method, url, body, std::move(cb), retries);
}

void GraphClient::sendUrl(Method method, const QUrl &url, const QJsonObject &body,
                          GraphCallback cb, int retries)
{
    QNetworkRequest req(url);
    req.setRawHeader("Authorization",
                     QByteArray("Bearer ") + m_auth->accessToken().toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = nullptr;
    const QByteArray payload = body.isEmpty() ? QByteArray()
                                              : QJsonDocument(body).toJson(QJsonDocument::Compact);

    switch (method) {
    case Method::Get:    reply = m_nam->get(req); break;
    case Method::Post:   reply = m_nam->post(req, payload); break;
    case Method::Patch:  reply = m_nam->sendCustomRequest(req, "PATCH", payload); break;
    case Method::Delete: reply = m_nam->deleteResource(req); break;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, cb, method, url, body, retries] {
        handleReply(reply, cb, method, url, body, retries);
    });
}

void GraphClient::handleReply(QNetworkReply *reply, GraphCallback cb,
                              Method method, const QUrl &url,
                              const QJsonObject &body, int retries)
{
    reply->deleteLater();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();

    if (status == 401 && retries > 0) {
        // Token expired — refresh and retry once.
        connect(m_auth, &AuthManager::accessTokenChanged, this,
                [this, method, url, body, cb] {
                    sendUrl(method, url, body, cb, 0);
                }, Qt::SingleShotConnection);
        m_auth->requestFreshToken();
        return;
    }

    if (status >= 200 && status < 300) {
        if (data.isEmpty()) {
            cb(QJsonValue(), QString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(data);
        cb(doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()), QString());
        return;
    }

    // Always prefix with "HTTP <status>" so callers can detect specific codes
    // (e.g. 410 for expired delta tokens) without changing the callback shape.
    QString errMsg = QStringLiteral("HTTP %1").arg(status);
    const auto errDoc = QJsonDocument::fromJson(data);
    if (errDoc.isObject()) {
        const auto err = errDoc.object().value(QStringLiteral("error")).toObject();
        const QString msg = err.value(QStringLiteral("message")).toString();
        if (!msg.isEmpty()) errMsg = QStringLiteral("HTTP %1: %2").arg(status).arg(msg);
    } else if (reply->error() != QNetworkReply::NoError) {
        errMsg = QStringLiteral("HTTP %1: %2").arg(status).arg(reply->errorString());
    }
    cb(QJsonValue(), errMsg);
}

} // namespace Merkzettel
