/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd.
 ** Contact: Raine Makelainen <raine.makelainen@jollamobile.com>
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

#ifndef SOCIALDPLUGIN_H
#define SOCIALDPLUGIN_H

#include <QString>
#include <QObject>

#include "buteosyncfw_p.h"

#if defined(OUT_OF_PROCESS_PLUGIN)
#  define SOCIALDPLUGIN_EXPORT Q_DECL_EXPORT
#else
#  define SOCIALDPLUGIN_EXPORT Q_DECL_IMPORT
#endif

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
class SOCIALDPLUGIN_EXPORT SocialdPlugin : public Buteo::ClientPlugin
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
