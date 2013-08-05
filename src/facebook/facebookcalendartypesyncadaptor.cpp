/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "facebookcalendartypesyncadaptor.h"
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include "trace.h"
#include <extendedcalendar.h>
#include <extendedstorage.h>

// Update the following version if database schema changes e.g. new
// fields are added to the existing tables.
// It will make old tables dropped and creates new ones.
#define USER_VERSION 1
#define DB_NAME QLatin1String("facebook.db")

static const char *FACEBOOK = "Facebook";
static const char *FACEBOOK_COLOR = "#3b5998";

// TODO: use a database to track the events
// in order to perform events overriding, removal of all events on
// purging, and removal of old events
FacebookCalendarTypeSyncAdaptor::FacebookCalendarTypeSyncAdaptor(SyncService *syncService,
                                                                 QObject *parent)
    : FacebookDataTypeSyncAdaptor(syncService, SyncService::Calendars, parent)
{
    setInitialActive(initDatabase(serviceName(), SyncService::dataType(dataType),
                                  QLatin1String(SOCIALD_DATABASE_DIR), DB_NAME ,USER_VERSION));
}

FacebookCalendarTypeSyncAdaptor::~FacebookCalendarTypeSyncAdaptor()
{
}

void FacebookCalendarTypeSyncAdaptor::purgeDataForOldAccounts(const QList<int> &oldIds)
{
    // We clean all the entries in the calendar
    mKCal::ExtendedCalendar::Ptr calendar =
            mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QLatin1String("UTC")));
    mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(calendar);
    storage->open();
    QSqlQuery query(db);
    foreach (int accountId, oldIds) {
        query.prepare("SELECT fbEventId, incidenceId FROM events WHERE accountId=:accountId");
        query.bindValue(":accountId", accountId);
        if (!query.exec()) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("error: unable to execute events query for account %1. "\
                                        "Error %2")).arg(accountId).arg(query.lastError().text()));
            return;
        }

        QMap<QString, QString> facebookIdToIncidenceId;
        while (query.next()) {
            facebookIdToIncidenceId.insert(query.value(0).toString(),
                                           query.value(1).toString());
        }

        foreach (QString facebookId, facebookIdToIncidenceId.keys()) {
            QString incidenceId = facebookIdToIncidenceId.value(facebookId);
            storage->load(incidenceId);
            KCalCore::Event::Ptr event = calendar->event(incidenceId);
            if (!event.isNull()) {
                calendar->deleteEvent(event);
            }
        }

        query.prepare("DELETE FROM events WHERE accountId=:accountId");
        query.bindValue(":accountId", accountId);
        if (!query.exec()) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("error: unable to execute events deletin for account %1. "\
                                        "Error %2")).arg(accountId).arg(query.lastError().text()));
            return;
        }
    }

    storage->save();
    storage->close();

    foreach (int pid, oldIds) {
        // purge all data from our database
        removeAllData(QLatin1String("facebook"),
                SyncService::dataType(SyncService::Calendars),
                QString::number(pid));
    }
}

void FacebookCalendarTypeSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    TRACE(SOCIALD_DEBUG,
          QString(QLatin1String("Beginning Calendar sync for Facebook, account %1")).arg(accountId));

    requestEvents(accountId, accessToken);
}

void FacebookCalendarTypeSyncAdaptor::requestEvents(int accountId, const QString &accessToken,
                                                    const QString &until,
                                                    const QString &pagingToken)
{
    // TODO: continuation requests need these two.  if exists, also set limit = 5000.
    // if not set, set "since" to the timestamp value.
    Q_UNUSED(until);
    Q_UNUSED(pagingToken);


    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    QString fql = QString(QLatin1String("SELECT eid, name, description, is_date_only, location, "\
                                        "start_time, end_time, timezone, host FROM event WHERE "\
                                        "eid IN (SELECT eid FROM event_member WHERE uid = me() "\
                                        "AND start_time > 0)"));
    // We need support for some paging system
    // maybe by adding v ?
    // QDateTime limitTime = QDateTime::currentDateTime().addMonths(-1);
    // QString timestamp = QString::number(limitTime.toMSecsSinceEpoch() / 1000);
    // AND start_time > %1)")).arg(timestamp);
    queryItems.append(qMakePair<QString, QString>(QString(QLatin1String("q")), fql));;


    QUrl url(QLatin1String("https://graph.facebook.com/fql"));
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
    QNetworkReply *reply = networkAccessManager->get(QNetworkRequest(url));

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request events "\
                                      "from Facebook account with id %1")).arg(accountId));
    }
}

