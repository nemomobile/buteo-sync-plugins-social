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

#include "socialnetworksyncadaptor.h"
#include "socialdnetworkaccessmanager_p.h"
#include "trace.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QTimer>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include "buteosyncfw_p.h"

// libaccounts-qt5
#include <Accounts/Manager>
#include <Accounts/Account>
#include <Accounts/Service>

// libsocialcache
#include <socialnetworksyncdatabase.h>

namespace {
    QStringList validDataTypesInitialiser()
    {
        return QStringList()
                << QStringLiteral("Contacts")
                << QStringLiteral("Calendars")
                << QStringLiteral("Notifications")
                << QStringLiteral("Images")
                << QStringLiteral("Videos")
                << QStringLiteral("Posts")
                << QStringLiteral("Messages")
                << QStringLiteral("Emails")
                << QStringLiteral("Signon");
    }
}

SocialNetworkSyncAdaptor::SocialNetworkSyncAdaptor(const QString &serviceName,
                                                   SocialNetworkSyncAdaptor::DataType dataType,
                                                   QObject *parent)
    : QObject(parent)
    , m_dataType(dataType)
    , m_accountManager(new Accounts::Manager(this))
    , m_networkAccessManager(new SocialdNetworkAccessManager(this))
    , m_accountSyncProfile(NULL)
    , m_syncDb(new SocialNetworkSyncDatabase())
    , m_status(SocialNetworkSyncAdaptor::Invalid)
    , m_enabled(false)
    , m_syncAborted(false)
    , m_serviceName(serviceName)
{
}

SocialNetworkSyncAdaptor::~SocialNetworkSyncAdaptor()
{
    delete m_accountSyncProfile;
    delete m_syncDb;
}

