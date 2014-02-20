/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "constants_p.h"
#include <qtcontacts-extensions_impl.h>
#include <qcontactoriginmetadata_impl.h>

#include "facebookcontactsplugin.h"
#include "facebookcontactsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" FacebookContactsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new FacebookContactsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(FacebookContactsPlugin* plugin)
{
    delete plugin;
}

FacebookContactsPlugin::FacebookContactsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("facebook"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Contacts))
{
}

FacebookContactsPlugin::~FacebookContactsPlugin()
{
}

SocialNetworkSyncAdaptor *FacebookContactsPlugin::createSocialNetworkSyncAdaptor()
{
    return new FacebookContactSyncAdaptor(this);
}
