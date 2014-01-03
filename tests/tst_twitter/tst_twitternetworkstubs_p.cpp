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

static QByteArray createTwitterHomeTimelineData(const QString &generator)
{
    // XXX TODO
    return QByteArray();
}

static QByteArray createTwitterMentionsTimelineData(const QString &generator)
{
    // XXX TODO
    return QByteArray();
}

QByteArray TestNetworkReply::generateData(const QUrl &requestUrl, const QString &generator)
{
    // we inspect the request url and based upon what we see there, generate some data.
    QString host = requestUrl.host();
    QString path = requestUrl.path();

    if (host == QLatin1String("twitter.com")) {
        if (path == QLatin1String("/home")) {
            return createTwitterHomeTimelineData(generator);
        } else if (path == QLatin1String("/mentions")) {
            return createTwitterMentionsTimelineData(generator);
        }
    }

    // no test data function exists for this host/path combination
    qWarning() << Q_FUNC_INFO << "no test data function exists for:" << host << path;
    return QByteArray();
}