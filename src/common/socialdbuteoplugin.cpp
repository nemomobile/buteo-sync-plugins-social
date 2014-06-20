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

#include "socialdbuteoplugin.h"
#include "socialnetworksyncadaptor.h"
#include "trace.h"

#include <QCoreApplication>
#include <QTranslator>

#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusPendingCall>

#include "buteosyncfw_p.h"

#include <Accounts/Manager>
#include <Accounts/Account>
#include <Accounts/Service>

namespace {
    static const QString SyncProfileTemplatesKey = QStringLiteral("sync_profile_templates");
    static QString SyncProfileIdKey(const QString &templateProfileName)
    {
        return QStringLiteral("%1/%2").arg(templateProfileName).arg(Buteo::KEY_PROFILE_ID);
    }
    QString createProfile(Buteo::ProfileManager *profileManager,
                          const QString &templateProfileName,
                          Accounts::Account *account,
                          const Accounts::Service &srv,
                          bool enableProfile,
                          const QVariantMap &properties)
    {
        if (!account || !srv.isValid()) {
            qWarning() << "Invalid account or service";
            return QString();
        }
        if (templateProfileName.isEmpty()) {
            qWarning() << "Invalid templateProfileName";
            return QString();
        }

        Accounts::Service prevService = account->selectedService();
        account->selectService(srv);

        Buteo::SyncProfile *templateProfile = profileManager->syncProfile(templateProfileName);
        if (!templateProfile) {
            account->selectService(prevService);
            qWarning() << "Unable to load template profile:" << templateProfileName;
            return QString();
        }

        Buteo::SyncProfile *profile = templateProfile->clone();
        if (!profile) {
            delete templateProfile;
            account->selectService(prevService);
            qWarning() << "unable to clone template profile:" << templateProfileName;
            return QString();
        }

        QString accountIdStr = QString::number(account->id());
        profile->setName(templateProfileName + "-" + accountIdStr);
        profile->setKey(Buteo::KEY_DISPLAY_NAME, templateProfileName + "-" + account->displayName().toHtmlEscaped());
        profile->setKey(Buteo::KEY_ACCOUNT_ID, accountIdStr);
        profile->setBoolKey(Buteo::KEY_USE_ACCOUNTS, true);
        profile->setEnabled(enableProfile);

        // enable the profile schedule
        Buteo::SyncSchedule schedule = profile->syncSchedule();
        schedule.setScheduleEnabled(true);
        profile->setSyncSchedule(schedule);

        // set custom properties; note this may override any properties already set
        Q_FOREACH (const QString &key, properties.keys()) {
            profile->setKey(key, properties[key].toString());
        }

        QString profileId = profileManager->updateProfile(*profile);
        if (profileId.isEmpty()) {
            qWarning() << "Unable to save sync profile" << templateProfile->name();
        } else {
            account->setValue(SyncProfileIdKey(templateProfile->name()), profile->name());
        }

        account->selectService(prevService);
        delete profile;
        delete templateProfile;

        return profileId;
    }
}

SocialdButeoPlugin::SocialdButeoPlugin(const QString& pluginName,
                                       const Buteo::SyncProfile& profile,
                                       Buteo::PluginCbInterface *callbackInterface,
                                       const QString &socialServiceName,
                                       const QString &dataTypeName)
    : ClientPlugin(pluginName, profile, callbackInterface)
    , m_socialNetworkSyncAdaptor(0)
    , m_socialServiceName(socialServiceName)
    , m_dataTypeName(dataTypeName)
    , m_profileAccountId(0)
{
}

SocialdButeoPlugin::~SocialdButeoPlugin()
{
}

bool SocialdButeoPlugin::init()
{
    m_profileAccountId = profile().key(Buteo::KEY_ACCOUNT_ID).toInt();
    m_socialNetworkSyncAdaptor = createSocialNetworkSyncAdaptor();
    if (m_socialNetworkSyncAdaptor) {
        connect(m_socialNetworkSyncAdaptor, SIGNAL(statusChanged()), this, SLOT(syncStatusChanged()));
        return true;
    }

    return false;
}

