/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "facebooksignonplugin.h"
#include "facebooksignonsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" FacebookSignonPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new FacebookSignonPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(FacebookSignonPlugin* plugin)
{
    delete plugin;
}

FacebookSignonPlugin::FacebookSignonPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("facebook"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Signon))
{
}

FacebookSignonPlugin::~FacebookSignonPlugin()
{
}

SocialNetworkSyncAdaptor *FacebookSignonPlugin::createSocialNetworkSyncAdaptor()
{
    return new FacebookSignonSyncAdaptor(this);
}
