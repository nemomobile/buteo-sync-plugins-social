/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "facebookcontactsyncadaptor.h"
#include "syncservice.h"
#include "trace.h"
#include "constants_p.h"

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

#define SOCIALD_FACEBOOK_CONTACTS_ID_PREFIX QLatin1String("facebook-contacts-")
#define SOCIALD_FACEBOOK_CONTACTS_GROUPNAME QLatin1String("sociald-sync-facebook-contacts")
#define SOCIALD_FACEBOOK_CONTACTS_SYNCTARGET QLatin1String("facebook")
#define SOCIALD_FACEBOOK_CONTACTS_AVATAR_FILENAME(fbFriendId, avatarType) QString("%1/%2/%3-%4.jpg").arg(QLatin1String(PRIVILEGED_DATA_DIR)).arg(SyncService::dataType(dataType)).arg(fbFriendId).arg(avatarType)
#define SOCIALD_FACEBOOK_CONTACTS_AVATAR_BATCHSIZE 20

static const char *WHICH_FIELDS = "name,first_name,middle_name,last_name,link,website,"\
        "picture.type(large),cover,location,username,birthday,bio,gender,significant_other"\
        ",updated_time";
static const char *IDENTIFIER_KEY = "identifier";
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
    // This is a reimplemented method, used by AbstractImageDownloader
    QString outputFile(const QString &url, const QVariantMap &data) const;
    bool dbInit();
    void dbQueueImage(const QString &url, const QVariantMap &data, const QString &file);
    void dbWrite();
    bool dbClose();
private:
    bool m_initialized;
    FacebookContactsDatabase m_db;
};

FacebookContactImageDownloader::FacebookContactImageDownloader()
    : AbstractImageDownloader(), m_initialized(false)
{
}

QString FacebookContactImageDownloader::staticOutputFile(const QVariantMap &data)
{
    // We create the identifier by appending the type to the real identifier
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

bool FacebookContactImageDownloader::dbInit()
{
    if (!m_initialized) {
        m_db.initDatabase();
        m_initialized = true;
    }

    return m_db.isValid();
}

void FacebookContactImageDownloader::dbQueueImage(const QString &url, const QVariantMap &data,
                                                  const QString &file)
{
    Q_UNUSED(url)
    QString identifier = data.value(QLatin1String(IDENTIFIER_KEY)).toString();
    if (identifier.isEmpty()) {
        return;
    }
    int type = data.value(QLatin1String(TYPE_KEY)).toInt();

    switch (type) {
    case ContactPicture:
        m_db.updatePictureFile(identifier, file);
        break;
    case ContactCover:
        m_db.updateCoverFile(identifier, file);
        break;
    }
}

void  FacebookContactImageDownloader::dbWrite()
{
    m_db.write();
}

bool FacebookContactImageDownloader::dbClose()
{
    return m_db.closeDatabase();
}

FacebookContactSyncAdaptor::FacebookContactSyncAdaptor(SyncService *syncService, QObject *parent)
    : FacebookDataTypeSyncAdaptor(syncService, SyncService::Contacts, parent)
    , m_contactManager(aggregatingContactManager(this))
    , m_populatingAvatarsAccountId(-1)
{
    setInitialActive(false);
    if (!m_contactManager) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: no aggregating contact manager exists - Facebook contacts sync will be inactive")));
        return;
    }

    m_workerObject = new FacebookContactImageDownloader();

    // Establish some connections
    connect(this, &FacebookContactSyncAdaptor::requestQueue,
            m_workerObject, &AbstractImageDownloader::queue);
    connect(m_workerObject, &AbstractImageDownloader::imageDownloaded,
            this, &FacebookContactSyncAdaptor::slotImageDownloaded);

    m_db.initDatabase();
    setInitialActive(m_db.isValid());
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
    initRemovalDetection(accountId);
    requestData(accountId, accessToken);
}

