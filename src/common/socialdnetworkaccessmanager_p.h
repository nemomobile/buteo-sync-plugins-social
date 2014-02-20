/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef SOCIALD_QNAMFACTORY_P_H
#define SOCIALD_QNAMFACTORY_P_H

#include <QNetworkAccessManager>

class SocialdNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    SocialdNetworkAccessManager(QObject *parent = 0);

protected:
    QNetworkReply *createRequest(QNetworkAccessManager::Operation op,
                                 const QNetworkRequest &req,
                                 QIODevice *outgoingData = 0);
};

#endif