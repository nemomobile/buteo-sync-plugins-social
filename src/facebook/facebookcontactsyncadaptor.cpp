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

#define SOCIALD_FACEBOOK_CONTACTS_ID_PREFIX QLatin1String("facebook-contacts-")
#define SOCIALD_FACEBOOK_CONTACTS_GROUPNAME QLatin1String("sociald-sync-facebook-contacts")
#define SOCIALD_FACEBOOK_CONTACTS_SYNCTARGET QLatin1String("facebook")
#define SOCIALD_FACEBOOK_CONTACTS_AVATAR_FILENAME(fbFriendId, avatarType) QString("%1/%2/%3-%4.jpg").arg(QLatin1String(PRIVILEGED_DATA_DIR)).arg(SyncService::dataType(dataType)).arg(fbFriendId).arg(avatarType)
#define SOCIALD_FACEBOOK_CONTACTS_AVATAR_BATCHSIZE 20

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

    // we create a database at PRIVILEGED_DATA_DIR/Contacts/facebook.db
    QString contactSyncDb = QString("%1/%2/%3")
                .arg(QLatin1String(PRIVILEGED_DATA_DIR))
                .arg(SyncService::dataType(dataType))
                .arg(QLatin1String("facebook.db"));
    if (!QFile::exists(contactSyncDb)) {
        QDir dir(QString("%1/%2").arg(QLatin1String(PRIVILEGED_DATA_DIR)).arg(SyncService::dataType(dataType)));
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        QString absolutePath = dir.absoluteFilePath(QLatin1String("facebook.db"));
        QFile dbfile(absolutePath);
        if (!dbfile.open(QIODevice::ReadWrite)) {
            TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to create Facebook contacts database %1 - Facebook contacts sync will be inactive"))
                .arg(absolutePath));
            return;
        }
        dbfile.close();
    }

    // open the database in which we store our synced friend information
    QString connectionName = QString(QLatin1String("sociald/facebook/%1")).arg(SyncService::dataType(dataType));
    QString databaseName = contactSyncDb;
    m_contactSyncDb = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_contactSyncDb.setDatabaseName(databaseName);
    if (!m_contactSyncDb.open()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: unable to open Facebook contacts database %1 - Facebook contacts sync will be inactive"))
            .arg(databaseName));
        return;
    }

    // create the facebook contact db tables
    QSqlQuery query(m_contactSyncDb);
    query.prepare("CREATE TABLE IF NOT EXISTS friends ("
                  " fbFriendId VARCHAR(50),"
                  " accountId INTEGER,"
                  " pictureUrl VARCHAR,"
                  " coverUrl VARCHAR,"
                  " pictureFile VARCHAR,"
                  " coverFile VARCHAR,"
                  " PRIMARY KEY(fbFriendId, accountId))");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: unable to create friends table: %1 - Facebook contacts sync will be inactive"))
            .arg(query.lastError().text()));
        m_contactSyncDb.close();
        return;
    }

    // can sync, enabled
    setInitialActive(true);
}

FacebookContactSyncAdaptor::~FacebookContactSyncAdaptor()
{
    m_contactSyncDb.close();
}

void FacebookContactSyncAdaptor::sync(const QString &dataType)
{
    // initialise friend id lists so that we can perform removal detection.
    initRemovalDetectionLists();

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
    requestData(accountId, accessToken);
}