bool SocialdButeoPlugin::uninit()
{
    delete m_socialNetworkSyncAdaptor;
    m_socialNetworkSyncAdaptor = 0;
    return true;
}

bool SocialdButeoPlugin::startSync()
{
    // if the profile being triggered is the template profile, then we
    // need to ensure that the appropriate per-account profiles exist.
    if (m_profileAccountId == 0) {
        QList<Buteo::SyncProfile*> perAccountProfiles = ensurePerAccountSyncProfilesExist();
        m_socialNetworkSyncAdaptor->setAccountSyncProfile(NULL);

        // we need to trigger sync with each profile separately,
        // or (due to scheduling/etc) another plugin instance might
        // be created to sync that profile at the same time, and
        // we don't handle concurrency.
        foreach (Buteo::SyncProfile *perAccountProfile, perAccountProfiles) {
            QDBusMessage message = QDBusMessage::createMethodCall(
                    "com.meego.msyncd", "/synchronizer", "com.meego.msyncd", "startSync");
            message.setArguments(QVariantList() << perAccountProfile->name());
            QDBusConnection::sessionBus().asyncCall(message);
        }
    } else {
        m_socialNetworkSyncAdaptor->setAccountSyncProfile(profile().clone());
    }

    // now perform sync.  Note that for the template profile case, this will
    // result in a purge operation occurring (checking for removed accounts and
    // purging any synced data associated with those accounts).
    if (m_socialNetworkSyncAdaptor && m_socialNetworkSyncAdaptor->enabled()) {
        if (m_socialNetworkSyncAdaptor->status() == SocialNetworkSyncAdaptor::Inactive) {
            TRACE(SOCIALD_DEBUG,
                  QString(QLatin1String("performing sync of %1 from %2 for account %3"))
                  .arg(m_dataTypeName).arg(m_socialServiceName).arg(m_profileAccountId));
            m_socialNetworkSyncAdaptor->sync(m_dataTypeName, m_profileAccountId);
            return true;
        } else {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("%1 sync adaptor for %2 is still busy with last sync of account %3"))
                  .arg(m_socialServiceName)
                  .arg(m_dataTypeName)
                  .arg(m_profileAccountId));
        }
    } else {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("no enabled %1 sync adaptor for %2"))
              .arg(m_socialServiceName)
              .arg(m_dataTypeName));
    }
    return false;
}

void SocialdButeoPlugin::abortSync(Sync::SyncStatus)
{
    // TODO
}

bool SocialdButeoPlugin::cleanUp()
{
    m_profileAccountId = profile().key(Buteo::KEY_ACCOUNT_ID).toInt();
    if (!m_socialNetworkSyncAdaptor) {
        // might have already been initialized by the OOP framework via init().
        m_socialNetworkSyncAdaptor = createSocialNetworkSyncAdaptor();
    }

    if (m_socialNetworkSyncAdaptor && m_profileAccountId > 0) {
        m_socialNetworkSyncAdaptor->purgeDataForOldAccounts(QList<int>() << m_profileAccountId,
                                                            SocialNetworkSyncAdaptor::CleanUpPurge);
    }

    return true;
}

Buteo::SyncResults SocialdButeoPlugin::getSyncResults() const
{
    return m_syncResults;
}

void SocialdButeoPlugin::connectivityStateChanged(Sync::ConnectivityType, bool)
{
    // TODO, see TransportTracker.cpp:149
    // Sync::CONNECTIVITY_INTERNET, true|false
    // Kill all ongoing on false
    // "Free" single shot sync on wlan?
}

void SocialdButeoPlugin::syncStatusChanged()
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

