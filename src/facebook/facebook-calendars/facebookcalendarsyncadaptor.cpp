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
    setInitialActive(m_db.isValid());
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
    m_storage->open(); // we close it in finalCleanup()
    FacebookDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void FacebookCalendarSyncAdaptor::finalCleanup()
{
    // commit changes to db
    if (m_storageNeedsSave) {
        m_storage->save();
        m_storageNeedsSave = false;
    }

    if (!ghostEventCleanupPerformed()) {
        // Delete any events which are not associated with a notebook.
        // These events are ghost events, caused by a bug which previously
        // existed in the purgeDataForOldAccounts code.
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
                TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("deleting orphan event %1")).arg(incidence->uid()));
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

void FacebookCalendarSyncAdaptor::purgeDataForOldAccounts(const QList<int> &oldIds, SocialNetworkSyncAdaptor::PurgeMode mode)
{
    if (mode == SocialNetworkSyncAdaptor::CleanUpPurge) {
        // we need to initialise the storage
        m_storageNeedsSave = false;
        m_storage->open(); // we close it in finalCleanup()
    }

    // We clean all the entries in the calendar
    foreach (int accountId, oldIds) {
        foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
            if (notebook->pluginName() == QLatin1String(FACEBOOK)
                    && notebook->account() == QString::number(accountId)) {
                notebook->setIsReadOnly(false);
                m_storage->loadNotebookIncidences(notebook->uid());
                KCalCore::Incidence::List allIncidences;
                m_storage->allIncidences(&allIncidences, notebook->uid());
                foreach (const KCalCore::Incidence::Ptr incidence, allIncidences) {
                    m_calendar->deleteIncidence(m_calendar->incidence(incidence->uid()));
                }
                m_storage->deleteNotebook(notebook);
                m_storageNeedsSave = true;
            }
        }

        // Clean the database
        m_db.removeEvents(accountId);
        m_db.sync(accountId);
        m_db.wait();
    }

    if (mode == SocialNetworkSyncAdaptor::CleanUpPurge) {
        // and commit any changes made.
        finalCleanup();
    }
}

void FacebookCalendarSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    TRACE(SOCIALD_DEBUG,
          QString(QLatin1String("Beginning Calendar sync for Facebook, account %1")).arg(accountId));

    requestEvents(accountId, accessToken);
}

void FacebookCalendarSyncAdaptor::requestEvents(int accountId, const QString &accessToken,
                                                    const QString &until,
                                                    const QString &pagingToken)
{
    // TODO: continuation requests need these two.  if exists, also set limit = 5000.
    // if not set, set "since" to the timestamp value.
    Q_UNUSED(until);
    Q_UNUSED(pagingToken);

    int sinceSpan = m_accountSyncProfile
            ? m_accountSyncProfile->key(Buteo::KEY_SYNC_SINCE_DAYS_PAST, QStringLiteral("30")).toInt()
            : 30;
    uint startTime = QDateTime::currentDateTimeUtc().addDays(sinceSpan * -1).toTime_t();
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    QString fql = QString(QLatin1String("SELECT eid, name, description, is_date_only, location, "\
                                        "start_time, end_time, timezone, host FROM event WHERE "\
                                        "eid IN (SELECT eid FROM event_member WHERE uid = me() "\
                                        "AND rsvp_status != 'declined' AND start_time > %1)")).arg(startTime);
    // We need support for some paging system
    // maybe by adding v ?
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
        setupReplyTimeout(accountId, reply);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request events "\
                                      "from Facebook account with id %1")).arg(accountId));
    }
}

