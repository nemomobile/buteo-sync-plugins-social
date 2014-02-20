/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "facebookcalendarsplugin.h"
#include "facebookcalendarsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" FacebookCalendarsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new FacebookCalendarsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(FacebookCalendarsPlugin* plugin)
{
    delete plugin;
}

FacebookCalendarsPlugin::FacebookCalendarsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("facebook"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Calendars))
{
}

FacebookCalendarsPlugin::~FacebookCalendarsPlugin()
{
}

SocialNetworkSyncAdaptor *FacebookCalendarsPlugin::createSocialNetworkSyncAdaptor()
{
    return new FacebookCalendarSyncAdaptor(this);
}