void FacebookContactSyncAdaptor::requestData(int accountId, const QString &accessToken, const QString &fbFriendId, const QString &avatarUrl, const QString &avatarType, const QString &continuationRequest, const QDateTime &syncTimestamp)
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
            // Facebook's pagination API is pretty terrible.  Sometimes it includes this, sometimes not.
            QUrlQuery query(url);
            query.setQueryItems(queryItems);
            url.setQuery(query);
        }
    } else if (avatarUrl.isEmpty()) {
        // beginning a new sync via me/friends request.
        url = QUrl(QString(QLatin1String("https://graph.facebook.com/me/friends")));
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("limit")), QLatin1String("200")));
        QString whichFields = QLatin1String("name,first_name,middle_name,last_name,link,website,picture,cover,location,username,birthday,bio,gender,significant_other,updated_time");
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("fields")), whichFields));
        QUrlQuery query(url);
        query.setQueryItems(queryItems);
        url.setQuery(query);
    } else {
        // retrieving a particular avatar (picture or cover) of a friend.
        // note: we don't need the access token in this, as it's not a graph API query
        isAvatarRequest = true;
        url = QUrl(avatarUrl);
        QList<QPair<QString, QString> > avatarSizeQuery;
        avatarSizeQuery.append(QPair<QString, QString>(QString(QLatin1String("type")), QString(QLatin1String("large"))));
        QUrlQuery query(url);
        query.setQueryItems(avatarSizeQuery);
        url.setQuery(query);
    }

    QNetworkReply *reply = networkAccessManager->get(QNetworkRequest(url));

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("continuationRequest", continuationRequest);
        reply->setProperty("lastSyncTimestamp", timestamp);
        if (isAvatarRequest) {
            reply->setProperty("fbFriendId", fbFriendId);
            reply->setProperty("avatarType", avatarType);
            reply->setProperty("avatarUrl", avatarUrl);
        }
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        if (isAvatarRequest) {
            reply->setProperty("redirectCount", 0);
            connect(reply, SIGNAL(finished()), this, SLOT(avatarFinishedHandler()));
        } else {
            connect(reply, SIGNAL(finished()), this, SLOT(friendsFinishedHandler()));
        }

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
            m_avatarsToRequest.clear();

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

            // now save the batch of contacts.
            saveParsedContacts(accountId);

            // and request any avatars as necessary.  We do this after the saving
            // so that requests don't time out while we're waiting for file io.
            m_avatarsSemaphore = m_avatarsToRequest.size();
            requestAvatars(accessToken);
        }

        // paging if we need to retrieve more friends
        if (paging.contains("next")) {
            QString nextUrl = paging.value("next").toString();
            if (!nextUrl.isEmpty() && nextUrl != continuationRequest) {
                requestData(accountId, accessToken, QString(), QString(), QString(), nextUrl, lastSync);
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
        foreach (const QContactAvatar &avatar, contactAvatars) {
            if (avatar.value(QContactAvatar__FieldAvatarMetadata) == QLatin1String("cover")) {
                foundCover = true;
                if (cover.isEmpty()) {
                    // needs to be removed.
                    QContactAvatar contactAvatar = avatar;
                    REMOVE_DETAIL(contactAvatar);
                } else if (avatarUrlIsDifferent(QLatin1String("cover"), fbuid, accountId, cover)) {
                    // needs to be updated.
                    // note: we don't download the cover image here; the contacts app should
                    // do so only for contacts which we need the cover of.
                    QContactAvatar contactAvatar = avatar;
                    contactAvatar.setImageUrl(cover); // remote url.
                    SAVE_DETAIL(contactAvatar);
                }
            } else if (avatar.value(QContactAvatar__FieldAvatarMetadata) == QLatin1String("picture")) {
                foundPicture = true;
                if (picture.isEmpty()) {
                    // needs to be removed.
                    QContactAvatar contactAvatar = avatar;
                    REMOVE_DETAIL(contactAvatar);
                } else if (avatarUrlIsDifferent(QLatin1String("picture"), fbuid, accountId, picture)) {
                    // needs to be updated.  we set the value to be the (future) image filename.
                    QContactAvatar contactAvatar = avatar;
                    contactAvatar.setImageUrl(SOCIALD_FACEBOOK_CONTACTS_AVATAR_FILENAME(fbuid, QLatin1String("picture")));
                    SAVE_DETAIL(contactAvatar);

                    // we also queue up a request to download the thumbnail in the background.
                    FacebookContactSyncAdaptor::AvatarRequestData ard;
                    ard.accountId = accountId;
                    ard.fbuid = fbuid;
                    ard.url = QString(QLatin1String("http://graph.facebook.com/%1/picture")).arg(fbuid);
                    ard.type = QLatin1String("picture");
                    m_avatarsToRequest.append(ard);
                }
            }
        }
        if (!foundCover && !cover.isEmpty()) {
            // needs to be updated.
            // note: we don't download the cover image here; the contacts app should
            // do so only for contacts which we need the cover of.
            QContactAvatar contactAvatar;
            contactAvatar.setImageUrl(cover); // remote url.
            contactAvatar.setValue(QContactAvatar__FieldAvatarMetadata, QLatin1String("cover"));
            SAVE_DETAIL(contactAvatar);
        }
        if (!foundPicture && !picture.isEmpty()) {
            // needs to be updated.  we set the value to be the (future) image filename.
            QContactAvatar contactAvatar;
            contactAvatar.setImageUrl(SOCIALD_FACEBOOK_CONTACTS_AVATAR_FILENAME(fbuid, QLatin1String("picture")));
            contactAvatar.setValue(QContactAvatar__FieldAvatarMetadata, QLatin1String("picture"));
            SAVE_DETAIL(contactAvatar);

            // we also queue up a request to download the thumbnail in the background.
            FacebookContactSyncAdaptor::AvatarRequestData ard;
            ard.accountId = accountId;
            ard.fbuid = fbuid;
            ard.url = QString(QLatin1String("http://graph.facebook.com/%1/picture")).arg(fbuid);
            ard.type = QLatin1String("picture");
            m_avatarsToRequest.append(ard);
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
                m_newContactsToSave.append(fbuid);
            }
        }

        // it exists server side - ensure that we don't remove it (via removal detection list)
        if (!m_serverFriendIds.values(accountId).contains(fbuid)) {
            m_serverFriendIds.insert(accountId, fbuid);
        }
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse friend details, got: %1"))
                .arg(QStringList(blobDetails.keys()).join(QChar(','))));
        clearRemovalDetectionLists(); // don't perform server-side removal detection.
    }
}

