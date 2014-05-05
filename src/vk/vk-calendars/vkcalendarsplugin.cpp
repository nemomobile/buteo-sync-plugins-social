/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "vkcalendarsplugin.h"
#include "vkcalendarsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" VKCalendarsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new VKCalendarsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(VKCalendarsPlugin* plugin)
{
    delete plugin;
}

VKCalendarsPlugin::VKCalendarsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("vk"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Calendars))
{
}

VKCalendarsPlugin::~VKCalendarsPlugin()
{
}

SocialNetworkSyncAdaptor *VKCalendarsPlugin::createSocialNetworkSyncAdaptor()
{
    return new VKCalendarSyncAdaptor(this);
}
