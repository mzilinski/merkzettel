#include "graphclient.h"
#include "auth/authmanager.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

namespace Merkzettel {

namespace {
constexpr auto kGraphBase = "https://graph.microsoft.com/v1.0";
constexpr int kMaxConcurrent = 4;       // Outlook/To Do per-mailbox concurrency
constexpr int kMaxThrottleRetries = 5;  // then surface the 429 as an error

// "HTTP <status>[: <Graph error message>]" — callers match on the prefix.
QString errorMessage(int status, const QByteArray &data, QNetworkReply *reply)
{
    QString errMsg = QStringLiteral("HTTP %1").arg(status);
    const auto errDoc = QJsonDocument::fromJson(data);
    if (errDoc.isObject()) {
        const auto err = errDoc.object().value(QStringLiteral("error")).toObject();
        const QString msg = err.value(QStringLiteral("message")).toString();
        if (!msg.isEmpty()) errMsg = QStringLiteral("HTTP %1: %2").arg(status).arg(msg);
    } else if (reply->error() != QNetworkReply::NoError) {
        errMsg = QStringLiteral("HTTP %1: %2").arg(status).arg(reply->errorString());
    }
    return errMsg;
}
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
                          GraphCallback cb, int retries, int throttleAttempt)
{
    const QByteArray payload = body.isEmpty() ? QByteArray()
                                              : QJsonDocument(body).toJson(QJsonDocument::Compact);
    dispatch([this, method, url, payload]() -> QNetworkReply * {
        // Built when the job starts so a token refreshed while it was queued
        // is picked up.
        QNetworkRequest req(url);
        req.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_auth->accessToken().toUtf8());
        req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
        req.setRawHeader("Accept", "application/json");
        switch (method) {
        case Method::Get:    return m_nam->get(req);
        case Method::Post:   return m_nam->post(req, payload);
        case Method::Patch:  return m_nam->sendCustomRequest(req, "PATCH", payload);
        case Method::Delete: return m_nam->deleteResource(req);
        }
        return nullptr;
    }, [this, cb, method, url, body, retries, throttleAttempt](QNetworkReply *reply) {
        handleReply(reply, cb, method, url, body, retries, throttleAttempt);
    });
}

void GraphClient::dispatch(std::function<QNetworkReply *()> start,
                           std::function<void(QNetworkReply *)> finished)
{
    m_queue.append({std::move(start), std::move(finished)});
    pump();
}

void GraphClient::pump()
{
    while (!m_paused && m_inFlight < kMaxConcurrent && !m_queue.isEmpty()) {
        const Job job = m_queue.takeFirst();
        QNetworkReply *reply = job.start();
        if (!reply) continue;
        ++m_inFlight;
        connect(reply, &QNetworkReply::finished, this, [this, reply, finished = job.finished] {
            --m_inFlight;
            finished(reply);
            pump();
        });
    }
}

bool GraphClient::retryIfThrottled(QNetworkReply *reply, int throttleAttempt,
                                   std::function<void()> again)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if ((status != 429 && status != 503) || throttleAttempt >= kMaxThrottleRetries) return false;

    // Retry-After is in seconds; without it back off 2, 4, 8, ... s.
    bool ok = false;
    int seconds = reply->rawHeader("Retry-After").trimmed().toInt(&ok);
    if (!ok || seconds <= 0) seconds = 2 << throttleAttempt;
    seconds = qBound(1, seconds, 120);
    qInfo() << "Graph throttled (HTTP" << status << "), pausing" << seconds << "s";

    // Pause everything, not just this request: other requests would hit the
    // same per-mailbox limit and only extend the throttling.
    again();
    if (!m_paused) {
        m_paused = true;
        QTimer::singleShot(seconds * 1000, this, [this] {
            m_paused = false;
            pump();
        });
    }
    return true;
}

void GraphClient::handleReply(QNetworkReply *reply, GraphCallback cb,
                              Method method, const QUrl &url,
                              const QJsonObject &body, int retries, int throttleAttempt)
{
    reply->deleteLater();

    if (retryIfThrottled(reply, throttleAttempt, [this, method, url, body, cb, retries,
                                                   throttleAttempt] {
            sendUrl(method, url, body, cb, retries, throttleAttempt + 1);
        })) {
        return;
    }

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();

    if (status == 401 && retries > 0) {
        // Token expired — refresh and retry once.
        connect(m_auth, &AuthManager::accessTokenChanged, this,
                [this, method, url, body, cb, throttleAttempt] {
                    sendUrl(method, url, body, cb, 0, throttleAttempt);
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
    cb(QJsonValue(), errorMessage(status, data, reply));
}

void GraphClient::getBytes(const QString &path, GraphBytesCallback cb, int retries,
                           int throttleAttempt)
{
    dispatch([this, path]() -> QNetworkReply * {
        QNetworkRequest req(QUrl(QString::fromLatin1(kGraphBase) + path));
        req.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_auth->accessToken().toUtf8());
        return m_nam->get(req);
    }, [this, path, cb, retries, throttleAttempt](QNetworkReply *reply) {
        reply->deleteLater();
        if (retryIfThrottled(reply, throttleAttempt, [this, path, cb, retries, throttleAttempt] {
                getBytes(path, cb, retries, throttleAttempt + 1);
            })) {
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        if (status == 401 && retries > 0) {
            connect(m_auth, &AuthManager::accessTokenChanged, this,
                    [this, path, cb, throttleAttempt] { getBytes(path, cb, 0, throttleAttempt); },
                    Qt::SingleShotConnection);
            m_auth->requestFreshToken();
            return;
        }
        if (status >= 200 && status < 300) {
            cb(data, QString());
        } else {
            cb(QByteArray(), errorMessage(status, data, reply));
        }
    });
}

void GraphClient::putUploadChunk(const QUrl &uploadUrl, const QByteArray &chunk,
                                 qint64 offset, qint64 totalSize, GraphCallback cb,
                                 int throttleAttempt)
{
    dispatch([this, uploadUrl, chunk, offset, totalSize]() -> QNetworkReply * {
        QNetworkRequest req(uploadUrl);
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      QByteArrayLiteral("application/octet-stream"));
        req.setRawHeader("Content-Range", QStringLiteral("bytes %1-%2/%3")
                                              .arg(offset)
                                              .arg(offset + chunk.size() - 1)
                                              .arg(totalSize)
                                              .toLatin1());
        return m_nam->put(req, chunk);
    }, [this, uploadUrl, chunk, offset, totalSize, cb, throttleAttempt](QNetworkReply *reply) {
        reply->deleteLater();
        if (retryIfThrottled(reply, throttleAttempt,
                             [this, uploadUrl, chunk, offset, totalSize, cb, throttleAttempt] {
                putUploadChunk(uploadUrl, chunk, offset, totalSize, cb, throttleAttempt + 1);
            })) {
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        if (status >= 200 && status < 300) {
            const auto doc = QJsonDocument::fromJson(data);
            cb(doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(), QString());
        } else {
            cb(QJsonValue(), errorMessage(status, data, reply));
        }
    });
}

} // namespace Merkzettel
