/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef VKCALENDARSYNCADAPTOR_H
#define VKCALENDARSYNCADAPTOR_H

#include "vkdatatypesyncadaptor.h"

#include <QJsonObject>

#include <extendedcalendar.h>
#include <extendedstorage.h>

class VKCalendarSyncAdaptor : public VKDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    VKCalendarSyncAdaptor(QObject *parent);
    ~VKCalendarSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId = 0);

protected: // implementing VKDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalCleanup();

private:
    void requestEvents(int accountId, const QString &accessToken, int offset = 0);

private Q_SLOTS:
    void finishedHandler();

private:
    QMap<int, QMap<QString, QJsonObject> > m_eventObjects;
    mKCal::ExtendedCalendar::Ptr m_calendar;
    mKCal::ExtendedStorage::Ptr m_storage;
    bool m_storageNeedsSave;
};

#endif // VKCALENDARSYNCADAPTOR_H
