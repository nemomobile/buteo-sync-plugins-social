/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "googlecalendarsplugin.h"
#include "googlecalendarsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" GoogleCalendarsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new GoogleCalendarsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(GoogleCalendarsPlugin* plugin)
{
    delete plugin;
}

GoogleCalendarsPlugin::GoogleCalendarsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("google"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Calendars))
{
}

GoogleCalendarsPlugin::~GoogleCalendarsPlugin()
{
}

SocialNetworkSyncAdaptor *GoogleCalendarsPlugin::createSocialNetworkSyncAdaptor()
{
    return new GoogleCalendarSyncAdaptor(this);
}
