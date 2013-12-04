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
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <extendedcalendar.h>
#include <extendedstorage.h>


GoogleCalendarSyncAdaptor::GoogleCalendarSyncAdaptor(SyncService *syncService, QObject *parent)
    : GoogleDataTypeSyncAdaptor(syncService, SyncService::Calendars, parent)
{
    setInitialActive(true);
}

GoogleCalendarSyncAdaptor::~GoogleCalendarSyncAdaptor()
{
}

void GoogleCalendarSyncAdaptor::purgeDataForOldAccounts(const QList<int> &oldIds)
{
    // We clean all the entries in the calendar
    mKCal::ExtendedCalendar::Ptr calendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QLatin1String("UTC")));
    mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(calendar);
    storage->open();

    foreach (int accountId, oldIds) {
        // Delete the notebooks from the storage
        foreach (mKCal::Notebook::Ptr notebook, storage->notebooks()) {
            if (notebook->pluginName().startsWith(QString(QLatin1String("google-")))
                    && notebook->account() == QString::number(accountId)) {
                KCalCore::Incidence::List incidenceList;
                storage->allIncidences(&incidenceList, notebook->uid());
                foreach (KCalCore::Incidence::Ptr incidence, incidenceList) {
                    calendar->deleteIncidence(incidence);
                }
                storage->deleteNotebook(notebook);
            }
        }
    }

    storage->save();
    storage->close();
}

void GoogleCalendarSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    TRACE(SOCIALD_DEBUG,
          QString(QLatin1String("Beginning Calendar sync for Google, account %1"))
          .arg(accountId));

    m_serverCalendarIdToSummaryAndColor[accountId].clear();
    m_calendarIdToEventObjects[accountId].clear();
    requestCalendars(accountId, accessToken);
}

void GoogleCalendarSyncAdaptor::requestCalendars(int accountId, const QString &accessToken, const QString &pageToken)
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
        decrementSemaphore(accountId);
    }
}

void GoogleCalendarSyncAdaptor::calendarsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
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
            requestCalendars(accountId, accessToken,
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
    }

    if (!fetchingNextPage) {
        // we've finished loading all pages of calendar information
        // we now need to process the loaded information to determine
        // which calendars need to be added/updated/removed locally.
        updateLocalCalendarNotebooks(accountId, accessToken);
    }

    // we're finished with this request.
    decrementSemaphore(accountId);
}


void GoogleCalendarSyncAdaptor::updateLocalCalendarNotebooks(int accountId, const QString &accessToken)
{
    // any calendars which exist on the device but not the server need to be purged.
    mKCal::ExtendedCalendar::Ptr calendar =
            mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QLatin1String("UTC")));
    mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(calendar);
    storage->open();

    QStringList deviceCalendarIds;
    foreach (mKCal::Notebook::Ptr notebook, storage->notebooks()) {
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
                        || notebook->color() != m_serverCalendarIdToSummaryAndColor[accountId].value(currDeviceCalendarId).second) {
                    // summary or color changed server-side.
                    notebook->setName(m_serverCalendarIdToSummaryAndColor[accountId].value(currDeviceCalendarId).first);
                    notebook->setColor(m_serverCalendarIdToSummaryAndColor[accountId].value(currDeviceCalendarId).second);
                    storage->updateNotebook(notebook);
                }
            } else {
                // the calendar has been removed from the server.
                // we need to purge it from the device.
                KCalCore::Incidence::List incidenceList;
                storage->allIncidences(&incidenceList, notebook->uid());
                foreach (KCalCore::Incidence::Ptr incidence, incidenceList) {
                    calendar->deleteIncidence(incidence);
                }
                storage->deleteNotebook(notebook);
            }
        }
    }

    // any calendarIds which exist on the server but not the device need to be created.
    foreach (const QString &serverCalendarId, m_serverCalendarIdToSummaryAndColor[accountId].keys()) {
        if (!deviceCalendarIds.contains(serverCalendarId)) {
            mKCal::Notebook::Ptr notebook = mKCal::Notebook::Ptr(new mKCal::Notebook);
            notebook->setName(m_serverCalendarIdToSummaryAndColor[accountId].value(serverCalendarId).first);
            notebook->setColor(m_serverCalendarIdToSummaryAndColor[accountId].value(serverCalendarId).second);
            notebook->setPluginName(QStringLiteral("google-") + serverCalendarId);
            notebook->setAccount(QString::number(accountId));
            storage->addNotebook(notebook);
        }
    }

    // commit changes to calendar backend on device.
    // addNotebook/updateNotebook/deleteNotebook are all synchronous
    // but deleteIncidence may not be.
    storage->save();
    storage->close();

    // Finally, request the events for each calendar.
    foreach (const QString &calendarId, m_serverCalendarIdToSummaryAndColor[accountId].keys()) {
        requestEvents(accountId, accessToken, calendarId);
    }
}

