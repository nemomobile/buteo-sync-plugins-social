/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "facebookcontactsyncadaptor.h"
#include "syncservice.h"
#include "constants_p.h"
#include "trace.h"

#include <QtCore/QPair>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <QtGui/QImage>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <QtContacts/QContactManager>
#include <QtContacts/QContactDetailFilter>
#include <QtContacts/QContactIntersectionFilter>
#include <QtContacts/QContact>
#include <QtContacts/QContactSyncTarget>
#include <QtContacts/QContactGuid>
#include <QtContacts/QContactName>
#include <QtContacts/QContactNickname>
#include <QtContacts/QContactAvatar>
#include <QtContacts/QContactUrl>
#include <QtContacts/QContactGender>
#include <QtContacts/QContactNote>
#include <QtContacts/QContactBirthday>

#include <socialcache/abstractimagedownloader.h>

// sailfish-components-accounts-qt5
#include <accountmanager.h>
#include <account.h>

#define SOCIALD_FACEBOOK_CONTACTS_ID_PREFIX QLatin1String("facebook-contacts-")
#define SOCIALD_FACEBOOK_CONTACTS_GROUPNAME QLatin1String("sociald-sync-facebook-contacts")
#define SOCIALD_FACEBOOK_CONTACTS_SYNCTARGET QLatin1String("facebook")
#define SOCIALD_FACEBOOK_CONTACTS_AVATAR_FILENAME(fbFriendId, avatarType) QString("%1/%2/%3-%4.jpg").arg(QLatin1String(PRIVILEGED_DATA_DIR)).arg(SyncService::dataType(dataType)).arg(fbFriendId).arg(avatarType)
#define SOCIALD_FACEBOOK_CONTACTS_AVATAR_BATCHSIZE 20

static const char *WHICH_FIELDS = "name,first_name,middle_name,last_name,link,website,"\
        "picture.type(large),cover,username,birthday,bio,gender,updated_time";
static const char *IDENTIFIER_KEY = "identifier";
static const char *ACCOUNT_ID_KEY = "account_id";
static const char *TYPE_KEY = "type";

static QContactManager *aggregatingContactManager(QObject *parent)
{
    QContactManager *retn = new QContactManager(
            QString::fromLatin1("org.nemomobile.contacts.sqlite"),
            QMap<QString, QString>(),
            parent);
    if (retn->managerName() != QLatin1String("org.nemomobile.contacts.sqlite")) {
        // the manager specified is not the aggregating manager we depend on.
        delete retn;
        return 0;
    }

    return retn;
}

class FacebookContactImageDownloader: public AbstractImageDownloader
{
    Q_OBJECT

public:
    enum ImageType {
        InvalidImage,
        ContactPicture,
        ContactCover
    };
    explicit FacebookContactImageDownloader();
    static QString staticOutputFile(const QVariantMap &data);
protected:
    QString outputFile(const QString &url, const QVariantMap &data) const;
};

FacebookContactImageDownloader::FacebookContactImageDownloader()
    : AbstractImageDownloader()
{
}

QString FacebookContactImageDownloader::staticOutputFile(const QVariantMap &data)
{
    // We create the file identifier by appending the type to the real identifier
    QString identifier = data.value(QLatin1String(IDENTIFIER_KEY)).toString();
    if (identifier.isEmpty()) {
        return QString();
    }

    QString typeString = data.value(QLatin1String(TYPE_KEY)).toString();
    if (typeString.isEmpty()) {
        return QString();
    }

    identifier.append(typeString);

    return makeOutputFile(SocialSyncInterface::Facebook, SocialSyncInterface::Contacts, identifier);
}

QString FacebookContactImageDownloader::outputFile(const QString &url,
                                                   const QVariantMap &data) const
{
    Q_UNUSED(url)
    return staticOutputFile(data);
}

//------------------------------------------------

FacebookContactSyncAdaptor::FacebookContactSyncAdaptor(SyncService *syncService, QObject *parent)
    : FacebookDataTypeSyncAdaptor(syncService, SyncService::Contacts, parent)
    , m_contactManager(aggregatingContactManager(this))
{
    setInitialActive(false);
    if (!m_contactManager) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: no aggregating contact manager exists - Facebook contacts sync will be inactive")));
        return;
    }

    m_workerObject = new FacebookContactImageDownloader();
    connect(m_workerObject, &AbstractImageDownloader::imageDownloaded,
            this, &FacebookContactSyncAdaptor::slotImageDownloaded);

    setInitialActive(true);
}

FacebookContactSyncAdaptor::~FacebookContactSyncAdaptor()
{
    delete m_workerObject;
    delete m_contactManager;
}

void FacebookContactSyncAdaptor::sync(const QString &dataType)
{
    // call superclass impl.
    FacebookDataTypeSyncAdaptor::sync(dataType);
}

void FacebookContactSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    foreach (int pid, purgeIds) {
        // first, purge all data from QtContacts and also our cache db.
        purgeAccount(pid);
    }
}

void FacebookContactSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    // clear our cache lists if necessary.
    m_remoteContacts[accountId].clear();

    // begin requesting data.
    requestData(accountId, accessToken);
}

void FacebookContactSyncAdaptor::requestData(int accountId, const QString &accessToken,
                                             const QString &continuationRequest,
                                             const QDateTime &syncTimestamp)
{
    QUrl url;
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));

    QDateTime timestamp = syncTimestamp.isValid() ? syncTimestamp :
                          lastSyncTimestamp(QLatin1String("facebook"),
                                            SyncService::dataType(SyncService::Contacts),
                                            accountId);

    bool isAvatarRequest = false;
    if (!continuationRequest.isEmpty()) {
        // continuation of me/friends request
        url = QUrl(continuationRequest);
        if (!continuationRequest.contains(QLatin1String("access_token"))) {
            // Facebook's pagination API is pretty terrible. Sometimes it includes this, sometimes not.
            QUrlQuery query(url);
            query.setQueryItems(queryItems);
            url.setQuery(query);
        }
    } else {
        // beginning a new sync via me/friends request.
        url = QUrl(QString(QLatin1String("https://graph.facebook.com/me/friends")));
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("limit")), QLatin1String("200")));
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("fields")),
                                                  QLatin1String(WHICH_FIELDS)));
        QUrlQuery query(url);
        query.setQueryItems(queryItems);
        url.setQuery(query);
    }

    QNetworkReply *reply = networkAccessManager->get(QNetworkRequest(url));

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("continuationRequest", continuationRequest);
        reply->setProperty("lastSyncTimestamp", timestamp);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(friendsFinishedHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request %1 from Facebook account with id %2"))
                .arg(isAvatarRequest ? QLatin1String("avatar") : QLatin1String("friends list")).arg(accountId));
    }
}

