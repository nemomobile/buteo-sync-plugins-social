/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "syncservice.h"
#include "syncservice_p.h"
#include "trace.h"

#include "twitter/twitterhometimelinesyncadaptor.h"
#include "twitter/twittermentiontimelinesyncadaptor.h"
#include "facebook/facebookcontactsyncadaptor.h"
#include "facebook/facebookimagesyncadaptor.h"
#include "facebook/facebooknotificationsyncadaptor.h"
#include "facebook/facebookpostsyncadaptor.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

SyncServicePrivate::SyncServicePrivate(const QString &connectionName, SyncService *parent)
    : QObject(parent), q(parent)
{
    if (!QFile::exists(QString("%1/%2").arg(QLatin1String(SOCIALD_DATABASE_DIR)).arg(QLatin1String(SOCIALD_DATABASE_NAME)))) {
        QDir dir(SOCIALD_DATABASE_DIR);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        QString absolutePath = dir.absoluteFilePath(SOCIALD_DATABASE_NAME);
        QFile dbfile(absolutePath);
        if (!dbfile.open(QIODevice::ReadWrite)) {
            TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to create sociald database %1 - sociald will be inactive"))
                .arg(absolutePath));
            return;
        }
        dbfile.close();
    }

    // open the database in which we store our sync event information
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_db.setDatabaseName(QString("%1/%2").arg(QLatin1String(SOCIALD_DATABASE_DIR)).arg(QLatin1String(SOCIALD_DATABASE_NAME)));
    if (!m_db.open()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: unable to open sociald database %1 - sociald will be inactive"))
            .arg(QLatin1String(SOCIALD_DATABASE_NAME)));
        return;
    }

    // create the sociald db tables
    // syncedData = service, accountIdentifier, dataType, createdTimestamp, syncTimestamp, datumIdentifier
    // syncTimestamps = service, accountIdentifier, dataType, syncTimestamp
    QSqlQuery query(m_db);
    query.prepare( "CREATE TABLE IF NOT EXISTS syncedData (id VARCHAR(50) PRIMARY KEY, serviceName VARCHAR(20), accountIdentifier VARCHAR(50), dataType VARCHAR(16), createdTimestamp VARCHAR(30), syncTimestamp VARCHAR(30), datumIdentifier VARCHAR(50))");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: unable to create syncedData table: %1 - sociald will be inactive"))
            .arg(query.lastError().text()));
        return;
    }

    query.prepare( "CREATE TABLE IF NOT EXISTS syncTimestamps (id INTEGER PRIMARY KEY, serviceName VARCHAR(20), accountIdentifier VARCHAR(50), dataType VARCHAR(16), syncTimestamp VARCHAR(30))");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: unable to create syncTimestamps table: %1 - sociald will be inactive"))
            .arg(query.lastError().text()));
        return;
    }

    

    // TODO: use a plugin system or something?  For now, this is fine.

    // Facebook
    {
        QLatin1String facebookService("facebook");
        m_supportedServices.append(facebookService);
        m_supportedDataTypes.insert(facebookService, QStringList() <<
                                    SyncService::dataType(SyncService::Notifications) <<
                                    SyncService::dataType(SyncService::Images) <<
                                    SyncService::dataType(SyncService::Posts) <<
                                    SyncService::dataType(SyncService::Contacts));
        // TODO: Contacts, Calendar Events
    }

    // Google+
    {
        // TODO
    }

    // Twitter
    {
        QLatin1String twitterService("twitter");
        m_supportedServices.append(twitterService);
        m_supportedDataTypes.insert(twitterService, QStringList() <<
                                    SyncService::dataType(SyncService::Notifications) <<
                                    SyncService::dataType(SyncService::Posts));
    }
}

SyncServicePrivate::~SyncServicePrivate()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

