/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKIMAGESPLUGIN_H
#define FACEBOOKIMAGESPLUGIN_H

#include "socialdbuteoplugin.h"
#include <ClientPlugin.h>

class FacebookImagesPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    FacebookImagesPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~FacebookImagesPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" FacebookImagesPlugin* createPlugin(const QString& pluginName,
                                                const Buteo::SyncProfile& profile,
                                                Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(FacebookImagesPlugin* client);

#endif // FACEBOOKIMAGESPLUGIN_H
