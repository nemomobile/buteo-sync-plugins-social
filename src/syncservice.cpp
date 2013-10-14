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
#include "facebook/facebookcalendartypesyncadaptor.h"
#include "google/googlecontactsyncadaptor.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>


static const int USER_VERSION = 1;
static inline QString pragmaUserVersionQuery() {
    return QString("PRAGMA user_version=%1").arg(USER_VERSION);
}

SyncServicePrivate::SyncServicePrivate(const QString &connectionName, SyncService *parent)
    : QObject(parent), q(parent)
{
    // TODO: use a plugin system or something?  For now, this is fine.

    // Facebook
    {
        QLatin1String facebookService("facebook");
        m_supportedServices.append(facebookService);
        m_supportedDataTypes.insert(facebookService, QStringList() <<
                                    SyncService::dataType(SyncService::Notifications) <<
                                    SyncService::dataType(SyncService::Images) <<
                                    SyncService::dataType(SyncService::Posts) <<
                                    SyncService::dataType(SyncService::Contacts) <<
                                    SyncService::dataType(SyncService::Calendars));
    }

    // Google+
    {
        QLatin1String googleService("google");
        m_supportedServices.append(googleService);
        m_supportedDataTypes.insert(googleService, QStringList() <<
                                    SyncService::dataType(SyncService::Contacts));
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
        } else if (dataType == SyncService::dataType(SyncService::Calendars)) {
            return new FacebookCalendarTypeSyncAdaptor(q, parent);
        } else {
            return 0;
        }
    } else if (socialService == "google") {
        if (dataType == SyncService::dataType(SyncService::Contacts)) {
            return new GoogleContactSyncAdaptor(q, parent);
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
