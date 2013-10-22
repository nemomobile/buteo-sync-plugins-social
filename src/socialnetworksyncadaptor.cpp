/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "socialnetworksyncadaptor.h"
#include "syncservice.h"
#include "trace.h"

#include <QtCore/QJsonDocument>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>

#include <QtNetwork/QNetworkAccessManager>

// sailfish-components-accounts-qt5
#include <accountmanager.h>
#include <account.h>

#include <socialnetworksyncdatabase.h>

/*
    Remarks on timestamps

    The timezone issue is a pretty big one, as services might
    provide time in different timezone, and that the user might
    be in another timezone.

    To make everything consistant, all time should be stored using
    UTC time. It is because Qt might have some troubles storing
    timezone in SQLITE databases. (Or is it SQLITE that have
    some troubles with timezone ?)

    Beware however, that all our APIs (eventfeed, notification)
    uses local time. So you have to perform a conversion before
    using date and time retrieved from the SQLITE database.

    By convention, all methods in sociald returning QDateTime
    object will return the UTC time, with their timeSpec set to
    Qt::UTC. Be sure to perform conversion before using them.
*/

SocialNetworkSyncAdaptor::SocialNetworkSyncAdaptor(QString serviceName,
                                                   SyncService::DataType dataType,
                                                   SyncService *syncService, QObject *parent)
    : QObject(parent)
    , dataType(dataType)
    , accountManager(new AccountManager(this))
    , networkAccessManager(new QNetworkAccessManager(this))
    , m_syncDb(new SocialNetworkSyncDatabase())
    , m_status(SocialNetworkSyncAdaptor::Invalid)
    , m_serviceName(serviceName)
    , m_syncService(syncService)
{
    qWarning() << "Initializing database";
    m_syncDb->initDatabase();
}

SocialNetworkSyncAdaptor::~SocialNetworkSyncAdaptor()
{
    delete m_syncDb;
}

SocialNetworkSyncAdaptor::Status SocialNetworkSyncAdaptor::status() const
{
    return m_status;
}

bool SocialNetworkSyncAdaptor::enabled() const
{
    return m_enabled;
}

QString SocialNetworkSyncAdaptor::serviceName() const
{
    return m_serviceName;
}

void SocialNetworkSyncAdaptor::sync(const QString &dataType)
{
    Q_UNUSED(dataType)
    TRACE(SOCIALD_ERROR, QString(QLatin1String("error: should be overridden by derived types")));
}

void SocialNetworkSyncAdaptor::checkAccounts(SyncService::DataType dataType, QList<int> *newIds, QList<int> *purgeIds, QList<int> *updateIds)
{
    QList<int> knownIds = syncedAccounts(SyncService::dataType(dataType));
    QList<int> currentIds = accountManager->accountIdentifiers();
    TRACE(SOCIALD_DEBUG,
            QString(QLatin1String("have found %1 accounts which support a sync service; determining old/new/update sets..."))
            .arg(currentIds.size()));

    foreach (int currId, currentIds) {
        Account *act = accountManager->account(currId);
        if (!act || !(act->supportedServiceNames().size() > 0 &&
                      act->supportedServiceNames().at(0).startsWith(m_serviceName))) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("account %1 does not support service %2, ignoring"))
                    .arg(currId).arg(m_serviceName));
            continue; // not same account as m_serviceName.  Ignore it.
        }

        // if the account has been disabled or disabled with the sync service, we purge it.
        if (act->enabled() && act->isEnabledWithService(QString(QLatin1String("%1-sync")).arg(m_serviceName))) {
            if (knownIds.contains(currId)) {
                knownIds.removeAll(currId);
                updateIds->append(currId);
            } else {
                newIds->append(currId);
            }
        }
    }

    // anything left in knownIds must belong to an old, removed account.
    foreach (int id, knownIds) {
        purgeIds->append(id);
    }
}

/*!
    \internal
    Called when the semaphores decreased to 0, this method is used
    to finalize something, like saving all data to a database.
    
    You can call incrementSemaphore to perform asynchronous tasks
    in this method. finalize will then be called again when the 
    asynchronous task is finished (and when decrementSemaphore is
    called), be sure to have a condition check in order not to run
    into an infinite loop.
    
    It is unsafe to call decrementSemaphore in this method, as 
    the semaphore handling method will find that the semaphore
    went to 0 twice and will perform cleanup operations twice.
    Please call decrementSemaphore at the end of the asynchronous
    task (preferably in a slot), and only call incrementSemaphore 
    for asynchronous tasks.
 */
void SocialNetworkSyncAdaptor::finalize(int accountId)
{
    Q_UNUSED(accountId)
}

/*!
    \internal
    Returns the last sync timestamp for the given service, account and data type.
    If data from prior to this timestamp is received in subsequent requests, it does not need to be synced.
    This function will return an invalid QDateTime if no synchronisation has occurred.
*/
QDateTime SocialNetworkSyncAdaptor::lastSyncTimestamp(const QString &serviceName,
                                                      const QString &dataType,
                                                      int accountId) const
{
    return m_syncDb->lastSyncTimestamp(serviceName, dataType, accountId);
}

