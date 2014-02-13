/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Lucien Xu <lucien.xu@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKCALENDARTYPESYNCADAPTOR_H
#define FACEBOOKCALENDARTYPESYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"
#include "internaldatabasemanipulationinterface.h"

#include <extendedcalendar.h>
#include <extendedstorage.h>

#include <socialcache/facebookcalendardatabase.h>

class FacebookCalendarTypeSyncAdaptor
        : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookCalendarTypeSyncAdaptor(SyncService *syncService, QObject *parent);
    ~FacebookCalendarTypeSyncAdaptor();

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void sync(const QString &dataTypeString);
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

#endif // FACEBOOKCALENDARTYPESYNCADAPTOR_H