void FacebookContactSyncAdaptor::saveParsedContacts(int accountId)
{
    QList<QContact> allContactsNeedingSave = m_contactsToSave.values();
    if (!m_contactManager->saveContacts(&allContactsNeedingSave)) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to save %1 friends for account %2"))
                .arg(allContactsNeedingSave.size()).arg(accountId));
        clearRemovalDetectionLists(); // don't perform server-side removal detection.
        return;
    }

    // add new contact ids to local cache db for account.
    QVariantList friendIdValues;
    QVariantList accountIdValues;
    QVariantList pictureUrlValues;
    QVariantList coverUrlValues;
    QVariantList pictureFileValues;
    QVariantList coverFileValues;
    foreach (const QString &fbFriendId, m_newContactsToSave) {
        if (!friendIdValues.contains(QVariant(fbFriendId))) {
            friendIdValues << fbFriendId;
            accountIdValues << accountId;
            pictureUrlValues << QString();
            coverUrlValues << QString();
            pictureFileValues << QString();
            coverFileValues << QString();
        }
    }

    QSqlQuery query(m_contactSyncDb);
    query.prepare("INSERT INTO friends (fbFriendId, accountId, pictureUrl, coverUrl, pictureFile, coverFile)"
                  " VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(friendIdValues);
    query.addBindValue(accountIdValues);
    query.addBindValue(pictureUrlValues);
    query.addBindValue(coverUrlValues);
    query.addBindValue(pictureFileValues);
    query.addBindValue(coverFileValues);
    if (!query.execBatch()) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to update cache db with %1 new friends for account %2: %3"))
                .arg(m_newContactsToSave.size()).arg(accountId).arg(query.lastError().text()));
        clearRemovalDetectionLists(); // don't perform server-side removal detection.
    }
}

void FacebookContactSyncAdaptor::requestAvatars(const QString &accessToken)
{
    // instead of requesting all avatars, we request them in small batches.
    // when those requests are finished, the handler invokes this function again,
    // and so on until the entire queue is emptied.
    // This is for two reasons:
    //  - don't fill up memory with buffered image results which we haven't flushed to disk
    //  - don't time out requests because we take a long time to do io/writes.

    // firstly, save a batch of buffered image results
    saveAvatars();

    // secondly, request the next batch.
    int batchSize = qMin(m_avatarsToRequest.size(), SOCIALD_FACEBOOK_CONTACTS_AVATAR_BATCHSIZE);
    while (batchSize > 0) {
        batchSize--;
        FacebookContactSyncAdaptor::AvatarRequestData ard = m_avatarsToRequest.takeFirst();
        requestData(ard.accountId, accessToken, ard.fbuid, ard.url, ard.type);
    }
}

