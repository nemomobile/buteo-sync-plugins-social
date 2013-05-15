/*
 * Copyright (C) 2013 Jolla Ltd. <chris.adams@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "syncservice.h"
#include "syncservice_p.h"
#include "trace.h"

#include "facebook/facebooksyncadaptor.h"
#include "twitter/twittersyncadaptor.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

// all intervals are in milliseconds
#define SOCIALD_POLLING_INTERVAL_48_HOURS     172800000
#define SOCIALD_POLLING_INTERVAL_24_HOURS     86400000
#define SOCIALD_POLLING_INTERVAL_12_HOURS     43200000
#define SOCIALD_POLLING_INTERVAL_6_HOURS      21600000
#define SOCIALD_POLLING_INTERVAL_2_HOURS      7200000
#define SOCIALD_POLLING_INTERVAL_1_HOUR       3600000
#define SOCIALD_POLLING_INTERVAL_HALF_HOUR    1800000
#define SOCIALD_POLLING_INTERVAL_QUARTER_HOUR 900000
#define SOCIALD_POLLING_INTERVAL_3_MINUTES    180000
#define SOCIALD_POLLING_INTERVAL_MINUTE       60000
#define SOCIALD_POLLING_INTERVAL_DEFAULT      SOCIALD_POLLING_INTERVAL_24_HOURS
#define SOCIALD_POLLING_INTERVAL_DEBUG        SOCIALD_POLLING_INTERVAL_3_MINUTES

SyncServicePrivate::SyncServicePrivate(SyncService *parent)
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
    m_db = QSqlDatabase::addDatabase("QSQLITE", QLatin1String("sociald"));
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
        FacebookSyncAdaptor *fbsa = new FacebookSyncAdaptor(parent);
        m_adaptors.insert(QLatin1String("facebook"), fbsa);
        QMap<QString, QPair<int, QTimer*> > facebookTimers;

        // Notifications
        {
            QTimer *fbnt = new QTimer(this);
            fbnt->setInterval(SOCIALD_POLLING_INTERVAL_QUARTER_HOUR);
            fbnt->setSingleShot(false);
            fbnt->setProperty("socialService", QLatin1String("facebook"));
            fbnt->setProperty("dataType", SyncService::dataType(SyncService::Notifications));
            connect(fbnt, SIGNAL(timeout()), this, SLOT(pollingTimerTriggered()));
            fbnt->start();
            facebookTimers.insert(SyncService::dataType(SyncService::Notifications),
                                  QPair<int, QTimer*>(SOCIALD_POLLING_INTERVAL_QUARTER_HOUR, fbnt));
        }

        // Images
        {
            QTimer *fbit = new QTimer(this);
            fbit->setInterval(SOCIALD_POLLING_INTERVAL_6_HOURS);
            fbit->setSingleShot(false);
            fbit->setProperty("socialService", QLatin1String("facebook"));
            fbit->setProperty("dataType", SyncService::dataType(SyncService::Images));
            connect(fbit, SIGNAL(timeout()), this, SLOT(pollingTimerTriggered()));
            fbit->start();
            facebookTimers.insert(SyncService::dataType(SyncService::Images),
                                  QPair<int, QTimer*>(SOCIALD_POLLING_INTERVAL_6_HOURS, fbit));
        }

        // Contacts
        {
            // TODO
        }

        // Calendar Events
        {
            // TODO
        }

        // Feed Posts
        {
            QTimer *fbpt = new QTimer(this);
            fbpt->setInterval(SOCIALD_POLLING_INTERVAL_1_HOUR);
            fbpt->setSingleShot(false);
            fbpt->setProperty("socialService", QLatin1String("facebook"));
            fbpt->setProperty("dataType", SyncService::dataType(SyncService::Posts));
            connect(fbpt, SIGNAL(timeout()), this, SLOT(pollingTimerTriggered()));
            fbpt->start();
            facebookTimers.insert(SyncService::dataType(SyncService::Posts),
                                  QPair<int, QTimer*>(SOCIALD_POLLING_INTERVAL_1_HOUR, fbpt));
        }

        m_pollingTimers.insert(QLatin1String("facebook"), facebookTimers);
    }

    // Google+
    {
        // TODO
    }

    // Twitter
    {
        TwitterSyncAdaptor *tsa = new TwitterSyncAdaptor(parent);
        m_adaptors.insert(QLatin1String("twitter"), tsa);
        QMap<QString, QPair<int, QTimer*> > twitterTimers;

        // Mentions Timeline (Notifications)
        {
            QTimer *tnt = new QTimer(this);
            tnt->setInterval(SOCIALD_POLLING_INTERVAL_QUARTER_HOUR);
            tnt->setSingleShot(false);
            tnt->setProperty("socialService", QLatin1String("twitter"));
            tnt->setProperty("dataType", SyncService::dataType(SyncService::Notifications));
            connect(tnt, SIGNAL(timeout()), this, SLOT(pollingTimerTriggered()));
            tnt->start();
            twitterTimers.insert(SyncService::dataType(SyncService::Notifications),
                                 QPair<int, QTimer*>(SOCIALD_POLLING_INTERVAL_QUARTER_HOUR, tnt));
        }

        // Home Timeline (Posts)
        {
            QTimer *tpt = new QTimer(this);
            tpt->setInterval(SOCIALD_POLLING_INTERVAL_1_HOUR);
            tpt->setSingleShot(false);
            tpt->setProperty("socialService", QLatin1String("twitter"));
            tpt->setProperty("dataType", SyncService::dataType(SyncService::Posts));
            connect(tpt, SIGNAL(timeout()), this, SLOT(pollingTimerTriggered()));
            tpt->start();
            twitterTimers.insert(SyncService::dataType(SyncService::Posts),
                                 QPair<int, QTimer*>(SOCIALD_POLLING_INTERVAL_1_HOUR, tpt));
        }

        m_pollingTimers.insert(QLatin1String("twitter"), twitterTimers);
    }
}

SyncServicePrivate::~SyncServicePrivate()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

void SyncServicePrivate::pollingTimerTriggered()
{
    QTimer *t = qobject_cast<QTimer*>(sender());
    QString socialService = t->property("socialService").toString();
    QString dataType = t->property("dataType").toString();
    TRACE(SOCIALD_DEBUG,
            QString(QLatin1String("triggering sync of %1 from %2 due to polling timer"))
            .arg(dataType).arg(socialService));
    q->sync(socialService, QStringList() << dataType);
}

// --------------------------------------------------

SyncService::SyncService(QObject *parent)
    : QObject(parent), d(new SyncServicePrivate(this))
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
    return d->m_adaptors.keys();
}

/*
    A social service's sync adaptor will be enabled if
    a valid account with the social service exists.
    Attempting to sync() with a supported social service
    which is not enabled will result in a no-op.
*/
QStringList SyncService::enabledSocialServices() const
{
    QStringList retn;
    QStringList allServices = d->m_adaptors.keys();
    foreach (const QString &srv, allServices) {
        if (d->m_adaptors.value(srv)->enabled()) {
            retn.append(srv);
        }
    }

    return retn;
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

    return d->m_pollingTimers.value(socialService).keys();
}