void FacebookContactSyncAdaptor::friendsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QString continuationRequest = reply->property("continuationRequest").toString();
    QDateTime lastSync = reply->property("lastSyncTimestamp").toDateTime();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok && parsed.contains(QLatin1String("data"))) {
        // we expect "data" and possibly "paging"
        QJsonArray data = parsed.value(QLatin1String("data")).toArray();
        QJsonObject paging = parsed.value(QLatin1String("paging")).toObject(); // may not exist, if no more results.

        if (!data.size()) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("no more friends received for account %1"))
                    .arg(accountId));
        } else {
            // for each friend, retrieve the detailed information.
            for (int i = 0; i < data.size(); ++i) {
                QJsonObject currFriend = data.at(i).toObject();
                QString friendId = currFriend.value(QLatin1String("id")).toString();
                QString friendName = currFriend.value(QLatin1String("name")).toString();
                if (friendId.isEmpty()) {
                    // strange error.  ignore this entry.
                    TRACE(SOCIALD_DEBUG,
                            QString(QLatin1String("strange entry in friends data list for account %1: %2, %3, keys:"))
                            .arg(accountId).arg(friendId).arg(friendName) << currFriend.keys());
                    continue;
                }

                // parse detailed information.  Note that we batch up the saves.
                bool needsSaving = false;
                QContact parsedContact = parseContactDetails(currFriend, accountId, &needsSaving);
                if (needsSaving) {
                    m_remoteContacts[accountId].append(parsedContact);
                }
            }
        }

        // paging if we need to retrieve more friends
        if (paging.contains("next")) {
            QString nextUrl = paging.value("next").toString();
            if (!nextUrl.isEmpty() && nextUrl != continuationRequest) {
                requestData(accountId, accessToken, nextUrl, lastSync);
            }
        } else {
            // we're finished - we should attempt to update our local database.
            int addedCount = 0, modifiedCount = 0, removedCount = 0, unchangedCount = 0;
            bool success = storeToLocal(accessToken, accountId, &addedCount, &modifiedCount, &removedCount, &unchangedCount);
            TRACE(SOCIALD_INFORMATION,
                  QString(QLatin1String("Facebook contact sync with account %1 finished with result: %2: a: %3 m: %4 r: %5 u: %6. Continuing to load avatars..."))
                  .arg(accountId).arg(success ? "SUCCESS" : "ERROR").arg(addedCount).arg(modifiedCount).arg(removedCount).arg(unchangedCount));
        }
    } else {
        QString message = isError ?
                          QLatin1String("error: error occurred during friends request with account %1; got: %2") :
                          QLatin1String("error: unable to parse friends data from request with account %1; got: %2");

        TRACE(SOCIALD_ERROR,
              message.arg(accountId).arg(QString::fromLatin1(replyData.constData())));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

#define SAVE_DETAIL(detail)                 \
    do {                                    \
        *needsSaving = true;                \
        newOrExisting.saveDetail(&detail);  \
    } while (0)

#define REMOVE_DETAIL(detail)               \
    do {                                    \
        *needsSaving = true;                \
        newOrExisting.removeDetail(&detail);\
    } while (0)

QContact FacebookContactSyncAdaptor::parseContactDetails(const QJsonObject &blobDetails, int accountId, bool *needsSaving)
{
    if (blobDetails.contains(QLatin1String("id"))) {
        // we expect friend user data.
        QString fbuid = blobDetails.value(QLatin1String("id")).toString();
        QString name = blobDetails.value(QLatin1String("name")).toString();
        QString firstName = blobDetails.value(QLatin1String("first_name")).toString();
        QString middleName = blobDetails.value(QLatin1String("middle_name")).toString();
        QString lastName = blobDetails.value(QLatin1String("last_name")).toString();
        QString link = blobDetails.value(QLatin1String("link")).toString(); // link to user's profile on facebook
        QString website = blobDetails.value(QLatin1String("website")).toString(); // personal website.
        QString picture;
        QJsonObject pictureData = blobDetails.value(QLatin1String("picture")).toObject().value(QLatin1String("data")).toObject();
        if (!pictureData.value(QLatin1String("is_silhouette")).toBool()) {
            picture = pictureData.value(QLatin1String("url")).toString();
        }
        QString cover = blobDetails.value(QLatin1String("cover")).toObject().value(QLatin1String("source")).toString();
        QString username = blobDetails.value(QLatin1String("username")).toString();
        QString birthdayStr = blobDetails.value(QLatin1String("birthday")).toString();
        QDateTime birthday = QDateTime::fromString(birthdayStr, Qt::ISODate);
        if (!birthday.isValid()) {
            // manually parse the birthday.  It's usually in MM/DD/YYYY format,
            // but sometimes MM/DD format - which we ignore.
            birthday = QDateTime::fromString(birthdayStr, "MM/dd/yyyy");
        }
        QString bio = blobDetails.value(QLatin1String("bio")).toString();
        QString gender = blobDetails.value(QLatin1String("gender")).toString();

        // now build the appropriate QtContacts details etc.
        *needsSaving = false;
        bool isNewContact = false;
        QContact newOrExisting = newOrExistingContact(fbuid, &isNewContact);

        // sync target is unique
        QContactSyncTarget contactSyncTarget = newOrExisting.detail<QContactSyncTarget>();
        if (contactSyncTarget.syncTarget().isEmpty()) {
            // must be a "new" contact - set the sync target.
            *needsSaving = true;
            contactSyncTarget.setSyncTarget(SOCIALD_FACEBOOK_CONTACTS_SYNCTARGET);
            if (!newOrExisting.saveDetail(&contactSyncTarget)) {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: unable to save updated sync target for friend %1 of account %2"))
                        .arg(name).arg(accountId));
                *needsSaving = false;
                return QContact();
            }
        }

        // guid is unique
        QContactGuid contactGuid = newOrExisting.detail<QContactGuid>();
        if (contactGuid.guid() != fbuid) {
            *needsSaving = true;
            contactGuid.setGuid(fbuid);
            if (!newOrExisting.saveDetail(&contactGuid)) {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: unable to save updated guid for friend %1 of account %2"))
                        .arg(name).arg(accountId));
                *needsSaving = false;
                return QContact();
            }
        }

        // name is unique
        QContactName contactName = newOrExisting.detail<QContactName>();
        if (!firstName.isEmpty() || !middleName.isEmpty() || !lastName.isEmpty()) {
            if (contactName.firstName() != firstName || contactName.middleName() != middleName || contactName.lastName() != lastName) {
                *needsSaving = true;
                contactName.setFirstName(firstName);
                contactName.setMiddleName(middleName);
                contactName.setLastName(lastName);
                if (!newOrExisting.saveDetail(&contactName)) {
                    TRACE(SOCIALD_ERROR,
                            QString(QLatin1String("error: unable to save updated name for friend %1 of account %2"))
                            .arg(name).arg(accountId));
                    *needsSaving = false;
                    return QContact();
                }
            }
        } else if (!name.isEmpty()) {
            // the name should consist of just first/middle/last parts, so ignore it.
        } else {
            // should never happen, anyway, but remove the name if the updated details lack it.
            REMOVE_DETAIL(contactName);
        }

        // errors while saving / removing further details are considered "mostly unimportant"

        // but url is not unique (can have link + can have website)
        QList<QContactUrl> urls = newOrExisting.details<QContactUrl>();
        QUrl websiteUrl(website);
        QUrl linkUrl(link);
        bool haveSavedLink = false;
        bool haveSavedWebsite = false;
        foreach (const QContactUrl &curl, urls) {
            if (curl.subType() == QContactUrl::SubTypeBlog) {
                // this is the "link" detail.  determine whether it needs an update.
                if (link.isEmpty()) {
                    // needs to be removed.
                    QContactUrl contactUrl = curl;
                    REMOVE_DETAIL(contactUrl);
                } else {
                    if (curl.url() != linkUrl.toString()) {
                        // needs to be updated.
                        QContactUrl contactUrl = curl;
                        contactUrl.setUrl(QUrl(link));
                        SAVE_DETAIL(contactUrl);
                    }
                    haveSavedLink = true;
                }
            } else if (curl.subType() == QContactUrl::SubTypeHomePage) {
                // this is the "website" detail.  determine whether it needs an update.
                if (website.isEmpty()) {
                    // needs to be removed.
                    QContactUrl contactUrl = curl;
                    REMOVE_DETAIL(contactUrl);
                } else {
                    if (curl.url() != websiteUrl.toString()) {
                        // needs to be updated.
                        QContactUrl contactUrl = curl;
                        contactUrl.setUrl(QUrl(website));
                        SAVE_DETAIL(contactUrl);
                    }
                    haveSavedWebsite = true;
                }
            }
        }

        if (!haveSavedLink && !link.isEmpty()) {
            // create a new url detail.
            QContactUrl contactUrl;
            contactUrl.setSubType(QContactUrl::SubTypeBlog);
            contactUrl.setUrl(QUrl(link));
            SAVE_DETAIL(contactUrl);
        }

        if (!haveSavedWebsite && !website.isEmpty()) {
            // create a new url detail.
            QContactUrl contactUrl;
            contactUrl.setSubType(QContactUrl::SubTypeHomePage);
            contactUrl.setUrl(QUrl(website));
            SAVE_DETAIL(contactUrl);
        }

        // avatar is not unique, can have both picture + cover
        QList<QContactAvatar> contactAvatars = newOrExisting.details<QContactAvatar>();
        bool foundCover = false;
        bool foundPicture = false;
        foreach (const QContactAvatar &avatar, contactAvatars) {
            if (avatar.value(QContactAvatar__FieldAvatarMetadata) == QLatin1String("cover")) {
                foundCover = true;
                if (cover.isEmpty()) {
                    // needs to be removed.
                    QContactAvatar contactAvatar = avatar;
                    REMOVE_DETAIL(contactAvatar);
                } else {
                    QVariantMap data;
                    data.insert(IDENTIFIER_KEY, fbuid);
                    data.insert(TYPE_KEY, FacebookContactImageDownloader::ContactCover);
                    data.insert(ACCOUNT_ID_KEY, accountId);
                    QString avatarFileName = FacebookContactImageDownloader::staticOutputFile(data);
                    if (!QFile::exists(avatarFileName)) { // XXX TODO: staticOutputFile() should take URL_KEY as parameter, else server-side change will not be detected!
                        m_queuedAvatarDownloads[accountId].append(qMakePair<QString, QVariantMap>(cover, data));
                    }

                    QContactAvatar contactAvatar = avatar;
                    contactAvatar.setImageUrl(avatarFileName);
                    SAVE_DETAIL(contactAvatar);
                }
            } else if (avatar.value(QContactAvatar__FieldAvatarMetadata) == QLatin1String("picture")) {
                foundPicture = true;
                if (picture.isEmpty()) {
                    // needs to be removed.
                    QContactAvatar contactAvatar = avatar;
                    REMOVE_DETAIL(contactAvatar);
                } else {
                    QVariantMap data;
                    data.insert(IDENTIFIER_KEY, fbuid);
                    data.insert(TYPE_KEY, FacebookContactImageDownloader::ContactPicture);
                    data.insert(ACCOUNT_ID_KEY, accountId);
                    QString avatarFileName = FacebookContactImageDownloader::staticOutputFile(data);
                    if (!QFile::exists(avatarFileName)) { // XXX TODO: staticOutputFile() should take URL_KEY as parameter, else server-side change will not be detected!
                        m_queuedAvatarDownloads[accountId].append(qMakePair<QString, QVariantMap>(picture, data));
                    }

                    QContactAvatar contactAvatar = avatar;
                    contactAvatar.setImageUrl(avatarFileName);
                    SAVE_DETAIL(contactAvatar);
                }
            }
        }
        if (!foundCover && !cover.isEmpty()) {
            QVariantMap data;
            data.insert(IDENTIFIER_KEY, fbuid);
            data.insert(TYPE_KEY, FacebookContactImageDownloader::ContactCover);
            data.insert(ACCOUNT_ID_KEY, accountId);
            QString avatarFileName = FacebookContactImageDownloader::staticOutputFile(data);
            if (!QFile::exists(avatarFileName)) { // XXX TODO: staticOutputFile() should take URL_KEY as parameter, else server-side change will not be detected!
                m_queuedAvatarDownloads[accountId].append(qMakePair<QString, QVariantMap>(cover, data));
            }

            // needs to be updated.  we set the value to be the (future) image filename.
            QContactAvatar contactAvatar;
            contactAvatar.setImageUrl(avatarFileName);
            contactAvatar.setValue(QContactAvatar__FieldAvatarMetadata, QLatin1String("cover"));
            SAVE_DETAIL(contactAvatar);
        }
        if (!foundPicture && !picture.isEmpty()) {
            QVariantMap data;
            data.insert(IDENTIFIER_KEY, fbuid);
            data.insert(TYPE_KEY, FacebookContactImageDownloader::ContactPicture);
            data.insert(ACCOUNT_ID_KEY, accountId);
            QString avatarFileName = FacebookContactImageDownloader::staticOutputFile(data);
            if (!QFile::exists(avatarFileName)) { // XXX TODO: staticOutputFile() should take URL_KEY as parameter, else server-side change will not be detected!
                m_queuedAvatarDownloads[accountId].append(qMakePair<QString, QVariantMap>(picture, data));
            }

            // needs to be updated.  we set the value to be the (future) image filename.
            QContactAvatar contactAvatar;
            contactAvatar.setImageUrl(avatarFileName);
            contactAvatar.setValue(QContactAvatar__FieldAvatarMetadata, QLatin1String("picture"));
            SAVE_DETAIL(contactAvatar);
        }

        // nickname (username) is unique
        QContactNickname contactNickname = newOrExisting.detail<QContactNickname>();
        if (username.isEmpty()) {
            // must be removed
            REMOVE_DETAIL(contactNickname);
        } else if (contactNickname.nickname() != username) {
            // must be updated
            contactNickname.setNickname(username);
            SAVE_DETAIL(contactNickname);
        }

        // birthday is unique.
        QContactBirthday contactBirthday = newOrExisting.detail<QContactBirthday>();
        if (birthday.isValid()) {
            // the remote friend has a birthday.
            if (contactBirthday.dateTime() != birthday) {
                // the local contact's birthday must be updated
                contactBirthday.setDateTime(birthday);
                SAVE_DETAIL(contactBirthday);
            }
        } else {
            // the remote friend does not have a birthday.
            if (contactBirthday.key() != 0) {
                // the local contact does have a birthday.
                // it must be removed
                REMOVE_DETAIL(contactBirthday);
            }
        }

        // bio (note) is unique
        QContactNote contactNote = newOrExisting.detail<QContactNote>();
        if (bio.isEmpty()) {
            // must be removed
            REMOVE_DETAIL(contactNote);
        } else if (contactNote.note() != bio) {
            // must be updated
            contactNote.setNote(bio);
            SAVE_DETAIL(contactNote);
        }

        // gender is unique
        QContactGender contactGender = newOrExisting.detail<QContactGender>();
        if (gender.isEmpty()) {
            // must be removed
            REMOVE_DETAIL(contactGender);
        } else if (gender.startsWith('m', Qt::CaseInsensitive) && contactGender.gender() != QContactGender::GenderMale) {
            // must be updated
            contactGender.setGender(QContactGender::GenderMale);
            SAVE_DETAIL(contactGender);
        } else if (gender.startsWith('f', Qt::CaseInsensitive) && contactGender.gender() != QContactGender::GenderFemale) {
            // must be updated
            contactGender.setGender(QContactGender::GenderFemale);
            SAVE_DETAIL(contactGender);
        } else if (!gender.startsWith('f', Qt::CaseInsensitive) && !gender.startsWith('m', Qt::CaseInsensitive) && contactGender.gender() != QContactGender::GenderUnspecified) {
            // must be updated
            contactGender.setGender(QContactGender::GenderUnspecified);
            SAVE_DETAIL(contactGender);
        }

        // Now that we've built up the contact, flag it for saving to the database if required.
        return newOrExisting; // will only be saved if *needsSaving == true.
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse friend details, got: %1"))
                .arg(QStringList(blobDetails.keys()).join(QChar(','))));
    }

    *needsSaving = false;
    return QContact();
}

