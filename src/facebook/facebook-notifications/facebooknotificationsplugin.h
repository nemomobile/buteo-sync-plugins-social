/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKNOTIFICATIONSPLUGIN_H
#define FACEBOOKNOTIFICATIONSPLUGIN_H

#include "socialdbuteoplugin.h"
#include <ClientPlugin.h>

class FacebookNotificationsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    FacebookNotificationsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~FacebookNotificationsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" FacebookNotificationsPlugin* createPlugin(const QString& pluginName,
                                                const Buteo::SyncProfile& profile,
                                                Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(FacebookNotificationsPlugin* client);

#endif // FACEBOOKNOTIFICATIONSPLUGIN_H
