/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
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

#include "googlecalendarsyncadaptor.h"
#include "trace.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QSettings>

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

//----------------------------------------------

#define RFC3339_FORMAT      "%Y-%m-%dT%H:%M:%S%:z"
#define RFC3339_FORMAT_NTZC "%Y-%m-%dT%H:%M:%S%z"
#define KDATEONLY_FORMAT    "%Y-%m-%d"
#define QDATEONLY_FORMAT    "yyyy-MM-dd"
#define KLONGTZ_FORMAT      "%:Z"

namespace {

QString gCalEventId(KCalCore::Incidence::Ptr event)
{
    return event->customProperty("jolla-sociald", "gcal-id");
}
void setGCalEventId(KCalCore::Incidence::Ptr event, const QString &id)
{
    event->setCustomProperty("jolla-sociald", "gcal-id", id);
}

QJsonArray recurrenceArray(KCalCore::Event::Ptr event, KCalCore::ICalFormat &icalFormat)
{
    QJsonArray retn;
    KCalCore::Recurrence *kcalRecurrence = event->recurrence();
    Q_FOREACH (KCalCore::RecurrenceRule *rrule, kcalRecurrence->rRules()) {
        QString rruleStr = icalFormat.toString(rrule);
        rruleStr.replace("\r\n", "");
        retn.append(QJsonValue(rruleStr));
    }
    Q_FOREACH (KCalCore::RecurrenceRule *exrule, kcalRecurrence->exRules()) {
        QString exruleStr = icalFormat.toString(exrule);
        exruleStr.replace("RRULE", "EXRULE");
        exruleStr.replace("\r\n", "");
        retn.append(QJsonValue(exruleStr));
    }
    Q_FOREACH (const QDate &rdate, kcalRecurrence->rDates()) {
        retn.append(QJsonValue(QString::fromLatin1("RDATE:%1").arg(rdate.toString("yyyy-MM-dd"))));
    }
    Q_FOREACH (const QDate &exdate, kcalRecurrence->exDates()) {
        retn.append(QJsonValue(QString::fromLatin1("EXDATE:%1").arg(exdate.toString("yyyy-MM-dd"))));
    }
    return retn;
}

QJsonObject kCalToJson(KCalCore::Event::Ptr event, KCalCore::ICalFormat &icalFormat)
{
    QString eventId = gCalEventId(event);
    QJsonObject start, end;

    // insert the date/time and timeZone information into the Json object.
    // note that timeZone is required for recurring events, for some reason.
    if (event->dtStart().isDateOnly() || (event->allDay() && event->dtStart().time() == QTime(0,0,0))) {
        start.insert(QLatin1String("date"), event->dtStart().date().toString(QDATEONLY_FORMAT));
    } else {
        start.insert(QLatin1String("dateTime"), event->dtStart().toString(RFC3339_FORMAT));
        start.insert(QLatin1String("timeZone"), QJsonValue(event->dtStart().toString(KLONGTZ_FORMAT)));
    }
    if (event->dtEnd().isDateOnly() || (event->allDay() && event->dtEnd().time() == QTime(0,0,0))) {
        // note: for iCal spec, allDay events need to have an end date of real-end-date+1 as end date is exclusive.
        end.insert(QLatin1String("date"), event->dateEnd().addDays(1).toString(QDATEONLY_FORMAT));
    } else {
        end.insert(QLatin1String("dateTime"), event->dtEnd().toString(RFC3339_FORMAT));
        end.insert(QLatin1String("timeZone"), QJsonValue(event->dtEnd().toString(KLONGTZ_FORMAT)));
    }

    QJsonObject retn;
    if (!eventId.isEmpty()) retn.insert(QLatin1String("id"), eventId);
    if (event->recurrence()) retn.insert(QLatin1String("recurrence"), recurrenceArray(event, icalFormat));
    retn.insert(QLatin1String("summary"), event->summary());
    retn.insert(QLatin1String("description"), event->description());
    retn.insert(QLatin1String("location"), event->location());
    retn.insert(QLatin1String("start"), start);
    retn.insert(QLatin1String("end"), end);
    retn.insert(QLatin1String("sequence"), QString::number(event->revision()+1));
    //retn.insert(QLatin1String("locked"), event->readOnly()); // only allow locking server-side.
    // we may wish to support locking/readonly from local side also, in the future.

    return retn;
}

void extractStartAndEnd(const QJsonObject &eventData,
                        bool *startExists,
                        bool *endExists,
                        bool *startIsDateOnly,
                        bool *endIsDateOnly,
                        bool *isAllDay,
                        KDateTime *start,
                        KDateTime *end)
{
    *startIsDateOnly = false, *endIsDateOnly = false;
    QString startTimeString, endTimeString;
    QJsonObject startTimeData = eventData.value(QLatin1String("start")).toObject();
    QJsonObject endTimeData = eventData.value(QLatin1String("end")).toObject();
    if (!startTimeData.value(QLatin1String("date")).toVariant().toString().isEmpty()) {
        *startExists = true;
        *startIsDateOnly = true; // all-day event.
        startTimeString = startTimeData.value(QLatin1String("date")).toVariant().toString();
    } else if (!startTimeData.value(QLatin1String("dateTime")).toVariant().toString().isEmpty()) {
        *startExists = true;
        startTimeString = startTimeData.value(QLatin1String("dateTime")).toVariant().toString();
    } else {
        *startExists = false;
    }
    if (!endTimeData.value(QLatin1String("date")).toVariant().toString().isEmpty()) {
        *endExists = true;
        *endIsDateOnly = true; // all-day event.
        endTimeString = endTimeData.value(QLatin1String("date")).toVariant().toString();
    } else if (!endTimeData.value(QLatin1String("dateTime")).toVariant().toString().isEmpty()) {
        *endExists = true;
        endTimeString = endTimeData.value(QLatin1String("dateTime")).toVariant().toString();
    } else {
        *endExists = false;
    }

    if (*startExists) {
        if (!*startIsDateOnly) {
            KDateTime parsedStartTime = KDateTime::fromString(startTimeString, RFC3339_FORMAT);
            KDateTime ntzcStartTime = KDateTime::fromString(startTimeString, RFC3339_FORMAT_NTZC);
            if (ntzcStartTime.time() > parsedStartTime.time()) parsedStartTime = ntzcStartTime;

            // different format?  let KDateTime detect the format automatically.
            if (parsedStartTime.isNull()) {
                parsedStartTime = KDateTime::fromString(startTimeString);
            }

            *start = parsedStartTime.toLocalZone();
        } else {
            *start = KDateTime(QDate::fromString(startTimeString, QDATEONLY_FORMAT), QTime(), KDateTime::ClockTime);
            // note: don't call start->setDateOnly(true); or mkcal doesn't like it.
        }
    }

    if (*endExists) {
        if (!*endIsDateOnly) {
            KDateTime parsedEndTime = KDateTime::fromString(endTimeString, RFC3339_FORMAT);
            KDateTime ntzcEndTime = KDateTime::fromString(endTimeString, RFC3339_FORMAT_NTZC);
            if (ntzcEndTime.time() > parsedEndTime.time()) parsedEndTime = ntzcEndTime;

            // different format?  let KDateTime detect the format automatically.
            if (parsedEndTime.isNull()) {
                parsedEndTime = KDateTime::fromString(endTimeString);
            }

            *end = parsedEndTime.toLocalZone();
        } else {
            // Special handling for all-day events is required.
            if (*startExists && *startIsDateOnly) {
                if (QDate::fromString(startTimeString, QDATEONLY_FORMAT)
                        == QDate::fromString(endTimeString, QDATEONLY_FORMAT)) {
                    // single-day all-day event
                    *endExists = false;
                    *isAllDay = true;
                } else if (QDate::fromString(startTimeString, QDATEONLY_FORMAT)
                        == QDate::fromString(endTimeString, QDATEONLY_FORMAT).addDays(-1)) {
                    // Google will send a single-day all-day event has having an end-date
                    // of startDate+1 to conform to iCal spec.  Hence, this is actually
                    // a single-day all-day event, despite the difference in end-date.
                    *endExists = false;
                    *isAllDay = true;
                } else {
                    // multi-day all-day event.
                    // as noted above, Google will send all-day events as having an end-date
                    // of real-end-date+1 in order to conform to iCal spec (exclusive end dt).
                    *start = KDateTime(QDate::fromString(startTimeString, QDATEONLY_FORMAT), QTime(), KDateTime::ClockTime);
                    *end = KDateTime(QDate::fromString(endTimeString, QDATEONLY_FORMAT).addDays(-1), QTime(), KDateTime::ClockTime);
                    *isAllDay = true;
                }
            } else {
                *end = KDateTime(QDate::fromString(endTimeString, QDATEONLY_FORMAT).addDays(-1), QTime(), KDateTime::ClockTime);
                // note: don't call end->setDateOnly(true); or mkcal doesn't like it.
                *isAllDay = false;
            }
        }
    }
}

void extractRecurrence(const QJsonArray &recurrence, KCalCore::Event::Ptr event, KCalCore::ICalFormat &icalFormat)
{
    KCalCore::Recurrence *kcalRecurrence = event->recurrence();

    if (!recurrence.size()) {
        kcalRecurrence->unsetRecurs();
        return;
    }

    for (int i = 0; i < recurrence.size(); ++i) {
        QString ruleStr = recurrence.at(i).toString();
        if (ruleStr.toLower().startsWith(QString::fromLatin1("rrule:"))) {
            KCalCore::RecurrenceRule *rrule = new KCalCore::RecurrenceRule;
            if (!icalFormat.fromString(rrule, ruleStr.mid(6))) {
                SOCIALD_LOG_DEBUG("unable to parse RRULE information:" << ruleStr << "\n" <<
                                  "from:" << QString::fromUtf8(QJsonDocument(recurrence).toJson()));
            } else {
                kcalRecurrence->addRRule(rrule);
            }
        } else if (ruleStr.toLower().startsWith(QString::fromLatin1("exrule:"))) {
            KCalCore::RecurrenceRule *exrule = new KCalCore::RecurrenceRule;
            if (!icalFormat.fromString(exrule, ruleStr.mid(7))) {
                SOCIALD_LOG_DEBUG("unable to parse EXRULE information:" << ruleStr << "\n"
                                  "from:" << QString::fromUtf8(QJsonDocument(recurrence).toJson()));
            } else {
                kcalRecurrence->addExRule(exrule);
            }
        } else if (ruleStr.toLower().startsWith(QString::fromLatin1("rdate:"))) {
            QDate rdate = QDate::fromString(ruleStr.mid(6), "yyyy-MM-dd");
            if (!rdate.isValid()) {
                SOCIALD_LOG_DEBUG("unable to parse RDATE information:" << ruleStr << "\n"
                                  "from:" << QString::fromUtf8(QJsonDocument(recurrence).toJson()));
            } else {
                kcalRecurrence->addRDate(rdate);
            }
        } else if (ruleStr.toLower().startsWith(QString::fromLatin1("exdate:"))) {
            QDate exdate = QDate::fromString(ruleStr.mid(7), "yyyy-MM-dd");
            if (!exdate.isValid()) {
                SOCIALD_LOG_DEBUG("unable to parse EXDATE information:" << ruleStr << "\n"
                                  "from:" << QString::fromUtf8(QJsonDocument(recurrence).toJson()));
            } else {
                kcalRecurrence->addExDate(exdate);
            }
        } else {
            SOCIALD_LOG_DEBUG("unknown recurrence information:" << ruleStr << "\n"
                              "from:" << QString::fromUtf8(QJsonDocument(recurrence).toJson()));
        }
    }
}

void jsonToKCal(const QJsonObject &json, KCalCore::Event::Ptr event, KCalCore::ICalFormat &icalFormat)
{
    KDateTime start, end;
    bool startExists = false, endExists = false;
    bool startIsDateOnly = false, endIsDateOnly = false;
    bool isAllDay = false;
    extractStartAndEnd(json, &startExists, &endExists, &startIsDateOnly, &endIsDateOnly, &isAllDay, &start, &end);
    setGCalEventId(event, json.value(QLatin1String("id")).toVariant().toString());
    extractRecurrence(json.value(QLatin1String("recurrence")).toArray(), event, icalFormat);
    event->setReadOnly(json.value(QLatin1String("locked")).toVariant().toBool());
    event->setSummary(json.value(QLatin1String("summary")).toVariant().toString());
    event->setDescription(json.value(QLatin1String("description")).toVariant().toString());
    event->setLocation(json.value(QLatin1String("location")).toVariant().toString());
    event->setRevision(json.value(QLatin1String("sequence")).toVariant().toInt());
    if (startExists) {
        event->setDtStart(start);
    }
    if (endExists) {
        event->setHasEndDate(true);
        event->setDtEnd(end);
    } else {
        event->setHasEndDate(false);
    }
    if (isAllDay) {
        event->setAllDay(isAllDay);
    }
}

// returns true if the last sync was marked as successful, and then marks the current
// sync as being unsuccessful.  The sync adapter should set it to true manually
// once sync succeeds.
bool wasLastSyncSuccessful(int accountId)
{
    QString settingsFileName = QString::fromLatin1("%1/%2/gcal.ini")
            .arg(QString::fromLatin1(PRIVILEGED_DATA_DIR))
            .arg(QString::fromLatin1(SYNC_DATABASE_DIR));
    QSettings settingsFile(settingsFileName, QSettings::IniFormat);
    bool retn = settingsFile.value(QString::fromLatin1("%1-success").arg(accountId), QVariant::fromValue<bool>(false)).toBool();
    settingsFile.setValue(QString::fromLatin1("%1-success").arg(accountId), QVariant::fromValue<bool>(false));
    settingsFile.sync();
    return retn;
}

void setLastSyncSuccessful(QList<int> accountIds)
{
    QString settingsFileName = QString::fromLatin1("%1/%2/gcal.ini")
            .arg(QString::fromLatin1(PRIVILEGED_DATA_DIR))
            .arg(QString::fromLatin1(SYNC_DATABASE_DIR));
    QSettings settingsFile(settingsFileName, QSettings::IniFormat);
    Q_FOREACH(int accountId, accountIds) {
        settingsFile.setValue(QString::fromLatin1("%1-success").arg(accountId), QVariant::fromValue<bool>(true));
    }
    settingsFile.sync();
}

}

