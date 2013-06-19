/*
 * Copyright (C) 2013 Jolla Ltd. <chris.adams@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

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

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
//QtMobility
#include <qmobilityglobal.h>
#endif

#include <QtContacts/QContactManager>
#include <QtContacts/QContact>
#include <QtContacts/QContactFetchRequest>

//libsignon-qt
#include <SignOn/SessionData>
#include <SignOn/Error>

class MEventFeed;

USE_CONTACTS_NAMESPACE

class FacebookPostSyncAdaptor : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookPostSyncAdaptor(SyncService *parent);
    ~FacebookPostSyncAdaptor();

    void sync(const QString &dataType);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);

private:
    void requestMe(int accountId, const QString &accessToken);
    void requestPosts(int accountId, const QString &accessToken,
                              const QString &until = QString(), const QString &pagingToken = QString());
    bool haveAlreadyPostedEvent(const QString &postId, const QString &title, const QDateTime &createdTime);
    bool fromIsSelfContact(const QString &fromName, const QString &fromFbUid) const;

private Q_SLOTS:
    void finishedMeHandler();
    void finishedPostsHandler();
    void contactFetchStateChangedHandler(QContactAbstractRequest::State newState);

private:
    QContactManager m_contactManager;
    QList<QContact> m_contacts;
    QContact m_selfContact;
    QContactFetchRequest *m_contactFetchRequest;
    MEventFeed *m_eventFeed;
    QStringList m_selfFbuids; // facebook user id strings of "me" objects

    // for busy/inactive detection.
    void decrementSemaphore(int accountId);
    void incrementSemaphore(int accountId);
    QMap<int, int> m_accountSyncSemaphores;
};

#endif // FACEBOOKPOSTSYNCADAPTOR_H