QList<QContactId> FacebookContactSyncAdaptor::contactIdsForGuid(const QString &fbuid)
{
    QContactDetailFilter guidFilter;
    guidFilter.setDetailType(QContactDetail::TypeGuid, QContactGuid::FieldGuid);
    guidFilter.setValue(fbuid);
    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(SOCIALD_FACEBOOK_CONTACTS_SYNCTARGET);
    QContactIntersectionFilter fil;
    fil << guidFilter << syncTargetFilter;
    QList<QContactId> cids = m_contactManager->contactIds(fil);
    return cids;
}

QContact FacebookContactSyncAdaptor::newOrExistingContact(const QString &fbuid, bool *isNewContact)
{
    // returns a QContact from the database which represents the
    // friend with the given fbuid, or a new, empty contact if no
    // such contact exists in the database.

    QList<QContactId> cids = contactIdsForGuid(fbuid);
    if (cids.size() < 1) {
        // new contact, not represented in the db.
        QContact retn;
        *isNewContact = true;
        return retn;
    } else if (cids.size() > 1) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: friend %1 represented multiple times in QtContacts db"))
                .arg(fbuid));
        // return the first one anyway.  Flow down.
    }

    *isNewContact = false;
    return m_contactManager->contact(cids.at(0));
}

void FacebookContactSyncAdaptor::slotImageDownloaded(const QString &url, const QString &path,
                                                     const QVariantMap &data)
{
    Q_UNUSED(url)
    Q_UNUSED(path)

    // Load finished, we just decrement semaphore
    decrementSemaphore(data.value(ACCOUNT_ID_KEY).toInt());
}

