/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "googlesignonplugin.h"
#include "googlesignonsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" GoogleSignonPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new GoogleSignonPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(GoogleSignonPlugin* plugin)
{
    delete plugin;
}

GoogleSignonPlugin::GoogleSignonPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("google"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Signon))
{
}

GoogleSignonPlugin::~GoogleSignonPlugin()
{
}

SocialNetworkSyncAdaptor *GoogleSignonPlugin::createSocialNetworkSyncAdaptor()
{
    return new GoogleSignonSyncAdaptor(this);
}