GoogleCalendarSyncAdaptor::GoogleCalendarSyncAdaptor(QObject *parent)
    : GoogleDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Calendars, parent)
    , m_calendar(mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QLatin1String("UTC"))))
    , m_storage(mKCal::ExtendedCalendar::defaultStorage(m_calendar))
    , m_storageNeedsSave(false)
{
    setInitialActive(m_idDb.isValid());
}

GoogleCalendarSyncAdaptor::~GoogleCalendarSyncAdaptor()
{
}

QString GoogleCalendarSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("google-calendars");
}

void GoogleCalendarSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    m_storageNeedsSave = false;
    m_storage->open(); // we close it in finalCleanup()
    GoogleDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void GoogleCalendarSyncAdaptor::finalCleanup()
{
    // commit changes to db
    if (m_storageNeedsSave) {
        m_storage->save();
    }
    m_storage->close();
    m_idDb.sync();
    m_idDb.wait();

    // set the success status for each of our account settings.
    QList<int> succeededAccounts;
    Q_FOREACH (int accountId, m_syncSucceeded.keys()) {
        if (m_syncSucceeded.value(accountId)) {
            succeededAccounts.append(accountId);
        }
    }
    if (succeededAccounts.size()) {
        setLastSyncSuccessful(succeededAccounts);
    }
}

void GoogleCalendarSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode)
{
    if (mode == SocialNetworkSyncAdaptor::CleanUpPurge) {
        // need to initialise the database
        m_storageNeedsSave = false;
        m_storage->open(); // we close it in finalCleanup()
    }

    // We clean all the entries in the calendar
    // Delete the notebooks from the storage
    foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
        if (notebook->pluginName().startsWith(QString(QLatin1String("google-")))
                && notebook->account() == QString::number(oldId)) {
            // remove the incidences and delete the notebook
            notebook->setIsReadOnly(false);
            m_storage->loadNotebookIncidences(notebook->uid());
            KCalCore::Incidence::List incidenceList;
            m_storage->allIncidences(&incidenceList, notebook->uid());
            foreach (KCalCore::Incidence::Ptr incidence, incidenceList) {
                m_calendar->deleteIncidence(m_calendar->incidence(incidence->uid()));
            }
            m_storage->deleteNotebook(notebook);
            m_storageNeedsSave = true;
        }
    }

    // Delete ids from our local->remote id mapping
    m_idDb.removeEvents(oldId);

    // Delete last update times
    m_idDb.removeLastUpdateTimes(oldId);

    if (mode == SocialNetworkSyncAdaptor::CleanUpPurge) {
        // and commit any changes made.
        finalCleanup();
    }
}

void GoogleCalendarSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    SOCIALD_LOG_DEBUG("beginning Calendar sync for Google, account" << accountId);

    bool needCleanSync = !wasLastSyncSuccessful(accountId);
    m_serverCalendarIdToSummaryAndColor[accountId].clear();
    m_calendarIdToEventObjects[accountId].clear();
    m_syncSucceeded[accountId] = true; // set to false on error
    requestCalendars(accountId, accessToken, needCleanSync);
}

void GoogleCalendarSyncAdaptor::requestCalendars(int accountId, const QString &accessToken, bool needCleanSync, const QString &pageToken)
{
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("key"), accessToken));
    if (!pageToken.isEmpty()) { // continuation request
        queryItems.append(QPair<QString, QString>(QString::fromLatin1("pageToken"),
                                                  pageToken));
    }

    QUrl url(QLatin1String("https://www.googleapis.com/calendar/v3/users/me/calendarList"));
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("GData-Version", "3.0");
    request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                         QString(QLatin1String("Bearer ") + accessToken).toUtf8());

    QNetworkReply *reply = m_networkAccessManager->get(request);

    // we're requesting data.  Increment the semaphore so that we know we're still busy.
    incrementSemaphore(accountId);

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("needCleanSync", QVariant::fromValue<bool>(needCleanSync));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(calendarsFinishedHandler()));

        setupReplyTimeout(accountId, reply);
    } else {
        SOCIALD_LOG_ERROR("unable to request calendars from Google account with id" << accountId);
        m_syncSucceeded[accountId] = false;
        decrementSemaphore(accountId);
    }
}

void GoogleCalendarSyncAdaptor::calendarsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    bool needCleanSync = reply->property("needCleanSync").toBool();
    QByteArray replyData = reply->readAll();
    bool isError = reply->property("isError").toBool();

    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    // parse the calendars' metadata from the response.
    bool fetchingNextPage = false;
    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok) {
        // first, check to see if there are more pages of calendars to fetch
        if (parsed.find(QLatin1String("nextPageToken")) != parsed.end()
                && !parsed.value(QLatin1String("nextPageToken")).toVariant().toString().isEmpty()) {
            fetchingNextPage = true;
            requestCalendars(accountId, accessToken, needCleanSync,
                             parsed.value(QLatin1String("nextPageToken")).toVariant().toString());
        }

        // second, parse the calendars' metadata
        QJsonArray items = parsed.value(QStringLiteral("items")).toArray();
        for (int i = 0; i < items.count(); ++i) {
            QJsonObject currCalendar = items.at(i).toObject();
            if (!currCalendar.isEmpty() && currCalendar.find(QStringLiteral("id")) != currCalendar.end()) {
                // we only sync calendars which the user owns (ie, not autogenerated calendars)
                QString accessRole = currCalendar.value(QStringLiteral("accessRole")).toString();
                if (accessRole == QStringLiteral("owner")) {
                    QString currCalendarId = currCalendar.value(QStringLiteral("id")).toString();
                    QString currCalendarSummary = currCalendar.value(QStringLiteral("summary")).toString();
                    QString currCalendarBgColor = currCalendar.value(QStringLiteral("backgroundColor")).toString();
                    QPair<QString, QString> summaryAndColor(currCalendarSummary, currCalendarBgColor);
                    m_serverCalendarIdToSummaryAndColor[accountId].insert(currCalendarId, summaryAndColor);
                }
            }
        }
    } else {
        // error occurred during request.
        SOCIALD_LOG_ERROR("unable to parse calendar data from request with account" << accountId << ";" <<
                          "got:" << QString::fromLatin1(replyData.constData()));
        m_syncSucceeded[accountId] = false;
    }

    if (!fetchingNextPage) {
        // we've finished loading all pages of calendar information
        // we now need to process the loaded information to determine
        // which calendars need to be added/updated/removed locally.
        updateLocalCalendarNotebooks(accountId, accessToken, needCleanSync);
    }

    // we're finished with this request.
    decrementSemaphore(accountId);
}