void SocialdButeoPlugin::updateResults(const Buteo::SyncResults &results)
{
    m_syncResults = results;
    m_syncResults.setScheduled(true);
}

// This function is called when the non-per-account profile is triggered.
// The implementation does:
// - get all profiles from the ProfileManager
// - get all accounts from the AccountManager
// - build a mapping of profile -> account for the current data type. (should be one-to-one for the datatype).
// - any account which doesn't have a profile, print an error.
// - check the enabled status of the account -> ensure that the enabled status is reflected in the profile.
// It then returns a list of the appropriate (per account for this data-type) sync profiles.
// The caller takes ownership of the list.
QList<Buteo::SyncProfile*> SocialdButeoPlugin::ensurePerAccountSyncProfilesExist()
{
    Accounts::Manager am;
    Accounts::AccountIdList accountIds = am.accountList();
    QList<Buteo::SyncProfile*> syncProfiles = m_profileManager.allSyncProfiles();
    QMap<Accounts::Account*, Buteo::SyncProfile*> perAccountProfiles;

    Accounts::Service dataTypeSyncService = am.service(m_socialNetworkSyncAdaptor->syncServiceName());
    if (!dataTypeSyncService.isValid()) {
        qWarning() << Q_FUNC_INFO << "Invalid data type sync service name specified:"
                   << m_socialNetworkSyncAdaptor->syncServiceName();
        return QList<Buteo::SyncProfile*>();
    }

    for (int i = 0; i < accountIds.size(); ++i) {
        Accounts::Account *currAccount = am.account(accountIds.at(i));
        if (!currAccount || currAccount->id() == 0
                || m_socialNetworkSyncAdaptor->syncServiceName().split('-').first() != currAccount->providerName()) {
            // we only generate per-account sync profiles for accounts which
            // are provided by the provider which provides our sync service.
            continue;
        }

        // for the current account, find the associated sync profile.
        bool foundProfile = false;
        for (int j = 0; j < syncProfiles.size(); ++j) {
            if (syncProfiles[j]->key(Buteo::KEY_ACCOUNT_ID).toInt() == QString::number(currAccount->id()).toInt()
                    && syncProfiles[j]->clientProfile() != NULL
                    && syncProfiles[j]->clientProfile()->name() == profile().clientProfile()->name()) {
                // we have found the sync profile for this datatype for this account.
                foundProfile = true;
                perAccountProfiles.insert(currAccount, syncProfiles.takeAt(j));
                break;
            }
        }

        if (!foundProfile) {
            // it should have been generated for the account when the account was added.
            TRACE(SOCIALD_INFORMATION,
                    QString(QLatin1String("no per-account %1 sync profile exists for account: %2"))
                    .arg(profile().name()).arg(currAccount->id()));

            // create the per-account profile... we shouldn't need to do this...
            QString profileName = createProfile(&m_profileManager, profile().name(), currAccount, dataTypeSyncService, true, QVariantMap());
            Buteo::SyncProfile *newProfile = m_profileManager.syncProfile(profileName);
            if (!newProfile) {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("unable to create per-account %1 sync profile for account: %2"))
                        .arg(profile().name()).arg(currAccount->id()));
            } else {
                // enable the sync schedule for the profile.
                Buteo::SyncSchedule schedule = newProfile->syncSchedule();
                schedule.setScheduleEnabled(true);
                newProfile->setSyncSchedule(schedule);
                m_profileManager.updateProfile(*newProfile);
                // and return the profile in the map.
                perAccountProfiles.insert(currAccount, newProfile);
            }
        }
    }

    // Every account now has the appropriate sync profile.
    qDeleteAll(syncProfiles); // these are for the wrong data type, ignore them.
    QList<Buteo::SyncProfile *> retn;
    foreach (Accounts::Account *acc, perAccountProfiles.keys()) {
        retn.append(perAccountProfiles[acc]);
        acc->deleteLater();
    }

    return retn;
}