void FacebookCalendarSyncAdaptor::finishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QByteArray replyData = reply->readAll();
    bool isError = reply->property("isError").toBool();

    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok) {
        QList<FacebookEvent::ConstPtr> dbEvents = m_db.events(accountId);

        TRACE(SOCIALD_DEBUG,
              QString(QLatin1String("%1 events in the database")).arg(dbEvents.count()));

        // Search for the Facebook Notebook
        // Create one if not found (TODO: check if it failed)
        // TODO: set a name to the notebook
        mKCal::Notebook::List facebookNotebooks;
        foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
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
                m_storage->loadNotebookIncidences(notebook->uid());
                KCalCore::Incidence::List incidenceList;
                m_storage->allIncidences(&incidenceList, notebook->uid());
                foreach (KCalCore::Incidence::Ptr incidence, incidenceList) {
                    m_calendar->deleteIncidence(m_calendar->incidence(incidence->uid()));
                }
                m_storage->deleteNotebook(notebook);
                m_storageNeedsSave = true;
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
            notebook->setDescription(accountManager->account(accountId)->displayName());
            notebook->setIsReadOnly(true);
            m_storage->addNotebook(notebook);
            m_storageNeedsSave = true;
        } else {
            notebook = facebookNotebooks.first();
            bool changed = false;

            if (notebook->description().isEmpty()) {
                notebook->setDescription(accountManager->account(accountId)->displayName());
                changed = true;
            }

            if (!notebook->isReadOnly()) {
                notebook->setIsReadOnly(true);
                changed = true;
            }

            if (changed) {
                m_storage->updateNotebook(notebook);
                m_storageNeedsSave = true;
            }
        }

        // Useful maps
        // Calendar events map contains the events that are loaded from the calendar
        // Db events map contains the events that are from the database
        // incidences set is updated and entries taken when existing incidences are found
        // so that the remaining incidences id are those who should be removed.
        QMap<QString, KCalCore::Event::Ptr> calendarEventsMap;
        QMap<QString, FacebookEvent::ConstPtr> dbEventsMap;
        QSet<QString> incidencesSet;

        // Set notebook writeable locally.
        notebook->setIsReadOnly(false);

        // We load incidences that are associated to Facebook into memory
        foreach (const FacebookEvent::ConstPtr &dbEvent, dbEvents) {
            QString incidenceId = dbEvent->incidenceId();
            m_storage->load(incidenceId);
            KCalCore::Event::Ptr event = m_calendar->event(incidenceId);
            if (!event.isNull()) {
                dbEventsMap.insert(dbEvent->fbEventId(), dbEvent);
                calendarEventsMap.insert(dbEvent->fbEventId(), event);
                incidencesSet.insert(incidenceId);
            }
        }

        // Parse the event list
        QJsonArray dataList = parsed.value(QLatin1String("data")).toArray();
        foreach (QJsonValue data, dataList) {
            QJsonObject dataMap = data.toObject();
            QString eventId = dataMap.value(QLatin1String("eid")).toVariant().toString();
            QString startTimeString = dataMap.value(QLatin1String("start_time")).toString();
            QString endTimeString = dataMap.value(QLatin1String("end_time")).toString();
            if (endTimeString.isEmpty()) {
                // workaround for empty ET events
                endTimeString = startTimeString;
            }
            bool isDateOnly = dataMap.value(QLatin1String("is_date_only")).toBool();
            KDateTime startTime, endTime;
            bool endExists = true;
            if (!isDateOnly) {
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
                startTime = parsedStartTime.toLocalZone();
                endTime = parsedEndTime.toLocalZone();
            } else {
                // mkcal date-only event semantics:
                // if a date-only event lasts only one day, set isAllDay to true, but don't set an end date.
                // if a date-only event lasts multiple days, set isAllDay to true, and set an end date.
                // Use ClockTime format, so that it doesn't get offset according to timezone.
                startTime = KDateTime(QDate::fromString(startTimeString, "yyyy-MM-dd"), QTime(), KDateTime::ClockTime);
                endTime   = KDateTime(QDate::fromString(endTimeString,   "yyyy-MM-dd"), QTime(), KDateTime::ClockTime);
                if (endTime == startTime) {
                    endExists = false; // single-day all day event; don't set endDt.
                }
            }
            QString summary = dataMap.value(QLatin1String("name")).toString();
            QString description = dataMap.value(QLatin1String("description")).toString();

            // Check if this event already exists
            bool update = false;
            KCalCore::Event::Ptr event;
            if (calendarEventsMap.contains(eventId)) {
                FacebookEvent::ConstPtr dbEvent = dbEventsMap.value(eventId);
                incidencesSet.remove(dbEvent->incidenceId());
                update = true;
                event = calendarEventsMap.value(eventId);
            }

            if (!update) {
                event = KCalCore::Event::Ptr(new KCalCore::Event);
            } else {
                event->startUpdates();
            }
            m_db.addSyncedEvent(eventId, accountId, event->uid());

            // Set the property of the event
            event->setSummary(summary);
            event->setDescription(description);
            event->setDtStart(startTime);
            if (endExists) {
                event->setDtEnd(endTime);
                event->setHasEndDate(true);
            } else {
                event->setHasEndDate(false);
            }
            if (isDateOnly) {
                event->setAllDay(true);
            }
            event->setReadOnly(true);

            if (update) {
                event->endUpdates();
            } else {
                m_calendar->addEvent(event, notebook->uid());
            }
        }

        // Remove all other incidences
        foreach (const QString &incidence, incidencesSet) {
            KCalCore::Incidence::Ptr incidencePtr = m_calendar->incidence(incidence);
            if (incidencePtr) {
                m_calendar->deleteIncidence(incidencePtr);
            }
        }

        // Perform removal and insertions
        m_db.sync(accountId);
        m_db.wait();
        m_storageNeedsSave = true;

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
