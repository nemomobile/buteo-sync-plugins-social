/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "vkdatatypesyncadaptor.h"
#include "trace.h"

#include <QtCore/QVariantMap>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QByteArray>

//libsailfishkeyprovider
#include <sailfishkeyprovider.h>

//libaccounts-qt
#include <Accounts/Manager>
#include <Accounts/Account>
#include <Accounts/AccountService>
#include <Accounts/Service>

//libsignon-qt
#include <SignOn/SessionData>
#include <SignOn/Identity>

//-----------------

VKDataTypeSyncAdaptor::UserProfile::UserProfile()
{
}

VKDataTypeSyncAdaptor::UserProfile::~UserProfile()
{
}

VKDataTypeSyncAdaptor::UserProfile::UserProfile(const UserProfile &other)
{
    operator=(other);
}

VKDataTypeSyncAdaptor::UserProfile &VKDataTypeSyncAdaptor::UserProfile::operator=(const UserProfile &other)
{
    if (&other == this) {
        return *this;
    }
    uid = other.uid;
    firstName = other.firstName;
    lastName = other.lastName;
    icon = other.icon;
    return *this;
}

VKDataTypeSyncAdaptor::UserProfile VKDataTypeSyncAdaptor::UserProfile::fromJsonObject(const QJsonObject &object)
{
    UserProfile user;
    user.uid = int(object.value(QStringLiteral("id")).toDouble());
    user.firstName = object.value(QStringLiteral("first_name")).toString();
    user.lastName = object.value(QStringLiteral("last_name")).toString();
    user.icon = object.value(QStringLiteral("photo_50")).toString();
    return user;
}

QString VKDataTypeSyncAdaptor::UserProfile::name() const
{
    // TODO locale-specific joining of names
    QString personName;
    if (!firstName.isEmpty()) {
        personName += firstName;
    }
    if (!lastName.isEmpty()) {
        if (!firstName.isEmpty()) {
            personName += ' ';
        }
        personName += lastName;
    }
    return personName;
}

//-----------------

VKDataTypeSyncAdaptor::GroupProfile::GroupProfile()
{
}

VKDataTypeSyncAdaptor::GroupProfile::~GroupProfile()
{
}

VKDataTypeSyncAdaptor::GroupProfile::GroupProfile(const GroupProfile &other)
{
    operator=(other);
}

VKDataTypeSyncAdaptor::GroupProfile &VKDataTypeSyncAdaptor::GroupProfile::operator=(const GroupProfile &other)
{
    if (&other == this) {
        return *this;
    }
    uid = other.uid;
    name = other.name;
    screenName = other.screenName;
    icon = other.icon;
    return *this;
}

VKDataTypeSyncAdaptor::GroupProfile VKDataTypeSyncAdaptor::GroupProfile::fromJsonObject(const QJsonObject &object)
{
    GroupProfile user;
    user.uid = int(object.value(QStringLiteral("id")).toDouble());
    user.name = object.value(QStringLiteral("name")).toString();
    user.screenName = object.value(QStringLiteral("screen_name")).toString();
    user.icon = object.value(QStringLiteral("photo_50")).toString();
    return user;
}

//-----------------

VKDataTypeSyncAdaptor::UserProfile VKDataTypeSyncAdaptor::findUserProfile(const QList<UserProfile> &profiles, int uid)
{
    Q_FOREACH (const UserProfile &user, profiles) {
        if (user.uid == uid) {
            return user;
        }
    }
    return UserProfile();
}

VKDataTypeSyncAdaptor::GroupProfile VKDataTypeSyncAdaptor::findGroupProfile(const QList<GroupProfile> &profiles, int uid)
{
    // sometimes the uid is given as negative to signify a group, eg in newsfeed.get
    int positiveUid = uid < 0 ? uid*-1 : uid;
    Q_FOREACH (const GroupProfile &group, profiles) {
        if (group.uid == positiveUid) {
            return group;
        }
    }
    return GroupProfile();
}

//-----------------

QDateTime VKDataTypeSyncAdaptor::parseVKDateTime(const QJsonValue &v)
{
    if (v.type() != QJsonValue::Double) {
        return QDateTime();
    }
    int t = int(v.toDouble());
    return QDateTime::fromTime_t(t);
}


