/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Raine Makelainen <raine.makelainen@jollamobile.com>
 **
 ****************************************************************************/

#ifndef SOCIALDPLUGIN_H
#define SOCIALDPLUGIN_H

#include <QString>
#include <QObject>

#include "buteosyncfw_p.h"

/*
   This plugin implementation provides a simple way
   to trigger syncs of all datatypes for all accounts,
   as opposed to a sync for only a specific datatype
   of a specific account, via the following profile:
       sociald.All.xml

   It also allows triggering syncs of a specific data
   type for all accounts, via the following profiles:
       sociald.google.Calendars.xml
       sociald.google.Contacts.xml
       sociald.facebook.Calendars.xml
       sociald.facebook.Contacts.xml
       sociald.facebook.Images.xml
       sociald.facebook.Notifications.xml
       sociald.facebook.Posts.xml
       sociald.twitter.Notifications.xml
       sociald.twitter.Posts.xml

   Note that it does not extend SocialdButeoPlugin
   (from common.pri) as it uses a different mechanism.
*/
class SocialdPlugin : public Buteo::ClientPlugin
{
    Q_OBJECT

public:
    SocialdPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~SocialdPlugin();

    bool init();
    bool uninit();
    bool startSync();
    void abortSync(Sync::SyncStatus status = Sync::SYNC_ABORTED);
    Buteo::SyncResults getSyncResults() const;
    bool cleanUp();

public slots:
    void connectivityStateChanged(Sync::ConnectivityType type, bool state);

private:
    void updateResults(const Buteo::SyncResults &results);
    Buteo::SyncResults m_syncResults;
    QString m_dataType;
    QString m_serviceName;
};

extern "C" SocialdPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(SocialdPlugin* client);

#endif // SOCIALDPLUGIN_H
