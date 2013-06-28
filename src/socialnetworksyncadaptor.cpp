/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "socialnetworksyncadaptor.h"
#include "syncservice.h"
#include "trace.h"

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <QtNetwork/QNetworkAccessManager>

// sailfish-components-accounts-qt5
#include <accountmanager.h>
#include <account.h>

SocialNetworkSyncAdaptor::SocialNetworkSyncAdaptor(QString serviceName, SyncService *syncService, QObject *parent)
    : QObject(parent)
    , m_status(SocialNetworkSyncAdaptor::Invalid)
    , m_serviceName(serviceName)
    , m_accountManager(new AccountManager(this))
    , m_qnam(new QNetworkAccessManager(this))
    , q(syncService)
{
}

SocialNetworkSyncAdaptor::~SocialNetworkSyncAdaptor()
{
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
    QList<int> knownIds;
    QStringList knownIdStrings = accountIdsWithSyncTimestamp(m_serviceName, SyncService::dataType(dataType));
    foreach (const QString &kis, knownIdStrings) {
        bool ok = true;
        int intId = kis.toInt(&ok);
        if (ok) {
            knownIds.append(intId);
        } else {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: unable to convert known id string to int: %1"))
                    .arg(kis));
        }
    }

    QList<int> currentIds = m_accountManager->accountIdentifiers();
    TRACE(SOCIALD_DEBUG,
            QString(QLatin1String("have found %1 accounts which support a sync service; determining old/new/update sets..."))
            .arg(currentIds.size()));
    for (int i = 0; i < currentIds.size(); ++i) {
        int currId = currentIds.at(i);
        Account *act = m_accountManager->account(currId);
        if (!act || !(act->supportedServiceNames().size() > 0 &&
                      act->supportedServiceNames().at(0).startsWith(m_serviceName))) {
            continue; // not same account as m_serviceName.  Ignore it.
        }

        if (knownIds.contains(currId)) {
            knownIds.removeOne(currId);
            updateIds->append(currId);
        } else {
            newIds->append(currId);
        }
    }

    // anything left in knownIds must belong to an old, removed account.
    for (int i = 0; i < knownIds.size(); ++i) {
        purgeIds->append(knownIds.at(i));
    }
}

/*!
    \internal
    Returns the last sync timestamp for the given service, account and data type.
    If data from prior to this timestamp is received in subsequent requests, it does not need to be synced.
    This function will return an invalid QDateTime if no synchronisation has occurred.
*/
QDateTime SocialNetworkSyncAdaptor::lastSyncTimestamp(const QString &serviceName, const QString &dataType, const QString &accountId) const
{
    if (!q->database()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: database not available")));
        return QDateTime();
    }

    QSqlQuery query(*q->database());
    query.prepare("SELECT syncTimestamp FROM syncTimestamps WHERE serviceName = :sn AND accountIdentifier = :aid AND dataType = :dt");
    query.bindValue(":sn", serviceName);
    query.bindValue(":aid", accountId);
    query.bindValue(":dt", dataType);
    bool success = query.exec();
    if (!success) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute query: %1")).arg(query.lastError().text()));
        return QDateTime();
    }

    if (query.next()) {
        return QDateTime::fromString(query.value(0).toString(), Qt::ISODate);
    }

    return QDateTime();
}

/*!
    \internal
    Updates the last sync timestamp for the given service, account and data type to the given \a timestamp.
*/
bool SocialNetworkSyncAdaptor::updateLastSyncTimestamp(const QString &serviceName, const QString &dataType, const QString &accountId, const QDateTime &timestamp)
{
    if (!q->database()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: database not available")));
        return false;
    }

    QSqlQuery query(*q->database());
    query.prepare("INSERT INTO syncTimestamps (serviceName, accountIdentifier, dataType, syncTimestamp) VALUES (:sn, :aid, :dt, :st)");
    query.bindValue(":sn", serviceName);
    query.bindValue(":aid", accountId);
    query.bindValue(":dt", dataType);
    query.bindValue(":st", timestamp.toString(Qt::ISODate));
    bool success = query.exec();
    if (!success) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute query: %1")).arg(query.lastError().text()));
    }
    return success;
}

/*!
    \internal
    Returns the synced timestamp for datum identified by the given \a datumIdentifier.
    If the given datum hasn't been marked as synced, this function will return an invalid QDateTime.
*/
QDateTime SocialNetworkSyncAdaptor::whenSyncedDatum(const QString &serviceName, const QString &datumIdentifier) const
{
    if (!q->database()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: database not available")));
        return QDateTime();
    }

    QSqlQuery query(*q->database());
    query.prepare("SELECT syncTimestamp FROM syncedData WHERE serviceName = :sn AND datumIdentifier = :di");
    query.bindValue(":sn", serviceName);
    query.bindValue(":di", datumIdentifier);
    bool success = query.exec();
    if (!success) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute query: %1")).arg(query.lastError().text()));
        return QDateTime();
    }

    if (query.next()) {
        return QDateTime::fromString(query.value(0).toString(), Qt::ISODate);
    }

    return QDateTime();
}

