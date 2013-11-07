/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Raine Makelainen <raine.makelainen@jollamobile.com>
 **
 ****************************************************************************/

#include <PluginCbInterface.h>
#include <LogMacros.h>

#include <QCoreApplication>
#include <QTranslator>

#include "trace.h"
#include "buteosocialsync.h"

#include "syncservice.h"
#include "socialnetworksyncadaptor.h"

extern "C" ButeoSocial* createPlugin(const QString& pluginName, const Buteo::SyncProfile& profile, Buteo::PluginCbInterface *callbackInterface)
{
    return new ButeoSocial(pluginName, profile, callbackInterface);
}

extern "C" void destroyPlugin(ButeoSocial* plugin)
{
    delete plugin;
}

ButeoSocial::ButeoSocial(const QString& pluginName, const Buteo::SyncProfile& profile, Buteo::PluginCbInterface *callbackInterface)
    : ClientPlugin(pluginName, profile, callbackInterface)
    , m_syncService(0)
    , m_socialNetworkSyncAdaptor(0)
{
    QString translationPath("/usr/share/translations/");
    // QTranslator life-cycles owner by ButeoSocial and removed by its own destructor
    QTranslator *engineeringEnglish = new QTranslator(this);
    engineeringEnglish->load("sociald_eng_en", translationPath);
    QCoreApplication::instance()->installTranslator(engineeringEnglish);

    QTranslator *translator = new QTranslator(this);
    translator->load(QLocale(), "sociald", "-", translationPath);
    QCoreApplication::instance()->installTranslator(translator);
}

ButeoSocial::~ButeoSocial()
{
}

bool ButeoSocial::init()
{
    // Profile names look like twitter.Posts, facebook.Posts, etc.
    QString profile = getProfileName();
    QStringList servicePlusDataType = profile.split(".");
    if (servicePlusDataType.length() == 2) {
        m_serviceName = servicePlusDataType.at(0);
        m_dataType = servicePlusDataType.at(1);
        // Database connections are created per profile
        m_syncService = new SyncService(profile, this);
        m_socialNetworkSyncAdaptor = m_syncService->createAdaptor(m_serviceName, m_dataType, this);
        if (m_socialNetworkSyncAdaptor) {
            if (!m_socialNetworkSyncAdaptor->enabled()) {
                TRACE(SOCIALD_DEBUG,
                      QString(QLatin1String("%1 is not currently enabled")).arg(m_serviceName));
                return false;
            }

            connect(m_socialNetworkSyncAdaptor, SIGNAL(statusChanged()), this, SLOT(syncStatusChanged()));
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

bool ButeoSocial::uninit()
{
    delete m_socialNetworkSyncAdaptor;
    m_socialNetworkSyncAdaptor = 0;
    delete m_syncService;
    m_syncService = 0;

    return true;
}

bool ButeoSocial::startSync()
{
    if (m_socialNetworkSyncAdaptor && m_socialNetworkSyncAdaptor->enabled()) {
        if (m_socialNetworkSyncAdaptor->status() == SocialNetworkSyncAdaptor::Inactive) {
            TRACE(SOCIALD_DEBUG,
                  QString(QLatin1String("performing sync of %1 from %2"))
                  .arg(m_dataType).arg(m_serviceName));
            m_socialNetworkSyncAdaptor->sync(m_dataType);
            return true;
        } else {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("%1 sync adaptor for %2 is still busy with last sync"))
                  .arg(m_serviceName)
                  .arg(m_dataType));
        }
    } else {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("no enabled %1 sync adaptor for %2"))
              .arg(m_serviceName)
              .arg(m_dataType));
    }
    return false;
}

void ButeoSocial::abortSync(Sync::SyncStatus)
{
    // TODO ? Should we do something over here?
}

bool ButeoSocial::cleanUp()
{
    // TODO anything needed?, see synchronizer.cpp:875
    // PluginRunner gets destroyed rigth after cleanup()
    return true;
}

Buteo::SyncResults ButeoSocial::getSyncResults() const
{
    return m_syncResults;
}

void ButeoSocial::connectivityStateChanged(Sync::ConnectivityType, bool)
{
    // TODO, see TransportTracker.cpp:149
    // Sync::CONNECTIVITY_INTERNET, true|false
    // Kill all ongoing on false
    // "Free" single shot sync on wlan?
}

void ButeoSocial::syncStatusChanged()
{
    if (m_socialNetworkSyncAdaptor) {
        SocialNetworkSyncAdaptor::Status syncStatus = m_socialNetworkSyncAdaptor->status();
        // Busy change comes when sync starts -> let's ignore that.
        if (syncStatus == SocialNetworkSyncAdaptor::Inactive) {
            updateResults(Buteo::SyncResults(QDateTime::currentDateTime(), Buteo::SyncResults::SYNC_RESULT_SUCCESS, Buteo::SyncResults::NO_ERROR));
            emit success(getProfileName(), QString("%1 update succeeded").arg(getProfileName()));
        } else if (syncStatus != SocialNetworkSyncAdaptor::Busy) {
            updateResults(Buteo::SyncResults(QDateTime::currentDateTime(), Buteo::SyncResults::SYNC_RESULT_FAILED, Buteo::SyncResults::ABORTED));
            emit error(getProfileName(), QString("%1 update failed").arg(getProfileName()), Buteo::SyncResults::SYNC_RESULT_FAILED);
        }
    } else {
        updateResults(Buteo::SyncResults(QDateTime::currentDateTime(), Buteo::SyncResults::SYNC_RESULT_FAILED, Buteo::SyncResults::ABORTED));
        emit error(getProfileName(), QString("%1 update failed").arg(getProfileName()), Buteo::SyncResults::SYNC_RESULT_FAILED);
    }
}

void ButeoSocial::updateResults(const Buteo::SyncResults &results)
{
    m_syncResults = results;
    m_syncResults.setScheduled(true);
}
