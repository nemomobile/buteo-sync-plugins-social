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

#ifndef SYNCSERVICE_H
#define SYNCSERVICE_H

#include <QtCore/QObject>
#include <QtDBus/QDBusContext>

#include <QtCore/QStringList>
#include <QtCore/QString>
#include <QtCore/QMap>

class QSqlDatabase;

class SyncServicePrivate;
class SyncService : public QObject
{
    Q_OBJECT

public:    
    enum DataType {
        Contacts = 1,   // "Contacts"
        Calendars,      // "Calendars"
        Notifications,  // "Notifications"
        Images,         // "Images"
        Videos,         // "Videos"
        Posts,          // "Posts"
        Messages,       // "Messages"
        Emails          // "Emails"
    };
    static QStringList validDataTypes();
    static QString dataType(DataType t);

public:
    SyncService(QObject *parent = 0);
    ~SyncService();

    QStringList supportedSocialServices() const; // the services for which we have written a SocialNetworkSyncAdapter
    QStringList enabledSocialServices() const;   // the subset of supported services for which we have an enabled account.
    QStringList supportedDataTypes(const QString &socialService) const; // the sync data types supported for the service.

    void sync(const QString &socialService, const QStringList &types); // manual sync.

private:
    QSqlDatabase *database() const;
    friend class SocialNetworkSyncAdaptor;

private:
    SyncServicePrivate *d;
    friend class SyncServicePrivate;
};

#endif // SYNCSERVICE_H
