/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Raine Makelainen <raine.makelainen@jollamobile.com>
 **
 ****************************************************************************/

#ifndef SOCIALDBUTEOPLUGIN_H
#define SOCIALDBUTEOPLUGIN_H

#include <ProfileManager.h>
#include <ClientPlugin.h>
#include <SyncResults.h>

/*
   Datatype-specific implementations of this class
   allow per-account sync profiles for that data type.
*/

class SocialNetworkSyncAdaptor;
class SocialdButeoPlugin : public Buteo::ClientPlugin
{
    Q_OBJECT

protected:
    virtual SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor() = 0;

public:
    SocialdButeoPlugin(const QString& pluginName,
                       const Buteo::SyncProfile& profile,
                       Buteo::PluginCbInterface *cbInterface,
                       const QString &socialServiceName,
                       const QString &dataTypeName);
    virtual ~SocialdButeoPlugin();

    bool init();
    bool uninit();
    bool startSync();
    void abortSync(Sync::SyncStatus status = Sync::SYNC_ABORTED);
    Buteo::SyncResults getSyncResults() const;
    bool cleanUp();

public Q_SLOTS:
    void connectivityStateChanged(Sync::ConnectivityType type, bool state);

private Q_SLOTS:
    void syncStatusChanged();

protected:
    QList<Buteo::SyncProfile*> ensurePerAccountSyncProfilesExist();

private:
    void updateResults(const Buteo::SyncResults &results);
    Buteo::SyncResults m_syncResults;
    Buteo::ProfileManager m_profileManager;
    SocialNetworkSyncAdaptor *m_socialNetworkSyncAdaptor;
    QString m_socialServiceName;
    QString m_dataTypeName;
    int m_profileAccountId;
};

/*
   Convenience macros for datatype-specific derived types.
   Currently commented out because I don't know how useful they are.

   eg:
   SOCIALD_BUTEO_PLUGIN_DERIVED_DECL(Facebook, Images)
   SOCIALD_BUTEO_PLUGIN_DERIVED_IMPL(facebook, Facebook, Images,
                                     FacebookImageSyncAdaptor,
                                     facebookimagesyncadaptor)

#define SOCIALD_BUTEO_PLUGIN_DERIVED_DECL(                                   \
            cap_provider,                                                    \
            cap_datatype)                                                    \
                                                                             \
#include <ClientPlugin.h>                                                    \
                                                                             \
class cap_provider ## cap_datatype ## Plugin : public SocialdButeoPlugin     \
{                                                                            \
    Q_OBJECT                                                                 \
                                                                             \
public:                                                                      \
    cap_provider ## cap_datatype ## Plugin(                                  \
              const QString& pluginName,                                     \
              const Buteo::SyncProfile& profile,                             \
              Buteo::PluginCbInterface *cbInterface);                        \
    ~ ## cap_provider ## cap_datatype ## Plugin();                           \
                                                                             \
protected:                                                                   \
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();              \
};                                                                           \
                                                                             \
extern "C" cap_provider ## cap_datatype ## Plugin* createPlugin(             \
        const QString& pluginName,                                           \
        const Buteo::SyncProfile& profile,                                   \
        Buteo::PluginCbInterface *cbInterface);                              \
                                                                             \
extern "C" void destroyPlugin(cap_provider ## cap_datatype ## Plugin* client);



#define SOCIALD_BUTEO_PLUGIN_DERIVED_IMPL(                                   \
            lower_provider,                                                  \
            cap_provider,                                                    \
            cap_datatype,                                                    \
            cap_adaptor_name,                                                \
            lower_adaptor_name)                                              \
                                                                             \
#include "lower_adaptor_name##.h"                                            \
#include "socialnetworksyncadaptor.h"                                        \
                                                                             \
extern "C" cap_provider ## cap_datatype ## Plugin*                           \
createPlugin(const QString& pluginName,                                      \
             const Buteo::SyncProfile& profile,                              \
             Buteo::PluginCbInterface *callbackInterface)                    \
{                                                                            \
    return new cap_provider ## cap_datatype ## Plugin(                       \
            pluginName, profile, callbackInterface);                         \
}                                                                            \
                                                                             \
extern "C" void destroyPlugin(cap_provider ## cap_datatype ## Plugin* plugin)\
{                                                                            \
    delete plugin;                                                           \
}                                                                            \
                                                                             \
cap_provider##cap_datatype##Plugin::cap_provider##cap_datatype##Plugin(      \
                             const QString& pluginName,                      \
                             const Buteo::SyncProfile& profile,              \
                             Buteo::PluginCbInterface *callbackInterface)    \
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,             \
                         QStringLiteral(#lower_provider),                    \
                         SocialNetworkSyncAdaptor::dataTypeName(             \
                             SocialNetworkSyncAdaptor::##cap_datatype))      \
{                                                                            \
}                                                                            \
                                                                             \
cap_provider##cap_datatype##Plugin::~##cap_provider##cap_datatype##cap_datatypePlugin()\
{                                                                            \
}                                                                            \
                                                                             \
SocialNetworkSyncAdaptor*                                                    \
cap_provider ## cap_datatype ## Plugin::createSocialNetworkSyncAdaptor()     \
{                                                                            \
    return new cap_adaptor_name(this);                                       \
}

*/

#endif // SOCIALDBUTEOPLUGIN_H
