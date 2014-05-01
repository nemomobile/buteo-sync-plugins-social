/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Bea Lam <bea.lam@jollamobile.com>
 **
 ****************************************************************************/

#ifndef VKPOSTSYNCADAPTOR_H
#define VKPOSTSYNCADAPTOR_H

#include "vkdatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

#include <socialcache/vkpostsdatabase.h>

class VKPostSyncAdaptor : public VKDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    VKPostSyncAdaptor(QObject *parent);
    ~VKPostSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId = 0);

protected: // implementing VKDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private:
    void requestPosts(int accountId, const QString &accessToken);

private Q_SLOTS:
    void finishedPostsHandler();

private:
    QDateTime parseVKDateTime(const QJsonValue &v);
    void saveVKPostFromObject(int accountId, const QJsonObject &post, const QList<UserProfile> &userProfiles);
    VKPostsDatabase m_db;
};

#endif // VKPOSTSYNCADAPTOR_H
