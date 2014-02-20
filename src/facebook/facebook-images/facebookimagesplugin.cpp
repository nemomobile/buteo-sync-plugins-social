/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "facebookimagesplugin.h"
#include "facebookimagesyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" FacebookImagesPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new FacebookImagesPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(FacebookImagesPlugin* plugin)
{
    delete plugin;
}

FacebookImagesPlugin::FacebookImagesPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("facebook"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Images))
{
}

FacebookImagesPlugin::~FacebookImagesPlugin()
{
}

SocialNetworkSyncAdaptor *FacebookImagesPlugin::createSocialNetworkSyncAdaptor()
{
    return new FacebookImageSyncAdaptor(this);
}
