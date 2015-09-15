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

#ifndef GOOGLECALENDARSYNCADAPTOR_H
#define GOOGLECALENDARSYNCADAPTOR_H

#include "googledatatypesyncadaptor.h"

#include <QtCore/QString>
#include <QtCore/QMultiMap>
#include <QtCore/QPair>
#include <QtCore/QJsonObject>

#include <extendedcalendar.h>
#include <extendedstorage.h>
#include <icalformat.h>
#include <kdatetime.h>

class GoogleCalendarSyncAdaptor : public GoogleDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    GoogleCalendarSyncAdaptor(QObject *parent);
    ~GoogleCalendarSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId);

protected: // implementing GoogleDataTypeSyncAdaptor interface
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
    void beginSync(int accountId, const QString &accessToken);
    void finalCleanup();

private:
    enum ChangeType {
        NoChange = 0,
        Insert = 1,
        Modify = 2,
        Delete = 3,
        DeleteOccurrence = 4, // used to identify downsynced status->CANCELLED changes only
        CleanSync = 5 // delete followed by insert.
    };

    struct UpsyncChange {
        UpsyncChange() : accountId(0), upsyncType(NoChange) {}
        int accountId;
        QString accessToken;
        ChangeType upsyncType;
        QString kcalEventId;
        KDateTime recurrenceId;
        QString calendarId;
        QString eventId;
        QByteArray eventData;
    };

    void requestCalendars(int accountId, const QString &accessToken,
                          bool needCleanSync, const QString &pageToken = QString());
    void requestEvents(int accountId, const QString &accessToken,
                       const QString &calendarId, bool needCleanSync,
                       const QString &pageToken = QString());
    void updateLocalCalendarNotebooks(int accountId, const QString &accessToken, bool needCleanSync);
    QList<UpsyncChange> determineSyncDelta(int accountId, const QString &accessToken,
                                           const QString &calendarId, const QDateTime &since);
    void upsyncChanges(int accountId, const QString &accessToken,
                       GoogleCalendarSyncAdaptor::ChangeType upsyncType,
                       const QString &kcalEventId, const KDateTime &recurrenceId, const QString &calendarId,
                       const QString &eventId,const QByteArray &eventData);

    void applyRemoteChangesLocally(int accountId);
    void updateLocalCalendarNotebookEvents(int accountId, const QString &calendarId);

    mKCal::Notebook::Ptr notebookForCalendarId(int accountId, const QString &calendarId) const;
    void finishedRequestingRemoteEvents(int accountId, const QString &accessToken, const QString &calendarId, const QDateTime &since, const QString &updateTimestampStr);

private Q_SLOTS:
    void calendarsFinishedHandler();
    void eventsFinishedHandler();
    void upsyncFinishedHandler();

private:
    struct CalendarInfo {
        CalendarInfo() : change(NoChange) {}
        QString summary;
        QString description;
        QString color;
        ChangeType change;
    };
    QMap<int, QMap<QString, CalendarInfo> > m_serverCalendarIdToCalendarInfo;
    QMap<int, QMap<QString, int> > m_serverCalendarIdToDefaultReminderTimes;
    QMap<int, QMultiMap<QString, QJsonObject> > m_calendarIdToEventObjects;
    QMap<int, QMap<QString, QString> > m_recurringEventIdToKCalUid;
    QMap<int, bool> m_syncSucceeded;
    QMap<int, QDateTime> m_prevSinceTimestamp;
    QMap<int, QDateTime> m_newSinceTimestamp;

    QStringList m_calendarsBeingRequested;               // calendarIds
    QMap<QString, QString> m_calendarsFinishedRequested; // calendarId to updated timestamp string
    QMultiMap<QString, QPair<GoogleCalendarSyncAdaptor::ChangeType, QJsonObject> > m_changesFromDownsync; // calendarId to change
    QMultiMap<QString, QPair<KCalCore::Event::Ptr, QJsonObject> > m_changesFromUpsync; // calendarId to event+upsyncResponse

    mKCal::ExtendedCalendar::Ptr m_calendar;
    mKCal::ExtendedStorage::Ptr m_storage;
    mutable KCalCore::ICalFormat m_icalFormat;
    bool m_storageNeedsSave;
    QDateTime m_originalLastSyncTimestamp;
};

#endif // GOOGLECALENDARSYNCADAPTOR_H
