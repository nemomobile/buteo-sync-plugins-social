/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "networkstubs_p.h"
#include "socialdnetworkaccessmanager_p.h"

#include <QDateTime>
#include <QTimer>

static QByteArray createGoogleCalendarsData(const QString &generator)
{
    // XXX TODO
    return QByteArray();
}

static QByteArray createGoogleContactsData(const QString &generator)
{
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