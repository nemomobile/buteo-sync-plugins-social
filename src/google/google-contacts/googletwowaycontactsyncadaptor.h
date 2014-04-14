/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef GOOGLETWOWAYCONTACTSYNCADAPTOR_H
#define GOOGLETWOWAYCONTACTSYNCADAPTOR_H

#include "googledatatypesyncadaptor.h"
#include "googlecontactstream.h"

#include <twowaycontactsyncadapter.h>

#include <QContactManager>
#include <QContact>
#include <QDateTime>
#include <QList>
#include <QPair>

QTCONTACTS_USE_NAMESPACE

class GoogleContactImageDownloader;
class GoogleTwoWayContactSyncAdaptor : public GoogleDataTypeSyncAdaptor, public QtContactsSqliteExtensions::TwoWayContactSyncAdapter
{
    Q_OBJECT

public:
    GoogleTwoWayContactSyncAdaptor(QObject *parent);
   ~GoogleTwoWayContactSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId = 0);

protected:
    // implementing the TWCSA interface
    void determineRemoteChanges(const QDateTime &remoteSince,
                                const QString &accountId);
    void upsyncLocalChanges(const QDateTime &localSince,
                            const QList<QContact> &locallyAdded,
                            const QList<QContact> &locallyModified,
                            const QList<QContact> &locallyDeleted,
                            const QString &accountId);

protected:
    // implementing GoogleDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);
    void finalCleanup();
    // implementing TWCSA interface
    bool testAccountProvenance(const QContact &contact, const QString &accountId);

private:
    void requestData(int accountId,
                     const QString &accessToken,
                     int startIndex = 0,
                     const QString &continuationRequest = QString(),
                     const QDateTime &syncTimestamp = QDateTime(),
                     bool isGroupRequest = false);
    void purgeAccount(int pid);

private Q_SLOTS:
    void accountInitialized();
    void groupsFinishedHandler();
    void contactsFinishedHandler();
    void imageDownloaded(const QString &url, const QString &path, const QVariantMap &metadata);

private:
    void continueSync(int accountId, const QString &accessToken);
    void upsyncLocalChangesList(int accountId);
    void storeToRemote(int accountId,
                       const QString &accessToken,
                       const QByteArray &encodedContactUpdates);
    void queueOutstandingAvatars(int accountId, const QString &accessToken);
    bool queueAvatarForDownload(int accountId, const QString &accessToken, const QString &contactGuid, const QString &imageUrl);
    void transformContactAvatars(QList<QContact> &remoteContacts, int accountId, const QString &accessToken);
    void downloadContactAvatarImage(int accountId, const QString &accessToken, const QUrl &imageUrl, const QString &filename);
    bool readExtraStateData(int accountId);
    bool storeExtraStateData(int accountId);

private Q_SLOTS:
    void postFinishedHandler();
    void postErrorHandler();

private:
    QContactManager m_contactManager;
    GoogleContactImageDownloader *m_workerObject;

    QMap<int, QString> m_accessTokens;
    QMap<int, QString> m_emailAddresses;
    QMap<int, QString> m_myContactsGroupAtomIds;
    QMap<int, QList<QContact> > m_remoteDels;
    QMap<int, QList<QContact> > m_remoteAddMods;
    QMap<int, QMap<QString, QStringList> > m_unsupportedXmlElements; // contact guid -> elements
    QMap<int, QMap<QString, QString> > m_contactEtags; // contact guid -> contact etag
    QMap<int, QMap<QString, QString> > m_contactIds; // contact guid -> contact id
    QMap<int, QMap<QString, QString> > m_contactAvatars; // contact guid -> remote avatar path
    QMap<int, QList<QPair<QContact, GoogleContactStream::UpdateType> > > m_localChanges;

    // the following are not preserved across sync runs via OOB.
    QMap<int, int> m_apiRequestsRemaining;
    QMap<int, QMap<QString, QString> > m_queuedAvatarsForDownload; // contact guid -> remote avatar path
    QMap<int, QMap<QString, QString> > m_downloadedContactAvatars; // contact guid -> local file path
};

#endif // GOOGLETWOWAYCONTACTSYNCADAPTOR_H