void FacebookContactSyncAdaptor::finalize(int accountId)
{
    if (m_populatingAvatarsAccountId == accountId) {
        TRACE(SOCIALD_DEBUG, QString(QLatin1String("Finished contacts sync")));
        return;
    }

    TRACE(SOCIALD_DEBUG, QString(QLatin1String("Finalized call: fetching avatars")));
    purgeDetectedRemovals();

    // We are finalizing: write into contacts db and avatar db
    // If we fail, we quit.
    if (!m_contactManager->saveContacts(&m_newContactsToSave)) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to save %1 friends for account %2"))
                .arg(m_newContactsToSave.size()).arg(accountId));
        return;
    }
    m_db.write();

    // We retrieve avatars
    QList<FacebookContact::ConstPtr> contacts = m_db.contacts(accountId);
    foreach (const FacebookContact::ConstPtr &contact, contacts) {
        QString identifier = contact->fbFriendId();
        if (contact->pictureFile().isEmpty() && !contact->pictureUrl().isEmpty()) {
            QVariantMap data;
            data.insert(IDENTIFIER_KEY, identifier);
            data.insert(TYPE_KEY, FacebookContactImageDownloader::ContactPicture);
            emit requestQueue(contact->pictureUrl(), data);
            m_populatingAvatarsAccountId = accountId;
            incrementSemaphore(accountId);
        }

        if (contact->coverFile().isEmpty() && !contact->coverUrl().isEmpty()) {
            QVariantMap data;
            data.insert(IDENTIFIER_KEY, identifier);
            data.insert(TYPE_KEY, FacebookContactImageDownloader::ContactCover);
            emit requestQueue(contact->coverUrl(), data);
            m_populatingAvatarsAccountId = accountId;
            incrementSemaphore(accountId);
        }
    }

    // When we arrive here, if we incremented semaphores we will continue loading
    // and download the contact photos, but if we didn't, then the sync will
    // be finished
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
            // clear the previous batch of contacts
            m_contactsToSave.clear();
            m_newContactsToSave.clear();

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
                parseContactDetails(currFriend, accountId);
            }
        }

        // paging if we need to retrieve more friends
        if (paging.contains("next")) {
            QString nextUrl = paging.value("next").toString();
            if (!nextUrl.isEmpty() && nextUrl != continuationRequest) {
                requestData(accountId, accessToken, nextUrl, lastSync);
            }
        }
    } else {
        QString message = isError ?
                          QLatin1String("error: error occurred during friends request with account %1; got: %2") :
                          QLatin1String("error: unable to parse friends data from request with account %1; got: %2");

        TRACE(SOCIALD_ERROR,
              message.arg(accountId).arg(QString::fromLatin1(replyData.constData())));
        clearRemovalDetectionLists(); // don't perform server-side removal detection.
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

void FacebookContactSyncAdaptor::slotImageDownloaded(const QString &url, const QString &path,
                                                     const QVariantMap &data)
{
    Q_UNUSED(url)
    Q_UNUSED(path)
    Q_UNUSED(data)

    // Load finished, we just decrement semaphore
    decrementSemaphore(m_populatingAvatarsAccountId);
}

#define SAVE_DETAIL(detail)                 \
    do {                                    \
        needsSaving = true;                 \
        newOrExisting.saveDetail(&detail);  \
    } while (0)

#define REMOVE_DETAIL(detail)               \
    do {                                    \
        needsSaving = true;                 \
        newOrExisting.removeDetail(&detail);\
    } while (0)

void FacebookContactSyncAdaptor::parseContactDetails(const QJsonObject &blobDetails, int accountId)
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
        // TODO: location.
        QString username = blobDetails.value(QLatin1String("username")).toString();
        QString birthdayStr = blobDetails.value(QLatin1String("birthday")).toString();
        QDateTime birthday = QDateTime::fromString(birthdayStr, Qt::ISODate);
        QString bio = blobDetails.value(QLatin1String("bio")).toString();
        QString gender = blobDetails.value(QLatin1String("gender")).toString();

        // now build the appropriate QtContacts details etc.
        bool isNewContact = false;
        bool needsSaving = false;
        bool needsSavingInDb = !m_cachedFriendIds.value(accountId).contains(fbuid);
        QContact newOrExisting = newOrExistingContact(fbuid, &isNewContact);

        // sync target is unique
        QContactSyncTarget contactSyncTarget = newOrExisting.detail<QContactSyncTarget>();
        if (contactSyncTarget.syncTarget().isEmpty()) {
            // must be a "new" contact - set the sync target.
            needsSaving = true;
            contactSyncTarget.setSyncTarget(SOCIALD_FACEBOOK_CONTACTS_SYNCTARGET);
            if (!newOrExisting.saveDetail(&contactSyncTarget)) {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: unable to save updated sync target for friend %1 of account %2"))
                        .arg(name).arg(accountId));
                clearRemovalDetectionLists(); // don't perform server-side removal detection.
                return;
            }
        }

        // guid is unique
        QContactGuid contactGuid = newOrExisting.detail<QContactGuid>();
        if (contactGuid.guid() != fbuid) {
            needsSaving = true;
            contactGuid.setGuid(fbuid);
            if (!newOrExisting.saveDetail(&contactGuid)) {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: unable to save updated guid for friend %1 of account %2"))
                        .arg(name).arg(accountId));
                clearRemovalDetectionLists(); // don't perform server-side removal detection.
                return;
            }
        }

        // name is unique
        QContactName contactName = newOrExisting.detail<QContactName>();
        if (!firstName.isEmpty() || !middleName.isEmpty() || !lastName.isEmpty()) {
            if (contactName.firstName() != firstName || contactName.middleName() != middleName || contactName.lastName() != lastName) {
                needsSaving = true;
                contactName.setFirstName(firstName);
                contactName.setMiddleName(middleName);
                contactName.setLastName(lastName);
                if (!newOrExisting.saveDetail(&contactName)) {
                    TRACE(SOCIALD_ERROR,
                            QString(QLatin1String("error: unable to save updated name for friend %1 of account %2"))
                            .arg(name).arg(accountId));
                    clearRemovalDetectionLists(); // don't perform server-side removal detection.
                    return;
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
                } else if (curl.url() != linkUrl.toString()) {
                    // needs to be updated.
                    QContactUrl contactUrl = curl;
                    contactUrl.setUrl(QUrl(link));
                    SAVE_DETAIL(contactUrl);
                    haveSavedLink = true;
                }
            } else if (curl.subType() == QContactUrl::SubTypeHomePage) {
                // this is the "website" detail.  determine whether it needs an update.
                if (website.isEmpty()) {
                    // needs to be removed.
                    QContactUrl contactUrl = curl;
                    REMOVE_DETAIL(contactUrl);
                } else if (curl.url() != websiteUrl.toString()) {
                    // needs to be updated.
                    QContactUrl contactUrl = curl;
                    contactUrl.setUrl(QUrl(website));
                    SAVE_DETAIL(contactUrl);
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

        FacebookContact::ConstPtr contact;
        if (m_cachedFriendIds.value(accountId).contains(fbuid)) {
            contact = m_db.contact(fbuid, accountId);
        }

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

                    QContactAvatar contactAvatar = avatar;
                    contactAvatar.setImageUrl(FacebookContactImageDownloader::staticOutputFile(data));
                    SAVE_DETAIL(contactAvatar);
                }

                // We reset the image file: we want it to be downloaded again
                if (m_cachedFriendIds.value(accountId).contains(fbuid)) {
                    if (contact->coverUrl() != cover) {
                        m_db.updateCoverFile(fbuid, QString());
                        needsSavingInDb = true;
                    }
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

                    QContactAvatar contactAvatar = avatar;
                    contactAvatar.setImageUrl(FacebookContactImageDownloader::staticOutputFile(data));
                    SAVE_DETAIL(contactAvatar);
                }

                // We reset the image file: we want it to be downloaded again
                if (m_cachedFriendIds.value(accountId).contains(fbuid)) {
                    if (contact->pictureUrl() != picture) {
                        m_db.updatePictureFile(fbuid, QString());
                        needsSavingInDb = true;
                    }
                }
            }
        }
        if (!foundCover && !cover.isEmpty()) {

            QVariantMap data;
            data.insert(IDENTIFIER_KEY, fbuid);
            data.insert(TYPE_KEY, FacebookContactImageDownloader::ContactCover);

            // needs to be updated.
            // note: we don't download the cover image here; the contacts app should
            // do so only for contacts which we need the cover of.
            QContactAvatar contactAvatar;
            contactAvatar.setImageUrl(FacebookContactImageDownloader::staticOutputFile(data));
            contactAvatar.setValue(QContactAvatar__FieldAvatarMetadata, QLatin1String("cover"));
            SAVE_DETAIL(contactAvatar);
        }
        if (!foundPicture && !picture.isEmpty()) {

            QVariantMap data;
            data.insert(IDENTIFIER_KEY, fbuid);
            data.insert(TYPE_KEY, FacebookContactImageDownloader::ContactPicture);

            // needs to be updated.  we set the value to be the (future) image filename.
            QContactAvatar contactAvatar;
            contactAvatar.setImageUrl(FacebookContactImageDownloader::staticOutputFile(data));
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
        if (birthdayStr.isEmpty()) {
            // must be removed
            REMOVE_DETAIL(contactBirthday);
        } else if (contactBirthday.dateTime() != birthday) {
            // must be updated
            contactBirthday.setDateTime(birthday);
            SAVE_DETAIL(contactBirthday);
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

        // XXX TODO: Location and Significant Other

        // Now that we've built up the contact, flag it for saving to the database if required.
        if (needsSaving) {
            m_contactsToSave.insert(fbuid, newOrExisting);
            if (isNewContact) {
                m_newContactsToSave.append(newOrExisting);
            }
        }

        if (m_cachedFriendIds.value(accountId).contains(fbuid)) {
            m_cachedFriendIds[accountId].remove(fbuid);
        }

        if (needsSavingInDb) {
            m_db.addSyncedContact(fbuid, accountId, picture, cover);
        }
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse friend details, got: %1"))
                .arg(QStringList(blobDetails.keys()).join(QChar(','))));
        clearRemovalDetectionLists(); // don't perform server-side removal detection.
    }
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

