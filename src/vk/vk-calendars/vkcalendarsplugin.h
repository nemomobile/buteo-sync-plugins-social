/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef VKCALENDARSPLUGIN_H
#define VKCALENDARSPLUGIN_H

#include "socialdbuteoplugin.h"

class VKCalendarsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    VKCalendarsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~VKCalendarsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" VKCalendarsPlugin* createPlugin(const QString& pluginName,
                                           const Buteo::SyncProfile& profile,
                                           Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(VKCalendarsPlugin* client);

#endif // VKCALENDARSPLUGIN_H
