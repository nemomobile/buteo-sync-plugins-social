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

#include "onedrivebackupsyncadaptor.h"
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
    QString alldata = QString::fromUtf8(data);
    QStringList alldatasplit = alldata.split('\n');
    Q_FOREACH (const QString &s, alldatasplit) {
        SOCIALD_LOG_DEBUG(s);
    }
}

OneDriveBackupSyncAdaptor::OneDriveBackupSyncAdaptor(QObject *parent)
    : OneDriveDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Backup, parent)
    , m_remoteAppDir(QStringLiteral("drive/special/approot"))
{
    setInitialActive(true);
}

OneDriveBackupSyncAdaptor::~OneDriveBackupSyncAdaptor()
{
}

QString OneDriveBackupSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("onedrive-backup");
}

void OneDriveBackupSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    OneDriveDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void OneDriveBackupSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode)
{
    purgeAccount(oldId);
}

void OneDriveBackupSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    QString defaultRemotePath = QString::fromLatin1("backups");
    QString defaultLocalPath = QString::fromLatin1("%1/Backups/")
                               .arg(QString::fromLatin1(PRIVILEGED_DATA_DIR));

    // read from dconf some key values, which determine the direction of sync etc.
    MGConfItem localPathConf("/SailfishOS/vault/OneDrive/localPath");
    MGConfItem remotePathConf("/SailfishOS/vault/OneDrive/remotePath");
    MGConfItem directionConf("/SailfishOS/vault/OneDrive/direction");
    QString localPath = localPathConf.value(QString()).toString();
    QString remotePath = remotePathConf.value(QString()).toString();
    QString direction = directionConf.value(QString()).toString();
    if (localPath.isEmpty()) {
        localPath = defaultLocalPath;
    }
    if (remotePath.isEmpty()) {
        remotePath = defaultRemotePath;
    }

    // create local directory if it doesn't exist
    QDir localDir;
    if (!localDir.mkpath(localPath)) {
        SOCIALD_LOG_ERROR("Could not create local backup directory:" << localPath << "for OneDrive account:" << accountId);
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    // either upsync or downsync as required.
    if (direction == Buteo::VALUE_TO_REMOTE) {
        uploadData(accountId, accessToken, localPath, remotePath);
    } else if (direction == Buteo::VALUE_FROM_REMOTE) {
        requestData(accountId, accessToken, localPath, remotePath);
    } else {
        SOCIALD_LOG_ERROR("No direction set for OneDrive Backup sync with account:" << accountId);
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }
}

void OneDriveBackupSyncAdaptor::requestData(int accountId, const QString &accessToken,
                                            const QString &localPath, const QString &remotePath,
                                            const QString &remoteFile, const QString &redirectUrl)
{
    // step one: get the remote path and its children metadata.
    // step two: for each (non-folder) child in metadata, download it.

    QUrl url;
    if (accessToken.isEmpty()) {
        // content request to a temporary URL, since it doesn't require access token.
        url = QUrl(redirectUrl);
    } else {
        // directory or file info request.  We use the path and sign with access token.
        if (remoteFile.isEmpty()) {
            // directory request.  expand the children.
            url = QUrl(QStringLiteral("https://api.onedrive.com/v1.0/%1:/%2:/").arg(m_remoteAppDir).arg(remotePath));
            QUrlQuery query(url);
            QList<QPair<QString, QString> > queryItems;
            queryItems.append(QPair<QString, QString>(QStringLiteral("expand"), QStringLiteral("children")));
            query.setQueryItems(queryItems);
            url.setQuery(query);
        } else {
            // file request, download its metadata.  That will contain a content URL which we will redirect to.
            url = QUrl(QStringLiteral("https://api.onedrive.com/v1.0/%1:/%2/%3").arg(m_remoteAppDir).arg(remotePath).arg(remoteFile));
        }
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
        reply->setProperty("redirectUrl", redirectUrl);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        if (remoteFile.isEmpty()) {
            connect(reply, SIGNAL(finished()), this, SLOT(remotePathFinishedHandler()));
        } else {
            connect(reply, SIGNAL(finished()), this, SLOT(remoteFileFinishedHandler()));
        }

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, reply);
    } else {
        SOCIALD_LOG_ERROR("unable to create download request:" << remotePath << remoteFile << redirectUrl <<
                          "for OneDrive account with id" << accountId);
    }
}

void OneDriveBackupSyncAdaptor::remotePathFinishedHandler()
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
        SOCIALD_LOG_ERROR("error occurred when performing Backup remote path request for OneDrive account" << accountId << ":");
        debugDumpResponse(data);
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(data, &ok);
    if (!ok || !parsed.contains("children")) {
        SOCIALD_LOG_ERROR("no backup data exists in reply from OneDrive with account" << accountId << ", got:");
        debugDumpResponse(data);
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    QJsonArray children = parsed.value("children").toArray();
    Q_FOREACH (const QJsonValue &child, children) {
        const QString childName = child.toObject().value("name").toString();
        if (child.toObject().keys().contains("folder")) {
            SOCIALD_LOG_DEBUG("ignoring folder:" << childName << "under remote backup path:" << remotePath << "for OneDrive account:" << accountId);
        } else {
            SOCIALD_LOG_DEBUG("found remote backup object:" << childName << "for OneDrive account:" << accountId);
            requestData(accountId, accessToken, localPath, remotePath, childName);
        }
    }

    decrementSemaphore(accountId);
}

