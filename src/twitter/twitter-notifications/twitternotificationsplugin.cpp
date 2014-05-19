/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "twitternotificationsplugin.h"
#include "twittermentiontimelinesyncadaptor.h"
#include "socialnetworksyncadaptor.h"

#include <QTranslator>
#include <QCoreApplication>

extern "C" TwitterNotificationsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new TwitterNotificationsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(TwitterNotificationsPlugin* plugin)
{
    delete plugin;
}

TwitterNotificationsPlugin::TwitterNotificationsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("twitter"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Notifications))
{
    QString translationPath("/usr/share/translations/");
    // QTranslator life-cycle: owned by ButeoSocial and removed by its own destructor
    QTranslator *engineeringEnglish = new QTranslator(this);
    engineeringEnglish->load("lipstick-jolla-home-twitter-notif_eng_en", translationPath);
    QCoreApplication::instance()->installTranslator(engineeringEnglish);

    QTranslator *translator = new QTranslator(this);
    translator->load(QLocale(), "lipstick-jolla-home-twitter-notif", "-", translationPath);
    QCoreApplication::instance()->installTranslator(translator);
}

TwitterNotificationsPlugin::~TwitterNotificationsPlugin()
{
}

SocialNetworkSyncAdaptor *TwitterNotificationsPlugin::createSocialNetworkSyncAdaptor()
{
    return new TwitterMentionTimelineSyncAdaptor(this);
}
