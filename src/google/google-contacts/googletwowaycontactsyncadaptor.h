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

protected: // implementing GoogleDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalCleanup();

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
    QMap<int, QList<QPair<QContact, GoogleContactStream::UpdateType> > > m_localChanges;
};

#endif // GOOGLETWOWAYCONTACTSYNCADAPTOR_H
