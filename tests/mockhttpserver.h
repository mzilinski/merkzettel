// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QHash>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <functional>

// Minimal HTTP/1.1 server for unit tests. Every request is answered by the
// handler; the reply can be delayed to keep requests "in flight" so the
// client's concurrency limit becomes observable.
class MockHttpServer : public QTcpServer
{
public:
    struct Request {
        QByteArray method;
        QByteArray path;                       // includes the query string
        QHash<QByteArray, QByteArray> headers; // lower-case names
        QByteArray body;
    };
    struct Response {
        int status = 200;
        QByteArray body = "{}";
        QHash<QByteArray, QByteArray> headers;
        int delayMs = 0;
    };
    using Handler = std::function<Response(const Request &)>;

    explicit MockHttpServer(Handler handler)
        : m_handler(std::move(handler))
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *s = nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, this, [this, s] { onReadyRead(s); });
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });
        listen(QHostAddress::LocalHost);
    }

    QString baseUrl() const { return QStringLiteral("http://127.0.0.1:%1").arg(serverPort()); }
    int requestCount() const { return m_requests; }
    int maxConcurrent() const { return m_maxConcurrent; }

private:
    void onReadyRead(QTcpSocket *s)
    {
        QByteArray &buf = m_buffers[s];
        buf += s->readAll();
        const int headerEnd = buf.indexOf("\r\n\r\n");
        if (headerEnd < 0) return;

        Request req;
        const QList<QByteArray> lines = buf.left(headerEnd).split('\n');
        const QList<QByteArray> start = lines.value(0).trimmed().split(' ');
        req.method = start.value(0);
        req.path = start.value(1);
        for (int i = 1; i < lines.size(); ++i) {
            const int colon = lines[i].indexOf(':');
            if (colon > 0)
                req.headers.insert(lines[i].left(colon).trimmed().toLower(),
                                   lines[i].mid(colon + 1).trimmed());
        }
        const int length = req.headers.value("content-length").toInt();
        if (buf.size() < headerEnd + 4 + length) return; // wait for the body
        req.body = buf.mid(headerEnd + 4, length);
        m_buffers.remove(s);

        ++m_requests;
        ++m_concurrent;
        m_maxConcurrent = qMax(m_maxConcurrent, m_concurrent);
        const Response resp = m_handler(req);
        QPointer<QTcpSocket> guard(s);
        QTimer::singleShot(resp.delayMs, this, [this, guard, resp] {
            --m_concurrent;
            if (!guard) return;
            QByteArray out = "HTTP/1.1 " + QByteArray::number(resp.status) + " X\r\n";
            out += "Content-Type: application/json\r\n";
            out += "Content-Length: " + QByteArray::number(resp.body.size()) + "\r\n";
            out += "Connection: close\r\n";
            for (auto it = resp.headers.cbegin(); it != resp.headers.cend(); ++it)
                out += it.key() + ": " + it.value() + "\r\n";
            out += "\r\n" + resp.body;
            guard->write(out);
            guard->disconnectFromHost();
        });
    }

    Handler m_handler;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    int m_requests = 0;
    int m_concurrent = 0;
    int m_maxConcurrent = 0;
};
