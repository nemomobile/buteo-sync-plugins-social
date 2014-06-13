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

#include "twitterpostsplugin.h"
#include "twitterhometimelinesyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" TwitterPostsPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new TwitterPostsPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(TwitterPostsPlugin* plugin)
{
    delete plugin;
}

TwitterPostsPlugin::TwitterPostsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("twitter"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Posts))
{
}

TwitterPostsPlugin::~TwitterPostsPlugin()
{
}

SocialNetworkSyncAdaptor *TwitterPostsPlugin::createSocialNetworkSyncAdaptor()
{
    return new TwitterHomeTimelineSyncAdaptor(this);
}
