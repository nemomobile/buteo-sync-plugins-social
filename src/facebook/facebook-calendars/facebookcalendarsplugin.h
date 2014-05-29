/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKCALENDARSPLUGIN_H
#define FACEBOOKCALENDARSPLUGIN_H

#include "socialdbuteoplugin.h"

class SOCIALDBUTEOPLUGIN_EXPORT FacebookCalendarsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    FacebookCalendarsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~FacebookCalendarsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" FacebookCalendarsPlugin* createPlugin(const QString& pluginName,
                                               const Buteo::SyncProfile& profile,
                                               Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(FacebookCalendarsPlugin* client);

#endif // FACEBOOKCALENDARSPLUGIN_H
