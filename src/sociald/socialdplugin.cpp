/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Raine Makelainen <raine.makelainen@jollamobile.com>
 **
 ****************************************************************************/

#include "socialdplugin.h"
#include "trace.h"

#include <QCoreApplication>
#include <QTranslator>
#include <QStringList>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

#include <PluginCbInterface.h>
#include <LogMacros.h>

extern "C" SocialdPlugin* createPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface)
{
    return new SocialdPlugin(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(SocialdPlugin* plugin)
{
    delete plugin;
}

SocialdPlugin::SocialdPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : ClientPlugin(pluginName, profile, callbackInterface)
{
}

SocialdPlugin::~SocialdPlugin()
{
}

bool SocialdPlugin::init()
{
    // sociald plugin profiles are either sociald.All.xml or
    // of the form sociald.<provider>.<Datatype>.xml
    QString profile = getProfileName();
    if (profile == QStringLiteral("sociald.All")) {
        m_dataType.clear();
        m_serviceName.clear();
        return true;
    }

    // specific datatype sync.
    QStringList servicePlusDataType = profile.split(".");
    if (servicePlusDataType.length() == 3 && servicePlusDataType.at(0) == QStringLiteral("sociald")) {
        m_serviceName = servicePlusDataType.at(1);
        m_dataType = servicePlusDataType.at(2);
        return true;
    }

    return false;
}

bool SocialdPlugin::uninit()
{
    return true;
}

bool SocialdPlugin::startSync()
{
    QStringList startSyncParams;
    if (!m_dataType.isEmpty() && !m_serviceName.isEmpty()) {
        // trigger sync of specific data type with all accounts.
        startSyncParams.append(QStringLiteral("%1.%2").arg(m_serviceName, m_dataType));
    } else {
        // trigger sync of all known data types with all accounts.
        startSyncParams << "google.Calendars";
        startSyncParams << "google.Contacts";
        startSyncParams << "facebook.Calendars";
        startSyncParams << "facebook.Contacts";
        startSyncParams << "facebook.Images";
        startSyncParams << "facebook.Notifications";
        //startSyncParams << "facebook.Posts";
        startSyncParams << "twitter.Notifications";
        startSyncParams << "twitter.Posts";
    }

    foreach (const QString &param, startSyncParams) {
        QDBusMessage message = QDBusMessage::createMethodCall(
                "com.meego.msyncd", "/synchronizer", "com.meego.msyncd", "startSync");
        message.setArguments(QVariantList() << param);
        QDBusConnection::sessionBus().asyncCall(message);
    }

    // always "succeed" even though the actual sync may fail.
    updateResults(Buteo::SyncResults(QDateTime::currentDateTime(),
                                     Buteo::SyncResults::SYNC_RESULT_SUCCESS,
                                     Buteo::SyncResults::NO_ERROR));
}

void SocialdPlugin::abortSync(Sync::SyncStatus)
{
}

bool SocialdPlugin::cleanUp()
{
    return true;
}

Buteo::SyncResults SocialdPlugin::getSyncResults() const
{
    return m_syncResults;
}

void SocialdPlugin::connectivityStateChanged(Sync::ConnectivityType, bool)
{
    // TODO, see TransportTracker.cpp:149
    // Sync::CONNECTIVITY_INTERNET, true|false
    // Kill all ongoing on false
    // "Free" single shot sync on wlan?
}

void SocialdPlugin::updateResults(const Buteo::SyncResults &results)
{
    m_syncResults = results;
    m_syncResults.setScheduled(true);
}
