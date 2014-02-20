/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "constants_p.h"
#include <qtcontacts-extensions_impl.h>
#include <qcontactoriginmetadata_impl.h>

#include "facebookpostsplugin.h"
#include "facebookpostsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" FacebookPostsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new FacebookPostsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(FacebookPostsPlugin* plugin)
{
    delete plugin;
}

FacebookPostsPlugin::FacebookPostsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("facebook"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Posts))
{
}

FacebookPostsPlugin::~FacebookPostsPlugin()
{
}

SocialNetworkSyncAdaptor *FacebookPostsPlugin::createSocialNetworkSyncAdaptor()
{
    return new FacebookPostSyncAdaptor(this);
}
