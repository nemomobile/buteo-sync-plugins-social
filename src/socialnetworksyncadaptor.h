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

#ifndef SOCIALNETWORKSYNCADAPTOR_H
#define SOCIALNETWORKSYNCADAPTOR_H

#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QString>

class QSqlDatabase;
class SyncService;
class SocialNetworkSyncAdaptor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)

public:
    enum Status {
        Initializing = 0,
        Inactive,
        Busy,
        Error,
        Invalid
    };

public:
    SocialNetworkSyncAdaptor(SyncService *parent = 0);
    virtual ~SocialNetworkSyncAdaptor();

    Status status() const;
    bool enabled() const;
    virtual void sync(const QString &dataType); // do we need a "queueSync"? should this function have a return value?

Q_SIGNALS:
    void statusChanged();
    void enabledChanged();

protected:
    QDateTime lastSyncTimestamp(const QString &serviceName, const QString &dataType, const QString &accountId) const;
    bool updateLastSyncTimestamp(const QString &serviceName, const QString &dataType, const QString &accountId, const QDateTime &timestamp);
    QDateTime whenSyncedDatum(const QString &serviceName, const QString &datumIdentifier) const;
    bool markSyncedDatum(const QString &localIdentifier, const QString &serviceName, const QString &dataType, const QString &accountId, const QDateTime &createdTimestamp, const QDateTime &syncedTimestamp, const QString &datumIdentifier);
    bool removeAllData(const QString &serviceName, const QString &dataType, const QString &accountId);
    QStringList accountIdsWithSyncTimestamp(const QString &serviceName, const QString &dataType);
    QStringList syncedDatumLocalIdentifiers(const QString &serviceName, const QString &dataType, const QString &accountId) const;
    void beginTransaction();
    void endTransaction();

    Status m_status;
    bool m_enabled;

private:
    SyncService *q;
};

#endif // SOCIALNETWORKSYNCADAPTOR_H
