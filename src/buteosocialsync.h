#ifndef BUTEOSOCIALSYNC_H
#define BUTEOSOCIALSYNC_H

#include <buteosyncfw/ClientPlugin.h>
#include <buteosyncfw/SyncResults.h>

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

private:
    void updateResults(const Buteo::SyncResults &results);
    Buteo::SyncResults syncResults;
};

extern "C" ButeoSocial* createPlugin(const QString& pluginName, const Buteo::SyncProfile& profile, Buteo::PluginCbInterface *cbInterface);
extern "C" void destroyPlugin(ButeoSocial* client);

#endif  // BUTEOSOCIALSYNC_H