/*!
    \internal
    Marks the datum identified by the given \a datumIdentifier as having been synced at the given \a syncedTimestamp.
*/
bool SocialNetworkSyncAdaptor::markSyncedDatum(const QString &localIdentifier, const QString &serviceName, const QString &dataType, const QString &accountId, const QDateTime &createdTimestamp, const QDateTime &syncedTimestamp, const QString &datumIdentifier)
{
    if (!q->database()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: database not available")));
        return false;
    }

    QSqlQuery query(*q->database());
    query.prepare("INSERT INTO syncedData (id, serviceName, accountIdentifier, dataType, createdTimestamp, syncTimestamp, datumIdentifier) VALUES (:id, :sn, :aid, :dt, :ct, :st, :di)");
    query.bindValue(":id", localIdentifier);
    query.bindValue(":sn", serviceName);
    query.bindValue(":aid", accountId);
    query.bindValue(":dt", dataType);
    query.bindValue(":st", createdTimestamp.toString(Qt::ISODate));
    query.bindValue(":st", syncedTimestamp.toString(Qt::ISODate));
    query.bindValue(":di", datumIdentifier);
    bool success = query.exec();
    if (!success) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute query: %1")).arg(query.lastError().text()));
    }
    return success;
}

/*!
    \internal
    Removes all rows from all sociald database tables which have the specified
    \a serviceName, \a dataType and \a accountId column values.
*/
bool SocialNetworkSyncAdaptor::removeAllData(const QString &serviceName, const QString &dataType, const QString &accountId)
{
    if (!q->database()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: database not available")));
        return false;
    }

    QSqlQuery query(*q->database());
    query.prepare("DELETE FROM syncedData WHERE serviceName = :sn AND dataType = :dt AND accountIdentifier = :aid");
    query.bindValue(":sn", serviceName);
    query.bindValue(":dt", dataType);
    query.bindValue(":aid", accountId);
    bool success = query.exec();
    if (!success) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute query: %1")).arg(query.lastError().text()));
    }

    query.prepare("DELETE FROM syncTimestamps WHERE serviceName = :sn AND dataType = :dt AND accountIdentifier = :aid");
    query.bindValue(":sn", serviceName);
    query.bindValue(":dt", dataType);
    query.bindValue(":aid", accountId);
    success = query.exec();
    if (!success) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute query: %1")).arg(query.lastError().text()));
    }

    return success;
}

/*!
    \internal
    Returns the list of identifiers of accounts which have been synced for
    the given \a serviceName and \a dataType.
*/
QStringList SocialNetworkSyncAdaptor::accountIdsWithSyncTimestamp(const QString &serviceName, const QString &dataType)
{
    if (!q->database()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: database not available")));
        return QStringList();
    }

    QStringList retn;

    QSqlQuery query(*q->database());
    query.prepare("SELECT DISTINCT accountIdentifier FROM syncTimestamps WHERE serviceName = :sn AND dataType = :dt");
    query.bindValue(":sn", serviceName);
    query.bindValue(":dt", dataType);
    bool success = query.exec();
    if (!success) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute query: %1")).arg(query.lastError().text()));
    }

    while (query.next()) {
        QString accountIdent = query.value(0).toString();
        if (!retn.contains(accountIdent)) {
            retn.append(accountIdent);
        }
    }

    return retn;
}

/*!
    Returns the list of "local" identifiers of all synced data associated
    with the given \a serviceName with the specified \a dataType synced
    from the account identified by the given \a accountId.
*/
QStringList SocialNetworkSyncAdaptor::syncedDatumLocalIdentifiers(const QString &serviceName, const QString &dataType, const QString &accountId) const
{
    if (!q->database()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: database not available")));
        return QStringList();
    }

    QStringList retn;

    QSqlQuery query(*q->database());
    query.prepare("SELECT DISTINCT id FROM syncedData WHERE serviceName = :sn AND dataType = :dt AND accountIdentifier = :aid");
    query.bindValue(":sn", serviceName);
    query.bindValue(":dt", dataType);
    query.bindValue(":aid", accountId);
    bool success = query.exec();
    if (!success) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute query: %1")).arg(query.lastError().text()));
    }

    while (query.next()) {
        QString accountIdent = query.value(0).toString();
        retn.append(accountIdent);
    }

    return retn;
}

/*!
    \internal
    Begins a transaction.
    It is not necessary to invoke this function, but if you do you must call endTransaction()
    to commit any modifications made to the database.
*/
void SocialNetworkSyncAdaptor::beginTransaction()
{
    if (!q->database()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: database not available")));
        return;
    }

    q->database()->transaction();
}

/*!
    \internal
    Commits all pending updates to the database.
    It is not necessary to invoke this function unless you previously called beginTransaction().
*/
void SocialNetworkSyncAdaptor::endTransaction()
{
    if (!q->database()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: database not available")));
        return;
    }

    q->database()->commit();
}

/*!
 * \internal
 * Changes status if there is real change and emits statusChanged() signal.
 */
void SocialNetworkSyncAdaptor::changeStatus(Status status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}
