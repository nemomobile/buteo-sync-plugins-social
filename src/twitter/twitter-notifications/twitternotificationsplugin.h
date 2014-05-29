/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef TWITTERNOTIFICATIONSPLUGIN_H
#define TWITTERNOTIFICATIONSPLUGIN_H

#include "socialdbuteoplugin.h"

class SOCIALDBUTEOPLUGIN_EXPORT TwitterNotificationsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    TwitterNotificationsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~TwitterNotificationsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" TwitterNotificationsPlugin* createPlugin(const QString& pluginName,
                                                const Buteo::SyncProfile& profile,
                                                Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(TwitterNotificationsPlugin* client);

#endif // TWITTERNOTIFICATIONSPLUGIN_H
