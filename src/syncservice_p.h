/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef SYNCSERVICE_P_H
#define SYNCSERVICE_P_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QtCore/QStringList>

class SocialNetworkSyncAdaptor;
class SyncService;
class SyncServicePrivate : public QObject
{
    Q_OBJECT

public:
    SyncServicePrivate(const QString &connectionName, SyncService *syncService);
    ~SyncServicePrivate();

    SocialNetworkSyncAdaptor *createAdaptor(const QString &socialService, const QString &dataType, QObject *parent);

    SyncService *q;
    QStringList m_supportedServices;
    QMap<QString, QStringList> m_supportedDataTypes;
};

#endif // SYNCSERVICE_P_H
