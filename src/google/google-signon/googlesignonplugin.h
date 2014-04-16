/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef GOOGLESIGNONPLUGIN_H
#define GOOGLESIGNONPLUGIN_H

#include "socialdbuteoplugin.h"
#include <ClientPlugin.h>

class GoogleSignonPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    GoogleSignonPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~GoogleSignonPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" GoogleSignonPlugin* createPlugin(const QString& pluginName,
                                            const Buteo::SyncProfile& profile,
                                            Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(GoogleSignonPlugin* client);

#endif // GOOGLESIGNONPLUGIN_H
