/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Lucien Xu <lucien.xu@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKCALENDARSYNCADAPTOR_H
#define FACEBOOKCALENDARSYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"

#include <extendedcalendar.h>
#include <extendedstorage.h>

#include <socialcache/facebookcalendardatabase.h>

class FacebookCalendarSyncAdaptor
        : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookCalendarSyncAdaptor(QObject *parent);
    ~FacebookCalendarSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId = 0);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalCleanup();

private:
    void requestEvents(int accountId, const QString &accessToken,
                       const QString &until = QString(), const QString &pagingToken = QString());

private Q_SLOTS:
    void finishedHandler();

private:
    mKCal::ExtendedCalendar::Ptr m_calendar;
    mKCal::ExtendedStorage::Ptr m_storage;
    FacebookCalendarDatabase m_db;
    bool m_storageNeedsSave;
};

#endif // FACEBOOKCALENDARSYNCADAPTOR_H
