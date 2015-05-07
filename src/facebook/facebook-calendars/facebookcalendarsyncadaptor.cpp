/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd.
 ** Contact: Lucien Xu <lucien.xu@jollamobile.com>
 **
 ** This program/library is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation.
 **
 ** This program/library is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 ** Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public
 ** License along with this program/library; if not, write to the Free
 ** Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 ** 02110-1301 USA
 **
 ****************************************************************************/

#include "facebookcalendarsyncadaptor.h"
#include "trace.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <extendedcalendar.h>
#include <extendedstorage.h>

#include <Accounts/Manager>
#include <Accounts/Account>

static const char *FACEBOOK = "Facebook";
static const char *FACEBOOK_COLOR = "#3B5998";

FacebookParsedEvent::FacebookParsedEvent()
    : m_isDateOnly(false)
    , m_endExists(false)
{
}

FacebookParsedEvent::FacebookParsedEvent(const FacebookParsedEvent &e)
{
    m_id = e.m_id;
    m_isDateOnly = e.m_isDateOnly;
    m_endExists = e.m_endExists;
    m_startTime = e.m_startTime;
    m_endTime = e.m_endTime;
    m_summary = e.m_summary;
    m_description = e.m_description;
}

namespace {
    // returns true if the ghost-event cleanup sync has been performed.
    bool ghostEventCleanupPerformed()
    {
        QString settingsFileName = QString::fromLatin1("%1/%2/fbcal.ini")
                .arg(QString::fromLatin1(PRIVILEGED_DATA_DIR))
                .arg(QString::fromLatin1(SYNC_DATABASE_DIR));
        QSettings settingsFile(settingsFileName, QSettings::IniFormat);
        return settingsFile.value(QString::fromLatin1("cleaned"), QVariant::fromValue<bool>(false)).toBool();
    }

    void setGhostEventCleanupPerformed()
    {
        QString settingsFileName = QString::fromLatin1("%1/%2/fbcal.ini")
                .arg(QString::fromLatin1(PRIVILEGED_DATA_DIR))
                .arg(QString::fromLatin1(SYNC_DATABASE_DIR));
        QSettings settingsFile(settingsFileName, QSettings::IniFormat);
        settingsFile.setValue(QString::fromLatin1("cleaned"), QVariant::fromValue<bool>(true));
        settingsFile.sync();
    }
}

FacebookCalendarSyncAdaptor::FacebookCalendarSyncAdaptor(QObject *parent)
    : FacebookDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Calendars, parent)
    , m_calendar(mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QLatin1String("UTC"))))
    , m_storage(mKCal::ExtendedCalendar::defaultStorage(m_calendar))
    , m_storageNeedsSave(false)
{
    setInitialActive(true);
}

FacebookCalendarSyncAdaptor::~FacebookCalendarSyncAdaptor()
{
}

QString FacebookCalendarSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("facebook-calendars");
}

void FacebookCalendarSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    m_storageNeedsSave = false;
    m_parsedEvents.clear();
    m_storage->open(); // we close it in finalCleanup()
    FacebookDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void FacebookCalendarSyncAdaptor::finalCleanup()
{
    if (syncAborted()) {
        SOCIALD_LOG_INFO("sync aborted, won't commit database changes");
        m_storage->close();
        return;
    }

    // commit changes to db
    if (m_storageNeedsSave) {
        // apply changes from sync
        m_storage->save();

        // set the facebook notebook back to read-only
        Q_FOREACH (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
            if (notebook->pluginName() == QLatin1String(FACEBOOK)) {
                notebook->setIsReadOnly(true);
                m_storage->updateNotebook(notebook);
                m_storage->save();
                break;
            }
        }

        // done.
        m_storageNeedsSave = false;
    }

    if (!ghostEventCleanupPerformed()) {
        // Delete any events which are not associated with a notebook.
        // These events are ghost events, caused by a bug which previously
        // existed in the purgeDataForOldAccount code.
        // The mkcal API doesn't allow us to determine which notebook a
        // given incidence belongs to, so we have to instead load
        // everything and then find the ones which are ophaned.
        m_storage->load();
        KCalCore::Incidence::List allIncidences = m_calendar->incidences();
        mKCal::Notebook::List allNotebooks = m_storage->notebooks();
        QSet<QString> notebookIncidenceUids;
        foreach (mKCal::Notebook::Ptr notebook, allNotebooks) {
            KCalCore::Incidence::List currNbIncidences;
            m_storage->allIncidences(&currNbIncidences, notebook->uid());
            foreach (KCalCore::Incidence::Ptr incidence, currNbIncidences) {
                notebookIncidenceUids.insert(incidence->uid());
            }
        }
        foreach (const KCalCore::Incidence::Ptr incidence, allIncidences) {
            if (!notebookIncidenceUids.contains(incidence->uid())) {
                // orphan/ghost incidence.  must be deleted.
                SOCIALD_LOG_DEBUG("deleting orphan event with uid:" << incidence->uid());
                m_calendar->deleteIncidence(incidence);
                m_storageNeedsSave = true;
            }
        }
        if (!m_storageNeedsSave || m_storage->save()) {
            setGhostEventCleanupPerformed();
        }
    }

    // done.
    m_storage->close();
}

void FacebookCalendarSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode)
{
    if (mode == SocialNetworkSyncAdaptor::CleanUpPurge) {
        // we need to initialise the storage
        m_storageNeedsSave = false;
        m_storage->open(); // we close it in finalCleanup()
    }

    // We clean all the entries in the calendar
    foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
        if (notebook->pluginName() == QLatin1String(FACEBOOK)
                && notebook->account() == QString::number(oldId)) {
            notebook->setIsReadOnly(false);
            m_storage->deleteNotebook(notebook);
            m_storageNeedsSave = true;
        }
    }

    if (mode == SocialNetworkSyncAdaptor::CleanUpPurge) {
        // and commit any changes made.
        finalCleanup();
    }
}

void FacebookCalendarSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    SOCIALD_LOG_DEBUG("beginning Calendar sync for Facebook account" << accountId);
    requestEvents(accountId, accessToken);
}

void FacebookCalendarSyncAdaptor::requestEvents(int accountId,
                                                const QString &accessToken,
                                                const QString &batchRequest)
{
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));

    QString batch = batchRequest;
    if (batch.isEmpty()) {
        // Create batch query of following format:
        //    [{ "method":"GET","relative_url":"me/events/created?include_headers=false&limit=200"},
        //     { "method":"GET","relative_url":"me/events/attending?include_headers=false&limit=200"},
        //     { "method":"GET","relative_url":"me/events/maybe?include_headers=false&limit=200"},
        //     { "method":"GET","relative_url":"me/events/not_replied?include_headers=false&limit=200"}]

        int sinceSpan = m_accountSyncProfile
                ? m_accountSyncProfile->key(Buteo::KEY_SYNC_SINCE_DAYS_PAST, QStringLiteral("30")).toInt()
                : 30;
        uint startTime = QDateTime::currentDateTimeUtc().addDays(sinceSpan * -1).toTime_t();
        QString since = QStringLiteral("since=") + QString::number(startTime);
        QString calendarQuery = QStringLiteral("{\"method\":\"GET\",\"relative_url\":\"me/events/%1?fields=id,name,start_time,end_time,is_date_only,description,location&include_headers=false&limit=200&")
                                  + since
                                  + QStringLiteral("\"}");

        batch = QStringLiteral("[")
              + calendarQuery.arg(QStringLiteral("created")) + QStringLiteral(",")
              + calendarQuery.arg(QStringLiteral("attending")) + QStringLiteral(",")
              + calendarQuery.arg(QStringLiteral("maybe")) + QStringLiteral(",")
              + calendarQuery.arg(QStringLiteral("not_replied"))
              + QStringLiteral("]");
    }
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("batch")), batch));

    QUrl url(graphAPI());
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);

    SOCIALD_LOG_DEBUG("performing request:" << url.toString());

    QNetworkReply *reply = m_networkAccessManager->post(QNetworkRequest(url), QByteArray());
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedHandler()));

        if (batchRequest.isEmpty()) {
            // we're requesting data.  Increment the semaphore so that we know we're still busy.
            incrementSemaphore(accountId);
        }
        setupReplyTimeout(accountId, reply);
    } else {
        SOCIALD_LOG_ERROR("unable to request events from Facebook account" << accountId);
    }
}

