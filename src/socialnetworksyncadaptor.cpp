/*
 * Copyright (C) 2013 Jolla Ltd. <chris.adams@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "socialnetworksyncadaptor.h"
#include "syncservice.h"
#include "trace.h"

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

SocialNetworkSyncAdaptor::SocialNetworkSyncAdaptor(SyncService *parent)
    : QObject(parent), m_status(SocialNetworkSyncAdaptor::Invalid), q(parent)
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

void SocialNetworkSyncAdaptor::sync(const QString &dataType)
{
    Q_UNUSED(dataType)
    TRACE(SOCIALD_ERROR, QString(QLatin1String("error: should be overridden by derived types")));
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

    QSqlQuery query;
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

    QSqlQuery query;
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

    QSqlQuery query;
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

    QSqlQuery query;
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

    QSqlQuery query;
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

    QSqlQuery query;
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

    QSqlQuery query;
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