void GoogleCalendarSyncAdaptor::updateLocalCalendarNotebooks(int accountId, const QString &accessToken, bool needCleanSync)
{
    // any calendars which exist on the device but not the server need to be purged.
    QStringList deviceCalendarIds;
    foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
        // notebook pluginName is of form: google-calendarId
        // where the calendarId comes from the server.
        if (notebook->pluginName().startsWith(QStringLiteral("google-"))
                && notebook->account() == QString::number(accountId)) {
            QString currDeviceCalendarId = notebook->pluginName().mid(7);
            if (m_serverCalendarIdToSummaryAndColor[accountId].contains(currDeviceCalendarId)) {
                // the server-side calendar exists on the device.
                // we don't need to purge it, but we may need to update its summary/color details.
                deviceCalendarIds.append(currDeviceCalendarId);
                if (notebook->name() != m_serverCalendarIdToSummaryAndColor[accountId].value(currDeviceCalendarId).first
                        || notebook->color() != m_serverCalendarIdToSummaryAndColor[accountId].value(currDeviceCalendarId).second
                        || notebook->isReadOnly()) {
                    // summary or color changed server-side.
                    notebook->setIsReadOnly(false);
                    notebook->setName(m_serverCalendarIdToSummaryAndColor[accountId].value(currDeviceCalendarId).first);
                    notebook->setColor(m_serverCalendarIdToSummaryAndColor[accountId].value(currDeviceCalendarId).second);
                    m_storage->updateNotebook(notebook);
                    m_storageNeedsSave = true;
                }
            } else {
                // the calendar has been removed from the server.
                // we need to purge it from the device.
                SOCIALD_LOG_DEBUG("removing calendar" << notebook->name() << "for Google account:" << accountId);
                m_storage->loadNotebookIncidences(notebook->uid());
                KCalCore::Incidence::List incidenceList;
                m_storage->allIncidences(&incidenceList, notebook->uid());
                foreach (KCalCore::Incidence::Ptr incidence, incidenceList) {
                    m_calendar->deleteIncidence(m_calendar->incidence(incidence->uid()));
                }
                m_storage->deleteNotebook(notebook);
                m_storageNeedsSave = true;
            }
        }
    }

    // any calendarIds which exist on the server but not the device need to be created.
    foreach (const QString &serverCalendarId, m_serverCalendarIdToSummaryAndColor[accountId].keys()) {
        if (!deviceCalendarIds.contains(serverCalendarId)) {
            SOCIALD_LOG_DEBUG("adding new calendar" << m_serverCalendarIdToSummaryAndColor[accountId].value(serverCalendarId).first <<
                              "for Google account:" << accountId);
            mKCal::Notebook::Ptr notebook = mKCal::Notebook::Ptr(new mKCal::Notebook);
            notebook->setIsReadOnly(false);
            notebook->setName(m_serverCalendarIdToSummaryAndColor[accountId].value(serverCalendarId).first);
            notebook->setColor(m_serverCalendarIdToSummaryAndColor[accountId].value(serverCalendarId).second);
            notebook->setPluginName(QStringLiteral("google-") + serverCalendarId);
            notebook->setAccount(QString::number(accountId));
            m_storage->addNotebook(notebook);
            m_storageNeedsSave = true;
        }
    }

    SOCIALD_LOG_DEBUG("Syncing calendar events for Google account: " << accountId << " CleanSync: " << needCleanSync);

    foreach (const QString &calendarId, m_serverCalendarIdToSummaryAndColor[accountId].keys()) {
        requestEvents(accountId, accessToken, calendarId, needCleanSync);
    }
}

void GoogleCalendarSyncAdaptor::requestEvents(int accountId, const QString &accessToken, const QString &calendarId,
                                              bool needCleanSync, const QString &pageToken)
{
    QString updatedMin = m_idDb.lastUpdateTime(calendarId, accountId);
    if (updatedMin.isEmpty()) {
        QDateTime buteoLastSync = lastSyncTimestamp(QLatin1String("google"),
                                                    SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Calendars),
                                                    accountId).addSecs(2); // add 2 secs to avoid fs sync time issues.
        updatedMin = buteoLastSync.toUTC().toString(Qt::ISODate);
        SOCIALD_LOG_DEBUG("No previous update timestamp for Google account: " << accountId
                          << ". Calendar Id: " << calendarId
                          << ". Using previous buteo sync timestamp: " << updatedMin);
    } else {
        // server timestamp is inclusive. Add one second to exclude events updated on previous round
        QDateTime modified = QDateTime::fromString(updatedMin, Qt::ISODate);
        modified.setTimeSpec(Qt::UTC);
        updatedMin = modified.addSecs(1).toString(Qt::ISODate);
        SOCIALD_LOG_DEBUG("Previous update timestamp for Google account: "
                          <<  accountId << ". Calendar Id: "
                          << calendarId << ". Timestamp: " << updatedMin);
    }

    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("key"),
                                              accessToken));
    if (!needCleanSync && !updatedMin.isEmpty()) {
        // we're doing a delta update.  We set the "since" field, and request deletions be shown.
        queryItems.append(QPair<QString, QString>(QString::fromLatin1("updatedMin"), updatedMin));
        queryItems.append(QPair<QString, QString>(QString::fromLatin1("showDeleted"),
                                                  QString::fromLatin1("true")));
    }
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("timeMin"),
                                              QDateTime::currentDateTimeUtc().addMonths(-3).toString(Qt::ISODate)));
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("timeMax"),
                                              QDateTime::currentDateTimeUtc().addMonths(12).toString(Qt::ISODate)));
    if (!pageToken.isEmpty()) { // continuation request
        queryItems.append(QPair<QString, QString>(QString::fromLatin1("pageToken"),
                                                  pageToken));
    }

    QUrl url(QString::fromLatin1("https://www.googleapis.com/calendar/v3/calendars/%1/events").arg(calendarId));
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("GData-Version", "3.0");
    request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                         QString(QLatin1String("Bearer ") + accessToken).toUtf8());

    QNetworkReply *reply = m_networkAccessManager->get(request);

    // we're requesting data.  Increment the semaphore so that we know we're still busy.
    incrementSemaphore(accountId);

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("calendarId", calendarId);
        reply->setProperty("needCleanSync", needCleanSync);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(eventsFinishedHandler()));

        SOCIALD_LOG_DEBUG("requesting calendar events for Google account:" << accountId << ":" << url.toString());

        setupReplyTimeout(accountId, reply);
    } else {
        SOCIALD_LOG_ERROR("unable to request events for calendar" << calendarId <<
                          "from Google account with id" << accountId);
        m_syncSucceeded[accountId] = false;
        decrementSemaphore(accountId);
    }
}

