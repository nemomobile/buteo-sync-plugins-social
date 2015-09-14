/****************************************************************************
 **
 ** Copyright (C) 2015 Jolla Ltd.
 ** Contact: Antti Seppälä <antti.seppala@jolla.com>
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

#include "onedriveimagesplugin.h"
#include "onedriveimagesyncadaptor.h"
#include "socialnetworksyncadaptor.h"

extern "C" OneDriveImagesPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new OneDriveImagesPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(OneDriveImagesPlugin* plugin)
{
    delete plugin;
}

OneDriveImagesPlugin::OneDriveImagesPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("onedrive"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Images))
{
}

OneDriveImagesPlugin::~OneDriveImagesPlugin()
{
}

SocialNetworkSyncAdaptor *OneDriveImagesPlugin::createSocialNetworkSyncAdaptor()
{
    return new OneDriveImageSyncAdaptor(this);
}
