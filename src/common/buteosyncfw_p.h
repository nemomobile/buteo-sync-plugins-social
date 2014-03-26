/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef SOCIALD_BUTEOSYNCFW_P_H
#define SOCIALD_BUTEOSYNCFW_P_H

// Several headers from Buteo SyncFW produce warnings
// This means we cannot use -Werror without wrapping them
// in GCC-specific pragmas to ignore warnings from Buteo.
#pragma GCC system_header
#include <SyncCommonDefs.h>
#include <SyncPluginBase.h>
#include <ProfileManager.h>
#include <ClientPlugin.h>
#include <SyncResults.h>
#include <ProfileEngineDefs.h>
#include <SyncProfile.h>
#include <Profile.h>
#include <PluginCbInterface.h>
#include <LogMacros.h>

#endif // SOCIALD_BUTEOSYNCFW_P_H
