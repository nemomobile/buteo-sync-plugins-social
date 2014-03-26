/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKCONTACTSPLUGIN_H
#define FACEBOOKCONTACTSPLUGIN_H

#include "socialdbuteoplugin.h"

class FacebookContactsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    FacebookContactsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~FacebookContactsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" FacebookContactsPlugin* createPlugin(const QString& pluginName,
                                                const Buteo::SyncProfile& profile,
                                                Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(FacebookContactsPlugin* client);

#endif // FACEBOOKCONTACTSPLUGIN_H
