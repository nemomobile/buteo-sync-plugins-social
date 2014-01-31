/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
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
        end.insert(QLatin1String("date"), event->dateEnd().toString(QDATEONLY_FORMAT));
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
            *start = KDateTime(QDate::fromString(endTimeString, QDATEONLY_FORMAT), QTime(), KDateTime::ClockTime);
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
            // Hack to work around mkcal issues: if the end date is the same
            // as the start date, then it's an all day event, and we have
            // to set some magic flags.
            if (*startExists && *startIsDateOnly) {
                if (QDate::fromString(startTimeString, QDATEONLY_FORMAT)
                        == QDate::fromString(endTimeString, QDATEONLY_FORMAT)) {
                    // single-day all-day event
                    *endExists = false;
                    *isAllDay = true;
                } else {
                    // multi-day all-day event.  Work around mkcal bugs.
                    *start = KDateTime(QDate::fromString(startTimeString, QDATEONLY_FORMAT), QTime(), KDateTime::ClockTime);
                    *end = KDateTime(QDate::fromString(endTimeString, QDATEONLY_FORMAT), QTime(), KDateTime::ClockTime);
                    *isAllDay = true;
                }
            } else {
                *end = KDateTime(QDate::fromString(endTimeString, QDATEONLY_FORMAT), QTime(), KDateTime::ClockTime);
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
                TRACE(SOCIALD_DEBUG,
                        QString::fromLatin1("unable to parse RRULE information: %1\nfrom: %2")
                        .arg(ruleStr).arg(QString::fromUtf8(QJsonDocument(recurrence).toJson())));
            } else {
                kcalRecurrence->addRRule(rrule);
            }
        } else if (ruleStr.toLower().startsWith(QString::fromLatin1("exrule:"))) {
            KCalCore::RecurrenceRule *exrule = new KCalCore::RecurrenceRule;
            if (!icalFormat.fromString(exrule, ruleStr.mid(7))) {
                TRACE(SOCIALD_DEBUG,
                        QString::fromLatin1("unable to parse EXRULE information: %1\nfrom: %2")
                        .arg(ruleStr).arg(QString::fromUtf8(QJsonDocument(recurrence).toJson())));
            } else {
                kcalRecurrence->addExRule(exrule);
            }
        } else if (ruleStr.toLower().startsWith(QString::fromLatin1("rdate:"))) {
            QDate rdate = QDate::fromString(ruleStr.mid(6), "yyyy-MM-dd");
            if (!rdate.isValid()) {
                TRACE(SOCIALD_DEBUG,
                        QString::fromLatin1("unable to parse RDATE information: %1\nfrom: %2")
                        .arg(ruleStr).arg(QString::fromUtf8(QJsonDocument(recurrence).toJson())));
            } else {
                kcalRecurrence->addRDate(rdate);
            }
        } else if (ruleStr.toLower().startsWith(QString::fromLatin1("exdate:"))) {
            QDate exdate = QDate::fromString(ruleStr.mid(7), "yyyy-MM-dd");
            if (!exdate.isValid()) {
                TRACE(SOCIALD_DEBUG,
                        QString::fromLatin1("unable to parse EXDATE information: %1\nfrom: %2")
                        .arg(ruleStr).arg(QString::fromUtf8(QJsonDocument(recurrence).toJson())));
            } else {
                kcalRecurrence->addExDate(exdate);
            }
        } else {
            TRACE(SOCIALD_DEBUG,
                    QString::fromLatin1("unknown recurrence information: %1\nfrom: %2")
                    .arg(ruleStr).arg(QString::fromUtf8(QJsonDocument(recurrence).toJson())));
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

//----------------------------------------------

GoogleCalendarSyncAdaptor::GoogleCalendarSyncAdaptor(SyncService *syncService, QObject *parent)
    : GoogleDataTypeSyncAdaptor(syncService, SyncService::Calendars, parent)
    , m_calendar(mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QLatin1String("UTC"))))
    , m_storage(mKCal::ExtendedCalendar::defaultStorage(m_calendar))
{
    setInitialActive(true);
}

GoogleCalendarSyncAdaptor::~GoogleCalendarSyncAdaptor()
{
}

void GoogleCalendarSyncAdaptor::sync(const QString &dataTypeString)
{
    m_storage->open(); // we close it in finalCleanup()
    GoogleDataTypeSyncAdaptor::sync(dataTypeString);
}

