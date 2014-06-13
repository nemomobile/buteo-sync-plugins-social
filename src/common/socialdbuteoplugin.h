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

#ifndef SOCIALDBUTEOPLUGIN_H
#define SOCIALDBUTEOPLUGIN_H

#include <QtCore/qglobal.h>
#include "buteosyncfw_p.h"

#if defined(OUT_OF_PROCESS_PLUGIN)
#  define SOCIALDBUTEOPLUGIN_EXPORT Q_DECL_EXPORT
#else
#  define SOCIALDBUTEOPLUGIN_EXPORT Q_DECL_IMPORT
#endif

/*
   Datatype-specific implementations of this class
   allow per-account sync profiles for that data type.
*/

class SocialNetworkSyncAdaptor;
class SOCIALDBUTEOPLUGIN_EXPORT SocialdButeoPlugin : public Buteo::ClientPlugin
{
    Q_OBJECT

protected:
    virtual SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor() = 0;

public:
    SocialdButeoPlugin(const QString& pluginName,
                       const Buteo::SyncProfile& profile,
                       Buteo::PluginCbInterface *cbInterface,
                       const QString &socialServiceName,
                       const QString &dataTypeName);
    virtual ~SocialdButeoPlugin();

    bool init();
    bool uninit();
    bool startSync();
    void abortSync(Sync::SyncStatus status = Sync::SYNC_ABORTED);
    Buteo::SyncResults getSyncResults() const;
    bool cleanUp();

public Q_SLOTS:
    void connectivityStateChanged(Sync::ConnectivityType type, bool state);

private Q_SLOTS:
    void syncStatusChanged();

protected:
    QList<Buteo::SyncProfile*> ensurePerAccountSyncProfilesExist();

private:
    void updateResults(const Buteo::SyncResults &results);
    Buteo::SyncResults m_syncResults;
    Buteo::ProfileManager m_profileManager;
    SocialNetworkSyncAdaptor *m_socialNetworkSyncAdaptor;
    QString m_socialServiceName;
    QString m_dataTypeName;
    int m_profileAccountId;
};

#endif // SOCIALDBUTEOPLUGIN_H