/*!
    \internal
    Updates the last sync timestamp for the given service, account and data type to the given \a timestamp.
*/
bool SocialNetworkSyncAdaptor::updateLastSyncTimestamp(const QString &serviceName,
                                                       const QString &dataType,
                                                       int accountId,
                                                       const QDateTime &timestamp)
{
    // Workaround
    // TODO: do better, with a queue
    m_syncDb->addSyncTimestamp(serviceName, dataType, accountId, timestamp);
    return m_syncDb->write();
}

/*!
    \internal
    Returns the list of identifiers of accounts which have been synced for
    the given \a dataType.
*/
QList<int> SocialNetworkSyncAdaptor::syncedAccounts(const QString &dataType)
{
    return m_syncDb->syncedAccounts(m_serviceName, dataType);
}

/*!
 * \internal
 * Changes status if there is real change and emits statusChanged() signal.
 */
void SocialNetworkSyncAdaptor::setStatus(Status status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

/*!
 * \internal
 * Should be used in constructors to set the initial state
 * of enabled and status, without emitting signals
 *
 */
void SocialNetworkSyncAdaptor::setInitialActive(bool enabled)
{
    m_enabled = enabled;
    if (enabled) {
        m_status = Inactive;
    } else {
        m_status = Invalid;
    }
}

/*!
 * \internal
 * Should be called by any specific sync adapter when
 * they've finished syncing data.  The transition from
 * busy status to inactive status is what causes the
 * Buteo plugin to emit the sync results (and allows
 * subsequent syncs to occur).
 */
void SocialNetworkSyncAdaptor::setFinishedInactive()
{
    TRACE(SOCIALD_INFORMATION, QString(QLatin1String("Finished %1 %2 sync at: %3"))
                               .arg(m_serviceName, SyncService::dataType(dataType),
                                    QDateTime::currentDateTime().toString(Qt::ISODate)));
    setStatus(SocialNetworkSyncAdaptor::Inactive);
}

void SocialNetworkSyncAdaptor::incrementSemaphore(int accountId)
{
    int semaphoreValue = m_accountSyncSemaphores.value(accountId);
    semaphoreValue += 1;
    m_accountSyncSemaphores.insert(accountId, semaphoreValue);
    TRACE(SOCIALD_DEBUG, QString(QLatin1String("incremented busy semaphore for account %1 to %2")).arg(accountId).arg(semaphoreValue));
}

void SocialNetworkSyncAdaptor::decrementSemaphore(int accountId)
{
    if (!m_accountSyncSemaphores.contains(accountId)) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: no such semaphore for account: %1")).arg(accountId));
        return;
    }

    int semaphoreValue = m_accountSyncSemaphores.value(accountId);
    semaphoreValue -= 1;
    TRACE(SOCIALD_DEBUG, QString(QLatin1String("decremented busy semaphore for account %1 to %2")).arg(accountId).arg(semaphoreValue));
    if (semaphoreValue < 0) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: busy semaphore is negative for account: %1")).arg(accountId));
        return;
    }
    m_accountSyncSemaphores.insert(accountId, semaphoreValue);

    if (semaphoreValue == 0) {
        finalize(accountId);

        // With the newer implementation, in finalize we can rereaise semaphores,
        // so if after calling finalize, the semaphore count is not the same anymore,
        // we shouldn't update the sync timestamp
        if (m_accountSyncSemaphores.value(accountId) > 0) {
            return;
        }

        // finished all outstanding sync requests for this account.
        // update the sync time in the global sociald database.
        updateLastSyncTimestamp(m_serviceName,
                                SyncService::dataType(dataType), accountId,
                                QDateTime::currentDateTime().toTimeSpec(Qt::UTC));

        // if all outstanding requests for all accounts have finished,
        // then update our status to Inactive / ready to handle more sync requests.
        bool allAreZero = true;
        QList<int> semaphores = m_accountSyncSemaphores.values();
        foreach (int sv, semaphores) {
            if (sv != 0) {
                allAreZero = false;
                break;
            }
        }

        if (allAreZero) {
            setFinishedInactive(); // Finished!
        }
    }
}

QJsonObject SocialNetworkSyncAdaptor::parseJsonObjectReplyData(const QByteArray &replyData, bool *ok)
{
    QJsonDocument jsonDocument = QJsonDocument::fromJson(replyData);
    *ok = !jsonDocument.isEmpty();
    if (*ok && jsonDocument.isObject()) {
        return jsonDocument.object();
    }
    *ok = false;
    return QJsonObject();
}

QJsonArray SocialNetworkSyncAdaptor::parseJsonArrayReplyData(const QByteArray &replyData, bool *ok)
{
    QJsonDocument jsonDocument = QJsonDocument::fromJson(replyData);
    *ok = !jsonDocument.isEmpty();
    if (*ok && jsonDocument.isArray()) {
        return jsonDocument.array();
    }
    *ok = false;
    return QJsonArray();
}