bool FacebookContactSyncAdaptor::remoteContactDiffersFromLocal(const QContact &remoteContact, const QContact &localContact) const
{
    // check to see if there are any differences between the remote and the local.
    QList<QContactDetail> remoteDetails = remoteContact.details();
    QList<QContactDetail> localDetails = localContact.details();

    // remove any problematic details from the lists (timestamps, etc)
    for (int i = remoteDetails.size() - 1; i >= 0; --i) {
        if (remoteDetails.at(i).type() == QContactDetail::TypeTimestamp
                || remoteDetails.at(i).type() == QContactDetail::TypeDisplayLabel
                || remoteDetails.at(i).type() == QContactDetail::TypePresence
                || remoteDetails.at(i).type() == QContactDetail::TypeGlobalPresence
                || remoteDetails.at(i).type() == QContactOriginMetadata::Type) {
            remoteDetails.removeAt(i);
        }
    }
    for (int i = localDetails.size() - 1; i >= 0; --i) {
        if (localDetails.at(i).type() == QContactDetail::TypeTimestamp
                || localDetails.at(i).type() == QContactDetail::TypeDisplayLabel
                || localDetails.at(i).type() == QContactDetail::TypePresence
                || localDetails.at(i).type() == QContactDetail::TypeGlobalPresence
                || localDetails.at(i).type() == QContactOriginMetadata::Type) {
            localDetails.removeAt(i);
        }
    }

    // compare the two lists to determine if any differences exist.
    // Note: we don't just check if the count is different, because
    // sometimes we can have discardable duplicates which are detected in the backend.
    foreach (const QContactDetail &rdet, remoteDetails) {
        // find all local details of the same type
        QList<QContactDetail> localOfSameType;
        foreach (const QContactDetail &ldet, localDetails) {
            if (ldet.type() == rdet.type()) {
                localOfSameType.append(ldet);
            }
        }

        // if none exist, then the remote differs
        if (localOfSameType.isEmpty()) {
            return true;
        }

        // if none of the local are the same, the remote differs
        // note that we only ensure that the remote values have matching local values,
        // and not vice versa, as the local backend can add extra data (eg,
        // synthesised minimal/normalised phone number forms).
        bool found = false;
        foreach (const QContactDetail &ldet, localOfSameType) {
            // we only check the "default" field values, and not LinkedDetailUris.
            QMap<int, QVariant> lvalues = ldet.values();
            QMap<int, QVariant> rvalues = rdet.values();
            bool noFieldValueDifferences = true;
            foreach (int valueKey, rvalues.keys()) {
                if (valueKey <= QContactDetail::FieldContext) {
                    if (rvalues.value(valueKey) != lvalues.value(valueKey)) {
                        noFieldValueDifferences = false;
                        break;
                    }
                }
            }
            if (noFieldValueDifferences) {
                found = true; // this detail matches.
                break;
            }
        }
        if (!found) {
            return true;
        }
    }

    // there were no differences between this remote and the local counterpart.
    return false;
}

