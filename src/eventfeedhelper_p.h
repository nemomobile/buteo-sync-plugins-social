/*
 * Copyright (C) 2013 Lucien XU <sfietkonstantin@free.fr>
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
 *   * The names of its contributors may not be used to endorse or promote 
 *     products derived from this software without specific prior written 
 *     permission.
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

#ifndef EVENTFEEDHELPER_P_H
#define EVENTFEEDHELPER_P_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>

#include "syncservice.h"

// meegotouchevents/meventfeed
#include <meventfeed.h>

class EventFeedHelper
{
public:
    inline static void manageEvent(const QString &icon, const QString title, const QString &body,
                                   const QStringList &imageList, const QDateTime &createdTime,
                                   const QString &footer, bool isVideo, const QString &url,
                                   const QString &serviceName, const QString &sourceDisplayName,
                                   const QVariantMap &metadata, int accountId,
                                   const QList<int> &accountIds,
                                   const QMap<int, QString> &profileImages,
                                   const QString &localIdentifier, const QString &group,
                                   SyncService::DataType dataType, const QString &postId,
                                   QList<SyncedDatum> &syncedData)
    {
        QString trueLocalIdentifier = localIdentifier;

        QList<int> newAccountIds = accountIds;
        std::sort(newAccountIds.begin(), newAccountIds.end());
        QVariantMap newMetadata = metadata;
        newMetadata.insert("accountIdCount", newAccountIds.count());
        for (int i = 0; i < newAccountIds.count(); i++) {
            newMetadata.insert(QString("accountId%1").arg(i), newAccountIds.at(i));
            newMetadata.insert(QString("profilePicture%1").arg(i),
                               profileImages.value(newAccountIds.at(i)));
        }

        if (localIdentifier.isEmpty()) {
            // Publish the post to the events feed.
            qlonglong eventId = MEventFeed::instance()->addItem(icon, title, body, imageList,
                                                                // Conversion from UTC to local time
                                                                createdTime.toLocalTime(), footer,
                                                                isVideo, url, serviceName,
                                                                sourceDisplayName, newMetadata);
            if (eventId == 0) {
                // failed.
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: failed to publish post/feed event: %1"))
                        .arg(body));
                return;
            } else {
                trueLocalIdentifier = QString::number(eventId);
            }
        } else {
            // Update the post to the event feed
            MEventFeed::instance()->updateItem(localIdentifier.toLongLong(),
                                               icon, title, body, imageList,
                                               // Conversion from UTC to local time
                                               createdTime.toLocalTime(), footer,
                                               isVideo, url, serviceName,
                                               sourceDisplayName, newMetadata);
        }

        if (!trueLocalIdentifier.isEmpty()) {
            // and store the fact that we have synced it to the events feed.
            SyncedDatum datum;
            datum.accountIdentifier = QString::number(accountId);
            datum.localIdentifier = QString(group + trueLocalIdentifier);
            datum.serviceName = serviceName;
            datum.dataType = SyncService::dataType(dataType);
            datum.createdTimestamp = createdTime;
            datum.syncedTimestamp = QDateTime::currentDateTime();
            datum.datumIdentifier = postId;

            syncedData.append(datum);
        }
    }
};


#endif // EVENTFEEDHELPER_P_H