VKDataTypeSyncAdaptor::VKDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::DataType dataType, QObject *parent)
    : SocialNetworkSyncAdaptor("vk", dataType, parent), m_triedLoading(false)
{
}

VKDataTypeSyncAdaptor::~VKDataTypeSyncAdaptor()
{
}

void VKDataTypeSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    if (dataTypeString != SocialNetworkSyncAdaptor::dataTypeName(m_dataType)) {
        SOCIALD_LOG_ERROR("VK" << SocialNetworkSyncAdaptor::dataTypeName(m_dataType) <<
                          "sync adaptor was asked to sync" << dataTypeString);
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    if (clientId().isEmpty()) {
        SOCIALD_LOG_ERROR("clientId could not be retrieved for VK account" << accountId);
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    setStatus(SocialNetworkSyncAdaptor::Busy);
    updateDataForAccount(accountId);
    SOCIALD_LOG_DEBUG("successfully triggered sync with profile:" << m_accountSyncProfile->name());
}

void VKDataTypeSyncAdaptor::updateDataForAccount(int accountId)
{
    Accounts::Account *account = Accounts::Account::fromId(m_accountManager, accountId, this);
    if (!account) {
        SOCIALD_LOG_ERROR("existing account with id" << accountId << "couldn't be retrieved");
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    // will be decremented by either signOnError or signOnResponse.
    incrementSemaphore(accountId);
    signIn(account);
}


void VKDataTypeSyncAdaptor::errorHandler(QNetworkReply::NetworkError err)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray replyData = reply->readAll();
    int accountId = reply->property("accountId").toInt();

    SOCIALD_LOG_ERROR(SocialNetworkSyncAdaptor::dataTypeName(m_dataType) <<
                      "request with account" << accountId <<
                      "experienced error:" << err <<
                      "HTTP:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    // set "isError" on the reply so that adapters know to ignore the result in the finished() handler
    reply->setProperty("isError", QVariant::fromValue<bool>(true));
    // Note: not all errors are "unrecoverable" errors, so we don't change the status here.

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (ok && parsed.contains(QLatin1String("error"))) {
        QJsonObject errorReply = parsed.value("error").toObject();
        // Password Changed on server side
        if (errorReply.value("code").toDouble() == 190 &&
                errorReply.value("error_subcode").toDouble() == 460) {
            int accountId = reply->property("accountId").toInt();
            Accounts::Account *account = Accounts::Account::fromId(m_accountManager, accountId, this);
            if (account) {
                setCredentialsNeedUpdate(account);
            }
        }
    }
}

void VKDataTypeSyncAdaptor::sslErrorsHandler(const QList<QSslError> &errs)
{
    QString sslerrs;
    foreach (const QSslError &e, errs) {
        sslerrs += e.errorString() + "; ";
    }
    if (errs.size() > 0) {
        sslerrs.chop(2);
    }
    SOCIALD_LOG_ERROR(SocialNetworkSyncAdaptor::dataTypeName(m_dataType) <<
                      "request with account" << sender()->property("accountId").toInt() <<
                      "experienced ssl errors:" << sslerrs);
    // set "isError" on the reply so that adapters know to ignore the result in the finished() handler
    sender()->setProperty("isError", QVariant::fromValue<bool>(true));
    // Note: not all errors are "unrecoverable" errors, so we don't change the status here.
}

QString VKDataTypeSyncAdaptor::clientId()
{
    if (!m_triedLoading) {
        loadClientId();
    }
    return m_clientId;
}

void VKDataTypeSyncAdaptor::loadClientId()
{
    m_triedLoading = true;
    char *cClientId = NULL;
    int cSuccess = SailfishKeyProvider_storedKey("vk", "vk-sync", "client_id", &cClientId);
    if (cSuccess != 0 || cClientId == NULL) {
        return;
    }

    m_clientId = QLatin1String(cClientId);
    free(cClientId);
    return;
}

void VKDataTypeSyncAdaptor::setCredentialsNeedUpdate(Accounts::Account *account)
{
    SOCIALD_LOG_INFO("sociald:VKontakte: setting CredentialsNeedUpdate to true for account:" << account->id());
    Accounts::Service srv(m_accountManager->service(syncServiceName()));
    account->selectService(srv);
    account->setValue(QStringLiteral("CredentialsNeedUpdate"), QVariant::fromValue<bool>(true));
    account->setValue(QStringLiteral("CredentialsNeedUpdateFrom"), QVariant::fromValue<QString>(QString::fromLatin1("sociald-vkontakte")));
    account->selectService(Accounts::Service());
    account->syncAndBlock();
}

void VKDataTypeSyncAdaptor::signIn(Accounts::Account *account)
{
    // Fetch clientId from keyprovider
    int accountId = account->id();
    if (!checkAccount(account) || clientId().isEmpty()) {
        decrementSemaphore(accountId);
        return;
    }

    // grab out a valid identity for the sync service.
    Accounts::Service srv(m_accountManager->service(syncServiceName()));
    account->selectService(srv);
    SignOn::Identity *identity = account->credentialsId() > 0 ? SignOn::Identity::existingIdentity(account->credentialsId()) : 0;
    if (!identity) {
        SOCIALD_LOG_ERROR("error: account has no valid credentials, cannot sign in:" << accountId);
        decrementSemaphore(accountId);
        return;
    }

    Accounts::AccountService accSrv(account, srv);
    QString method = accSrv.authData().method();
    QString mechanism = accSrv.authData().mechanism();
    SignOn::AuthSession *session = identity->createSession(method);
    if (!session) {
        SOCIALD_LOG_ERROR("error: could not create signon session for account:" << accountId);
        identity->deleteLater();
        decrementSemaphore(accountId);
        return;
    }

    QVariantMap signonSessionData = accSrv.authData().parameters();
    signonSessionData.insert("ClientId", clientId());
    signonSessionData.insert("UiPolicy", SignOn::NoUserInteractionPolicy);

    connect(session, SIGNAL(response(SignOn::SessionData)),
            this, SLOT(signOnResponse(SignOn::SessionData)),
            Qt::UniqueConnection);
    connect(session, SIGNAL(error(SignOn::Error)),
            this, SLOT(signOnError(SignOn::Error)),
            Qt::UniqueConnection);

    session->setProperty("account", QVariant::fromValue<Accounts::Account*>(account));
    session->setProperty("identity", QVariant::fromValue<SignOn::Identity*>(identity));
    session->process(SignOn::SessionData(signonSessionData), mechanism);
}

void VKDataTypeSyncAdaptor::signOnError(const SignOn::Error &error)
{
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession*>(sender());
    Accounts::Account *account = session->property("account").value<Accounts::Account*>();
    SignOn::Identity *identity = session->property("identity").value<SignOn::Identity*>();
    int accountId = account->id();
    SOCIALD_LOG_ERROR("credentials for account with id" << accountId <<
                      "couldn't be retrieved:" << error.type() << "," << error.message());

    // if the error is because credentials have expired, we
    // set the CredentialsNeedUpdate key.
    if (error.type() == SignOn::Error::UserInteraction) {
        setCredentialsNeedUpdate(account);
    }

    session->disconnect(this);
    identity->destroySession(session);
    identity->deleteLater();
    account->deleteLater();

    // if we couldn't sign in, we can't sync with this account.
    setStatus(SocialNetworkSyncAdaptor::Error);
    decrementSemaphore(accountId);
}

void VKDataTypeSyncAdaptor::signOnResponse(const SignOn::SessionData &responseData)
{
    QVariantMap data;
    foreach (const QString &key, responseData.propertyNames()) {
        data.insert(key, responseData.getProperty(key));
    }

    QString accessToken;
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession*>(sender());
    Accounts::Account *account = session->property("account").value<Accounts::Account*>();
    SignOn::Identity *identity = session->property("identity").value<SignOn::Identity*>();
    int accountId = account->id();

    if (data.contains(QLatin1String("AccessToken"))) {
        accessToken = data.value(QLatin1String("AccessToken")).toString();
    } else {
        SOCIALD_LOG_INFO("signon response for account with id" << accountId << "contained no oauth token");
    }

    session->disconnect(this);
    identity->destroySession(session);
    identity->deleteLater();
    account->deleteLater();

    if (!accessToken.isEmpty()) {
        beginSync(accountId, accessToken); // call the derived-class sync entrypoint.
    }

    decrementSemaphore(accountId);
}