void GoogleCalendarSyncAdaptor::eventsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString calendarId = reply->property("calendarId").toString();
    QString accessToken = reply->property("accessToken").toString();
    bool needCleanSync = reply->property("needCleanSync").toBool();
    QByteArray replyData = reply->readAll();
    bool isError = reply->property("isError").toBool();

    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    bool fetchingNextPage = false;
    bool ok = false;
    QString updated;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok) {
        // If there are more pages of results to fetch, ensure we fetch them
        if (parsed.find(QLatin1String("nextPageToken")) != parsed.end()
                && !parsed.value(QLatin1String("nextPageToken")).toVariant().toString().isEmpty()) {
            fetchingNextPage = true;
            requestEvents(accountId, accessToken, calendarId, needCleanSync,
                          parsed.value(QLatin1String("nextPageToken")).toVariant().toString());
        }

        updated = parsed.value(QLatin1String("updated")).toVariant().toString();

        // Parse the event list
        QJsonArray dataList = parsed.value(QLatin1String("items")).toArray();
        foreach (const QJsonValue &item, dataList) {
            QJsonObject eventData = item.toObject();

            // otherwise, we queue the event for insertion into the database.
            m_calendarIdToEventObjects[accountId].insertMulti(calendarId, eventData);
        }
    } else {
        // error occurred during request.
        SOCIALD_LOG_ERROR("unable to parse event data from request with account" << accountId << ";"
                          "got:" << QString::fromLatin1(replyData.constData()));
        m_syncSucceeded[accountId] = false;
    }

    if (!fetchingNextPage) {
        // we've finished loading all pages of event information
        // we now need to process the loaded information to determine
        // which events need to be added/updated/removed locally.
        QDateTime since = needCleanSync ? QDateTime()
                                        : lastSyncTimestamp(QLatin1String("google"),
                                                            SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Calendars),
                                                            accountId).addSecs(2); // add 2 secs to avoid fs sync time issues.

        if (!updated.isEmpty()) {
            m_idDb.setLastUpdateTime(calendarId, accountId, updated);
            SOCIALD_LOG_ERROR("Setting updated timestamp for Google account: " << accountId << ". Calendar Id: " << calendarId << ".  Timestamp: " << updated);
        }
        updateLocalCalendarNotebookEvents(accountId, accessToken, calendarId, since);
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