bool FacebookContactSyncAdaptor::storeToLocal(const QString &accessToken, int accountId, int *addedCount, int *modifiedCount, int *removedCount, int *unchangedCount)
{
    Q_UNUSED(accessToken)

    // steps:
    // 1) load current data from backend
    // 2) determine delta (add/mod/rem)
    // 3) apply delta

    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(SOCIALD_FACEBOOK_CONTACTS_SYNCTARGET);
    QContactFetchHint noRelationships;
    noRelationships.setOptimizationHints(QContactFetchHint::NoRelationships);

    QList<QContact> remoteContacts = m_remoteContacts[accountId];
    QList<QContact> localContacts = m_contactManager->contacts(syncTargetFilter, QList<QContactSortOrder>(), noRelationships);
    QList<QContact> remoteToSave;
    QList<QContactId> localToRemove;
    QList<QContactId> foundLocal;
    QString accountIdStr = QString::number(accountId);

    // we always use the remote server's data in conflicts
    for (int i = 0; i < remoteContacts.size(); ++i) {
        QContact rc = remoteContacts[i];
        QString guid = rc.detail<QContactGuid>().guid();
        if (guid.isEmpty()) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("skipping: cannot store remote Facebook contact with no guid:")) << rc);
            continue;
        }

        bool foundLocalToModify = false;
        bool localUnchanged = false;
        for (int j = 0; j < localContacts.size(); ++j) {
            const QContact &lc = localContacts[j];
            if (lc.detail<QContactGuid>().guid() == guid) {
                // We need to see if more than one account provides this contact.
                QContactOriginMetadata metaData = lc.detail<QContactOriginMetadata>();
                QStringList accountIds = metaData.groupId().split(',');

                QContactOriginMetadata modMetaData = rc.detail<QContactOriginMetadata>();
                modMetaData.setId(metaData.id());
                modMetaData.setGroupId(metaData.groupId());
                modMetaData.setEnabled(metaData.enabled());

                if (accountIds.contains(accountIdStr)) {
                    rc.saveDetail(&modMetaData);
                } else {
                    accountIds.append(accountIdStr);
                    modMetaData.setGroupId(accountIds.join(QString::fromLatin1(",")));
                    rc.saveDetail(&modMetaData);
                }

                // determine whether we need to update the locally stored contact at all
                if (remoteContactDiffersFromLocal(rc, lc)) {
                    // we clobber local data with remote data.
                    foundLocalToModify = true;
                    rc.setId(lc.id());
                    foundLocal.append(lc.id());
                } else {
                    // we shouldn't need to save this contact, it already exists locally.
                    localUnchanged = true;
                    rc.setId(lc.id());
                    foundLocal.append(lc.id());
                }

                break;
            }
        }

        if (localUnchanged) {
            *unchangedCount += 1;
        } else if (foundLocalToModify) {
            *modifiedCount += 1;
            remoteToSave.append(rc);
        } else {
            // adding a new contact
            *addedCount += 1;
            // need new metadata.
            QContactOriginMetadata metadata = rc.detail<QContactOriginMetadata>();
            metadata.setGroupId(accountIdStr);
            rc.saveDetail(&metadata);
            remoteToSave.append(rc);
        }
    }

    // any local contacts which exist without a remote counterpart
    // are "stale" and should be removed.  Alternatively, if the
    // contact is provided by a different account as well, we need
    // to remove this account from the metadata.
    for (int i = 0; i < localContacts.size(); ++i) {
        QContact lc = localContacts.at(i);
        if (!foundLocal.contains(lc.id())) {
            QContactOriginMetadata metadata = lc.detail<QContactOriginMetadata>();
            QStringList accountIds = metadata.groupId().split(',');
            if (accountIds.contains(accountIdStr)) {
                // this account used to provide this contact, but now does not.
                accountIds.removeAll(accountIdStr);
                if (accountIds.isEmpty()) {
                    // no other account provides this contact, it can be removed.
                    localToRemove.append(lc.id());
                    *removedCount += 1;
                } else {
                    // at least one other account provides this contact also.
                    metadata.setGroupId(accountIds.join(QString::fromLatin1(",")));
                    lc.saveDetail(&metadata);
                    remoteToSave.append(lc); // actually updating a local.
                    *removedCount += 1;      // but we consider it a removal from the account's pov.
                }
            } else {
                // it was always provided by some other account only.  Don't modify this one.
            }
        }
    }

    // now write the changes to the database.
    bool success = true;
    if (remoteToSave.size()) {
        success = m_contactManager->saveContacts(&remoteToSave);
        if (!success) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("Failed to save contacts: %1 - with account %2"))
                  .arg(m_contactManager->error()).arg(accountId));
        }
    }
    if (localToRemove.size()) {
        success = m_contactManager->removeContacts(localToRemove);
        if (!success) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("Failed to remove stale contacts: %1 - with account %2"))
                  .arg(m_contactManager->error()).arg(accountId));
        }
    }

    // and trigger downloading of avatars for friend contacts for this account.
    const QList<QPair<QString, QVariantMap> > &queuedDownloads = m_queuedAvatarDownloads[accountId];
    for (int i = 0; i < queuedDownloads.size(); ++i) {
        incrementSemaphore(accountId);
        const QPair<QString, QVariantMap> &queuedAvatarDownload = queuedDownloads[i];
        m_workerObject->queue(queuedAvatarDownload.first, queuedAvatarDownload.second);
    }

    // done.
    m_queuedAvatarDownloads[accountId].clear();
    m_remoteContacts[accountId].clear();
    return success;
}

