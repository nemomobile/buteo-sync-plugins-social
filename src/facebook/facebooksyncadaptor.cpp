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

#include "facebooksyncadaptor.h"
#include "facebooknotificationsyncadaptor.h"
#include "facebookimagesyncadaptor.h"

#include "syncservice.h"
#include "socialnetworksyncadaptor.h"
#include "trace.h"

//libaccounts-qt
#include <Accounts/Manager>
#include <Accounts/Service>
#include <Accounts/Account>

FacebookSyncAdaptor::FacebookSyncAdaptor(SyncService *parent)
    : SocialNetworkSyncAdaptor(parent)
    , m_accountManager(new Accounts::Manager(QLatin1String("sync"), this))
    , m_qnam(new QNetworkAccessManager(this))
{
    m_adaptors.insert(SyncService::dataType(SyncService::Notifications), new FacebookNotificationSyncAdaptor(parent, this));
    m_adaptors.insert(SyncService::dataType(SyncService::Images), new FacebookImageSyncAdaptor(parent, this));
    // TODO: Contacts / Calendar / etc.

    // TODO: actually subscribe to account changes and set enabled accordingly
    m_enabled = true;

    // TODO: might not even need the status... it might be useful in the future for reporting, though?
    m_status = SocialNetworkSyncAdaptor::Inactive;
}

FacebookSyncAdaptor::~FacebookSyncAdaptor()
{
}

void FacebookSyncAdaptor::sync(const QString &dataType)
{
    SocialNetworkSyncAdaptor *s = m_adaptors.value(dataType);
    if (s && s->enabled()) {
        if (s->status() == SocialNetworkSyncAdaptor::Inactive) {
            m_status = SocialNetworkSyncAdaptor::Busy;
            s->sync(dataType);
            // TODO: connect to syncFinished() signal, set status back to Inactive once all have finished?
        } else {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("facebook sync adaptor for %1 is still busy with last sync"))
                    .arg(dataType));
        }
    } else {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("no enabled facebook sync adaptor for %1"))
                .arg(dataType));
    }
}

void FacebookSyncAdaptor::checkAccounts(SyncService::DataType dataType, QList<int> *newIds, QList<int> *purgeIds, QList<int> *updateIds)
{
    QList<int> knownIds;
    QStringList knownIdStrings = accountIdsWithSyncTimestamp(QLatin1String("facebook"), SyncService::dataType(dataType));
    foreach (const QString &kis, knownIdStrings) {
        // XXX TODO: instead of QString::number(accountId) use fb user id.
        bool ok = true;
        int intId = kis.toInt(&ok);
        if (ok) {
            knownIds.append(intId);
        } else {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: unable to convert known id string to int: %1"))
                    .arg(kis));
        }
    }

    Accounts::AccountIdList currentIds = m_accountManager->accountList();
    TRACE(SOCIALD_DEBUG,
            QString(QLatin1String("have found %1 accounts which support a sync service; determining old/new/update sets..."))
            .arg(currentIds.size()));
    for (int i = 0; i < currentIds.size(); ++i) {
        int currId = currentIds.at(i);
        Accounts::Account *act = m_accountManager->account(currId);
        if (!act || act->providerName() != QLatin1String("facebook")) {
            continue; // not a facebook account.  Ignore it.
        }

        if (knownIds.contains(currId)) {
            knownIds.removeOne(currId);
            updateIds->append(currId);
        } else {
            newIds->append(currId);
        }
    }

    // anything left in knownIds must belong to an old, removed account.
    for (int i = 0; i < knownIds.size(); ++i) {
        purgeIds->append(knownIds.at(i));
    }
}
