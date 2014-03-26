/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef TWITTERPOSTSPLUGIN_H
#define TWITTERPOSTSPLUGIN_H

#include "socialdbuteoplugin.h"

class TwitterPostsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    TwitterPostsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~TwitterPostsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" TwitterPostsPlugin* createPlugin(const QString& pluginName,
                                                const Buteo::SyncProfile& profile,
                                                Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(TwitterPostsPlugin* client);

#endif // TWITTERPOSTSPLUGIN_H
