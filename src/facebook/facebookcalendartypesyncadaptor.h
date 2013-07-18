/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKCALENDARTYPESYNCADAPTOR_H
#define FACEBOOKCALENDARTYPESYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"
#include "databasemanipulationinterface.h"

class FacebookCalendarTypeSyncAdaptor
        : public FacebookDataTypeSyncAdaptor
        , private DatabaseManipulationInterface
{
    Q_OBJECT

public:
    FacebookCalendarTypeSyncAdaptor(SyncService *syncService, QObject *parent);
    ~FacebookCalendarTypeSyncAdaptor();

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);

private:
    void requestEvents(int accountId, const QString &accessToken,
                       const QString &until = QString(), const QString &pagingToken = QString());

private Q_SLOTS:
    void finishedHandler();

private:
    bool dbCreateTables();
    bool dbDropTables();

//private:
//    Notification *existingNemoNotification(int accountId);
};

#endif // FACEBOOKCALENDARTYPESYNCADAPTOR_H
