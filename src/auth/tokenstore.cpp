#include "tokenstore.h"

#include <qt6keychain/keychain.h>
#include <QDebug>

using namespace QKeychain;

namespace Merkzettel {

QString TokenStore::serviceName() { return QStringLiteral("merkzettel"); }
QString TokenStore::keyName() { return QStringLiteral("refresh_token"); }

TokenStore::TokenStore(QObject *parent) : QObject(parent) {}

void TokenStore::writeRefreshToken(const QString &token)
{
    auto *job = new WritePasswordJob(serviceName(), this);
    job->setKey(keyName());
    job->setTextData(token);
    connect(job, &Job::finished, this, [this, job] {
        if (job->error()) {
            Q_EMIT error(job->errorString());
        }
        job->deleteLater();
    });
    job->start();
}

void TokenStore::readRefreshToken()
{
    auto *job = new ReadPasswordJob(serviceName(), this);
    job->setKey(keyName());
    connect(job, &Job::finished, this, [this, job] {
        if (job->error() == QKeychain::EntryNotFound) {
            Q_EMIT tokenMissing();
        } else if (job->error()) {
            Q_EMIT error(job->errorString());
        } else {
            Q_EMIT tokenRead(job->textData());
        }
        job->deleteLater();
    });
    job->start();
}

void TokenStore::clear()
{
    auto *job = new DeletePasswordJob(serviceName(), this);
    job->setKey(keyName());
    connect(job, &Job::finished, this, [job] { job->deleteLater(); });
    job->start();
}

} // namespace Merkzettel
