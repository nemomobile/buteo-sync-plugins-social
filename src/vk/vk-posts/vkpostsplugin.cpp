/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "constants_p.h"
#include <qtcontacts-extensions_impl.h>
#include <qcontactoriginmetadata_impl.h>

#include "vkpostsplugin.h"
#include "vkpostsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" VKPostsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new VKPostsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(VKPostsPlugin* plugin)
{
    delete plugin;
}

VKPostsPlugin::VKPostsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("vk"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Posts))
{
}

VKPostsPlugin::~VKPostsPlugin()
{
}

SocialNetworkSyncAdaptor *VKPostsPlugin::createSocialNetworkSyncAdaptor()
{
    return new VKPostSyncAdaptor(this);
}