void FacebookContactSyncAdaptor::saveAvatars()
{
    QStringList avatarTypes;
    QStringList fileNames;
    QStringList avatarUrls;
    QStringList fbFriendIds;
    QList<int> accountIds;

    // save a batch of avatars to disk
    int batchSize = m_avatarsToSave.size();
    while (m_avatarsToSave.size()) {
        FacebookContactSyncAdaptor::AvatarReplyData ard = m_avatarsToSave.takeFirst();
        QString currFileName = saveImageToDisk(ard.accountId, ard.type, ard.fbuid, ard.data);
        if (!currFileName.isEmpty()) {
            // successfully saved.  update the database info.
            avatarTypes.append(ard.type);
            fileNames.append(currFileName);
            avatarUrls.append(ard.url);
            fbFriendIds.append(ard.fbuid);
            accountIds.append(ard.accountId);
        }
    }

    // update database with updated info.
    updateDatabaseWithImageInfo(avatarTypes, fileNames, avatarUrls, fbFriendIds, accountIds);

    TRACE(SOCIALD_DEBUG,
            QString(QLatin1String("saved a batch of %1 avatars"))
            .arg(batchSize));
}

void FacebookContactSyncAdaptor::avatarFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QString avatarType = reply->property("avatarType").toString();
    QString fbFriendId = reply->property("fbFriendId").toString();
    QString avatarUrl = reply->property("avatarUrl").toString();
    int redirectCount = reply->property("redirectCount").toInt();
    QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    bool needRedirect = false;

    if (isError) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error occurred during friend %1 avatar request: %2 %3"))
                .arg(fbFriendId).arg(reply->url().toString()).arg(QLatin1String(reply->readAll())));
    } else if (redirectUrl.isValid()) {
        if (redirectCount >= 4) {
            isError = true;
        } else {
            QNetworkReply *redirectReply = networkAccessManager->get(QNetworkRequest(redirectUrl));
            if (redirectReply) {
                needRedirect = true;
                redirectReply->setProperty("accountId", accountId);
                redirectReply->setProperty("accessToken", accessToken);
                redirectReply->setProperty("avatarType", avatarType);
                redirectReply->setProperty("fbFriendId", fbFriendId);
                redirectReply->setProperty("avatarUrl", avatarUrl);
                redirectReply->setProperty("redirectCount", redirectCount+1);
                connect(redirectReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
                connect(redirectReply, SIGNAL(finished()), this, SLOT(avatarFinishedHandler()));
            } else {
                isError = 1;
            }
        }

        if (isError) {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error occurred during friend %1 avatar redirect request: %2 # %3"))
                    .arg(fbFriendId).arg(redirectUrl.toString()).arg(redirectCount));
        }
    } else {
        QByteArray allData = reply->readAll();
        if (!isError) {
            // queue avatar for saving.
            FacebookContactSyncAdaptor::AvatarReplyData ard;
            ard.accountId = accountId;
            ard.fbuid = fbFriendId;
            ard.type = avatarType;
            ard.url = avatarUrl;
            ard.data = allData;
            m_avatarsToSave.append(ard);
        }
    }

    disconnect(reply);
    reply->deleteLater();

    if (!needRedirect) {
        m_avatarsSemaphore--;
        if ((m_avatarsSemaphore % SOCIALD_FACEBOOK_CONTACTS_AVATAR_BATCHSIZE) == 0) {
            // request more avatars if they exist
            // Note: due to the use of the modulo, the first few avatar writes might
            // be queued unfairly.
            requestAvatars(accessToken);
        }

        // we're finished this request.  Decrement our busy semaphore.
        decrementSemaphore(accountId);
    }
}

