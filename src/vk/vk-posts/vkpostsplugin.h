/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef VKPOSTSPLUGIN_H
#define VKPOSTSPLUGIN_H

#include "socialdbuteoplugin.h"

class VKPostsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    VKPostsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~VKPostsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" VKPostsPlugin* createPlugin(const QString& pluginName,
                                                const Buteo::SyncProfile& profile,
                                                Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(VKPostsPlugin* client);

#endif // VKPOSTSPLUGIN_H
