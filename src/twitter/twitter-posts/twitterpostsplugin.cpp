/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "twitterpostsplugin.h"
#include "twitterhometimelinesyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" TwitterPostsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new TwitterPostsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(TwitterPostsPlugin* plugin)
{
    delete plugin;
}

TwitterPostsPlugin::TwitterPostsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("twitter"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Posts))
{
}

TwitterPostsPlugin::~TwitterPostsPlugin()
{
}

SocialNetworkSyncAdaptor *TwitterPostsPlugin::createSocialNetworkSyncAdaptor()
{
    return new TwitterHomeTimelineSyncAdaptor(this);
}
