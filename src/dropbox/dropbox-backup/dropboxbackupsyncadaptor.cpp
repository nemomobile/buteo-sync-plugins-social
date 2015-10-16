/****************************************************************************
 **
 ** Copyright (C) 2015 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ** This program/library is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation.
 **
 ** This program/library is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 ** Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public
 ** License along with this program/library; if not, write to the Free
 ** Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 ** 02110-1301 USA
 **
 ****************************************************************************/

#include "dropboxbackupsyncadaptor.h"
#include "trace.h"

#include <QtCore/QPair>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <Accounts/Manager>
#include <Accounts/Account>

#include <MGConfItem>

static void debugDumpResponse(const QByteArray &data)
{
    QStringList lines = QString::fromUtf8(data).split('\n');
    Q_FOREACH (const QString &line, lines) {
        SOCIALD_LOG_DEBUG(line);
    }
}

DropboxBackupSyncAdaptor::DropboxBackupSyncAdaptor(QObject *parent)
    : DropboxDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Backup, parent)
{
    setInitialActive(true);
}

DropboxBackupSyncAdaptor::~DropboxBackupSyncAdaptor()
{
}

QString DropboxBackupSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("dropbox-backup");
}

void DropboxBackupSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    DropboxDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void DropboxBackupSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode)
{
    purgeAccount(oldId);
}

void DropboxBackupSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    QString defaultRemotePath = QString::fromLatin1("Backups");
    QString defaultLocalPath = QString::fromLatin1("%1/Backups/")
                               .arg(QString::fromLatin1(PRIVILEGED_DATA_DIR));

    // read from dconf some key values, which determine the direction of sync etc.
    MGConfItem localPathConf("/SailfishOS/vault/Dropbox/localPath");
    MGConfItem remotePathConf("/SailfishOS/vault/Dropbox/remotePath");
    MGConfItem remoteFileConf("/SailfishOS/vault/Dropbox/remoteFile");
    MGConfItem directionConf("/SailfishOS/vault/Dropbox/direction");
    QString localPath = localPathConf.value(QString()).toString();
    QString remotePath = remotePathConf.value(QString()).toString();
    QString remoteFile = remoteFileConf.value(QString()).toString();
    QString direction = directionConf.value(QString()).toString();

    // Immediately unset the keys to ensure that future scheduled
    // or manually triggered syncs fail, until the keys are set.
    // Specifically, the value of the direction key is important.
    localPathConf.set(QString());
    remotePathConf.set(QString());
    remoteFileConf.set(QString());
    directionConf.set(QString());

    // set defaults if required.
    if (localPath.isEmpty()) {
        localPath = defaultLocalPath;
    }
    if (remotePath.isEmpty()) {
        remotePath = defaultRemotePath;
    }
    if (!remoteFile.isEmpty()) {
        // dropbox requestData() function takes remoteFile param which has a fully specified path.
        remoteFile = QStringLiteral("%1/%2").arg(remotePath).arg(remoteFile);
    }

    // create local directory if it doesn't exist
    QDir localDir;
    if (!localDir.mkpath(localPath)) {
        SOCIALD_LOG_ERROR("Could not create local backup directory:" << localPath << "for Dropbox account:" << accountId);
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    // either upsync or downsync as required.
    if (direction == Buteo::VALUE_TO_REMOTE) {
        uploadData(accountId, accessToken, localPath, remotePath);
    } else if (direction == Buteo::VALUE_FROM_REMOTE) {
        requestData(accountId, accessToken, localPath, remotePath, remoteFile);
    } else {
        SOCIALD_LOG_ERROR("No direction set for Dropbox Backup sync with account:" << accountId);
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }
}

void DropboxBackupSyncAdaptor::requestData(int accountId, const QString &accessToken,
                                            const QString &localPath, const QString &remotePath,
                                            const QString &remoteFile, const QString &redirectUrl)
{
    // step one: get the remote path and its children metadata.
    // step two: for each (non-folder) child in metadata, download it.
    Q_UNUSED(redirectUrl);

    QUrl url;
    if (remoteFile.isEmpty()) {
        // folder content request
        url = QUrl(QStringLiteral("https://api.dropboxapi.com/1/metadata/auto/%1").arg(remotePath));
        SOCIALD_LOG_DEBUG("performing directory request:" << url.toString());
    } else {
        // file download request
        url = QUrl(QStringLiteral("https://content.dropboxapi.com/1/files/auto/%1").arg(remoteFile));
        SOCIALD_LOG_DEBUG("performing file request:" << url.toString());
    }

    QNetworkRequest req(url);
    req.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                     QString(QLatin1String("Bearer ")).toUtf8() + accessToken.toUtf8());

    QNetworkReply *reply = m_networkAccessManager->get(req);

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("localPath", localPath);
        reply->setProperty("remotePath", remotePath);
        reply->setProperty("remoteFile", remoteFile);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        if (remoteFile.isEmpty()) {
            connect(reply, SIGNAL(finished()), this, SLOT(remotePathFinishedHandler()));
        } else {
            connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(downloadProgressHandler(qint64,qint64)));
            connect(reply, SIGNAL(finished()), this, SLOT(remoteFileFinishedHandler()));
        }

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, reply, 10 * 60 * 1000); // 10 minutes
    } else {
        SOCIALD_LOG_ERROR("unable to create download request:" << remotePath << remoteFile << redirectUrl <<
                          "for Dropbox account with id" << accountId);
    }
}

