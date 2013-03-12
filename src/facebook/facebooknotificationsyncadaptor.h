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

#ifndef FACEBOOKNOTIFICATIONSYNCADAPTOR_H
#define FACEBOOKNOTIFICATIONSYNCADAPTOR_H

#include "socialnetworksyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

//QtMobility
#include <qmobilityglobal.h>
#include <QtContacts/QContactManager>
#include <QtContacts/QContactAbstractRequest>
#include <QtContacts/QContactFetchRequest>
#include <QtContacts/QContactFetchHint>
#include <QtContacts/QContact>

//libaccounts-qt
#include <Accounts/Manager>

//libsignon-qt
#include <SignOn/SessionData>
#include <SignOn/Error>

class MEventFeed;

QTM_USE_NAMESPACE

class FacebookNotificationSyncAdaptor : public SocialNetworkSyncAdaptor
{
    Q_OBJECT

public:
    FacebookNotificationSyncAdaptor(SyncService *parent = 0);
    ~FacebookNotificationSyncAdaptor();

    void sync(const QString &dataType);

private:
    void checkAccounts(QList<int> *newIds, QList<int> *purgeIds, QList<int> *updateIds);
    void purgeNotifications(const QList<int> &purgeIds);
    void createNotificationGroups(const QList<int> &newIds);
    void updateNotifications(const QList<int> &updateIds);
    void requestNotifications(int accountId, const QString &accessToken,
                              const QString &until = QString(), const QString &pagingToken = QString());
    bool haveAlreadyPostedNotification(const QString &notificationId, const QString &title, const QDateTime &createdTime);
    QContact findMatchingContact(const QString &nameString) const;
    QVariantMap parseReplyData(const QByteArray &replyData, bool *ok);

private Q_SLOTS:
    void signOnError(const SignOn::Error &err);
    void signOnResponse(const SignOn::SessionData &sdata);
    void errorHandler(QNetworkReply::NetworkError err);
    void sslErrorsHandler(const QList<QSslError> &errs);
    void finishedHandler();
    void contactFetchStateChangedHandler(QContactAbstractRequest::State newState);

private:
    QContactManager m_contactManager;
    QList<QContact> m_contacts;
    QContactFetchRequest *m_contactFetchRequest;
    Accounts::Manager *m_accountManager;
    QNetworkAccessManager *m_qnam;
    MEventFeed *m_eventFeed;
};

#endif // FACEBOOKNOTIFICATIONSYNCADAPTOR_H
