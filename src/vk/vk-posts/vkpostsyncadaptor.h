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
#include <socialcache/socialimagesdatabase.h>

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
    void requestProfiles(int accountId, const QString &accessToken);
    void determineOptimalImageSize();
    QDateTime lastSuccessfulSyncTime(int accountId);
    void setLastSuccessfulSyncTime(int accountId);

private Q_SLOTS:
    void finishedPostsHandler();
    void finishedProfilesHandler();

private:
    void parseVKPostFromObject(int accountId, const QJsonObject &post, const QList<UserProfile> &userProfiles, const QList<GroupProfile> &groupProfiles);
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
    struct ParsedPostData {
        ParsedPostData() : accountId(0) {}
        ParsedPostData(const QString &identifier, const QDateTime &createdTime,
                       const QString &body, const VKPostsDatabase::Post &post,
                       const QList<QPair<QString, SocialPostImage::ImageType> > &images,
                       const QString &posterName,
                       const QString &posterIcon,
                       int accountId)
            : identifier(identifier), createdTime(createdTime)
            , body(body), post(post)
            , images(images), posterName(posterName)
            , posterIcon(posterIcon), accountId(accountId) {}
        QString identifier;
        QDateTime createdTime;
        QString body;
        VKPostsDatabase::Post post;
        QList<QPair<QString, SocialPostImage::ImageType> > images;
        QString posterName;
        QString posterIcon;
        int accountId;
    };

    QList<PostData> m_postsToAdd;
    QList<ParsedPostData> m_parsedPosts;
    VKPostsDatabase m_db;
    QString m_optimalImageSize;
    SocialImagesDatabase m_imageCacheDb;
};

#endif // VKPOSTSYNCADAPTOR_H