void FacebookContactSyncAdaptor::finalize(int accountId)
{
    TRACE(SOCIALD_DEBUG, QString(QLatin1String("Finished Facebook contacts sync for account %1")).arg(accountId));
}

void FacebookContactSyncAdaptor::purgeAccount(int pid)
{
    int purgeCount = 0;
    int modifiedCount = 0;

    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(SOCIALD_FACEBOOK_CONTACTS_SYNCTARGET);
    QContactFetchHint noRelationships;
    noRelationships.setOptimizationHints(QContactFetchHint::NoRelationships);

    QString accountIdStr = QString::number(pid);
    QList<QContact> localContacts = m_contactManager->contacts(syncTargetFilter, QList<QContactSortOrder>(), noRelationships);
    QList<QContact> contactsToUpdate;
    QList<QContactId> contactsToRemove;
    for (int i = 0; i < localContacts.size(); ++i) {
        QContact c = localContacts.at(i);
        QContactOriginMetadata metadata = c.detail<QContactOriginMetadata>();
        QStringList accountIds = metadata.groupId().split(',');
        if (accountIds.contains(accountIdStr)) {
            // this account used to provide this contact, and we're purging this account.
            accountIds.removeAll(accountIdStr);
            if (accountIds.isEmpty()) {
                // no other account provides this contact, it can be removed.
                contactsToRemove.append(c.id());
                purgeCount += 1;
            } else {
                // at least one other account provides this contact also.
                metadata.setGroupId(accountIds.join(QString::fromLatin1(",")));
                c.saveDetail(&metadata);
                contactsToUpdate.append(c);
                modifiedCount += 1;
            }
        } else {
            // it was always provided by some other account only.  Don't modify this one.
        }
    }

    // now write the changes to the database.
    bool success = true;
    if (contactsToUpdate.size()) {
        success = m_contactManager->saveContacts(&contactsToUpdate);
        if (!success) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("Failed to update contacts: %1 - during purge of account %2"))
                  .arg(m_contactManager->error()).arg(pid));
        }
    }
    if (contactsToRemove.size()) {
        success = m_contactManager->removeContacts(contactsToRemove);
        if (!success) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("Failed to remove stale contacts: %1 - during purge of account %2"))
                  .arg(m_contactManager->error()).arg(pid));
        }
    }

    if (success) {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("purged account %1 and successfully removed %2 friends (kept %3 modified friends)"))
                .arg(pid).arg(purgeCount).arg(modifiedCount));
    }
}