/*
    The sync service will automatically perform sync()
    requests at a particular polling interval.

    Sync requests for each data type are done separately,
    and the polling interval for each data type may be
    set separately (for example, to allow faster polling
    of Notifications than of Contacts).
*/
QMap<QString, int> SyncService::pollingIntervals(const QString &socialService) const
{
    QMap<QString, int> retn;
    QMap<QString, QPair<int, QTimer*> > pt = d->m_pollingTimers.value(socialService);
    foreach (const QString &dataType, pt.keys()) {
        retn.insert(dataType, pt.value(dataType).first);
    }

    return retn;
}

void SyncService::setPollingIntervals(const QString &socialService, const QMap<QString, int> &intervals)
{
    if (!supportedSocialServices().contains(socialService)) {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("%1 is not a supported social service"))
                .arg(socialService));
        return;
    }

    foreach (const QString &dataType, intervals.keys()) {
        setPollingInterval(socialService, dataType, intervals.value(dataType));
    }
}

/*
    Sets the polling interval for syncing data of the given \a dataType
    from the given \a socialService to the specified \a interval which
    is specified in milliseconds.

    Uses the default interval if the interval given is less than zero,
    and disables polling if the interval is zero.
*/
void SyncService::setPollingInterval(const QString &socialService, const QString &dataType, int interval)
{
    if (!supportedSocialServices().contains(socialService)) {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("%1 is not a supported social service"))
                .arg(socialService));
        return;
    }

    if (!supportedDataTypes(socialService).contains(dataType)) {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("%1 is not a supported data type"))
                .arg(dataType));
        return;
    }

    int newInterval = interval;
    if (newInterval < 0) {
        newInterval = SOCIALD_POLLING_INTERVAL_DEFAULT;
    }
    QMap<QString, QPair<int, QTimer*> > existingTimers = d->m_pollingTimers.value(socialService);
    if (existingTimers.value(dataType).first != newInterval) {
        QTimer *timerValue = existingTimers.value(dataType).second;
        existingTimers.insert(dataType, QPair<int, QTimer*>(newInterval, timerValue));
        d->m_pollingTimers.insert(socialService, existingTimers); // update the internal data structure.
        if (timerValue) { // should never be null, but checking just in case.
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("setting polling interval for %1 from %2 to %3 milliseconds"))
                    .arg(dataType).arg(socialService).arg(newInterval));
            if (newInterval == 0) {
                // Polling interval of zero means "stop polling"
                timerValue->stop();
            } else {
                timerValue->setInterval(newInterval);
                if (!timerValue->isActive()) {
                    timerValue->start();
                }
            }
        }
    }
}

/*
    Triggers sync of all data of the specified data types
    from the specified social service.
*/
void SyncService::sync(const QString &socialService, const QStringList &types)
{
    if (!supportedSocialServices().contains(socialService)) {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("%1 is not a supported social service"))
                .arg(socialService));
        return;
    }

    if (!enabledSocialServices().contains(socialService)) {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("%1 is not currently enabled"))
                .arg(socialService));
        return; // no-op.
    }

    SocialNetworkSyncAdaptor *adaptor = d->m_adaptors.value(socialService);
    QStringList allTypes = validDataTypes();
    QStringList supportedTypes = supportedDataTypes(socialService);
    foreach (const QString &dataType, types) {
        if (!allTypes.contains(dataType)) {
            TRACE(SOCIALD_INFORMATION,
                    QString(QLatin1String("%1 is not a valid data type"))
                    .arg(dataType));
        } else {
            if (supportedTypes.contains(dataType)) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("performing sync of %1 from %2"))
                        .arg(dataType).arg(socialService));
                adaptor->sync(dataType);
            } else {
                TRACE(SOCIALD_INFORMATION,
                        QString(QLatin1String("%1 is not a supported data type"))
                        .arg(dataType));
            }
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
