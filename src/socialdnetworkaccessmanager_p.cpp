/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "socialdnetworkaccessmanager_p.h"

/* The default implementation is just a normal QNetworkAccessManager */

SocialdNetworkAccessManager::SocialdNetworkAccessManager(QObject *parent)
    : QNetworkAccessManager(parent)
{
}

QNetworkReply *SocialdNetworkAccessManager::createRequest(
                                 QNetworkAccessManager::Operation op,
                                 const QNetworkRequest &req,
                                 QIODevice *outgoingData)
{
    return QNetworkAccessManager::createRequest(op, req, outgoingData);
}
