/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef SYNCSERVICE_H
#define SYNCSERVICE_H

#include <QtCore/QObject>
#include <QtDBus/QDBusContext>

#include <QtCore/QStringList>
#include <QtCore/QString>
#include <QtCore/QMap>

class QSqlDatabase;
class SocialNetworkSyncAdaptor;

class SyncServicePrivate;
class SyncService : public QObject
{
    Q_OBJECT

public:
    enum DataType {
        Contacts = 1,   // "Contacts"
        Calendars,      // "Calendars"
        Notifications,  // "Notifications"
        Images,         // "Images"
        Videos,         // "Videos"
        Posts,          // "Posts"
        Messages,       // "Messages"
        Emails          // "Emails"
    };
    static QStringList validDataTypes();
    static QString dataType(DataType t);

public:
    SyncService(const QString &serviceName, QObject *parent = 0);
    ~SyncService();

    QStringList supportedSocialServices() const; // the services for which we have written a SocialNetworkSyncAdapter
    QStringList supportedDataTypes(const QString &socialService) const; // the sync data types supported for the service.

    SocialNetworkSyncAdaptor *createAdaptor(const QString &socialService, const QString &dataType, QObject *parent);

private:
    QSqlDatabase *database() const;
    friend class SocialNetworkSyncAdaptor;

private:
    SyncServicePrivate *d;
    friend class SyncServicePrivate;
};

#endif // SYNCSERVICE_H