void FacebookContactSyncAdaptor::finalCleanup()
{
    // Synchronously find any contacts which need to be removed,
    // which were somehow "left behind" by the sync process.

    // first, get a list of all existing, enabled Facebook account ids
    QList<int> facebookAccountIds;
    QList<int> purgeAccountIds;
    QList<int> currentAccountIds = accountManager->accountIdentifiers();
    foreach (int currId, currentAccountIds) {
        Account *act = accountManager->account(currId);
        if (act) {
            if (act->providerName() == QString(QLatin1String("facebook")) && act->enabled()
                    && act->isEnabledWithService(QString(QLatin1String("facebook-sync")))) {
                facebookAccountIds.append(currId);
            }
            act->deleteLater();
        }
    }

    // second, get all contacts which have been synced from Facebook.
    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(SOCIALD_FACEBOOK_CONTACTS_SYNCTARGET);
    QContactFetchHint noRelationships;
    noRelationships.setOptimizationHints(QContactFetchHint::NoRelationships);
    noRelationships.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactOriginMetadata::Type);
    QList<QContact> facebookContacts = m_contactManager->contacts(syncTargetFilter, QList<QContactSortOrder>(), noRelationships);

    // third, find all account ids from which contacts have been synced
    foreach (const QContact &contact, facebookContacts) {
        QContactOriginMetadata metadata = contact.detail<QContactOriginMetadata>();
        QStringList accountIds = metadata.groupId().split(',');
        foreach (const QString &accountIdStr, accountIds) {
            int purgeId = accountIdStr.toInt();
            if (purgeId && !facebookAccountIds.contains(purgeId)
                    && !purgeAccountIds.contains(purgeId)) {
                // this account no longer exists, and needs to be purged.
                purgeAccountIds.append(purgeId);
            }
        }
    }

    // fourth, purge all data for those account ids which no longer exist.
    if (purgeAccountIds.size()) {
        TRACE(SOCIALD_INFORMATION,
            QString(QLatin1String("finalCleanup() purging contacts from %1 non-existent Facebook accounts"))
            .arg(purgeAccountIds.size()));
        foreach (int purgeId, purgeAccountIds) {
            purgeAccount(purgeId);
        }
    }
}

#include "facebookcontactsyncadaptor.moc"