void DropboxBackupSyncAdaptor::remotePathFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray data = reply->readAll();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QString localPath = reply->property("localPath").toString();
    QString remotePath = reply->property("remotePath").toString();
    bool isError = reply->property("isError").toBool();
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);
    if (isError) {
        SOCIALD_LOG_ERROR("error occurred when performing Backup remote path request for Dropbox account" << accountId);
        debugDumpResponse(data);
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(data, &ok);
    if (!ok || !parsed.contains("contents")) {
        SOCIALD_LOG_ERROR("no backup data exists in reply from Dropbox with account" << accountId);
        debugDumpResponse(data);
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    QJsonArray contents = parsed.value("contents").toArray();
    Q_FOREACH (const QJsonValue &child, contents) {
        const QString childPath = child.toObject().value("path").toString();
        if (child.toObject().value("is_dir").toBool() == true) {
            SOCIALD_LOG_DEBUG("ignoring folder:" << childPath << "under remote backup path:" << remotePath << "for Dropbox account:" << accountId);
        } else {
            SOCIALD_LOG_DEBUG("found remote backup object:" << childPath << "for Dropbox account:" << accountId);
            requestData(accountId, accessToken, localPath, remotePath, childPath);
        }
    }

    decrementSemaphore(accountId);
}

void DropboxBackupSyncAdaptor::remoteFileFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray data = reply->readAll();
    int accountId = reply->property("accountId").toInt();
    QString localPath = reply->property("localPath").toString();
    QString remotePath = reply->property("remotePath").toString();
    QString remoteFile = reply->property("remoteFile").toString();
    bool isError = reply->property("isError").toBool();
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);
    if (isError) {
        SOCIALD_LOG_ERROR("error occurred when performing Backup remote file request for Dropbox account" << accountId);
        debugDumpResponse(data);
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    if (data.isEmpty()) {
        SOCIALD_LOG_INFO("remote file:" << remoteFile << "from" << remotePath << "is empty; ignoring");
    } else {
        const QString filename = QStringLiteral("%1/%2").arg(localPath).arg(remoteFile.split('/').last());
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly)) {
            SOCIALD_LOG_ERROR("could not open" << filename << "locally for writing!");
            setStatus(SocialNetworkSyncAdaptor::Error);
            decrementSemaphore(accountId);
        } else if (!file.write(data)) {
            SOCIALD_LOG_ERROR("could not write data to" << filename << "locally from" <<
                              remotePath << remoteFile << "for Dropbox account:" << accountId);
            setStatus(SocialNetworkSyncAdaptor::Error);
            decrementSemaphore(accountId);
        } else {
            SOCIALD_LOG_DEBUG("successfully wrote" << data.size() << "bytes to:" << filename << "from:" << remoteFile);
        }
        file.close();
    }

    decrementSemaphore(accountId);
}

void DropboxBackupSyncAdaptor::uploadData(int accountId, const QString &accessToken, const QString &localPath, const QString &remotePath, const QString &localFile)
{
    // step one: ensure the remote path exists (and if not, create it)
    // step two: upload every single file from the local path to the remote path.

    QNetworkReply *reply = 0;
    if (localFile.isEmpty()) {
        // attempt to create the remote path directory.
        QUrl url = QUrl(QStringLiteral("https://api.dropboxapi.com/1/fileops/create_folder"));
        QUrlQuery query(url);
        QList<QPair<QString, QString> > queryItems;
        queryItems.append(QPair<QString, QString>(QStringLiteral("root"), QStringLiteral("auto")));
        queryItems.append(QPair<QString, QString>(QStringLiteral("path"), QStringLiteral("Backups")));
        query.setQueryItems(queryItems);
        url.setQuery(query);

        QNetworkRequest req(url);
        req.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                         QString(QLatin1String("Bearer ")).toUtf8() + accessToken.toUtf8());
        SOCIALD_LOG_DEBUG("Attempting to create the remote directory:" << remotePath << "via request:" << url.toString());

        QByteArray data;
        reply = m_networkAccessManager->post(req, data);
    } else {
        // attempt to create a remote file.
        QUrl url = QUrl(QStringLiteral("https://content.dropboxapi.com/1/files_put/auto/%1/%2").arg(remotePath).arg(localFile));
        QUrlQuery query(url);
        QList<QPair<QString, QString> > queryItems;
        queryItems.append(QPair<QString, QString>(QStringLiteral("overwrite"), QStringLiteral("true")));
        query.setQueryItems(queryItems);
        url.setQuery(query);

        QString localFileName = QStringLiteral("%1/%2").arg(localPath).arg(localFile);
        QFile f(localFileName, this);
         if(!f.open(QIODevice::ReadOnly)){
             SOCIALD_LOG_ERROR("unable to open local file:" << localFileName << "for upload to Dropbox Backup with account:" << accountId);
         } else {
             QByteArray data(f.readAll());
             f.close();
             QNetworkRequest req(url);
             req.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                              QString(QLatin1String("Bearer ")).toUtf8() + accessToken.toUtf8());
             req.setHeader(QNetworkRequest::ContentLengthHeader, data.size());
             req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
             SOCIALD_LOG_DEBUG("Attempting to create the remote file:" << QStringLiteral("%1/%2").arg(remotePath).arg(localFile) << "via request:" << url.toString());
             reply = m_networkAccessManager->put(req, data);
        }
    }

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("localPath", localPath);
        reply->setProperty("remotePath", remotePath);
        reply->setProperty("localFile", localFile);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        if (localFile.isEmpty()) {
            connect(reply, SIGNAL(finished()), this, SLOT(createRemotePathFinishedHandler()));
        } else {
            connect(reply, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(uploadProgressHandler(qint64,qint64)));
            connect(reply, SIGNAL(finished()), this, SLOT(createRemoteFileFinishedHandler()));
        }

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, reply, 10 * 60 * 1000); // 10 minutes
    } else {
        SOCIALD_LOG_ERROR("unable to create upload request:" << localPath << localFile << "->" << remotePath <<
                          "for Dropbox account with id" << accountId);
    }
}