void GoogleCalendarSyncAdaptor::purgeDataForOldAccounts(const QList<int> &oldIds)
{
    // We clean all the entries in the calendar
    foreach (int accountId, oldIds) {
        // Delete the notebooks from the storage
        foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
            if (notebook->pluginName().startsWith(QString(QLatin1String("google-")))
                    && notebook->account() == QString::number(accountId)) {
                // remove the incidences and delete the notebook
                notebook->setIsReadOnly(false);
                m_storage->loadNotebookIncidences(notebook->uid());
                KCalCore::Incidence::List incidenceList;
                m_storage->allIncidences(&incidenceList, notebook->uid());
                foreach (KCalCore::Incidence::Ptr incidence, incidenceList) {
                    m_calendar->deleteIncidence(m_calendar->incidence(incidence->uid()));
                }
                m_storage->deleteNotebook(notebook);
            }
        }
    }

    m_storage->save();
}

void GoogleCalendarSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    TRACE(SOCIALD_DEBUG,
          QString(QLatin1String("Beginning Calendar sync for Google, account %1"))
          .arg(accountId));

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

    QNetworkReply *reply = networkAccessManager->get(request);

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
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request calendars"
                                      " from Google account with id %1"))
                .arg(accountId));
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
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse calendar data from request with account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
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
                }
            } else {
                // the calendar has been removed from the server.
                // we need to purge it from the device.
                TRACE(SOCIALD_DEBUG,
                      QString(QLatin1String("Removing calendar %1 for Google account: %2"))
                      .arg(notebook->name())
                      .arg(accountId));
                m_storage->loadNotebookIncidences(notebook->uid());
                KCalCore::Incidence::List incidenceList;
                m_storage->allIncidences(&incidenceList, notebook->uid());
                foreach (KCalCore::Incidence::Ptr incidence, incidenceList) {
                    m_calendar->deleteIncidence(m_calendar->incidence(incidence->uid()));
                }
                m_storage->deleteNotebook(notebook);
            }
        }
    }

    // any calendarIds which exist on the server but not the device need to be created.
    foreach (const QString &serverCalendarId, m_serverCalendarIdToSummaryAndColor[accountId].keys()) {
        if (!deviceCalendarIds.contains(serverCalendarId)) {
            TRACE(SOCIALD_DEBUG,
                  QString(QLatin1String("Adding new calendar %1 for Google account: %2"))
                  .arg(m_serverCalendarIdToSummaryAndColor[accountId].value(serverCalendarId).first)
                  .arg(accountId));
            mKCal::Notebook::Ptr notebook = mKCal::Notebook::Ptr(new mKCal::Notebook);
            notebook->setIsReadOnly(false);
            notebook->setName(m_serverCalendarIdToSummaryAndColor[accountId].value(serverCalendarId).first);
            notebook->setColor(m_serverCalendarIdToSummaryAndColor[accountId].value(serverCalendarId).second);
            notebook->setPluginName(QStringLiteral("google-") + serverCalendarId);
            notebook->setAccount(QString::number(accountId));
            m_storage->addNotebook(notebook);
        }
    }

    // commit changes to calendar backend on device.
    // addNotebook/updateNotebook/deleteNotebook are all synchronous
    // but deleteIncidence may not be.
    m_storage->save();

    // Finally, request the events for each calendar.
    // If the last sync was successful, we can do a fast sync (using change deltas).
    // NOTE: this "since" value was written at the completion of the previous
    // sync operation.  This means that there is a time-delta between when the
    // sync occurs, and when the timestamp is written to the database.  Any
    // local or remote modifications which occur during this time-delta might not
    // be synced correctly... XXX TODO: FIXME!!!
    QDateTime since = needCleanSync
                    ? QDateTime()
                    : lastSyncTimestamp(QLatin1String("google"),
                                        SyncService::dataType(SyncService::Calendars),
                                        accountId).addSecs(2); // add 2 secs to avoid fs sync time issues.

    TRACE(SOCIALD_DEBUG,
          QString(QLatin1String("Syncing calendar events for Google account: %1.  CleanSync: %2.  Since: %3."))
          .arg(accountId).arg(needCleanSync || !since.isValid()).arg(since.toString(Qt::ISODate)));

    foreach (const QString &calendarId, m_serverCalendarIdToSummaryAndColor[accountId].keys()) {
        requestEvents(accountId, accessToken, calendarId, since);
    }
}

