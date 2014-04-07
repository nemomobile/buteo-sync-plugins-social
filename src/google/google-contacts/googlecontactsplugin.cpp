/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "constants_p.h"
#include <qtcontacts-extensions_impl.h>
#include <qcontactoriginmetadata_impl.h>

#include "googlecontactsplugin.h"
#include "googlecontactsyncadaptor.h"
#include "googletwowaycontactsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" GoogleContactsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new GoogleContactsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(GoogleContactsPlugin* plugin)
{
    delete plugin;
}

GoogleContactsPlugin::GoogleContactsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("google"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Contacts))
{
}

GoogleContactsPlugin::~GoogleContactsPlugin()
{
}

SocialNetworkSyncAdaptor *GoogleContactsPlugin::createSocialNetworkSyncAdaptor()
{
    return new GoogleTwoWayContactSyncAdaptor(this);
}
