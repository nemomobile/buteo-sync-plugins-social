/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ****************************************************************************/

#include "socialdbuteoplugin.h"
#include "socialnetworksyncadaptor.h"
#include "trace.h"

#include <QCoreApplication>
#include <QTranslator>

#include <SyncClientInterface.h>
#include <ProfileEngineDefs.h>
#include <SyncProfile.h>
#include <Profile.h>
#include <PluginCbInterface.h>
#include <LogMacros.h>

#include <Accounts/Manager>
#include <Accounts/Account>
#include <Accounts/Service>


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
        Buteo::SyncClientInterface buteoClient;
        foreach (Buteo::SyncProfile *perAccountProfile, perAccountProfiles) {
            buteoClient.startSync(perAccountProfile->name());
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
    // TODO: ensure that this is ONLY called when the account is deleted,
    // not when the plugin is unloaded.
    if (m_socialNetworkSyncAdaptor && m_profileAccountId > 0) {
        m_socialNetworkSyncAdaptor->purgeDataForOldAccounts(QList<int>() << m_profileAccountId);
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

// Helper function to generate a per-account sync profile for a particular datatype.
Buteo::SyncProfile *generatePerAccountSyncProfile(Accounts::Account *account,
                                                  const Buteo::SyncProfile &templateProfile,
                                                  Buteo::ProfileManager &profileManager,
                                                  const Accounts::Service &syncService)
{
    QString accountIdStr = QString::number(account->id());
    Buteo::SyncProfile *perAccountProfile = templateProfile.clone();
    if (!perAccountProfile) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("unable to clone template profile: %1"))
              .arg(templateProfile.name()));
        return NULL;
    }

    bool profileEnabled = account->enabled();
    account->selectService(syncService);
    profileEnabled = profileEnabled && account->enabled();

    perAccountProfile->setName(templateProfile.name() + "-" + accountIdStr);
    perAccountProfile->setKey(Buteo::KEY_DISPLAY_NAME, templateProfile.name() + "-" + account->displayName());
    perAccountProfile->setKey(Buteo::KEY_ACCOUNT_ID, accountIdStr);
    perAccountProfile->setBoolKey(Buteo::KEY_USE_ACCOUNTS, true);
    perAccountProfile->setEnabled(profileEnabled);
    profileManager.updateProfile(*perAccountProfile);

    account->setValue(QStringLiteral("%1/%2").arg(templateProfile.name()).arg(Buteo::KEY_PROFILE_ID),
                      perAccountProfile->name());
    account->selectService(Accounts::Service());
    account->syncAndBlock();

    return perAccountProfile;
}

// This function is called when the non-per-account profile is triggered.
// The implementation does:
// - get all profiles from the ProfileManager
// - get all accounts from the AccountManager
// - build a mapping of profile -> account for the current data type. (should be one-to-one for the datatype).
// - any account which doesn't have a profile, generate the profile for it.
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
            // we need to generate a sync profile for this account.
            TRACE(SOCIALD_INFORMATION,
                    QString(QLatin1String("generating a %1 sync profile for account: %2"))
                    .arg(profile().name()).arg(currAccount->id()));
            Buteo::SyncProfile *generated = generatePerAccountSyncProfile(currAccount,
                                                                          profile(),
                                                                          m_profileManager,
                                                                          dataTypeSyncService);
            if (generated) {
                perAccountProfiles.insert(currAccount, generated);
            } else {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("couldn't generate per-account sync profile: %1:%2"))
                        .arg(currAccount->id()).arg(profile().name()));
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
