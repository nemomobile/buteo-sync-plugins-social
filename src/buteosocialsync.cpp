#include <buteosyncfw/PluginCbInterface.h>
#include <buteosyncfw/LogMacros.h>

#include <syncservice.h>

#include "buteosocialsync.h"
#include "syncservice.h"

#include <QDebug>

static SyncService *g_syncService = 0;

extern "C" ButeoSocial* createPlugin(const QString& pluginName, const Buteo::SyncProfile& profile, Buteo::PluginCbInterface *callbackInterface)
{
    if (!g_syncService) {
        g_syncService = new SyncService;
    }

    return new ButeoSocial(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(ButeoSocial* plugin)
{
    delete plugin;
}

ButeoSocial::ButeoSocial(const QString& pluginName, const Buteo::SyncProfile& profile, Buteo::PluginCbInterface *callbackInterface)
    : ClientPlugin(pluginName, profile, callbackInterface)
{
    qDebug() << Q_FUNC_INFO << pluginName;
}

ButeoSocial::~ButeoSocial()
{
}

bool ButeoSocial::init()
{
    qDebug() << Q_FUNC_INFO;
    return true;
}

bool ButeoSocial::uninit()
{
    qDebug() << Q_FUNC_INFO;
    return true;
}

bool ButeoSocial::startSync()
{
    qDebug() << Q_FUNC_INFO;
    emit success(getProfileName(), "Event Feed Example update succeeded");
    return true;
}

void ButeoSocial::abortSync(Sync::SyncStatus)
{
    qDebug() << Q_FUNC_INFO;
}

bool ButeoSocial::cleanUp()
{
    qDebug() << Q_FUNC_INFO;
    return true;
}

Buteo::SyncResults ButeoSocial::getSyncResults() const
{
    qDebug() << Q_FUNC_INFO;
    return syncResults;
}

void ButeoSocial::connectivityStateChanged(Sync::ConnectivityType, bool)
{
    qDebug() << Q_FUNC_INFO;
}

void ButeoSocial::updateResults(const Buteo::SyncResults &results)
{
    qDebug() << Q_FUNC_INFO;
    syncResults = results;
    syncResults.setScheduled(true);
}
