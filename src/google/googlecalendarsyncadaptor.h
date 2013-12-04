/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef GOOGLECALENDARSYNCADAPTOR_H
#define GOOGLECALENDARSYNCADAPTOR_H

#include "googledatatypesyncadaptor.h"

#include <QtCore/QString>
#include <QtCore/QMultiMap>
#include <QtCore/QPair>
#include <QtCore/QJsonObject>

class GoogleCalendarSyncAdaptor : public GoogleDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    GoogleCalendarSyncAdaptor(SyncService *syncService, QObject *parent);
    ~GoogleCalendarSyncAdaptor();

protected: // implementing GoogleDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);

private:
    void requestCalendars(int accountId, const QString &accessToken, const QString &pageToken = QString());
    void requestEvents(int accountId, const QString &accessToken, const QString &calendarId, const QString &pageToken = QString());
    void updateLocalCalendarNotebooks(int accountId, const QString &accessToken);
    void updateLocalCalendarNotebookEvents(int accountId, const QString &accessToken, const QString &calendarId);

private Q_SLOTS:
    void calendarsFinishedHandler();
    void eventsFinishedHandler();

private:
    QMap<int, QMap<QString, QPair<QString, QString> > > m_serverCalendarIdToSummaryAndColor;
    QMap<int, QMultiMap<QString, QJsonObject> > m_calendarIdToEventObjects;
};

#endif // GOOGLECALENDARSYNCADAPTOR_H
