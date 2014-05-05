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
#include <accountmanager.h>
#include <account.h>

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
            uint startTime = json.value(QStringLiteral("start_date")).toString().toUInt();
            event->setDtStart(KDateTime(QDateTime::fromTime_t(startTime)));
            if (json.contains(QStringLiteral("end_date")) && !json.value(QStringLiteral("end_date")).toString().isEmpty()) {
                uint endTime = json.value(QStringLiteral("end_date")).toString().toUInt();
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

void VKCalendarSyncAdaptor::finalCleanup()
{
    // commit changes to db
    if (m_storageNeedsSave) {
        m_storage->save();
    }
    m_calendar->close();
    m_storage->close();
}

void VKCalendarSyncAdaptor::purgeDataForOldAccounts(const QList<int> &oldIds)
{
    // We clean all the entries in the calendar
    foreach (int accountId, oldIds) {
        // Delete the notebook and all events in it from the storage
        foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
            if (notebook->pluginName() == QStringLiteral(SOCIALD_VK_NAME)
                    && notebook->account() == QString::number(accountId)) {
                KCalCore::Incidence::List allEvents;
                m_storage->allIncidences(&allEvents, notebook->uid());
                foreach (const KCalCore::Incidence::Ptr event, allEvents) {
                    m_calendar->deleteIncidence(event);
                }
                m_storage->deleteNotebook(notebook);
                m_storageNeedsSave = true;
            }
        }
    }
}

void VKCalendarSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    TRACE(SOCIALD_DEBUG,
          QString(QLatin1String("Beginning Calendar sync for VK, account %1"))
          .arg(accountId));

    m_eventObjects[accountId].clear();
    requestEvents(accountId, accessToken);
}

