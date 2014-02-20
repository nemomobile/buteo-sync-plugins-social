/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef GOOGLECONTACTSPLUGIN_H
#define GOOGLECONTACTSPLUGIN_H

#include "socialdbuteoplugin.h"
#include <ClientPlugin.h>

class GoogleContactsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    GoogleContactsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~GoogleContactsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" GoogleContactsPlugin* createPlugin(const QString& pluginName,
                                              const Buteo::SyncProfile& profile,
                                              Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(GoogleContactsPlugin* client);

#endif // GOOGLECONTACTSPLUGIN_H
