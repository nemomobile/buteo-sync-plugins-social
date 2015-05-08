/****************************************************************************
 **
 ** Copyright (C) 2015 Jolla Ltd.
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

#ifndef VKIMAGESPLUGIN_H
#define VKIMAGESPLUGIN_H

#include "socialdbuteoplugin.h"

class SOCIALDBUTEOPLUGIN_EXPORT VKImagesPlugin : public SocialdButeoPlugin
{
    Q_OBJECT

public:
    VKImagesPlugin(const QString& pluginName,
                   const Buteo::SyncProfile& profile,
                   Buteo::PluginCbInterface *cbInterface);
    ~VKImagesPlugin();

protected:
    SocialNetworkSyncAdaptor *createSocialNetworkSyncAdaptor();
};

extern "C" VKImagesPlugin* createPlugin(const QString& pluginName,
                                        const Buteo::SyncProfile& profile,
                                        Buteo::PluginCbInterface *cbInterface);

extern "C" void destroyPlugin(VKImagesPlugin* client);

#endif // VKIMAGESPLUGIN_H