void FacebookContactSyncAdaptor::initRemovalDetection(int accountId)
{
    clearRemovalDetectionLists();
    QStringList contactIds = m_db.contactIds(accountId);
    foreach (const QString contactId, contactIds) {
        m_cachedFriendIds[accountId].insert(contactId);
    }
}

void FacebookContactSyncAdaptor::purgeDetectedRemovals()
{
    foreach (int accountId, m_cachedFriendIds.keys()) {
        QStringList fbContactIds = m_cachedFriendIds.value(accountId).toList();
        purgeContacts(fbContactIds, accountId);

        m_db.removeContacts(fbContactIds);
    }
}

bool FacebookContactSyncAdaptor::purgeContacts(const QStringList &friendIds, int accountId)
{

    bool errorOccurred = false;
    QList<QContactId> doomedContacts;
    foreach (const QString friendId, friendIds) {
        // We need to remove the contact from Qt Contacts
        QList<QContactId> doomedContact = contactIdsForGuid(friendId);
        if (doomedContact.length() < 1) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("error: friend %1 doesn't exist in QtContacts db"))
                  .arg(friendId));
            // flow on and purge the friend from the cache db nonetheless.
            // this is not a fatal error, as we aren't leaving stale data anywhere.
        } else {
            if (doomedContact.length() > 1) {
                TRACE(SOCIALD_ERROR,
                      QString(QLatin1String("error: friend %1 represented multiple times in QtContacts db"))
                      .arg(friendId));
                // flow on and purge all of them nonetheless
                // this is not a fatal error, as we aren't leaving stale data anywhere.
            }

            // delete the contact (or contacts in the above case...)
        }

        doomedContacts.append(doomedContact);
    }

    if (!m_contactManager->removeContacts(doomedContacts)) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to remove friends from QtContacts db")));
        errorOccurred = true;
    }

    if (!errorOccurred) {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("successfully deleted removed friends from local databases")));
        return true;
    }

    return false;
}

void FacebookContactSyncAdaptor::purgeAccount(int accountId)
{
    QStringList contactIds = m_db.contactIds(accountId);
    purgeContacts(contactIds, accountId);
    m_db.removeContacts(accountId);
}

void FacebookContactSyncAdaptor::clearRemovalDetectionLists()
{
    // This function should be called if a request errors out.
    // If the lists are empty, we won't purge anything.
    m_cachedFriendIds.clear();
}

#include "facebookcontactsyncadaptor.moc"