void FacebookCalendarSyncAdaptor::finishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QByteArray replyData = reply->readAll();
    bool isError = reply->property("isError").toBool();

    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    SOCIALD_LOG_TRACE("request finished, got response:");
    Q_FOREACH (const QString &line, QString::fromUtf8(replyData).split('\n', QString::SkipEmptyParts)) {
        SOCIALD_LOG_TRACE(line);
    }

    QStringList ongoingRequests;
    QJsonArray array;
    bool ok = false;
    QJsonDocument jsonDocument = QJsonDocument::fromJson(replyData);
    if (!jsonDocument.isEmpty() && jsonDocument.isArray()) {
        array = jsonDocument.array();
        if (array.count() > 0) {
            ok = true;
        }
    }

    if (!isError && ok) {
        foreach (QJsonValue value, array) {
            // Go through each entry in batch reply and process the events it contains
            if (!value.isObject()) {
                SOCIALD_LOG_ERROR("Facebook calendar batch reply entry is not an object for account " << accountId);
                continue;
            }

            QJsonObject entry = value.toObject();
            if (entry.value(QLatin1String("code")).toInt() != 200) {
                SOCIALD_LOG_ERROR("Facebook calendar batch request for account "
                                  << accountId << " failed with " << entry.value("code").toInt());
                continue;
            }

            if (!entry.contains(QLatin1String("body"))) {
                SOCIALD_LOG_ERROR("Facebook calendar batch reply entry doesn't contain body field for account " << accountId);
                continue;
            }

            QJsonDocument bodyDocument = QJsonDocument::fromJson(entry.value(QLatin1String("body")).toString().toUtf8());
            if (bodyDocument.isEmpty()) {
                SOCIALD_LOG_ERROR("Facebook calendar batch reply body is empty for account " << accountId);
                continue;
            }

            QJsonObject parsed = bodyDocument.object();
            if (!parsed.contains(QLatin1String("data"))) {
                SOCIALD_LOG_ERROR("Facebook calendar batch reply entry doesn't contain data for account " << accountId);
                continue;
            }

            if (parsed.contains(QLatin1String("paging"))) {
                QJsonObject paging = parsed.value(QLatin1String("paging")).toObject();
                if (paging.contains(QLatin1String("next"))) {
                    QString nextQuery = paging.value(QLatin1String("next")).toString();
                    ongoingRequests.append(nextQuery);
                }
            }

            // Parse the event list
            QJsonArray dataList = parsed.value(QLatin1String("data")).toArray();
            foreach (QJsonValue data, dataList) {
                QJsonObject dataMap = data.toObject();
                QString eventId = dataMap.value(QLatin1String("id")).toVariant().toString();

                if (m_parsedEvents.contains(eventId)) {
                    // event was already handled by this batch request
                    continue;
                }

                FacebookParsedEvent parsedEvent;
                parsedEvent.m_id = eventId;

                QString startTimeString = dataMap.value(QLatin1String("start_time")).toString();
                QString endTimeString = dataMap.value(QLatin1String("end_time")).toString();
                if (endTimeString.isEmpty()) {
                    // workaround for empty ET events
                    endTimeString = startTimeString;
                }
                parsedEvent.m_isDateOnly = dataMap.value(QLatin1String("is_date_only")).toBool();
                parsedEvent.m_endExists = true;
                if (!parsedEvent.m_isDateOnly) {
                    KDateTime parsedStartTime = KDateTime::fromString(startTimeString);
                    KDateTime parsedEndTime = KDateTime::fromString(endTimeString);

                    // Sometimes KDateTime cannot parse the timezone
                    // even if it should support it
                    // We are then doing it manually
                    if (parsedStartTime.isNull()) {
                        parsedStartTime = KDateTime::fromString(startTimeString,
                                                                QLatin1String("%Y-%m-%dT%H:%M:%S%z"));
                    }
                    if (parsedEndTime.isNull()) {
                        parsedEndTime = KDateTime::fromString(endTimeString,
                                                              QLatin1String("%Y-%m-%dT%H:%M:%S%z"));
                    }
                    parsedEvent.m_startTime = parsedStartTime.toLocalZone();
                    parsedEvent.m_endTime = parsedEndTime.toLocalZone();
                } else {
                    // mkcal date-only event semantics:
                    // if a date-only event lasts only one day, set isAllDay to true, but don't set an end date.
                    // if a date-only event lasts multiple days, set isAllDay to true, and set an end date.
                    // Use ClockTime format, so that it doesn't get offset according to timezone.
                    parsedEvent.m_startTime = KDateTime(QDate::fromString(startTimeString, "yyyy-MM-dd"), QTime(), KDateTime::ClockTime);
                    parsedEvent.m_endTime   = KDateTime(QDate::fromString(endTimeString,   "yyyy-MM-dd"), QTime(), KDateTime::ClockTime);
                    if (parsedEvent.m_endTime == parsedEvent.m_startTime) {
                        parsedEvent.m_endExists = false; // single-day all day event; don't set endDt.
                    }
                }
                parsedEvent.m_summary = dataMap.value(QLatin1String("name")).toString();
                parsedEvent.m_description = dataMap.value(QLatin1String("description")).toString();
                parsedEvent.m_location = dataMap.value(QLatin1String("location")).toString();
                m_parsedEvents[eventId] = parsedEvent;
            }
        }

        if (ongoingRequests.count() > 0) {
            // Form next batch request for still ongoing requests
            QString nextBatch("[");
            foreach (const QString next, ongoingRequests) {
                QUrl nextUrl(next);
                nextBatch.append(QStringLiteral("{\"method\":\"GET\",\"relative_url\":\"me/events?include_headers=false&"));
                nextBatch.append(nextUrl.query());
                nextBatch.append(QStringLiteral("\"},"));
            }
            nextBatch.chop(1);      // remove last comma
            nextBatch.append(QStringLiteral("]"));
            requestEvents(accountId, accessToken, nextBatch);
        } else {
            SOCIALD_LOG_DEBUG("finished all requests, about to perform database update");
            processParsedEvents(accountId);
            decrementSemaphore(accountId);
        }
    } else {
        // Error occurred during request.
        SOCIALD_LOG_ERROR("unable to parse calendar data from request with account"
                          << accountId << ", got:" << QString::fromLatin1(replyData.constData()));
        decrementSemaphore(accountId);
    }
}