bool FacebookContactSyncAdaptor::avatarUrlIsDifferent(const QString &avatarType, const QString &fbFriendId, int accountId, const QString &avatarUrl)
{
    if (fbFriendId.isEmpty()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: empty friend id while detecting avatar - aborting")));
        return false;
    }

    bool isCover = avatarType == QLatin1String("cover");
    QSqlQuery query(m_contactSyncDb);
    query.prepare(QString(QLatin1String(
                 "SELECT %1Url FROM friends WHERE fbFriendId = :fbfid AND accountId = :aid"))
                 .arg(isCover ? QLatin1String("cover") : QLatin1String("picture")));
    query.bindValue(":fbfid", fbFriendId);
    query.bindValue(":aid", accountId);

    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: could not query existing image url for %1: %2"))
              .arg(fbFriendId).arg(query.lastError().text()));
        return false;
    }

    if (query.next()) {
        QString currentUrl = query.value(0).toString();
        if (currentUrl != avatarUrl) {
            return true;
        }

        return false;
    }

    if (!avatarUrl.isEmpty()) {
        return true; // the new avatar url is non-empty, the current one is empty / nonexistent.
    }

    return false; // if they're both empty, they're not different
}

bool FacebookContactSyncAdaptor::removeAvatarFromDisk(const QString &fbFriendId, int accountId, const QString &avatarType)
{
    // only remove it if this friend isn't a friend of multiple accounts.
    QSqlQuery query(m_contactSyncDb);
    query.prepare(QString(QLatin1String(
                  "SELECT %1File, COUNT(accountId), MAX(accountId)"
                  " FROM friends"
                  " GROUP BY %1File"
                  " HAVING COUNT(accountId) = 1"
                  " AND MAX(accountId) = :aid"))
                 .arg(avatarType));
    query.bindValue(":fbfid", fbFriendId);
    query.bindValue(":aid", accountId);

    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: could not query existing image file for %1: %2"))
              .arg(fbFriendId).arg(query.lastError().text()));
        return false;
    }

    if (query.next()) {
        // have a file we need to remove.
        QString doomedFile = query.value(0).toString();
        if (QFile::exists(doomedFile)) {
            QFile::remove(doomedFile);
        }
    }

    return true;
}

QString FacebookContactSyncAdaptor::saveImageToDisk(int accountId, const QString &avatarType, const QString &fbFriendId, const QByteArray &data)
{
    if (fbFriendId.isEmpty()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: empty friend id while downloading avatar - aborting")));
        return QString();
    }

    if (avatarType.isEmpty()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: empty avatar type while downloading avatar - aborting")));
        return QString();
    }

    QImage image;
    bool loadedOk = image.loadFromData(data);
    if (!loadedOk || image.isNull()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: downloaded %1 for %2 but image data not valid"))
              .arg(avatarType).arg(fbFriendId));
        return QString();
    }

    // first, if any existing image exists, we need to remove it from disk.
    removeAvatarFromDisk(fbFriendId, accountId, avatarType);

    // save the new image (eg fbfriendid-picture.jpg or fbfriendid-cover.jpg)
    QString newName = SOCIALD_FACEBOOK_CONTACTS_AVATAR_FILENAME(fbFriendId, avatarType);

    bool saveOk = image.save(newName);
    if (!saveOk) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: downloaded %1 for %2 but could not save image"))
              .arg(avatarType).arg(fbFriendId));
        return QString();
    }

    return newName; // success.  Return the filename we saved the image as.
}

