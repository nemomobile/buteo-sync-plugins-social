/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Raine Makelainen <raine.makelainen@jollamobile.com>
 **
 ****************************************************************************/

#ifndef BUTEOSOCIALSYNC_H
#define BUTEOSOCIALSYNC_H

#include <ClientPlugin.h>
#include <SyncResults.h>

class SocialNetworkSyncAdaptor;

class ButeoSocial : public Buteo::ClientPlugin
{
    Q_OBJECT

public:
    ButeoSocial(const QString& pluginName, const Buteo::SyncProfile& profile, Buteo::PluginCbInterface *cbInterface );
    virtual ~ButeoSocial();
    virtual bool init();
    virtual bool uninit();
    virtual bool startSync();
    virtual void abortSync(Sync::SyncStatus status = Sync::SYNC_ABORTED);
    virtual Buteo::SyncResults getSyncResults() const;
    virtual bool cleanUp();

public slots:
    virtual void connectivityStateChanged(Sync::ConnectivityType type, bool state);

private slots:
    void syncStatusChanged();

private:
    void updateResults(const Buteo::SyncResults &results);
    Buteo::SyncResults m_syncResults;
    SocialNetworkSyncAdaptor *m_socialNetworkSyncAdaptor;
    QString m_dataType;
    QString m_serviceName;
};

extern "C" ButeoSocial* createPlugin(const QString& pluginName, const Buteo::SyncProfile& profile, Buteo::PluginCbInterface *cbInterface);
extern "C" void destroyPlugin(ButeoSocial* client);

#endif  // BUTEOSOCIALSYNC_H
