/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Raine Makelainen <raine.makelainen@jollamobile.com>
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