void GoogleCalendarSyncAdaptor::updateLocalCalendarNotebookEvents(int accountId, const QString &accessToken, const QString &calendarId, const QDateTime &since)
{
    Q_UNUSED(accessToken) // in the future, we might need it to download images/data associated with the event.

    // Search for the device Notebook matching this CalendarId
    bool found = false;
    mKCal::Notebook::Ptr googleNotebook;
    foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
        if (notebook->pluginName() == QString::fromLatin1("google-%1").arg(calendarId)
                && notebook->account() == QString::number(accountId)) {
            googleNotebook = notebook;
            found = true;
        }
    }

    if (!found) {
        SOCIALD_LOG_ERROR("calendar" << calendarId <<
                          "doesn't have a notebook for Google account with id" << accountId);
        m_syncSucceeded[accountId] = false;
        return;
    }

    // Set notebook writeable locally.
    googleNotebook->setIsReadOnly(false);

    // check to see if we're doing a delta update or a clean sync
    m_storage->loadNotebookIncidences(googleNotebook->uid());
    KCalCore::Incidence::List deletedList, addedList, updatedList, allList;
    QMap<QString, KCalCore::Event::Ptr> allMap, updatedMap;
    QMap<QString, QString> deletedMap; // gcalId to incidenceUid
    if (since.isValid()) {
        // delta sync.  populate our lists.
        m_storage->allIncidences(&allList, googleNotebook->uid());
        m_storage->deletedIncidences(&deletedList, KDateTime(since), googleNotebook->uid());  // TODO: since UTC?
        m_storage->insertedIncidences(&addedList, KDateTime(since), googleNotebook->uid());   // TODO: since UTC?
        m_storage->modifiedIncidences(&updatedList, KDateTime(since), googleNotebook->uid()); // TODO: since UTC?
        Q_FOREACH(const KCalCore::Incidence::Ptr incidence, allList) {
            QString gcalId = gCalEventId(incidence);
            KCalCore::Event::Ptr eventPtr = m_calendar->event(incidence->uid());
            if (gcalId.size() && eventPtr) {
                allMap.insert(gcalId, eventPtr);
            } // else, newly added locally, no gcalId yet.
        }
        Q_FOREACH(const KCalCore::Incidence::Ptr incidence, updatedList) {
            QString gcalId = gCalEventId(incidence);
            KCalCore::Event::Ptr eventPtr = m_calendar->event(incidence->uid());
            if (gcalId.size() && eventPtr) {
                updatedMap.insert(gcalId, eventPtr);
            } // else, newly added+updated locally, no gcalId yet.
        }
        Q_FOREACH(const KCalCore::Incidence::Ptr incidence, deletedList) {
            // We would like to do the following, but mkcal removes
            // custom properties of deleted incidences:
            //QString gcalId = gCalEventId(incidence);
            // Instead, read from the out-of-band id database.
            QString gcalId = m_idDb.gcalEventId(accountId, googleNotebook->uid(), incidence->uid());
            if (gcalId.size()) {
                m_idDb.removeEvent(accountId, gcalId); // it has been removed from mkcal.
                deletedMap.insert(gcalId, incidence->uid());
                updatedMap.remove(gcalId); // don't upsync updates to deleted events.
            } // else, newly added+deleted locally, no gcalId yet.
        }
    } else {
        // clean sync requires clobbering the notebook,
        // just in case a previous sync left artifacts.
        QString nbName = googleNotebook->name();
        QString nbColor = googleNotebook->color();
        QString nbPluginName = googleNotebook->pluginName();
        QString nbAccount = googleNotebook->account();
        QString nbUid = googleNotebook->uid();

        m_storage->deleteNotebook(googleNotebook);

        googleNotebook = mKCal::Notebook::Ptr(new mKCal::Notebook);
        googleNotebook->setUid(nbUid);
        googleNotebook->setIsReadOnly(false);
        googleNotebook->setName(nbName);
        googleNotebook->setColor(nbColor);
        googleNotebook->setPluginName(nbPluginName);
        googleNotebook->setAccount(nbAccount);
        m_storage->addNotebook(googleNotebook);

        m_storage->loadNotebookIncidences(googleNotebook->uid());
        m_storageNeedsSave = true;

        // clean the local->remote id mappings for this notebook (if they exist)
        if (!nbUid.isEmpty()) {
            m_idDb.removeEvents(accountId, nbUid);
        }
    }

    // for each each of the events downloaded from the server, create a local event.
    int remoteAdded = 0, remoteModified = 0, remoteRemoved = 0;
    foreach (const QJsonObject &eventData, m_calendarIdToEventObjects[accountId].values(calendarId)) {
        QString eventId = eventData.value(QLatin1String("id")).toVariant().toString();
        bool eventWasDeletedRemotely = eventData.value(QLatin1String("status")).toVariant().toString() == QString::fromLatin1("cancelled");
        if (eventWasDeletedRemotely) {
            // delete existing event.
            remoteRemoved++;

            m_idDb.removeEvent(accountId, eventId);
            if (allMap.contains(eventId)) {
                m_calendar->deleteEvent(allMap.value(eventId));
                m_storageNeedsSave = true;
            } // else already deleted locally, can ignore.
        } else if (deletedMap.contains(eventId)) {
            // event was deleted locally, can ignore.
        } else if (allMap.contains(eventId)) {
            // modify existing event.
            remoteModified++;
            KCalCore::Event::Ptr event = allMap.value(eventId);

            // if both local and server were modified, prefer server.
            updatedMap.remove(eventId);

            // then, update local event appropriately.
            event->startUpdates();
            jsonToKCal(eventData, event, m_icalFormat);
            event->endUpdates();
            m_storageNeedsSave = true;
        } else {
            // add a new local event
            remoteAdded++;
            KCalCore::Event::Ptr event = KCalCore::Event::Ptr(new KCalCore::Event);
            jsonToKCal(eventData, event, m_icalFormat); // direct conversion
            m_calendar->addEvent(event, googleNotebook->uid());
            m_storageNeedsSave = true;
            m_idDb.insertEvent(accountId, eventId, googleNotebook->uid(), event->uid());
        }
    }

    SOCIALD_LOG_INFO((since.isValid() ? "Delta" : "Clean") <<
                     "sync with Google calendar" << googleNotebook->name() << "for account" << accountId << ":"
                     "remote A/M/R:" << remoteAdded << "/" << remoteModified << "/" << remoteRemoved);

    // only upsync changes if we're doing a delta sync, and upsync is enabled
    if (!m_accountSyncProfile || m_accountSyncProfile->syncDirection() != Buteo::SyncProfile::SYNC_DIRECTION_FROM_REMOTE) {
        if (since.isValid()) {
            // And push our changes up to the server.  XXX TODO: Request Batching!
            int localAdded = 0, localModified = 0, localRemoved = 0;

            // first, push up deletions.
            Q_FOREACH (const QString &deletedGcalId, deletedMap.keys()) {
                QString incidenceUid = deletedMap.value(deletedGcalId);
                localRemoved++;
                upsyncChanges(accountId, accessToken, GoogleCalendarSyncAdaptor::UpsyncDelete,
                              incidenceUid, calendarId, deletedGcalId, QByteArray());
            }

            // second, push up modifications.
            Q_FOREACH (const QString &updatedGcalId, updatedMap.keys()) {
                KCalCore::Event::Ptr event = updatedMap.value(updatedGcalId);
                if (event) {
                    localModified++;
                    upsyncChanges(accountId, accessToken, GoogleCalendarSyncAdaptor::UpsyncModify,
                                  event->uid(), calendarId, updatedGcalId, QJsonDocument(kCalToJson(event, m_icalFormat)).toJson());
                }
            }

            // finally, push up insertions.
            Q_FOREACH (KCalCore::Incidence::Ptr incidence, addedList) {
                KCalCore::Event::Ptr event = m_calendar->event(incidence->uid());
                if (event) {
                    localAdded++;
                    upsyncChanges(accountId, accessToken, GoogleCalendarSyncAdaptor::UpsyncInsert,
                                  event->uid(), calendarId, QString(), QJsonDocument(kCalToJson(event, m_icalFormat)).toJson());
                }
            }

            SOCIALD_LOG_INFO("Delta sync with Google calendar" << googleNotebook->name() << "for account" << accountId << ":" <<
                             "local A/M/R:" << localAdded << "/" << localModified << "/" << localRemoved);
        }
    } else {
        SOCIALD_LOG_INFO("skipping upload of local calendar changes due to profile direction setting for account" << accountId);
    }
}