void FacebookContactSyncAdaptor::updateDatabaseWithImageInfo(const QStringList &avatarTypes, const QStringList &fileNames, const QStringList &avatarUrls, const QStringList &fbFriendIds, const QList<int> &accountIds)
{
    // firstly, group according to cover/picture.
    QVariantList pictureFileNames;
    QVariantList pictureAvatarUrls;
    QVariantList pictureFbFriendIds;
    QVariantList pictureAccountIds;

    QVariantList coverFileNames;
    QVariantList coverAvatarUrls;
    QVariantList coverFbFriendIds;
    QVariantList coverAccountIds;

    for (int i = 0; i < avatarTypes.size(); ++i) {
        if (avatarTypes[i] == QLatin1String("picture")) {
            pictureFileNames.append(fileNames[i]);
            pictureAvatarUrls.append(avatarUrls[i]);
            pictureFbFriendIds.append(fbFriendIds[i]);
            pictureAccountIds.append(accountIds[i]);
        } else {
            coverFileNames.append(fileNames[i]);
            coverAvatarUrls.append(avatarUrls[i]);
            coverFbFriendIds.append(fbFriendIds[i]);
            coverAccountIds.append(accountIds[i]);
        }
    }

    // update the database.
    QSqlQuery query(m_contactSyncDb);
    if (pictureFileNames.size()) {
        query.prepare(QString(QLatin1String(
                     "UPDATE friends SET pictureFile = ?, pictureUrl = ?"
                     " WHERE fbFriendId = ? AND accountId = ?")));
        query.addBindValue(pictureFileNames);
        query.addBindValue(pictureAvatarUrls);
        query.addBindValue(pictureFbFriendIds);
        query.addBindValue(pictureAccountIds);
        if (!query.execBatch()) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("error: failed to update database with avatar (picture) info: %1"))
                  .arg(query.lastError().text()));
            // NOTE: we should clean up the files here, but since we use a standard naming pattern,
            // the QtContact will (should) still reference it if it exists.
        }
    }
    if (coverFileNames.size()) {
        query.prepare(QString(QLatin1String(
                     "UPDATE friends SET coverFile = ?, coverUrl = ?"
                     " WHERE fbFriendId = ? AND accountId = ?")));
        query.addBindValue(coverFileNames);
        query.addBindValue(coverAvatarUrls);
        query.addBindValue(coverFbFriendIds);
        query.addBindValue(coverAccountIds);
        if (!query.execBatch()) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("error: failed to update database with avatar (cover) info: %1"))
                  .arg(query.lastError().text()));
            // NOTE: we should clean up the files here, but since we use a standard naming pattern,
            // the QtContact will (should) still reference it if it exists.
        }
    }

    // NOTE: no need to update the QtContacts database, as the avatar image field
    // always points to the local file location.
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

void FacebookContactSyncAdaptor::initRemovalDetectionLists()
{
    // This function should be called as part of the ::sync() preamble.
    // Clear our internal state variables which we use to track server-side deletions.
    // We have to do it this way, as results can be spread across multiple requests
    // if Facebook returns results in paginated form.
    m_cachedFriendIds.clear();
    m_serverFriendIds.clear();

    QSqlQuery query(m_contactSyncDb);
    query.prepare("SELECT fbFriendId, accountId FROM friends");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute friend ids query: %1")).arg(query.lastError().text()));
        return;
    } else {
        while (query.next()) {
            m_cachedFriendIds.insert(query.value(1).toInt(), query.value(0).toString());
        }
    }
}

void FacebookContactSyncAdaptor::purgeDetectedRemovals()
{
    // This function should be called once the synchronization process is completed.

    // first, build up a dictionary in memory of "friendId to count of accounts which have that friend".
    QMap<QString, int> friendCounts;
    QList<int> accountIds = m_cachedFriendIds.keys();
    foreach (int accountId, accountIds) {
        QStringList cachedFriendsOfAccount = m_cachedFriendIds.values(accountId);
        foreach (const QString &currFriend, cachedFriendsOfAccount) {
            friendCounts[currFriend] += 1;
        }
    }

    // second, look at the server-side friends which still exist for each account
    // if the cached-friends contains friends which don't exist server-side, then
    // we need to purge that friend.  When we do so, we may also need to purge the
    // associated QtContact if the count in the previously built hash is now zero.
    int expectedPurgeFriendCount = 0;
    int actualPurgeFriendCount = 0;
    foreach (int accountId, accountIds) {
        QStringList cachedFriendsOfAccount = m_cachedFriendIds.values(accountId);
        QStringList serverFriendsOfAccount = m_serverFriendIds.values(accountId);
        foreach (const QString &currFriend, cachedFriendsOfAccount) {
            if (!serverFriendsOfAccount.contains(currFriend)) {
                // we will purge this friend.  reduce the count of accounts who have this friend.
                expectedPurgeFriendCount += 1;
                friendCounts[currFriend] -= 1;

                // check if we also need to remove the QtContact.
                bool removeContact = (friendCounts[currFriend] == 0);

                // purge the friend and possibly the associated contact.
                if (purgeFriend(currFriend, accountId, removeContact)) {
                    actualPurgeFriendCount += 1;
                }
            }
        }
    }

    if (expectedPurgeFriendCount != actualPurgeFriendCount) {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("unable to purge all friends: expected to remove %1, removed %2"))
                .arg(expectedPurgeFriendCount).arg(expectedPurgeFriendCount));
    } else if (actualPurgeFriendCount != 0) {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("successfully purged %1 friends"))
                .arg(actualPurgeFriendCount));
    }
}