void GoogleCalendarSyncAdaptor::requestEvents(int accountId, const QString &accessToken, const QString &calendarId, const QString &pageToken)
{
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("key"),
                                              accessToken));
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("singleEvents"),
                                              QString::fromLatin1("true")));
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("timeMin"),
                                              QDateTime::currentDateTimeUtc().addMonths(-1).toString(Qt::ISODate)));
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("timeMax"),
                                              QDateTime::currentDateTimeUtc().addMonths(6).toString(Qt::ISODate)));
    if (!pageToken.isEmpty()) { // continuation request
        queryItems.append(QPair<QString, QString>(QString::fromLatin1("pageToken"),
                                                  pageToken));
    }
    // NOTE: can use updatedMin to only return events modified since last sync...

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
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(eventsFinishedHandler()));
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request events for calendar %1"
                                      " from Google account with id %2"))
                .arg(calendarId).arg(accountId));
        decrementSemaphore(accountId);
    }
}

void GoogleCalendarSyncAdaptor::eventsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString calendarId = reply->property("calendarId").toString();
    QString accessToken = reply->property("accessToken").toString();
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
            requestEvents(accountId, accessToken, calendarId,
                          parsed.value(QLatin1String("nextPageToken")).toVariant().toString());
        }

        // Parse the event list
        QJsonArray dataList = parsed.value(QLatin1String("items")).toArray();
        foreach (const QJsonValue &item, dataList) {
            QJsonObject eventData = item.toObject();

            // if this is the definition of a recurring event, we ignore it.
            if (eventData.find(QLatin1String("recurrence")) != eventData.end()) {
                continue;
            }

            // otherwise, we queue the event for insertion into the database.
            m_calendarIdToEventObjects[accountId].insertMulti(calendarId, eventData);
        }
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse calendar data from request with account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
    }

    if (!fetchingNextPage) {
        // we've finished loading all pages of event information
        // we now need to process the loaded information to determine
        // which events need to be added/updated/removed locally.
        updateLocalCalendarNotebookEvents(accountId, accessToken, calendarId);
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

void GoogleCalendarSyncAdaptor::updateLocalCalendarNotebookEvents(int accountId, const QString &accessToken, const QString &calendarId)
{
    Q_UNUSED(accessToken) // in the future, we might need it to download images/data associated with the event.

    // We open the calendar and storage associated to it
    mKCal::ExtendedCalendar::Ptr calendar =
            mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QLatin1String("UTC")));
    mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(calendar);
    storage->open();

    // Search for the device Notebook matching this CalendarId
    bool found = false;
    mKCal::Notebook::Ptr googleNotebook;
    foreach (mKCal::Notebook::Ptr notebook, storage->notebooks()) {
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
        return;
    }

    // purge all incidences which exist in the notebook.
    // TODO: don't do purge+rewrite, instead do delta update.
    KCalCore::Incidence::List incidenceList;
    storage->allIncidences(&incidenceList, googleNotebook->uid());
    foreach (KCalCore::Incidence::Ptr incidence, incidenceList) {
        calendar->deleteIncidence(incidence);
    }

    // for each each of the events downloaded from the server, create a local event.
    foreach (const QJsonObject &eventData, m_calendarIdToEventObjects[accountId].values(calendarId)) {
        QString eventId = eventData.value(QLatin1String("id")).toVariant().toString();
        QString eventSummary = eventData.value(QLatin1String("summary")).toVariant().toString();
        QString eventDescription = eventData.value(QLatin1String("description")).toVariant().toString();
        QString eventLocation = eventData.value(QLatin1String("location")).toVariant().toString();

        bool isDateOnly = false;
        QString startTimeString, endTimeString;
        QJsonObject startTimeData = eventData.value(QLatin1String("start")).toObject();
        QJsonObject endTimeData = eventData.value(QLatin1String("end")).toObject();
        if (startTimeData.value(QLatin1String("date")).toVariant().toString().isEmpty()) {
            startTimeString = startTimeData.value(QLatin1String("dateTime")).toVariant().toString();
            endTimeString = endTimeData.value(QLatin1String("dateTime")).toVariant().toString();
        } else {
            isDateOnly = true; // all-day event.
            startTimeString = startTimeData.value(QLatin1String("date")).toVariant().toString();
            endTimeString = endTimeData.value(QLatin1String("date")).toVariant().toString();
        }

        KDateTime startTime;
        KDateTime endTime;
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
            startTime = KDateTime::fromString(startTimeString, QLatin1String("%Y-%m-%d"));
            startTime.setTime(QTime(0, 0));
            endTime = startTime;
        }

        // Set the property of the event
        KCalCore::Event::Ptr event = KCalCore::Event::Ptr(new KCalCore::Event);
        event->setUid(eventId);
        event->setSummary(eventSummary);
        event->setDescription(eventDescription);
        event->setLocation(eventLocation);
        event->setDtStart(startTime);
        if (!isDateOnly) {
            event->setDtEnd(endTime);
        } else {
            event->setAllDay(true);
        }
        event->setReadOnly(true);
        calendar->addEvent(event, googleNotebook->uid());
    }

    // Write changes to calendar
    storage->save();
    storage->close();
}