// The SocialNetworkSyncAdaptor takes ownership of the sync profiles.
void SocialNetworkSyncAdaptor::setAccountSyncProfile(Buteo::SyncProfile* perAccountSyncProfile)
{
    delete m_accountSyncProfile;
    m_accountSyncProfile = perAccountSyncProfile;
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

bool SocialNetworkSyncAdaptor::syncAborted() const
{
    return m_syncAborted;
}

void SocialNetworkSyncAdaptor::sync(const QString &dataType, int accountId)
{
    Q_UNUSED(dataType)
    Q_UNUSED(accountId)
    SOCIALD_LOG_ERROR("sync() must be overridden by derived types");
}

void SocialNetworkSyncAdaptor::abortSync(Sync::SyncStatus status)
{
    SOCIALD_LOG_INFO("forcing timeout of outstanding replies due to abort:" << status);
    m_syncAborted = true;
    triggerReplyTimeouts();
}

/*!
 * \brief SocialNetworkSyncAdaptor::checkAccount
 * \param account
 * \return true if synchronization of this adaptor's datatype is enabled for the account
 *
 * The default implementation checks that the account is enabled
 * with the accounts&sso service associated with this sync adaptor.
 */
bool SocialNetworkSyncAdaptor::checkAccount(Accounts::Account *account)
{
    bool globallyEnabled = account->enabled();
    Accounts::Service srv(m_accountManager->service(syncServiceName()));
    if (!srv.isValid()) {
        SOCIALD_LOG_INFO("invalid service" << syncServiceName() <<
                         "specified, account" << account->id() <<
                         "will be disabled for" << m_serviceName << dataTypeName(m_dataType) << "sync");
        return false;
    }
    account->selectService(srv);
    bool serviceEnabled = account->enabled();
    account->selectService(Accounts::Service());
    return globallyEnabled && serviceEnabled;
}

/*!
    \internal
    Called when the semaphores for all accounts have been decreased
    to zero.  This is the final function which is called prior to
    telling buteo that the sync plugin can be destroyed.
    The implementation MUST be synchronous.
*/
void SocialNetworkSyncAdaptor::finalCleanup()
{
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
    m_syncDb->commit();
    m_syncDb->wait();
    return m_syncDb->writeStatus() == AbstractSocialCacheDatabase::Finished;
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
    finalCleanup();
    SOCIALD_LOG_INFO("Finished" << m_serviceName << SocialNetworkSyncAdaptor::dataTypeName(m_dataType) <<
                     "sync at:" << QDateTime::currentDateTime().toString(Qt::ISODate));
    setStatus(SocialNetworkSyncAdaptor::Inactive);
}

void SocialNetworkSyncAdaptor::incrementSemaphore(int accountId)
{
    int semaphoreValue = m_accountSyncSemaphores.value(accountId);
    semaphoreValue += 1;
    m_accountSyncSemaphores.insert(accountId, semaphoreValue);
    SOCIALD_LOG_DEBUG("incremented busy semaphore for account" << accountId << "to:" << semaphoreValue);
}

void SocialNetworkSyncAdaptor::decrementSemaphore(int accountId)
{
    if (!m_accountSyncSemaphores.contains(accountId)) {
        SOCIALD_LOG_ERROR("no such semaphore for account" << accountId);
        return;
    }

    int semaphoreValue = m_accountSyncSemaphores.value(accountId);
    semaphoreValue -= 1;
    SOCIALD_LOG_DEBUG("decremented busy semaphore for account" << accountId << "to:" << semaphoreValue);
    if (semaphoreValue < 0) {
        SOCIALD_LOG_ERROR("busy semaphore is negative for account" << accountId);
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
                                SocialNetworkSyncAdaptor::dataTypeName(m_dataType), accountId,
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

void SocialNetworkSyncAdaptor::timeoutReply()
{
    QTimer *timer = qobject_cast<QTimer*>(sender());
    QNetworkReply *reply = timer->property("networkReply").value<QNetworkReply*>();
    int accountId = timer->property("accountId").toInt();

    SOCIALD_LOG_ERROR("network request timed out while performing sync with account" << accountId);

    m_networkReplyTimeouts[accountId].remove(reply);
    reply->setProperty("isError", QVariant::fromValue<bool>(true));
    reply->finished(); // invoke finished, so that the error handling there decrements the semaphore etc.
    reply->disconnect();
}

void SocialNetworkSyncAdaptor::setupReplyTimeout(int accountId, QNetworkReply *reply)
{
    // this function should be called whenever a new network request is performed.
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(60000);
    timer->setProperty("accountId", accountId);
    timer->setProperty("networkReply", QVariant::fromValue<QNetworkReply*>(reply));
    connect(timer, SIGNAL(timeout()), this, SLOT(timeoutReply()));
    timer->start();
    m_networkReplyTimeouts[accountId].insert(reply, timer);
}

void SocialNetworkSyncAdaptor::removeReplyTimeout(int accountId, QNetworkReply *reply)
{
    // this function should be called by the finished() handler for the reply.
    QTimer *timer = m_networkReplyTimeouts[accountId].value(reply);
    if (!reply) {
        return;
    }

    delete timer;
    m_networkReplyTimeouts[accountId].remove(reply);
}

void SocialNetworkSyncAdaptor::triggerReplyTimeouts()
{
    // if we've lost network connectivity, we should immediately timeout all replies.
    Q_FOREACH (int accountId, m_networkReplyTimeouts.keys()) {
        Q_FOREACH (QTimer *timer, m_networkReplyTimeouts[accountId]) {
            timer->stop();
            timer->setInterval(1);
            timer->start();
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

/*
    Valid data types are data types which are known to the API.
    Note that just because a data type is valid does not mean
    that it will necessarily be supported by a given social network
    sync adaptor.
*/
QStringList SocialNetworkSyncAdaptor::validDataTypes()
{
    static QStringList retn(validDataTypesInitialiser());
    return retn;
}

/*
    String for Enum since the DBus API uses strings
*/
QString SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::DataType t)
{
    switch (t) {
        case SocialNetworkSyncAdaptor::Contacts:      return QStringLiteral("Contacts");
        case SocialNetworkSyncAdaptor::Calendars:     return QStringLiteral("Calendars");
        case SocialNetworkSyncAdaptor::Notifications: return QStringLiteral("Notifications");
        case SocialNetworkSyncAdaptor::Images:        return QStringLiteral("Images");
        case SocialNetworkSyncAdaptor::Videos:        return QStringLiteral("Videos");
        case SocialNetworkSyncAdaptor::Posts:         return QStringLiteral("Posts");
        case SocialNetworkSyncAdaptor::Messages:      return QStringLiteral("Messages");
        case SocialNetworkSyncAdaptor::Emails:        return QStringLiteral("Emails");
        case SocialNetworkSyncAdaptor::Signon:        return QStringLiteral("Signon");
        default: break;
    }

    return QString();
}
