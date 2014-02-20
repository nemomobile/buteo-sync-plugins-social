/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "facebooknotificationsplugin.h"
#include "facebooknotificationsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" FacebookNotificationsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new FacebookNotificationsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(FacebookNotificationsPlugin* plugin)
{
    delete plugin;
}

FacebookNotificationsPlugin::FacebookNotificationsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("facebook"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Notifications))
{
}

FacebookNotificationsPlugin::~FacebookNotificationsPlugin()
{
}

SocialNetworkSyncAdaptor *FacebookNotificationsPlugin::createSocialNetworkSyncAdaptor()
{
    return new FacebookNotificationSyncAdaptor(this);
}
