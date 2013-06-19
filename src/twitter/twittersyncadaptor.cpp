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

#include "twittersyncadaptor.h"
#include "twittermentiontimelinesyncadaptor.h"
#include "twitterhometimelinesyncadaptor.h"

#include "syncservice.h"
#include "socialnetworksyncadaptor.h"
#include "trace.h"

//libaccounts-qt
#include <Accounts/Manager>
#include <Accounts/Service>
#include <Accounts/Account>

TwitterSyncAdaptor::TwitterSyncAdaptor(QLatin1String serviceName, SyncService *parent)
    : SocialNetworkSyncAdaptor(serviceName, parent)
{
    m_adaptors.insert(SyncService::dataType(SyncService::Notifications), new TwitterMentionTimelineSyncAdaptor(parent));
    m_adaptors.insert(SyncService::dataType(SyncService::Posts), new TwitterHomeTimelineSyncAdaptor(parent));
    // TODO: Contacts / Calendar / etc.

    // TODO: actually subscribe to account changes and set enabled accordingly
    m_enabled = true;

    // TODO: might not even need the status... it might be useful in the future for reporting, though?
    m_status = SocialNetworkSyncAdaptor::Inactive;
}

TwitterSyncAdaptor::~TwitterSyncAdaptor()
{
}

void TwitterSyncAdaptor::sync(const QString &dataType)
{
    SocialNetworkSyncAdaptor *s = m_adaptors.value(dataType);
    if (s && s->enabled()) {
        if (s->status() == SocialNetworkSyncAdaptor::Inactive) {
            m_status = SocialNetworkSyncAdaptor::Busy;
            s->sync(dataType);
            // TODO: connect to syncFinished() signal, set status back to Inactive once all have finished?
        } else {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("Twitter sync adaptor for %1 is still busy with last sync"))
                    .arg(dataType));
        }
    } else {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("no enabled Twitter sync adaptor for %1"))
                .arg(dataType));
    }
}


