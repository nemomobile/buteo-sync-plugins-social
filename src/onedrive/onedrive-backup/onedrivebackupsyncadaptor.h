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

#ifndef ONEDRIVEBACKUPSYNCADAPTOR_H
#define ONEDRIVEBACKUPSYNCADAPTOR_H

#include "onedrivedatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>
#include <QtSql/QSqlDatabase>

class OneDriveBackupSyncAdaptor : public OneDriveDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    OneDriveBackupSyncAdaptor(QObject *parent);
    ~OneDriveBackupSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId);

protected: // implementing OneDriveDataTypeSyncAdaptor interface
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);
    void finalCleanup();

private:
    void initialiseAppFolderRequest(int accountId, const QString &accessToken,
                                    const QString &localPath, const QString &remotePath,
                                    const QString &remoteFile, const QString &syncDirection);
    void requestData(int accountId, const QString &accessToken,
                     const QString &localPath, const QString &remotePath,
                     const QString &remoteFile = QString(), const QString &redirectUrl = QString());
    void uploadData(int accountId, const QString &accessToken,
                    const QString &localPath, const QString &remotePath,
                    const QString &localFile = QString());
    void purgeAccount(int accountId);

private Q_SLOTS:
    void initialiseAppFolderFinishedHandler();
    void remotePathFinishedHandler();
    void remoteFileFinishedHandler();
    void createRemotePathFinishedHandler();
    void createRemoteFileFinishedHandler();
    void downloadProgressHandler(qint64 bytesReceived, qint64 bytesTotal);
    void uploadProgressHandler(qint64 bytesSent, qint64 bytesTotal);

private:
    QString m_remoteAppDir;
};

#endif // ONEDRIVEBACKUPSYNCADAPTOR_H