void GoogleCalendarSyncAdaptor::requestEvents(int accountId, const QString &accessToken, const QString &calendarId, const QDateTime &since, const QString &pageToken)
{
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("key"),
                                              accessToken));
    if (since.isValid()) {
        // we're doing a delta update.  We set the "since" field, and request deletions be shown.
        queryItems.append(QPair<QString, QString>(QString::fromLatin1("updatedMin"),
                                                  since.toUTC().toString(Qt::ISODate)));
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

    QNetworkReply *reply = networkAccessManager->get(request);

    // we're requesting data.  Increment the semaphore so that we know we're still busy.
    incrementSemaphore(accountId);

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("calendarId", calendarId);
        reply->setProperty("since", since);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(eventsFinishedHandler()));

        TRACE(SOCIALD_DEBUG,
              QString(QLatin1String("Requesting calendar events for Google account: %1: %2"))
              .arg(accountId).arg(url.toString()));
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request events for calendar %1"
                                      " from Google account with id %2"))
                .arg(calendarId).arg(accountId));
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
    QDateTime since = reply->property("since").toDateTime();
    QByteArray replyData = reply->readAll();
    bool isError = reply->property("isError").toBool();

    disconnect(reply);
    reply->deleteLater();

    bool fetchingNextPage = false;
    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok) {
        // If there are more pages of results to fetch, ensure we fetch them
        if (parsed.find(QLatin1String("nextPageToken")) != parsed.end()
                && !parsed.value(QLatin1String("nextPageToken")).toVariant().toString().isEmpty()) {
            fetchingNextPage = true;
            requestEvents(accountId, accessToken, calendarId, since,
                          parsed.value(QLatin1String("nextPageToken")).toVariant().toString());
        }

        // Parse the event list
        QJsonArray dataList = parsed.value(QLatin1String("items")).toArray();
        foreach (const QJsonValue &item, dataList) {
            QJsonObject eventData = item.toObject();

            // otherwise, we queue the event for insertion into the database.
            m_calendarIdToEventObjects[accountId].insertMulti(calendarId, eventData);
        }
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse event data from request with account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
        m_syncSucceeded[accountId] = false;
    }

    if (!fetchingNextPage) {
        // we've finished loading all pages of event information
        // we now need to process the loaded information to determine
        // which events need to be added/updated/removed locally.
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
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: calendar %1 doesn't have a notebook"
                                      " for Google account with id %2"))
                .arg(calendarId).arg(accountId));
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
            // XXX TODO: FIXME: this doesn't seem to work...
            // mkcal deletes extended properties of deleted incidences?
            // We cannot find the gcal event id of deleted incidences...
            QString gcalId = gCalEventId(incidence);
            if (gcalId.size()) {
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

        m_storage->save();
        m_storage->loadNotebookIncidences(googleNotebook->uid());
    }

    // for each each of the events downloaded from the server, create a local event.
    int remoteAdded = 0, remoteModified = 0, remoteRemoved = 0;
    foreach (const QJsonObject &eventData, m_calendarIdToEventObjects[accountId].values(calendarId)) {
        QString eventId = eventData.value(QLatin1String("id")).toVariant().toString();
        bool eventWasDeletedRemotely = eventData.value(QLatin1String("status")).toVariant().toString() == QString::fromLatin1("cancelled");
        if (eventWasDeletedRemotely) {
            // delete existing event.
            remoteRemoved++;
            if (allMap.contains(eventId)) {
                m_calendar->deleteEvent(allMap.value(eventId));
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
        } else {
            // add a new local event
            remoteAdded++;
            KCalCore::Event::Ptr event = KCalCore::Event::Ptr(new KCalCore::Event);
            jsonToKCal(eventData, event, m_icalFormat); // direct conversion
            m_calendar->addEvent(event, googleNotebook->uid());
        }
    }

    TRACE(SOCIALD_INFORMATION,
          QString(QLatin1String("%1 sync with Google calendar %2 for account %3: remote A/M/R: %4 / %5 / %6"))
          .arg(since.isValid() ? "Delta" : "Clean")
          .arg(googleNotebook->name()).arg(accountId)
          .arg(remoteAdded).arg(remoteModified).arg(remoteRemoved));

    // only upsync changes if we're doing a delta sync.
    if (since.isValid()) {
        // And push our changes up to the server.  XXX TODO: Request Batching!
        int localAdded = 0, localModified = 0, localRemoved = 0;

        // first, push up deletions.
        Q_FOREACH (const QString &deletedGcalId, deletedMap) {
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

        TRACE(SOCIALD_INFORMATION,
              QString(QLatin1String("Delta sync with Google calendar %1 for account %2: local A/M/R: %3 / %4 / %5"))
              .arg(googleNotebook->name()).arg(accountId)
              .arg(localAdded).arg(localModified).arg(localRemoved));
    }

    // Write changes to local calendar.
    m_storage->save();
}

void GoogleCalendarSyncAdaptor::finalCleanup()
{
    // commit changes to db
    m_storage->save();
    m_storage->close();

    // set the success status for each of our account settings.
    QList<int> succeededAccounts;
    Q_FOREACH (int accountId, m_syncSucceeded.keys()) {
        if (m_syncSucceeded.value(accountId)) {
            succeededAccounts.append(accountId);
        }
    }
    setLastSyncSuccessful(succeededAccounts);
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
            reply = networkAccessManager->post(request, eventData);
            break;
        case GoogleCalendarSyncAdaptor::UpsyncModify:
            upsyncTypeStr = QString::fromLatin1("Modify");
            reply = networkAccessManager->put(request, eventData);
            break;
        case GoogleCalendarSyncAdaptor::UpsyncDelete: // flow through
        default:
            upsyncTypeStr = QString::fromLatin1("Delete");
            reply = networkAccessManager->deleteResource(request);
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

        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("Upsyncing change: %1 to calendarId: %2 of account %3:\n%4\n"))
                .arg(upsyncTypeStr).arg(calendarId).arg(accountId).arg(QString::fromUtf8(eventData)));
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request upsync for calendar %1"
                                      " from Google account with id %2"))
                .arg(calendarId).arg(accountId));
        m_syncSucceeded[accountId] = false;
        decrementSemaphore(accountId);
    }
}