SocialNetworkSyncAdaptor *SyncServicePrivate::createAdaptor(const QString &socialService, const QString &dataType, QObject *parent)
{
    if (socialService == "twitter") {
        if (dataType == SyncService::dataType(SyncService::Notifications)) {
            return new TwitterMentionTimelineSyncAdaptor(q, parent);
        } else if (dataType == SyncService::dataType(SyncService::Posts)) {
            return new TwitterHomeTimelineSyncAdaptor(q, parent);
        } else {
            return 0;
        }
    } else if (socialService == "facebook") {
        if (dataType == SyncService::dataType(SyncService::Notifications)) {
            return new FacebookNotificationSyncAdaptor(q, parent);
        } else if (dataType == SyncService::dataType(SyncService::Images)) {
            return new FacebookImageSyncAdaptor(q, parent);
        } else if (dataType == SyncService::dataType(SyncService::Posts)) {
            return new FacebookPostSyncAdaptor(q, parent);
        } else if (dataType == SyncService::dataType(SyncService::Contacts)) {
            return new FacebookContactSyncAdaptor(q, parent);
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

// --------------------------------------------------

SyncService::SyncService(const QString &serviceName, QObject *parent)
    : QObject(parent)
    , d(new SyncServicePrivate(QString("sociald%1").arg(serviceName), this))
{
}

SyncService::~SyncService()
{
}

/*
    The sync service supports a particular social service
    if it is able to service sync() requests for the given
    social service.
*/
QStringList SyncService::supportedSocialServices() const
{
    return d->m_supportedServices;
}

/*
    Each social service's sync adaptor can support zero
    or more of the valid data types.  Attempting to sync
    a data type which isn't supported by a social network
    will result in a no-op.
*/
QStringList SyncService::supportedDataTypes(const QString &socialService) const
{
    if (!supportedSocialServices().contains(socialService)) {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("%1 is not a supported social service"))
                .arg(socialService));
        return QStringList(); // invalid social service parameter.
    }

    return d->m_supportedDataTypes.value(socialService);
}

SocialNetworkSyncAdaptor *SyncService::createAdaptor(const QString &socialService, const QString &dataType, QObject *parent)
{
    if (!supportedSocialServices().contains(socialService)) {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("%1 is not a supported social service"))
                .arg(socialService));
        return 0;
    }

    QStringList allTypes = validDataTypes();
    QStringList supportedTypes = supportedDataTypes(socialService);
    if (!allTypes.contains(dataType)) {
        TRACE(SOCIALD_INFORMATION,
              QString(QLatin1String("%1 is not a valid data type"))
              .arg(dataType));
        return 0;
    } else {
        if (supportedTypes.contains(dataType)) {
            return d->createAdaptor(socialService, dataType, parent);
        } else {
            TRACE(SOCIALD_INFORMATION,
                  QString(QLatin1String("%1 is not a supported data type"))
                  .arg(dataType));
            return 0;
        }
    }
}

QSqlDatabase *SyncService::database() const
{
    if (!d || !d->m_db.isOpen()) {
        return 0;
    }
    return &d->m_db;
}

/*
    Valid data types are data types which are known to the API.
    Note that just because a data type is valid does not mean
    that it will necessarily be supported by a given social network
    sync adaptor.
*/
QStringList SyncService::validDataTypes()
{
    return QStringList()
            << QLatin1String("Contacts")
            << QLatin1String("Calendars")
            << QLatin1String("Notifications")
            << QLatin1String("Images")
            << QLatin1String("Videos")
            << QLatin1String("Posts")
            << QLatin1String("Messages")
            << QLatin1String("Emails");
}

/*
    String for Enum since the DBus API uses strings
*/
QString SyncService::dataType(SyncService::DataType t)
{
    switch (t) {
        case SyncService::Contacts: return QLatin1String("Contacts");
        case SyncService::Calendars: return QLatin1String("Calendars");
        case SyncService::Notifications: return QLatin1String("Notifications");
        case SyncService::Images: return QLatin1String("Images");
        case SyncService::Videos: return QLatin1String("Videos");
        case SyncService::Posts: return QLatin1String("Posts");
        case SyncService::Messages: return QLatin1String("Messages");
        case SyncService::Emails: return QLatin1String("Emails");
        default: break;
    }

    return QString();
}
