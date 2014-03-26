/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKPOSTSPLUGIN_H
#define FACEBOOKPOSTSPLUGIN_H

#include "socialdbuteoplugin.h"

class FacebookPostsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    FacebookPostsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~FacebookPostsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" FacebookPostsPlugin* createPlugin(const QString& pluginName,
                                                const Buteo::SyncProfile& profile,
                                                Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(FacebookPostsPlugin* client);

#endif // FACEBOOKPOSTSPLUGIN_H
