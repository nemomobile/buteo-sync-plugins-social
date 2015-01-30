/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
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

#include "twitterdatatypesyncadaptor.h"
#include "trace.h"

#include <QtCore/QDebug>

#include <QtCore/QVariantMap>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QUuid>
#include <QtCore/qmath.h>

#include <QCryptographicHash>
#include <QJsonDocument>

//libsailfishkeyprovider
#include <sailfishkeyprovider.h>

// libaccounts-qt5
#include <Accounts/Manager>
#include <Accounts/Account>
#include <Accounts/Service>
#include <Accounts/AccountService>

//libsignon-qt: SignOn::NoUserInteractionPolicy
#include <SignOn/Identity>
#include <SignOn/AuthSession>
#include <SignOn/SessionData>

TwitterDataTypeSyncAdaptor::TwitterDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::DataType dataType, QObject *parent)
    : SocialNetworkSyncAdaptor("twitter", dataType, parent), m_triedLoading(false)
{
}

TwitterDataTypeSyncAdaptor::~TwitterDataTypeSyncAdaptor()
{
}

void TwitterDataTypeSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    if (dataTypeString != SocialNetworkSyncAdaptor::dataTypeName(m_dataType)) {
        SOCIALD_LOG_ERROR("Twitter" << SocialNetworkSyncAdaptor::dataTypeName(m_dataType) <<
                          "sync adaptor was asked to sync" << dataTypeString);
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    if (consumerKey().isEmpty() || consumerSecret().isEmpty()) {
        SOCIALD_LOG_ERROR("secrets could not be retrieved for twitter account" << accountId);
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    setStatus(SocialNetworkSyncAdaptor::Busy);
    updateDataForAccount(accountId);
    SOCIALD_LOG_DEBUG("successfully triggered sync with profile:" << m_accountSyncProfile->name());
}

void TwitterDataTypeSyncAdaptor::updateDataForAccount(int accountId)
{
    Accounts::Account *account = m_accountManager->account(accountId);
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


QString TwitterDataTypeSyncAdaptor::consumerKey()
{
    if (!m_triedLoading) {
        loadConsumerKeyAndSecret();
    }
    return m_consumerKey;
}

QString TwitterDataTypeSyncAdaptor::consumerSecret()
{
    if (!m_triedLoading) {
        loadConsumerKeyAndSecret();
    }
    return m_consumerSecret;
}

void TwitterDataTypeSyncAdaptor::errorHandler(QNetworkReply::NetworkError err)
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
    if (ok && parsed.contains(QLatin1String("errors"))) {
        QJsonArray dataList = parsed.value(QLatin1String("errors")).toArray();
        // API v1.1 returns only one element in the array, but looks like these
        // are constantly updated: https://dev.twitter.com/docs/error-codes-responses
        foreach (QJsonValue data, dataList) {
            QJsonObject dataMap = data.toObject();
            if (dataMap.value("code").toDouble() == 32 || dataMap.value("code").toDouble() == 89) {
                Accounts::Account *account = m_accountManager->account(accountId);
                if (account) {
                    setCredentialsNeedUpdate(account);
                }
            }
        }
    }
}

void TwitterDataTypeSyncAdaptor::sslErrorsHandler(const QList<QSslError> &errs)
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

// This function taken from http://qt-project.org/wiki/HMAC-SHA1 which is in the public domain
// and carries no licensing requirements (as at 2013-05-09)
static QString hmacSha1(const QString &signingKey, const QString &baseString)
{
    QByteArray key = signingKey.toLatin1();
    QByteArray baseArray = baseString.toLatin1();

    int blockSize = 64; // HMAC-SHA-1 block size, defined in SHA-1 standard
    if (key.length() > blockSize) { // if key is longer than block size (64), reduce key length with SHA-1 compression
        key = QCryptographicHash::hash(key, QCryptographicHash::Sha1);
    }

    QByteArray innerPadding(blockSize, char(0x36)); // initialize inner padding with char "6"
    QByteArray outerPadding(blockSize, char(0x5c)); // initialize outer padding with char "\"
    // ascii characters 0x36 ("6") and 0x5c ("\") are selected because they have large
    // Hamming distance (http://en.wikipedia.org/wiki/Hamming_distance)

    for (int i = 0; i < key.length(); i++) {
        innerPadding[i] = innerPadding[i] ^ key.at(i); // XOR operation between every byte in key and innerpadding, of key length
        outerPadding[i] = outerPadding[i] ^ key.at(i); // XOR operation between every byte in key and outerpadding, of key length
    }

    // result = hash ( outerPadding CONCAT hash ( innerPadding CONCAT baseArray ) ).toBase64
    QByteArray total = outerPadding;
    QByteArray part = innerPadding;
    part.append(baseArray);
    total.append(QCryptographicHash::hash(part, QCryptographicHash::Sha1));
    QByteArray hashed = QCryptographicHash::hash(total, QCryptographicHash::Sha1);
    return hashed.toBase64();
}

QString TwitterDataTypeSyncAdaptor::authorizationHeader(int accountId, const QString &oauthToken, const QString &oauthTokenSecret, const QString &requestMethod, const QString &requestUrl, const QList<QPair<QString, QString> > &parameters)
{
    Q_UNUSED(accountId);

    // Twitter requires all requests to be signed with an authorization header.
    QString key = consumerKey();
    QString secret = consumerSecret();

    if (key.isEmpty() || secret.isEmpty()) {
        return QString();
    }

    QString oauthNonce = QString::fromLatin1(QUuid::createUuid().toByteArray().toBase64());
    QString oauthSignature;
    QString oauthSigMethod = QLatin1String("HMAC-SHA1");
    QString oauthTimestamp = QString::number(qFloor(QDateTime::currentMSecsSinceEpoch() / 1000.0));
    //QString oauthToken; // already passed in as parameter.
    QString oauthVersion = QLatin1String("1.0");

    // now build up the encoded parameters map.  We use a map to perform alphabetical sorting.
    QMap<QString, QString> encodedParams;
    encodedParams.insert(QUrl::toPercentEncoding(QLatin1String("oauth_consumer_key")),
                         QUrl::toPercentEncoding(key));
    encodedParams.insert(QUrl::toPercentEncoding(QLatin1String("oauth_nonce")),
                         QUrl::toPercentEncoding(oauthNonce));
    encodedParams.insert(QUrl::toPercentEncoding(QLatin1String("oauth_signature_method")),
                         QUrl::toPercentEncoding(oauthSigMethod));
    encodedParams.insert(QUrl::toPercentEncoding(QLatin1String("oauth_timestamp")),
                         QUrl::toPercentEncoding(oauthTimestamp));
    encodedParams.insert(QUrl::toPercentEncoding(QLatin1String("oauth_token")),
                         QUrl::toPercentEncoding(oauthToken));
    encodedParams.insert(QUrl::toPercentEncoding(QLatin1String("oauth_version")),
                         QUrl::toPercentEncoding(oauthVersion));
    for (int i = 0; i < parameters.size(); ++i) {
        QPair<QString, QString> param = parameters.at(i);
        encodedParams.insert(QUrl::toPercentEncoding(param.first),
                             QUrl::toPercentEncoding(param.second));
    }

    QString parametersString;
    QStringList keys = encodedParams.keys();
    foreach (const QString &key, keys) {
        parametersString += key + QLatin1String("=") + encodedParams.value(key) + QLatin1String("&");
    }
    parametersString.chop(1);

    QString signatureBaseString = requestMethod.toUpper() + QLatin1String("&")
                                + QUrl::toPercentEncoding(requestUrl) + QLatin1String("&")
                                + QUrl::toPercentEncoding(parametersString);

    QString signingKey = QUrl::toPercentEncoding(secret) + QLatin1String("&")
                       + QUrl::toPercentEncoding(oauthTokenSecret);

    oauthSignature = hmacSha1(signingKey, signatureBaseString);
    encodedParams.insert(QUrl::toPercentEncoding(QLatin1String("oauth_signature")),
                         QUrl::toPercentEncoding(oauthSignature));

    // now generate the Authorization header from the encoded parameters map.
    // we need to remove the query items from the encoded parameters map first.
    QString authHeader = QLatin1String("OAuth ");
    for (int i = 0; i < parameters.size(); ++i) {
        QPair<QString, QString> param = parameters.at(i);
        encodedParams.remove(QUrl::toPercentEncoding(param.first));
    }
    keys = encodedParams.keys();
    foreach (const QString &key, keys) {
        authHeader += key + QLatin1String("=\"") + encodedParams.value(key) + QLatin1String("\", ");
    }
    authHeader.chop(2);
    return authHeader;
}

QDateTime TwitterDataTypeSyncAdaptor::parseTwitterDateTime(const QString &tdt)
{
    // Twitter use the following format ddd MMM dd hh:mm:ss +0000 yyyy
    // The +0000 should always be +0000 since it relates to UTC time
    // We are using it like that but it might break if Twitter change their
    // API or if +0000 is not constant.
    // Twitter use english in their date, so we need to use an english
    // locale to parse the date

    QLocale locale (QLocale::English, QLocale::UnitedStates);
    QDateTime time = locale.toDateTime(tdt, "ddd MMM dd HH:mm:ss +0000 yyyy");
    time.setTimeSpec(Qt::UTC);

    return time;
}

void TwitterDataTypeSyncAdaptor::loadConsumerKeyAndSecret()
{
    m_triedLoading = true;
    char *cConsumerKey = NULL;
    char *cConsumerSecret = NULL;
    int ckSuccess = SailfishKeyProvider_storedKey("twitter", "twitter-sync", "consumer_key", &cConsumerKey);
    int csSuccess = SailfishKeyProvider_storedKey("twitter", "twitter-sync", "consumer_secret", &cConsumerSecret);

    if (ckSuccess != 0 || cConsumerKey == NULL || csSuccess != 0 || cConsumerSecret == NULL) {
        SOCIALD_LOG_INFO("No valid OAuth2 keys found");
        return;
    }

    m_consumerKey = QLatin1String(cConsumerKey);
    m_consumerSecret = QLatin1String(cConsumerSecret);
    free(cConsumerKey);
    free(cConsumerSecret);
}

void TwitterDataTypeSyncAdaptor::setCredentialsNeedUpdate(Accounts::Account *account)
{
    qWarning() << "sociald:Twitter: setting CredentialsNeedUpdate to true for account:" << account->id();
    Accounts::Service srv(m_accountManager->service(syncServiceName()));
    account->selectService(srv);
    account->setValue(QStringLiteral("CredentialsNeedUpdate"), QVariant::fromValue<bool>(true));
    account->setValue(QStringLiteral("CredentialsNeedUpdateFrom"), QVariant::fromValue<QString>(QString::fromLatin1("sociald-twitter")));
    account->selectService(Accounts::Service());
    account->syncAndBlock();
}

void TwitterDataTypeSyncAdaptor::signIn(Accounts::Account *account)
{
    // Fetch consumer key and secret from keyprovider
    QString key = consumerKey();
    QString secret = consumerSecret();
    int accountId = account->id();
    if (!checkAccount(account) || key.isEmpty() || secret.isEmpty()) {
        decrementSemaphore(accountId);
        return;
    }

    // grab out a valid identity for the sync service.
    Accounts::Service srv(m_accountManager->service(syncServiceName()));
    account->selectService(srv);
    SignOn::Identity *identity = account->credentialsId() > 0 ? SignOn::Identity::existingIdentity(account->credentialsId()) : 0;
    if (!identity) {
        SOCIALD_LOG_ERROR("account" << accountId << "has no valid credentials, cannot sign in");
        decrementSemaphore(accountId);
        return;
    }

    Accounts::AccountService accSrv(account, srv);
    QString method = accSrv.authData().method();
    QString mechanism = accSrv.authData().mechanism();
    SignOn::AuthSession *session = identity->createSession(method);
    if (!session) {
        SOCIALD_LOG_ERROR("could not create signon session for account" << accountId);
        identity->deleteLater();
        decrementSemaphore(accountId);
        return;
    }

    QVariantMap signonSessionData = accSrv.authData().parameters();
    signonSessionData.insert("ConsumerKey", key);
    signonSessionData.insert("ConsumerSecret", secret);
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

void TwitterDataTypeSyncAdaptor::signOnError(const SignOn::Error &error)
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

void TwitterDataTypeSyncAdaptor::signOnResponse(const SignOn::SessionData &responseData)
{
    QVariantMap data;
    foreach (const QString &key, responseData.propertyNames()) {
        data.insert(key, responseData.getProperty(key));
    }

    QString oauthToken;
    QString oauthTokenSecret;
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession*>(sender());
    Accounts::Account *account = session->property("account").value<Accounts::Account*>();
    SignOn::Identity *identity = session->property("identity").value<SignOn::Identity*>();
    int accountId = account->id();

    if (data.contains(QLatin1String("AccessToken"))) {
        oauthToken = data.value(QLatin1String("AccessToken")).toString();
    } else {
        SOCIALD_LOG_INFO("signon response for account with id" << accountId << "contained no oauth token");
    }

    if (data.contains(QLatin1String("TokenSecret"))) {
        oauthTokenSecret = data.value(QLatin1String("TokenSecret")).toString();
    } else {
        SOCIALD_LOG_INFO("signon response for account with id" << accountId << "contained no oauth token secret");
    }

    session->disconnect(this);
    identity->destroySession(session);
    identity->deleteLater();
    account->deleteLater();

    if (!oauthToken.isEmpty() && !oauthTokenSecret.isEmpty()) {
        beginSync(accountId, oauthToken, oauthTokenSecret); // call the derived-class sync entrypoint.
    }

    decrementSemaphore(accountId);
}