void GoogleCalendarSyncAdaptor::upsyncFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QString kcalEventId = reply->property("kcalEventId").toString();
    QString calendarId = reply->property("calendarId").toString();
    int upsyncType = reply->property("upsyncType").toInt();
    QByteArray replyData = reply->readAll();
    bool isError = reply->property("isError").toBool();

    disconnect(reply);
    reply->deleteLater();

    // parse the calendars' metadata from the response.
    if (isError) {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error occurred while upsyncing calendar data to Google account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
        m_syncSucceeded[accountId] = false;
    } else if (upsyncType == GoogleCalendarSyncAdaptor::UpsyncDelete && !replyData.isEmpty()) {
        // we expect an empty response body on success for Delete operations
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error occurred while upsyncing calendar event deletion to Google account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
        m_syncSucceeded[accountId] = false;
    } else {
        // we expect an event resource body on success
        bool ok = false;
        QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
        if (!ok) {
            QString typeStr = upsyncType == GoogleCalendarSyncAdaptor::UpsyncInsert
                            ? QString::fromLatin1("insertion")
                            : QString::fromLatin1("modification");
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error occurred while upsyncing calendar event %1 to Google account %2; got: %3"))
                    .arg(typeStr).arg(accountId).arg(QString::fromLatin1(replyData.constData())));
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
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: calendar %1 doesn't have a notebook"
                                              " for Google account with id %2"))
                        .arg(calendarId).arg(accountId));
                m_syncSucceeded[accountId] = false;
            } else {
                // update this event in the local calendar
                m_storage->loadNotebookIncidences(googleNotebook->uid());
                m_storage->load(kcalEventId);
                KCalCore::Event::Ptr event = m_calendar->event(kcalEventId);
                if (!event) {
                    TRACE(SOCIALD_ERROR,
                            QString(QLatin1String("event %1 was deleted locally during sync"
                                                  " of Google account with id %2"))
                            .arg(kcalEventId).arg(accountId));
                    m_syncSucceeded[accountId] = false;
                } else {
                    QString oldDTS = event->dtStart().toString(RFC3339_FORMAT);
                    QString oldDTE = event->dtEnd().toString(RFC3339_FORMAT);
                    event->startUpdates();
                    jsonToKCal(parsed, event, m_icalFormat);
                    TRACE(SOCIALD_DEBUG,
                            QString(QLatin1String("Two-way calendar sync with account %1:\n"
                                                  "  re-updating event %2:\n"
                                                  "  old start: %3 old end: %4\n"
                                                  "  new start: %5 new end: %6\n"))
                            .arg(accountId)
                            .arg(event->summary())
                            .arg(oldDTS)
                            .arg(oldDTE)
                            .arg(event->dtStart().toString(RFC3339_FORMAT))
                            .arg(event->dtEnd().toString(RFC3339_FORMAT)));
                    event->endUpdates();
                }
            }
        }
    }

    // we're finished with this request.
    decrementSemaphore(accountId);
}

