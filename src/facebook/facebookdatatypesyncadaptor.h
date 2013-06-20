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

#ifndef FACEBOOKDATATYPESYNCADAPTOR_H
#define FACEBOOKDATATYPESYNCADAPTOR_H

#include "socialnetworksyncadaptor.h"
#include "syncservice.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

//libaccounts-qt
#include <Accounts/Manager>

//libsignon-qt
#include <SignOn/SessionData>
#include <SignOn/Error>

/*
    Abstract interface for all of the data-specific sync adaptors
    which pull data from the Facebook social network.
*/

class FacebookDataTypeSyncAdaptor : public SocialNetworkSyncAdaptor
{
    Q_OBJECT

public:
    FacebookDataTypeSyncAdaptor(SyncService *syncService, SyncService::DataType dataType, QObject *parent);
    virtual ~FacebookDataTypeSyncAdaptor();
    virtual void sync(const QString &dataType);

protected:
    static QVariantMap parseReplyData(const QByteArray &replyData, bool *ok);
    virtual void updateDataForAccounts(const QList<int> &accountIds);
    virtual void purgeDataForOldAccounts(const QList<int> &oldIds) = 0;    // must at least implement these two
    virtual void beginSync(int accountId, const QString &accessToken) = 0; // must at least implement these two
    SyncService::DataType m_dataType;

protected Q_SLOTS:
    virtual void errorHandler(QNetworkReply::NetworkError err);
    virtual void sslErrorsHandler(const QList<QSslError> &errs);

private Q_SLOTS:
    void signOnError(const SignOn::Error &err);
    void signOnResponse(const SignOn::SessionData &sdata);
};

#endif // FACEBOOKDATATYPESYNCADAPTOR_H