void VKCalendarSyncAdaptor::requestEvents(int accountId, const QString &accessToken, int offset)
{
    QUrlQuery urlQuery;
    QUrl requestUrl = QUrl(QStringLiteral("https://api.vk.com/method/groups.get"));
    urlQuery.addQueryItem("access_token", accessToken);
    if (offset >= 1) urlQuery.addQueryItem ("offset", QString::number(offset));
    urlQuery.addQueryItem("count", QString::number(SOCIALD_VK_MAX_CALENDAR_ENTRY_RESULTS));
    urlQuery.addQueryItem("extended", QStringLiteral("1"));
    urlQuery.addQueryItem("fields", QStringLiteral("start_date,end_date,place,description"));
    // theoretically, could use filter=events but this always returns zero results.

    requestUrl.setQuery(urlQuery);
    QNetworkReply *reply = networkAccessManager->get(QNetworkRequest(requestUrl));

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
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request events "\
                                      "from VK account with id %1")).arg(accountId));
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
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("parsed calendar data from request with account %1; got: %2"))
                .arg(accountId).arg(QString::fromUtf8(replyData.constData())));

        // the zeroth index contains the count of response items
        QJsonArray items = parsed.value("response").toArray();
        int count = static_cast<int>(items.first().toDouble());
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("total communities returned in request with account %1: %2"))
                .arg(accountId).arg(count));
        bool needMorePages = false;
        if (count == 0 || count < SOCIALD_VK_MAX_CALENDAR_ENTRY_RESULTS) {
            // finished retrieving events.
        } else {
            needMorePages = true;
        }

        // parse the data in this page of results.
        for (int i = 1; i < items.size(); ++i) {
            QJsonObject currEvent = items.at(i).toObject();
            if (currEvent.isEmpty() || currEvent.value("type").toString() != QStringLiteral("event")) continue;

            int gid = 0;
            if (currEvent.value("id").toDouble() > 0) {
                gid = static_cast<int>(currEvent.value("id").toDouble());
            } else if (currEvent.value("gid").toDouble() > 0) {
                gid = static_cast<int>(currEvent.value("gid").toDouble());
            }
            if (gid > 0) {
                QString gidstr = QString::number(gid);
                m_eventObjects[accountId].insert(gidstr, currEvent);
            } else {
                qWarning() << "event has no id:" << currEvent;
            }
        }

        // if we need to request more data, do so.  otherwise, parse all of the results into mkcal events.
        if (needMorePages) {
            requestEvents(accountId, accessToken, offset + SOCIALD_VK_MAX_CALENDAR_ENTRY_RESULTS);
        } else {
            // convert the m_eventObjects to mkcal events, store in db or remove as required.
            bool foundVkNotebook = false;
            mKCal::Notebook::Ptr vkNotebook;
            foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
                if (notebook->pluginName() == QStringLiteral(SOCIALD_VK_NAME)
                        && notebook->account() == QString::number(accountId)) {
                    foundVkNotebook = true;
                    vkNotebook = notebook;
                }
            }

            if (!foundVkNotebook) {
                vkNotebook = mKCal::Notebook::Ptr(new mKCal::Notebook);
                vkNotebook->setUid(QUuid::createUuid().toString());
                vkNotebook->setName(QStringLiteral("VKontakte"));
                vkNotebook->setColor(QStringLiteral(SOCIALD_VK_COLOR));
                vkNotebook->setPluginName(QStringLiteral(SOCIALD_VK_NAME));
                vkNotebook->setAccount(QString::number(accountId));
                m_storage->addNotebook(vkNotebook);
                m_storageNeedsSave = true;
            }

            // We've found the notebook for this account.
            vkNotebook->setIsReadOnly(false); // temporarily
            // Build up a map of existing events, then determine A/M/R delta.
            int addedCount = 0, modifiedCount = 0, removedCount = 0;
            m_storage->loadNotebookIncidences(vkNotebook->uid());
            KCalCore::Incidence::List allIncidences;
            m_storage->allIncidences(&allIncidences, vkNotebook->uid());
            QSet<QString> serverSideEventIds = m_eventObjects[accountId].keys().toSet();
            foreach (const KCalCore::Incidence::Ptr incidence, allIncidences) {
                KCalCore::Event::Ptr event = m_calendar->event(incidence->uid());
                // when we add new events, we generate the uid like QUUID:vkId
                // to ensure that even after removal/re-add, the uid is unique.
                const QString &eventUid = event->uid();
                QString vkId = eventUid.mid(eventUid.indexOf(':'));
                if (!m_eventObjects[accountId].contains(vkId)) {
                    // this event was removed server-side since last sync.
                    m_storageNeedsSave = true;
                    m_calendar->deleteIncidence(event);
                    removedCount += 1;
                } else {
                    // this event was possibly modified server-side.
                    const QJsonObject &eventObject(m_eventObjects[accountId][vkId]);
                    if (eventNeedsUpdate(event, eventObject)) {
                        event->startUpdates();
                        jsonToKCal(vkId, eventObject, event, true);
                        event->endUpdates();
                        m_storageNeedsSave = true;
                        modifiedCount += 1;
                    }
                    serverSideEventIds.remove(vkId);
                }
            }

            // if we have any left over, they're additions.
            foreach (const QString &vkId, serverSideEventIds) {
                const QJsonObject &eventObject(m_eventObjects[accountId][vkId]);
                KCalCore::Event::Ptr event = KCalCore::Event::Ptr(new KCalCore::Event);
                jsonToKCal(vkId, eventObject, event, false); // direct conversion
                m_calendar->addEvent(event, vkNotebook->uid());
                m_storageNeedsSave = true;
                addedCount += 1;
            }

            // make the notebook read-only again
            vkNotebook->setIsReadOnly(true);

            // finished!
            TRACE(SOCIALD_INFORMATION,
                    QString(QLatin1String("finished calendars sync with account %1: a: %2 m: %3 r: %4"))
                    .arg(accountId).arg(addedCount).arg(modifiedCount).arg(removedCount));
        }
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse calendar data from request with account %1; got: %2"))
                .arg(accountId).arg(QString::fromUtf8(replyData.constData())));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}
