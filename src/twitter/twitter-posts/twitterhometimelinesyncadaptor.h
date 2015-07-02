/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd.
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

#ifndef TWITTERHOMETIMELINESYNCADAPTOR_H
#define TWITTERHOMETIMELINESYNCADAPTOR_H

#include "twitterdatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

#include <socialcache/twitterpostsdatabase.h>
#include <socialcache/socialimagesdatabase.h>

class TwitterHomeTimelineSyncAdaptor : public TwitterDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    TwitterHomeTimelineSyncAdaptor(QObject *parent);
    ~TwitterHomeTimelineSyncAdaptor();

    QString syncServiceName() const;

protected: // implementing TwitterDataTypeSyncAdaptor interface
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
    void beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret);
    void finalize(int accountId);

private:
    void requestMe(int accountId, const QString &oauthToken, const QString &oauthTokenSecret);
    void requestPosts(int accountId, const QString &oauthToken, const QString &oauthTokenSecret,
                      const QString &sinceId = QString(), const QString &fromUserId = QString());
    bool fromIsSelfContact(const QString &fromName, const QString &fromTwUid) const;

private Q_SLOTS:
    void finishedMeHandler();
    void finishedPostsHandler();

private:
    TwitterPostsDatabase m_db;
    SocialImagesDatabase m_imageCacheDb;
    QMap<int, QString> m_accountProfileImage;
    QStringList m_selfTuids; // twitter user id strings of "me" objects
    QMap<QString, QString> m_selfTScreenNames; // map of user id string to screen name
};

#endif // TWITTERHOMETIMELINESYNCADAPTOR_H