void FacebookCalendarTypeSyncAdaptor::finishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QDateTime lastSync = lastSyncTimestamp(serviceName(), SyncService::dataType(dataType),
                                           QString::number(accountId));
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QVariantMap parsed = FacebookDataTypeSyncAdaptor::parseReplyData(replyData, &ok);
    if (ok) {
        // We get the existing incidences
        QSqlQuery query(db);
        query.prepare("SELECT fbEventId, incidenceId FROM events WHERE accountId=:accountId");
        query.bindValue(":accountId", accountId);
        if (!query.exec()) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("error: unable to execute events query for account %1. "\
                                        "Error %2")).arg(accountId).arg(query.lastError().text()));
            return;
        }

        QMap<QString, QString> facebookIdToIncidenceId;
        while (query.next()) {
            facebookIdToIncidenceId.insert(query.value(0).toString(),
                                           query.value(1).toString());
        }

        TRACE(SOCIALD_DEBUG,
              QString(QLatin1String("%1 events in the database")).arg(facebookIdToIncidenceId.count()));

        // We open the calendar and storage associated to it
        mKCal::ExtendedCalendar::Ptr calendar =
                mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QLatin1String("UTC")));
        mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(calendar);
        storage->open();

        // Search for the Facebook Notebook
        // Create one if not found (TODO: check if it failed)
        // TODO: set a name to the notebook
        mKCal::Notebook::List facebookNotebooks;
        foreach (mKCal::Notebook::Ptr notebook, storage->notebooks()) {
            if (notebook->pluginName() == QLatin1String(FACEBOOK)
                && notebook->account() == QString::number(accountId)) {
                facebookNotebooks.append(notebook);
            }
        }

        TRACE(SOCIALD_DEBUG,
              QString(QLatin1String("Found %1 notebooks")).arg(facebookNotebooks.count()));

        // That should not happen, but we check it nevertheless
        // we should purge anything that is contained in these notebooks
        // and restart over
        if (facebookNotebooks.count() > 1) {
            TRACE(SOCIALD_DEBUG,
                  QString(QLatin1String("Resetting notebooks")));

            foreach (mKCal::Notebook::Ptr notebook, facebookNotebooks) {
                KCalCore::Incidence::List incidenceList;
                storage->allIncidences(&incidenceList, notebook->uid());

                foreach (KCalCore::Incidence::Ptr incidence, incidenceList) {
                    calendar->deleteIncidence(incidence);
                }

                storage->deleteNotebook(notebook);
            }

            facebookNotebooks.clear();
        }

        mKCal::Notebook::Ptr notebook;

        // We create the Facebook notebook
        if (facebookNotebooks.isEmpty()) {
            notebook = mKCal::Notebook::Ptr(new mKCal::Notebook);
            notebook->setName(QLatin1String(FACEBOOK));
            notebook->setPluginName(QLatin1String(FACEBOOK));
            notebook->setAccount(QString::number(accountId));
            notebook->setColor(QLatin1String(FACEBOOK_COLOR));
            storage->addNotebook(notebook);
        } else {
            notebook = facebookNotebooks.first();
        }
        // TODO, we might need the following code (but it do not work)
        // notebook->setIsReadOnly(false);

        // We first set all facebook ids to be deleted
        // if we find that the entry should still be displayed
        // we will remove it from this set, and update it.
        // Newer entries will be inserted in a different
        // map.
        QSet<QString> facebookEventsIdToBeDeleted = facebookIdToIncidenceId.keys().toSet();
        QMap<QString, QString> facebookIdAndIncidenceToBeAdded;

        QMap<QString, KCalCore::Event::Ptr> facebookIdToEvents;
        // We load incidences that are associated to Facebook into memory
        foreach (QString facebookId, facebookIdToIncidenceId.keys()) {
            QString incidenceId = facebookIdToIncidenceId.value(facebookId);
            storage->load(incidenceId);
            KCalCore::Event::Ptr event = calendar->event(incidenceId);
            if (!event.isNull()) {
                facebookIdToEvents.insert(facebookId, event);
            }
        }

        // Parse the event list
        QVariantList dataList = parsed.value(QLatin1String("data")).toList();
        foreach (QVariant data, dataList) {
            QVariantMap dataMap = data.toMap();
            QString eventId = dataMap.value(QLatin1String("eid")).toString();
            QString startTimeString = dataMap.value(QLatin1String("start_time")).toString();
            bool isDateOnly = dataMap.value(QLatin1String("is_date_only")).toBool();
            KDateTime startTime;
            if (!isDateOnly) {
                KDateTime parsedStartTime = KDateTime::fromString(startTimeString);

                // Sometimes KDateTime cannot parse the timezone
                // even if it should support it
                // We are then doing it manually
                if (parsedStartTime.isNull()) {
                    parsedStartTime = KDateTime::fromString(startTimeString,
                                                            QLatin1String("%Y-%m-%dT%H:%M:%S%z"));
                }
                startTime = parsedStartTime.toLocalZone();
            } else {
                startTime = KDateTime::fromString(startTimeString, QLatin1String("%Y-%m-%d"));
                startTime.setTime(QTime(0, 0));
            }
            QString summary = dataMap.value(QLatin1String("name")).toString();
            QString description = dataMap.value(QLatin1String("description")).toString();

            // Check if this event already exists
            bool update = false;
            KCalCore::Event::Ptr event;
            if (facebookEventsIdToBeDeleted.contains(eventId)) {
                facebookEventsIdToBeDeleted.remove(eventId);
                if (facebookIdToEvents.contains(eventId)) {
                    update = true;
                    event = facebookIdToEvents.value(eventId);
                }
            }

            if (!update) {
                event = KCalCore::Event::Ptr(new KCalCore::Event);
                facebookIdAndIncidenceToBeAdded.insert(eventId, event->uid());
            } else {
                event->startUpdates();
            }

            // Set the property of the event
            event->setSummary(summary);
            event->setDescription(description);
            event->setDtStart(startTime);
            event->setAllDay(isDateOnly);
            event->setReadOnly(true);

            if (update) {
                event->endUpdates();
            } else {
                calendar->addEvent(event, notebook->uid());
            }
        }

        // TODO, we might need the following code (but it do not work)
        // notebook->setIsReadOnly(true);

        // Write to calendar
        storage->save();
        storage->close();

        // Perform removal and insertions
        if (!facebookEventsIdToBeDeleted.isEmpty()) {
            query.prepare("DELETE FROM events WHERE fbEventId=?");
            QVariantList args;
            foreach (QString facebookId, facebookEventsIdToBeDeleted) {
                args.append(facebookId);
            }
            query.addBindValue(args);
            if (!query.execBatch()) {
                TRACE(SOCIALD_ERROR,
                      QString(QLatin1String("error: unable to execute events removal for "\
                                            "account %1. Error %2")).arg(accountId).arg(query.lastError().text()));
            }
        }

        if (!facebookIdAndIncidenceToBeAdded.isEmpty()) {
            query.prepare("INSERT INTO events VALUES (?, ?, ?)");
        }
        QVariantList args1;
        QVariantList args2;
        QVariantList args3;
        foreach (QString facebookId, facebookIdAndIncidenceToBeAdded.keys()) {
            args1.append(facebookId);
            args2.append(facebookIdAndIncidenceToBeAdded.value(facebookId));
            args3.append(accountId);
        }
        query.addBindValue(args1);
        query.addBindValue(args2);
        query.addBindValue(args3);
        if (!query.execBatch()) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("error: unable to execute events insertion for "\
                                        "account %1. Error %2")).arg(accountId).arg(query.lastError().text()));
        }

    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse calendar data from request with "\
                                      "account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

bool FacebookCalendarTypeSyncAdaptor::dbCreateTables()
{
    // create the facebook event db tables
    // events = fbEventId, fbUserId, incidenceId
    QSqlQuery query(db);
    query.prepare( "CREATE TABLE IF NOT EXISTS events ("
                   "fbEventId VARCHAR(50) UNIQUE PRIMARY KEY,"
                   "incidenceId VARCHAR(50),"
                   "accountId VARCHAR(50))");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: unable to create events table - "\
                                    "%1 %2 sync will be inactive. Error: %3"))
              .arg(serviceName(), SyncService::dataType(dataType), query.lastError().text()));
        return false;
    }

    if (!createPragmaVersion(serviceName(), SyncService::dataType(dataType), USER_VERSION)) {
        return false;
    }
    return true;
}

bool FacebookCalendarTypeSyncAdaptor::dbDropTables()
{
    QSqlQuery query(db);
    query.prepare("DROP TABLE IF EXISTS events");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: failed to delete events table. Error %1"))
              .arg(query.lastError().text()));
        return false;
    }

    return true;
}
