/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef VKCONTACTSPLUGIN_H
#define VKCONTACTSPLUGIN_H

#include "socialdbuteoplugin.h"

class VKContactsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    VKContactsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~VKContactsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" VKContactsPlugin* createPlugin(const QString& pluginName,
                                                const Buteo::SyncProfile& profile,
                                                Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(VKContactsPlugin* client);

#endif // VKCONTACTSPLUGIN_H
