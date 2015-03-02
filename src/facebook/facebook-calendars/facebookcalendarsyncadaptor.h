/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd.
 ** Contact: Lucien Xu <lucien.xu@jollamobile.com>
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

#ifndef FACEBOOKCALENDARSYNCADAPTOR_H
#define FACEBOOKCALENDARSYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"

#include <extendedcalendar.h>
#include <extendedstorage.h>

#include <socialcache/facebookcalendardatabase.h>

class FacebookParsedEvent
{
public:
    FacebookParsedEvent();
    FacebookParsedEvent(const FacebookParsedEvent &e);

public:
    QString m_id;
    bool m_isDateOnly;
    bool m_endExists;
    KDateTime m_startTime;
    KDateTime m_endTime;
    QString m_summary;
    QString m_description;
    QString m_location;
};

class FacebookCalendarSyncAdaptor
        : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookCalendarSyncAdaptor(QObject *parent);
    ~FacebookCalendarSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
    void beginSync(int accountId, const QString &accessToken);
    void finalCleanup();

private:
    void requestEvents(int accountId, const QString &accessToken,
                       const QString &batchRequest = QString());
    void processParsedEvents(int accountId);

private Q_SLOTS:
    void finishedHandler();

private:
    mKCal::ExtendedCalendar::Ptr m_calendar;
    mKCal::ExtendedStorage::Ptr m_storage;
    FacebookCalendarDatabase m_db;
    bool m_storageNeedsSave;
    QMap<QString, FacebookParsedEvent> m_parsedEvents;
};

#endif // FACEBOOKCALENDARSYNCADAPTOR_H