void DropboxBackupSyncAdaptor::createRemotePathFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray data = reply->readAll();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QString localPath = reply->property("localPath").toString();
    QString remotePath = reply->property("remotePath").toString();
    bool isError = reply->property("isError").toBool();
    int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);
    if (isError) {
        // we actually expect a conflict error if the folder already existed, which is fine.
        if (httpCode != 403) {
            // this must be a real error.
            SOCIALD_LOG_ERROR("remote path creation failed:" << httpCode);
            debugDumpResponse(data);
            setStatus(SocialNetworkSyncAdaptor::Error);
            decrementSemaphore(accountId);
            return;
        } else {
            SOCIALD_LOG_DEBUG("remote path creation had conflict: already exists:" << remotePath << ".  Continuing.");
        }
    }

    // upload all files from the local path to the remote server.
    SOCIALD_LOG_DEBUG("now uploading files from" << localPath << "to Dropbox folder:" << remotePath);
    QDir dir(localPath);
    QStringList localFiles = dir.entryList(QDir::Files);
    Q_FOREACH (const QString &localFile, localFiles) {
        SOCIALD_LOG_DEBUG("about to upload:" << localFile << "to Dropbox folder:" << remotePath);
        uploadData(accountId, accessToken, localPath, remotePath, localFile);
    }

    decrementSemaphore(accountId);
}

void DropboxBackupSyncAdaptor::createRemoteFileFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray data = reply->readAll();
    int accountId = reply->property("accountId").toInt();
    QString localPath = reply->property("localPath").toString();
    QString remotePath = reply->property("remotePath").toString();
    QString localFile = reply->property("localFile").toString();
    bool isError = reply->property("isError").toBool();
    int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);
    if (isError) {
        SOCIALD_LOG_ERROR("failed to backup file:" << localPath << localFile << "to:" << remotePath <<
                          "for Dropbox account:" << accountId << ", code:" << httpCode);
        debugDumpResponse(data);
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    SOCIALD_LOG_DEBUG("successfully uploaded backup of file:" << localPath << localFile << "to:" << remotePath <<
                      "for Dropbox account:" << accountId);
    decrementSemaphore(accountId);
}

void DropboxBackupSyncAdaptor::downloadProgressHandler(qint64 bytesReceived, qint64 bytesTotal)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString localPath = reply->property("localPath").toString();
    QString remotePath = reply->property("remotePath").toString();
    QString localFile = reply->property("localFile").toString();
    SOCIALD_LOG_DEBUG("Have download progress: bytesReceived:" << bytesReceived <<
                      "of" << bytesTotal << ", for" << localPath << localFile <<
                      "from" << remotePath << "with account:" << accountId);
}

void DropboxBackupSyncAdaptor::uploadProgressHandler(qint64 bytesSent, qint64 bytesTotal)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString localPath = reply->property("localPath").toString();
    QString remotePath = reply->property("remotePath").toString();
    QString localFile = reply->property("localFile").toString();
    SOCIALD_LOG_DEBUG("Have upload progress: bytesSent:" << bytesSent <<
                      "of" << bytesTotal << ", for" << localPath << localFile <<
                      "to" << remotePath << "with account:" << accountId);
}

void DropboxBackupSyncAdaptor::finalize(int accountId)
{
    SOCIALD_LOG_DEBUG("finished Dropbox backup sync for account" << accountId);
}

void DropboxBackupSyncAdaptor::purgeAccount(int)
{
    // TODO: delete the contents of the localPath directory?  probably not, could be shared between dropbox+onedrive
}

void DropboxBackupSyncAdaptor::finalCleanup()
{
    // nothing to do?
}

