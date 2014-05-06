/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef VKNOTIFICATIONSPLUGIN_H
#define VKNOTIFICATIONSPLUGIN_H

#include "socialdbuteoplugin.h"

class VKNotificationsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    VKNotificationsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~VKNotificationsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" VKNotificationsPlugin* createPlugin(const QString& pluginName,
                                                const Buteo::SyncProfile& profile,
                                                Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(VKNotificationsPlugin* client);

#endif // VKNOTIFICATIONSPLUGIN_H
