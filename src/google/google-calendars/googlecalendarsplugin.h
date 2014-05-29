/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#ifndef GOOGLECALENDARSPLUGIN_H
#define GOOGLECALENDARSPLUGIN_H

#include "socialdbuteoplugin.h"

class SOCIALDBUTEOPLUGIN_EXPORT GoogleCalendarsPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    GoogleCalendarsPlugin(const QString& pluginName,
                  const Buteo::SyncProfile& profile,
                  Buteo::PluginCbInterface *cbInterface);
    ~GoogleCalendarsPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" GoogleCalendarsPlugin* createPlugin(const QString& pluginName,
                                               const Buteo::SyncProfile& profile,
                                               Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(GoogleCalendarsPlugin* client);

#endif // GOOGLECALENDARSPLUGIN_H
