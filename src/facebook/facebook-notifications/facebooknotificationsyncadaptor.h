/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKNOTIFICATIONSYNCADAPTOR_H
#define FACEBOOKNOTIFICATIONSYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

class Notification;

class FacebookNotificationSyncAdaptor : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookNotificationSyncAdaptor(QObject *parent);
    ~FacebookNotificationSyncAdaptor();

    QString syncServiceName() const;

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);

private:
    void requestNotifications(int accountId, const QString &accessToken,
                              const QString &until = QString(),
                              const QString &pagingToken = QString());

private Q_SLOTS:
    void finishedHandler();

private:
    Notification *existingNemoNotification(int accountId);
};

#endif // FACEBOOKNOTIFICATIONSYNCADAPTOR_H
