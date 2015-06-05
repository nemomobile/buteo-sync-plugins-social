/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ** This program/library is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation.
 **
 ** This program/library is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 ** Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public
 ** License along with this program/library; if not, write to the Free
 ** Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 ** 02110-1301 USA
 **
 ****************************************************************************/

#include "twitternotificationsplugin.h"
#include "twitternotificationsyncadaptor.h"
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
    return new TwitterNotificationSyncAdaptor(this);
}