void GoogleCalendarSyncAdaptor::upsyncChanges(int accountId, const QString &accessToken,
                                              GoogleCalendarSyncAdaptor::UpsyncType upsyncType,
                                              const QString &kcalEventId, const QString &calendarId,
                                              const QString &eventId,const QByteArray &eventData)
{
    QUrl requestUrl = upsyncType == GoogleCalendarSyncAdaptor::UpsyncInsert
                    ? QUrl(QString::fromLatin1("https://www.googleapis.com/calendar/v3/calendars/%1/events").arg(calendarId))
                    : QUrl(QString::fromLatin1("https://www.googleapis.com/calendar/v3/calendars/%1/events/%2").arg(calendarId).arg(eventId));

    QNetworkRequest request(requestUrl);
    request.setRawHeader("GData-Version", "3.0");
    request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                         QString(QLatin1String("Bearer ") + accessToken).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QVariant::fromValue<QString>(QString::fromLatin1("application/json")));

    QNetworkReply *reply = 0;

    QString upsyncTypeStr;
    switch (upsyncType) {
        case GoogleCalendarSyncAdaptor::UpsyncInsert:
            upsyncTypeStr = QString::fromLatin1("Insert");
            reply = m_networkAccessManager->post(request, eventData);
            break;
        case GoogleCalendarSyncAdaptor::UpsyncModify:
            upsyncTypeStr = QString::fromLatin1("Modify");
            reply = m_networkAccessManager->put(request, eventData);
            break;
        case GoogleCalendarSyncAdaptor::UpsyncDelete: // flow through
        default:
            upsyncTypeStr = QString::fromLatin1("Delete");
            reply = m_networkAccessManager->deleteResource(request);
            break;
    }

    // we're performing a request.  Increment the semaphore so that we know we're still busy.
    incrementSemaphore(accountId);

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("kcalEventId", kcalEventId);
        reply->setProperty("calendarId", calendarId);
        reply->setProperty("eventId", eventId);
        reply->setProperty("upsyncType", static_cast<int>(upsyncType));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(upsyncFinishedHandler()));

        setupReplyTimeout(accountId, reply);

        SOCIALD_LOG_DEBUG("upsyncing change:" << upsyncTypeStr <<
                          "to calendarId:" << calendarId <<
                          "of account" << accountId << ":\n" <<
                          request.url().toString() << "\n" <<
                          QString::fromUtf8(eventData));
    } else {
        SOCIALD_LOG_ERROR("unable to request upsync for calendar" << calendarId <<
                          "from Google account with id" << accountId);
        m_syncSucceeded[accountId] = false;
        decrementSemaphore(accountId);
    }
}

void GoogleCalendarSyncAdaptor::upsyncFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString kcalEventId = reply->property("kcalEventId").toString();
    QString calendarId = reply->property("calendarId").toString();
    int upsyncType = reply->property("upsyncType").toInt();
    QByteArray replyData = reply->readAll();
    bool isError = reply->property("isError").toBool();

    // QNetworkReply can report an error even if there isn't one...
    if (isError && reply->error() == QNetworkReply::UnknownContentError
            && upsyncType == GoogleCalendarSyncAdaptor::UpsyncDelete) {
        isError = false; // not a real error; Google returns an empty response.
    }

    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    // parse the calendars' metadata from the response.
    if (isError) {
        // error occurred during request.
        SOCIALD_LOG_ERROR("error occurred while upsyncing calendar data to Google account" << accountId << ";" <<
                          "got:" << QString::fromLatin1(replyData.constData()));
        m_syncSucceeded[accountId] = false;
    } else if (upsyncType == GoogleCalendarSyncAdaptor::UpsyncDelete) {
        // we expect an empty response body on success for Delete operations
        if (!replyData.isEmpty()) {
            SOCIALD_LOG_ERROR("error occurred while upsyncing calendar event deletion to Google account" << accountId << ";" <<
                              "got:" << QString::fromLatin1(replyData.constData()));
            m_syncSucceeded[accountId] = false;
        }
    } else {
        // we expect an event resource body on success for Insert/Modify requests.
        bool ok = false;
        QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
        if (!ok) {
            QString typeStr = upsyncType == GoogleCalendarSyncAdaptor::UpsyncInsert
                            ? QString::fromLatin1("insertion")
                            : QString::fromLatin1("modification");
            SOCIALD_LOG_ERROR("error occurred while upsyncing calendar event" << typeStr <<
                              "to Google account" << accountId << ";" <<
                              "got:" << QString::fromLatin1(replyData.constData()));
            m_syncSucceeded[accountId] = false;
        } else {
            // update the event in our local database.
            // TODO: reduce code duplication between here and the other function.
            // Search for the device Notebook matching this CalendarId
            bool found = false;
            mKCal::Notebook::Ptr googleNotebook;
            foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
                if (notebook->pluginName() == QString::fromLatin1("google-%1").arg(calendarId)
                        && notebook->account() == QString::number(accountId)) {
                    googleNotebook = notebook;
                    found = true;
                }
            }

            if (!found) {
                SOCIALD_LOG_ERROR("calendar" << calendarId << "doesn't have a notebook for Google account with id" << accountId);
                m_syncSucceeded[accountId] = false;
            } else {
                // update this event in the local calendar
                m_storage->loadNotebookIncidences(googleNotebook->uid());
                m_storage->load(kcalEventId);
                KCalCore::Event::Ptr event = m_calendar->event(kcalEventId);
                if (!event) {
                    SOCIALD_LOG_ERROR("event" << kcalEventId << "was deleted locally during sync of Google account with id" << accountId);
                    m_syncSucceeded[accountId] = false;
                } else {
                    QString oldDTS = event->dtStart().toString(RFC3339_FORMAT);
                    QString oldDTE = event->dtEnd().toString(RFC3339_FORMAT);
                    event->startUpdates();
                    jsonToKCal(parsed, event, m_icalFormat);
                    SOCIALD_LOG_DEBUG("Two-way calendar sync with account" << accountId << ":\n" <<
                                      "  re-updating event" << event->summary() << ":\n" <<
                                      "  old start:" << oldDTS << ", old end:" << oldDTE << "\n" <<
                                      "  new start:" << event->dtStart().toString(RFC3339_FORMAT) <<
                                      ", new end:" << event->dtEnd().toString(RFC3339_FORMAT) << "\n");
                    event->endUpdates();
                    m_storageNeedsSave = true;
                    m_idDb.insertEvent(accountId, gCalEventId(event), googleNotebook->uid(), kcalEventId);
                }

                QString updated = parsed.value(QLatin1String("updated")).toVariant().toString();
                if (!updated.isEmpty()) {
                    m_idDb.setLastUpdateTime(calendarId, accountId, updated);
                }
            }
        }
    }

    // we're finished with this request.
    decrementSemaphore(accountId);
}
