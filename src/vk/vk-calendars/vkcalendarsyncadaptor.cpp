/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "vkcalendarsyncadaptor.h"
#include "trace.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <extendedcalendar.h>
#include <extendedstorage.h>

#define SOCIALD_VK_NAME "VK"
#define SOCIALD_VK_COLOR "#45668e"
#define SOCIALD_VK_MAX_CALENDAR_ENTRY_RESULTS 100

namespace {
    bool eventNeedsUpdate(KCalCore::Event::Ptr event, const QJsonObject &json)
    {
        // TODO: compare data, determine if we need to update
        Q_UNUSED(event)
        Q_UNUSED(json)
        return true;
    }
    void jsonToKCal(const QString &vkId, const QJsonObject &json, KCalCore::Event::Ptr event, bool isUpdate)
    {
        if (!isUpdate) {
            QString eventUid = QUuid::createUuid().toString().mid(1);
            eventUid.chop(1);
            eventUid += QStringLiteral(":%1").arg(vkId);
            event->setUid(eventUid);
        }
        event->setSummary(json.value(QStringLiteral("name")).toString());
        event->setDescription(json.value(QStringLiteral("description")).toString());
        event->setLocation(json.value(QStringLiteral("place")).toObject().value(QStringLiteral("title")).toString());
        if (json.contains(QStringLiteral("start_date"))) {
            uint startTime = json.value(QStringLiteral("start_date")).toDouble();
            event->setDtStart(KDateTime(QDateTime::fromTime_t(startTime)));
            if (json.contains(QStringLiteral("end_date"))) {
                uint endTime = json.value(QStringLiteral("end_date")).toDouble();
                event->setHasEndDate(true);
                event->setDtEnd(KDateTime(QDateTime::fromTime_t(endTime)));
            } else {
                event->setHasEndDate(false);
            }
        }
    }
}

VKCalendarSyncAdaptor::VKCalendarSyncAdaptor(QObject *parent)
    : VKDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Calendars, parent)
    , m_calendar(mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QLatin1String("UTC"))))
    , m_storage(mKCal::ExtendedCalendar::defaultStorage(m_calendar))
    , m_storageNeedsSave(false)
{
    setInitialActive(true);
}

VKCalendarSyncAdaptor::~VKCalendarSyncAdaptor()
{
}

QString VKCalendarSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("vk-calendars");
}

void VKCalendarSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    m_storageNeedsSave = false;
    m_storage->open(); // we close it in finalCleanup()
    VKDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void VKCalendarSyncAdaptor::finalize(int accountId)
{
    if (syncAborted()) {
        SOCIALD_LOG_DEBUG("sync aborted, skipping finalize of VK calendar events from account:" << accountId);
    } else {
        SOCIALD_LOG_DEBUG("finalizing VK calendar sync with account:" << accountId);
        // convert the m_eventObjects to mkcal events, store in db or remove as required.
        bool foundVkNotebook = false;
        Q_FOREACH (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
            if (notebook->pluginName() == QStringLiteral(SOCIALD_VK_NAME)
                    && notebook->account() == QString::number(accountId)) {
                foundVkNotebook = true;
                m_vkNotebook = notebook;
            }
        }

        if (!foundVkNotebook) {
            m_vkNotebook = mKCal::Notebook::Ptr(new mKCal::Notebook);
            m_vkNotebook->setUid(QUuid::createUuid().toString());
            m_vkNotebook->setName(QStringLiteral("VKontakte"));
            m_vkNotebook->setColor(QStringLiteral(SOCIALD_VK_COLOR));
            m_vkNotebook->setPluginName(QStringLiteral(SOCIALD_VK_NAME));
            m_vkNotebook->setAccount(QString::number(accountId));
            m_vkNotebook->setIsReadOnly(false); // temporarily
            m_storage->addNotebook(m_vkNotebook);
            m_storageNeedsSave = true;
        }

        // We've found the notebook for this account.
        // Build up a map of existing events, then determine A/M/R delta.
        int addedCount = 0, modifiedCount = 0, removedCount = 0;
        m_storage->loadNotebookIncidences(m_vkNotebook->uid());
        KCalCore::Incidence::List allIncidences;
        m_storage->allIncidences(&allIncidences, m_vkNotebook->uid());
        QSet<QString> serverSideEventIds = m_eventObjects[accountId].keys().toSet();
        Q_FOREACH (const KCalCore::Incidence::Ptr incidence, allIncidences) {
            KCalCore::Event::Ptr event = m_calendar->event(incidence->uid());
            // when we add new events, we generate the uid like QUUID:vkId
            // to ensure that even after removal/re-add, the uid is unique.
            const QString &eventUid = event->uid();
            int vkIdIdx = eventUid.indexOf(':') + 1;
            QString vkId = (vkIdIdx > 0 && eventUid.size() > vkIdIdx) ? eventUid.mid(eventUid.indexOf(':') + 1) : QString();
            if (!m_eventObjects[accountId].contains(vkId)) {
                // this event was removed server-side since last sync.
                m_vkNotebook->setIsReadOnly(false); // temporarily
                m_storageNeedsSave = true;
                m_calendar->deleteIncidence(event);
                removedCount += 1;
                SOCIALD_LOG_TRACE("deleted existing event:" << event->summary() << ":" << event->dtStart().toString());
            } else {
                // this event was possibly modified server-side.
                const QJsonObject &eventObject(m_eventObjects[accountId][vkId]);
                if (eventNeedsUpdate(event, eventObject)) {
                    m_vkNotebook->setIsReadOnly(false); // temporarily
                    event->startUpdates();
                    jsonToKCal(vkId, eventObject, event, true);
                    event->endUpdates();
                    m_storageNeedsSave = true;
                    modifiedCount += 1;
                    SOCIALD_LOG_TRACE("modified existing event:" << event->summary() << ":" << event->dtStart().toString());
                } else {
                    SOCIALD_LOG_TRACE("no modificiation necessary for existing event:" << event->summary() << ":" << event->dtStart().toString());
                }
                serverSideEventIds.remove(vkId);
            }
        }

        // if we have any left over, they're additions.
        Q_FOREACH (const QString &vkId, serverSideEventIds) {
            m_vkNotebook->setIsReadOnly(false); // temporarily
            const QJsonObject &eventObject(m_eventObjects[accountId][vkId]);
            KCalCore::Event::Ptr event = KCalCore::Event::Ptr(new KCalCore::Event);
            jsonToKCal(vkId, eventObject, event, false); // direct conversion
            if (!m_calendar->addEvent(event, m_vkNotebook->uid())) {
                SOCIALD_LOG_TRACE("failed to add new event:" << event->summary() << ":" << event->dtStart().toString() << "to notebook:" << m_vkNotebook->uid());
                continue;
            }
            m_storageNeedsSave = true;
            addedCount += 1;
            SOCIALD_LOG_TRACE("added new event:" << event->summary() << ":" << event->dtStart().toString() << "to notebook:" << m_vkNotebook->uid());
        }

        // finished!
        SOCIALD_LOG_INFO("finished calendars sync with VK account" << accountId <<
                         ": got A/M/R:" << addedCount << "/" << modifiedCount << "/" << removedCount);
    }
}

void VKCalendarSyncAdaptor::finalCleanup()
{
    // commit changes to db
    if (m_storageNeedsSave && !syncAborted()) {
        SOCIALD_LOG_DEBUG("saving changes in VK calendar to storage");
        m_storage->save();
        // the notebook will have been set writable.  make the notebook read-only again.
        m_vkNotebook->setIsReadOnly(true);
        m_storage->save();
    } else {
        SOCIALD_LOG_DEBUG("no changes to VK calendar - not saving storage");
    }
    m_calendar->close();
    m_storage->close();
}

void VKCalendarSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode)
{
    // Delete the notebook and all events in it from the storage
    Q_FOREACH (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
        if (notebook->pluginName() == QStringLiteral(SOCIALD_VK_NAME)
                && notebook->account() == QString::number(oldId)) {
            KCalCore::Incidence::List allEvents;
            m_storage->allIncidences(&allEvents, notebook->uid());
            Q_FOREACH (const KCalCore::Incidence::Ptr event, allEvents) {
                m_calendar->deleteIncidence(event);
            }
            m_storage->deleteNotebook(notebook);
            m_storageNeedsSave = true;
        }
    }
}

void VKCalendarSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    SOCIALD_LOG_DEBUG("Beginning Calendar sync for VK, account:" << accountId);
    m_eventObjects[accountId].clear();
    requestEvents(accountId, accessToken);
}

void VKCalendarSyncAdaptor::requestEvents(int accountId, const QString &accessToken, int offset)
{
    QUrlQuery urlQuery;
    QUrl requestUrl = QUrl(QStringLiteral("https://api.vk.com/method/groups.get"));
    urlQuery.addQueryItem("v", QStringLiteral("5.21")); // version
    urlQuery.addQueryItem("access_token", accessToken);
    if (offset >= 1) urlQuery.addQueryItem ("offset", QString::number(offset));
    urlQuery.addQueryItem("count", QString::number(SOCIALD_VK_MAX_CALENDAR_ENTRY_RESULTS));
    urlQuery.addQueryItem("extended", QStringLiteral("1"));
    urlQuery.addQueryItem("fields", QStringLiteral("start_date,end_date,place,description"));
    // theoretically, could use filter=events but this always returns zero results.

    requestUrl.setQuery(urlQuery);
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(requestUrl));

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("offset", offset);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, reply);
    } else {
        SOCIALD_LOG_ERROR("unable to request events from VK account with id:" << accountId);
    }
}

void VKCalendarSyncAdaptor::finishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString accessToken = reply->property("accessToken").toString();
    int accountId = reply->property("accountId").toInt();
    int offset = reply->property("offset").toInt();
    QByteArray replyData = reply->readAll();
    bool isError = reply->property("isError").toBool();

    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok) {
        // the zeroth index contains the count of response items
        QJsonArray items = parsed.value("response").toObject().value("items").toArray();
        int count = parsed.value("response").toObject().value("count").toInt();
        SOCIALD_LOG_DEBUG("total communities returned in request with account" << accountId << ":" << count);
        bool needMorePages = false;
        if (count == 0 || count < SOCIALD_VK_MAX_CALENDAR_ENTRY_RESULTS) {
            // finished retrieving events.
        } else {
            needMorePages = true;
        }

        // parse the data in this page of results.
        for (int i = 1; i < items.size(); ++i) {
            QJsonObject currEvent = items.at(i).toObject();
            if (currEvent.isEmpty() || currEvent.value("type").toString() != QStringLiteral("event")) {
                SOCIALD_LOG_DEBUG("ignoring community:" << currEvent.value("name").toString() << "as it is not an event");
                continue;
            }

            int gid = 0;
            if (currEvent.value("id").toDouble() > 0) {
                gid = currEvent.value("id").toInt();
            } else if (currEvent.value("gid").toInt() > 0) {
                gid = currEvent.value("gid").toInt();
            }
            if (gid > 0) {
                // we just cache them in memory here; we store them only if all
                // events are retrieved without sync being aborted / connection loss.
                QString gidstr = QString::number(gid);
                m_eventObjects[accountId].insert(gidstr, currEvent);
                SOCIALD_LOG_DEBUG("Have found event with id:" << gid << ":" << currEvent.value("name").toString());
            } else {
                qWarning() << "event has no id:" << currEvent;
            }
        }

        // if we need to request more data, do so.  otherwise, parse all of the results into mkcal events.
        if (needMorePages) {
            SOCIALD_LOG_DEBUG("need to fetch more pages of calendar results");
            requestEvents(accountId, accessToken, offset + SOCIALD_VK_MAX_CALENDAR_ENTRY_RESULTS);
        } else {
            SOCIALD_LOG_DEBUG("done fetching calendar results");
        }
    } else {
        // error occurred during request.
        SOCIALD_LOG_ERROR("unable to parse calendar data from request with account" << accountId <<
                          "; got:" << QString::fromUtf8(replyData));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}