bool FacebookContactSyncAdaptor::purgeFriend(const QString &friendId, int accountId, bool purgeContact)
{
    bool errorOccurred = false;
    if (purgeContact) {
        // purge the friend from the QtContacts db
        QList<QContactId> cids = contactIdsForGuid(friendId);
        if (cids.length() < 1) {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: friend %1 doesn't exist in QtContacts db"))
                    .arg(friendId));
            // flow on and purge the friend from the cache db nonetheless.
            // this is not a fatal error, as we aren't leaving stale data anywhere.
        } else {
            if (cids.length() > 1) {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: friend %1 represented multiple times in QtContacts db"))
                        .arg(friendId));
                // flow on and purge all of them nonetheless
                // this is not a fatal error, as we aren't leaving stale data anywhere.
            }

            // delete the contact (or contacts in the above case...)
            if (!m_contactManager->removeContacts(cids)) {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: unable to remove friend %1 from QtContacts db"))
                        .arg(friendId));
                errorOccurred = true;
            }
        }
    }

    // purge the avatars from disk
    removeAvatarFromDisk(friendId, accountId, QLatin1String("picture"));
    removeAvatarFromDisk(friendId, accountId, QLatin1String("cover"));

    // purge the friend from the cache db
    QSqlQuery query(m_contactSyncDb);
    query.prepare("DELETE FROM friends WHERE fbFriendId = :fbfid AND accountId = :aid");
    query.bindValue(":fbfid", friendId);
    query.bindValue(":aid", accountId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to execute friend deletion: %1"))
                .arg(query.lastError().text()));
        errorOccurred = true;
    }

    if (!errorOccurred) {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("successfully deleted removed friend %1 from local databases"))
                .arg(friendId));
        return true;
    }

    return false;
}

void FacebookContactSyncAdaptor::purgeAccount(int pid)
{
    // purge any QtContacts for friends which are only friends of this account
    QSqlQuery query(m_contactSyncDb);
    query.prepare("SELECT fbFriendId, COUNT(accountId), MAX(accountId)"
                  " FROM friends"
                  " GROUP BY fbFriendId"
                  " HAVING COUNT(accountId) = 1"
                  " AND MAX(accountId) = :aid");
    query.bindValue(":aid", pid);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to purge friend contacts of account %1: %2"))
                .arg(pid).arg(query.lastError().text()));
        return;
    }
    QStringList purgeQtContactFids;
    while (query.next()) {
        purgeQtContactFids.append(query.value(0).toString());
    }

    // purge any friends of this account
    query.prepare("SELECT fbFriendId FROM friends WHERE accountId = :aid");
    query.bindValue(":aid", pid);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to purge friend data of account %1: %2"))
                .arg(pid).arg(query.lastError().text()));
        return;
    }
    QStringList purgeAccountFriends;
    while (query.next()) {
        purgeAccountFriends.append(query.value(0).toString());
    }

    int purgeCount = 0;
    int failCount = 0;
    foreach (const QString &fid, purgeAccountFriends) {
        // we also purge the QtContact if no other account has that friend as a friend.
        if (purgeFriend(fid, pid, purgeQtContactFids.contains(fid))) {
            purgeCount += 1;
        } else {
            failCount += 1;
        }
    }

    TRACE(SOCIALD_INFORMATION,
            QString(QLatin1String("purged account %1 and successfully removed %2 friends (failed to remove %3 friends)"))
            .arg(pid).arg(purgeCount).arg(failCount));
}

void FacebookContactSyncAdaptor::clearRemovalDetectionLists()
{
    // This function should be called if a request errors out.
    // If the lists are empty, we won't purge anything.
    m_cachedFriendIds.clear();
}
