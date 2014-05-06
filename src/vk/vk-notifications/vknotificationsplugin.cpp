/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "vknotificationsplugin.h"
#include "vknotificationsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" VKNotificationsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new VKNotificationsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(VKNotificationsPlugin* plugin)
{
    delete plugin;
}

VKNotificationsPlugin::VKNotificationsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("vk"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Notifications))
{
}

VKNotificationsPlugin::~VKNotificationsPlugin()
{
}

SocialNetworkSyncAdaptor *VKNotificationsPlugin::createSocialNetworkSyncAdaptor()
{
    return new VKNotificationSyncAdaptor(this);
}