void OneDriveBackupSyncAdaptor::remoteFileFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray data = reply->readAll();
    int accountId = reply->property("accountId").toInt();
    QString localPath = reply->property("localPath").toString();
    QString remotePath = reply->property("remotePath").toString();
    QString remoteFile = reply->property("remoteFile").toString();
    QString redirectUrl = reply->property("redirectUrl").toString();
    bool isError = reply->property("isError").toBool();
    QString remoteFileName = QStringLiteral("%1/%2").arg(remotePath).arg(remoteFile);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);
    if (isError) {
        SOCIALD_LOG_ERROR("error occurred when performing Backup remote file request for OneDrive account" << accountId << ", got:");
        debugDumpResponse(data);
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    // if it was a file metadata request, then parse the content url location and redirect to that.
    // otherwise it was a file content request and we should save the data.
    if (redirectUrl.isEmpty()) {
        // we expect to be redirected from the file path to a temporary url to GET content/data.
        // note: no access token is required to access the content redirect url.
        bool ok = false;
        QJsonObject parsed = parseJsonObjectReplyData(data, &ok);
        if (!ok || !parsed.contains("@content.downloadUrl")) {
            SOCIALD_LOG_ERROR("no content redirect url exists in file metadata for file:" << remoteFile);
            debugDumpResponse(data);
            setStatus(SocialNetworkSyncAdaptor::Error);
            decrementSemaphore(accountId);
            return;
        }
        redirectUrl = parsed.value("@content.downloadUrl").toString();
        SOCIALD_LOG_DEBUG("redirected from:" << remoteFileName << "to:" << redirectUrl);
        requestData(accountId, QString(), localPath, remotePath, remoteFile, redirectUrl);
    } else {
        if (data.isEmpty()) {
            SOCIALD_LOG_INFO("remote file:" << remoteFileName << "is empty; ignoring");
        } else {
            const QString filename = QStringLiteral("%1/%2").arg(localPath).arg(remoteFile);
            QFile file(filename);
            file.open(QIODevice::WriteOnly); // TODO: error checking
            file.write(data);
            file.close();
            SOCIALD_LOG_DEBUG("successfully wrote" << data.size() << "bytes to:" << filename << "from:" << remoteFileName);
        }
    }

    decrementSemaphore(accountId);
}

void OneDriveBackupSyncAdaptor::uploadData(int accountId, const QString &accessToken, const QString &localPath, const QString &remotePath, const QString &localFile)
{
    // step one: ensure the remote path exists (and if not, create it)
    // step two: upload every single file from the local path to the remote path.

    QNetworkReply *reply = 0;
    if (localFile.isEmpty()) {
        // attempt to create the remote path directory.
        QString createFolderJson = QStringLiteral(
            "{"
                "\"name\": \"%1\","
                "\"folder\": { }"
            "}").arg(remotePath);
        QByteArray data = createFolderJson.toUtf8();

        QUrl url = QUrl(QStringLiteral("https://api.onedrive.com/v1.0/%1/children").arg(m_remoteAppDir));
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentLengthHeader, data.size());
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QVariant::fromValue<QString>(QString::fromLatin1("application/json")));
        request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                             QString(QLatin1String("Bearer ")).toUtf8() + accessToken.toUtf8());

        reply = m_networkAccessManager->post(request, data);
    } else {
        // attempt to create a remote file.
        QUrl url = QUrl(QStringLiteral("https://api.onedrive.com/v1.0/%1:/%2/%3:/content").arg(m_remoteAppDir).arg(remotePath).arg(localFile));
        QString localFileName = QStringLiteral("%1/%2").arg(localPath).arg(localFile);
        QFile f(localFileName, this);
         if(!f.open(QIODevice::ReadOnly)){
             SOCIALD_LOG_ERROR("unable to open local file:" << localFileName << "for upload to OneDrive Backup with account:" << accountId);
         } else {
             QByteArray data(f.readAll());
             f.close();
             QNetworkRequest req(url);
             req.setHeader(QNetworkRequest::ContentLengthHeader, data.size());
             req.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                              QString(QLatin1String("Bearer ")).toUtf8() + accessToken.toUtf8());
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
            connect(reply, SIGNAL(finished()), this, SLOT(createRemoteFileFinishedHandler()));
        }

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, reply);
    } else {
        SOCIALD_LOG_ERROR("unable to create upload request:" << localPath << localFile << "->" << remotePath <<
                          "for OneDrive account with id" << accountId);
    }
}

void OneDriveBackupSyncAdaptor::createRemotePathFinishedHandler()
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
        if (httpCode != 409) {
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
    SOCIALD_LOG_DEBUG("remote path now exists, attempting to upload local files");
    QDir dir(localPath);
    QStringList localFiles = dir.entryList(QDir::Files);
    Q_FOREACH (const QString &localFile, localFiles) {
        SOCIALD_LOG_DEBUG("uploading file:" << localFile << "from" << localPath << "to:" << remotePath);
        uploadData(accountId, accessToken, localPath, remotePath, localFile);
    }

    decrementSemaphore(accountId);
}

void OneDriveBackupSyncAdaptor::createRemoteFileFinishedHandler()
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
                          "for OneDrive account:" << accountId << ", code:" << httpCode);
        debugDumpResponse(data);
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    SOCIALD_LOG_DEBUG("successfully uploaded backup of file:" << localPath << localFile << "to:" << remotePath <<
                      "for OneDrive account:" << accountId);
    decrementSemaphore(accountId);
}

void OneDriveBackupSyncAdaptor::finalize(int accountId)
{
    SOCIALD_LOG_DEBUG("finished OneDrive backup sync for account" << accountId);
}

void OneDriveBackupSyncAdaptor::purgeAccount(int)
{
    // TODO: delete the contents of the localPath directory?  probably not, could be shared between onedrive+dropbox
}

void OneDriveBackupSyncAdaptor::finalCleanup()
{
    // nothing to do?
}

