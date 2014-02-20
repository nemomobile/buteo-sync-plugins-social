/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "twitternotificationsplugin.h"
#include "twittermentiontimelinesyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" TwitterNotificationsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new TwitterNotificationsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(TwitterNotificationsPlugin* plugin)
{
    delete plugin;
}

TwitterNotificationsPlugin::TwitterNotificationsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("twitter"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Notifications))
{
}

TwitterNotificationsPlugin::~TwitterNotificationsPlugin()
{
}

SocialNetworkSyncAdaptor *TwitterNotificationsPlugin::createSocialNetworkSyncAdaptor()
{
    return new TwitterMentionTimelineSyncAdaptor(this);
}
