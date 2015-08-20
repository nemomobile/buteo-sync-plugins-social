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
#include <QtCore/QJsonObject>
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
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private:
    void requestPosts(int accountId, const QString &accessToken);
    void determineOptimalImageSize();
    QDateTime lastSuccessfulSyncTime(int accountId);
    void setLastSuccessfulSyncTime(int accountId);

private Q_SLOTS:
    void finishedPostsHandler();

private:
    void saveVKPostFromObject(int accountId, const QJsonObject &post, const QList<UserProfile> &userProfiles, const QList<GroupProfile> &groupProfiles);
    struct PostData {
        PostData() : accountId(0) {}
        PostData(int accountId, const QJsonObject &object,
                 const QList<UserProfile> &userProfiles, const QList<GroupProfile> &groupProfiles)
            : accountId(accountId), post(object)
            , userProfiles(userProfiles), groupProfiles(groupProfiles) {}
        int accountId;
        QJsonObject post;
        QList<UserProfile> userProfiles;
        QList<GroupProfile> groupProfiles;
    };
    QList<PostData> m_postsToAdd;
    VKPostsDatabase m_db;
    QString m_optimalImageSize;
};

#endif // VKPOSTSYNCADAPTOR_H
