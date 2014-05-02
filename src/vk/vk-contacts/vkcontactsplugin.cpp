/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "constants_p.h"
#include <qtcontacts-extensions_impl.h>
#include <qcontactoriginmetadata_impl.h>

#include "vkcontactsplugin.h"
#include "vkcontactsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" VKContactsPlugin* createPlugin(const QString& pluginName,
                                          const Buteo::SyncProfile& profile,
                                          Buteo::PluginCbInterface *callbackInterface)
{
    return new VKContactsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(VKContactsPlugin* plugin)
{
    delete plugin;
}

VKContactsPlugin::VKContactsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("vk"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Contacts))
{
}

VKContactsPlugin::~VKContactsPlugin()
{
}

SocialNetworkSyncAdaptor *VKContactsPlugin::createSocialNetworkSyncAdaptor()
{
    return new VKContactSyncAdaptor(this);
}
