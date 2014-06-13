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

#ifndef FACEBOOKPOSTSYNCADAPTOR_H
#define FACEBOOKPOSTSYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

#include <QtContacts/QContactManager>
#include <QtContacts/QContact>
#include <QtContacts/QContactFetchRequest>

#include <socialcache/facebookpostsdatabase.h>

USE_CONTACTS_NAMESPACE

class FacebookPostSyncAdaptor : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookPostSyncAdaptor(QObject *parent);
    ~FacebookPostSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId = 0);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private:
    void requestMe(int accountId, const QString &accessToken);
    void requestPosts(int accountId, const QString &accessToken);
    bool fromIsSelfContact(const QString &fromName, const QString &fromFbUid) const;

private Q_SLOTS:
    void finishedMeHandler();
    void finishedPostsHandler();

private:
    FacebookPostsDatabase m_db;
    QContactManager *m_contactManager;
    QContact m_selfContact;
    QMap<int, QString> m_selfFacebookUserIds;
};

#endif // FACEBOOKPOSTSYNCADAPTOR_H