void FacebookCalendarSyncAdaptor::processParsedEvents(int accountId)
{
    // Search for the Facebook Notebook
    SOCIALD_LOG_DEBUG("Received" << m_parsedEvents.size() << "events from server; determining delta");
    mKCal::Notebook::Ptr fbNotebook;
    Q_FOREACH (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
        if (notebook->pluginName() == QLatin1String(FACEBOOK) && notebook->account() == QString::number(accountId)) {
            fbNotebook = notebook;
        }
    }

    if (!fbNotebook) {
        // create the notebook if required
        fbNotebook = mKCal::Notebook::Ptr(new mKCal::Notebook);
        fbNotebook->setName(QLatin1String(FACEBOOK));
        fbNotebook->setPluginName(QLatin1String(FACEBOOK));
        fbNotebook->setAccount(QString::number(accountId));
        fbNotebook->setColor(QLatin1String(FACEBOOK_COLOR));
        fbNotebook->setDescription(m_accountManager->account(accountId)->displayName());
        fbNotebook->setIsReadOnly(true);
        m_storage->addNotebook(fbNotebook);
        m_storageNeedsSave = true;
    } else {
        // update the notebook details if required
        bool changed = false;
        if (fbNotebook->description().isEmpty()) {
            fbNotebook->setDescription(m_accountManager->account(accountId)->displayName());
            changed = true;
        }

        if (changed) {
            m_storage->updateNotebook(fbNotebook);
            m_storageNeedsSave = true;
        }
    }

    // Set notebook writeable locally.
    fbNotebook->setIsReadOnly(false);

    // We load incidences that are associated to Facebook into memory
    KCalCore::Incidence::List dbEvents;
    m_storage->loadNotebookIncidences(fbNotebook->uid());
    if (!m_storage->allIncidences(&dbEvents, fbNotebook->uid())) {
        SOCIALD_LOG_ERROR("unable to load Facebook events from database");
        return;
    }

    // Now determine the delta to the events received from server.
    QSet<QString> seenLocalEvents;
    Q_FOREACH (const QString &fbId, m_parsedEvents.keys()) {
        // find the local event associated with this event.
        bool foundLocal = false;
        const FacebookParsedEvent &parsedEvent = m_parsedEvents[fbId];
        Q_FOREACH (KCalCore::Incidence::Ptr incidence, dbEvents) {
            if (incidence->uid().endsWith(QStringLiteral(":%1").arg(fbId))) {
                KCalCore::Event::Ptr event = m_calendar->event(incidence->uid());
                if (!event) continue; // not a valid event incidence.
                // found. If it has been modified remotely, then modify locally.
                foundLocal = true;
                seenLocalEvents.insert(incidence->uid());
                if (event->summary() != parsedEvent.m_summary ||
                    event->description() != parsedEvent.m_description ||
                    event->location() != parsedEvent.m_location ||
                    event->dtStart() != parsedEvent.m_startTime ||
                    (parsedEvent.m_endExists && event->dtEnd() != parsedEvent.m_endTime)) {
                    // the event has been changed remotely.
                    event->startUpdates();
                    event->setSummary(parsedEvent.m_summary);
                    event->setDescription(parsedEvent.m_description);
                    event->setLocation(parsedEvent.m_location);
                    event->setDtStart(parsedEvent.m_startTime);
                    if (parsedEvent.m_endExists) {
                        event->setDtEnd(parsedEvent.m_endTime);
                        event->setHasEndDate(true);
                    } else {
                        event->setHasEndDate(false);
                    }
                    if (parsedEvent.m_isDateOnly) {
                        event->setAllDay(true);
                    }
                    event->setReadOnly(true);
                    event->endUpdates();
                    m_storageNeedsSave = true;
                    SOCIALD_LOG_DEBUG("Facebook event" << event->uid() << "was modified on server");
                } else {
                    SOCIALD_LOG_DEBUG("Facebook event" << event->uid() << "is unchanged on server");
                }
            }
        }

        // if not found locally, it must be a new addition.
        if (!foundLocal) {
            KCalCore::Event::Ptr event = KCalCore::Event::Ptr(new KCalCore::Event);
            QString eventUid = QUuid::createUuid().toString();
            eventUid = eventUid.mid(1); // remove leading {
            eventUid.chop(1);           // remove trailing }
            eventUid = QStringLiteral("%1:%2").arg(eventUid).arg(fbId);
            event->setUid(eventUid);
            event->setSummary(parsedEvent.m_summary);
            event->setDescription(parsedEvent.m_description);
            event->setLocation(parsedEvent.m_location);
            event->setDtStart(parsedEvent.m_startTime);
            if (parsedEvent.m_endExists) {
                event->setDtEnd(parsedEvent.m_endTime);
                event->setHasEndDate(true);
            } else {
                event->setHasEndDate(false);
            }
            if (parsedEvent.m_isDateOnly) {
                event->setAllDay(true);
            }
            event->setReadOnly(true);
            m_calendar->addEvent(event, fbNotebook->uid());
            m_storageNeedsSave = true;
            SOCIALD_LOG_DEBUG("Facebook event" << event->uid() << "was added on server");
        }
    }

    // Any local events which were not seen, must have been removed remotely.
    Q_FOREACH (KCalCore::Incidence::Ptr incidence, dbEvents) {
        if (!seenLocalEvents.contains(incidence->uid())) {
            // note: have to delete from calendar after loaded from calendar.
            m_calendar->deleteIncidence(m_calendar->incidence(incidence->uid()));
            m_storageNeedsSave = true;
            SOCIALD_LOG_DEBUG("Facebook event" << incidence->uid() << "was deleted on server");
        }
    }
}
