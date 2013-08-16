/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Lucien Xu <lucien.xu@jollamobile.com>
 **
 ****************************************************************************/

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
