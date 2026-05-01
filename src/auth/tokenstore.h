#pragma once

#include <QObject>
#include <QString>

namespace Merkzettel {

class TokenStore : public QObject
{
    Q_OBJECT
public:
    explicit TokenStore(QObject *parent = nullptr);

    void writeRefreshToken(const QString &token);
    void readRefreshToken();
    void clear();

Q_SIGNALS:
    void tokenRead(const QString &token);
    void tokenMissing();
    void error(const QString &message);

private:
    static QString serviceName();
    static QString keyName();
};

} // namespace Merkzettel
