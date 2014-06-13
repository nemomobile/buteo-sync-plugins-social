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

#include "networkstubs_p.h"
#include "socialdnetworkaccessmanager_p.h"

#include <QDateTime>
#include <QTimer>

static QByteArray createGoogleCalendarsData(const QString &generator)
{
    Q_UNUSED(generator);
    // XXX TODO
    return QByteArray();
}

static QByteArray createGoogleContactsData(const QString &generator)
{
    Q_UNUSED(generator);
    // XXX TODO
    return QByteArray();
}

QByteArray TestNetworkReply::generateData(const QUrl &requestUrl, const QString &generator)
{
    // we inspect the request url and based upon what we see there, generate some data.
    QString host = requestUrl.host();
    QString path = requestUrl.path();

    if (host == QLatin1String("google.com")) {
        if (path == QLatin1String("/calendars")) {
            return createGoogleCalendarsData(generator);
        } else if (path == QLatin1String("/contacts")) {
            return createGoogleContactsData(generator);
        }
    }

    // no test data function exists for this host/path combination
    qWarning() << Q_FUNC_INFO << "no test data function exists for:" << host << path;
    return QByteArray();
}