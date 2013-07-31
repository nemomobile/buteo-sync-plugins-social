/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
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

// sailfish-components-accounts-qt5
#include <accountmanager.h>
#include <account.h>
#include <signinparameters.h>

//libsignon-qt: SignOn::NoUserInteractionPolicy
#include <SignOn/SessionData>

TwitterDataTypeSyncAdaptor::TwitterDataTypeSyncAdaptor(SyncService *syncService, SyncService::DataType dataType, QObject *parent)
    : SocialNetworkSyncAdaptor("twitter", syncService, parent)
    , m_dataType(dataType)
{
}

TwitterDataTypeSyncAdaptor::~TwitterDataTypeSyncAdaptor()
{
}

void TwitterDataTypeSyncAdaptor::sync(const QString &dataType)
{
    if (dataType != SyncService::dataType(m_dataType)) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: Twitter %1 sync adaptor was asked to sync %2"))
                .arg(SyncService::dataType(m_dataType)).arg(dataType));
        changeStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    // three stage process.
    // 1) if an account has been removed, we need to purge the data we retrieved with it
    // 2) if an account has been added, we need to pull data for the account
    // 3) for existing accounts, pull new data for the existing account

    QList<int> newIds, purgeIds, updateIds;
    checkAccounts(m_dataType, &newIds, &purgeIds, &updateIds);
    purgeDataForOldAccounts(purgeIds); // call the derived-class purge entrypoint.
    updateDataForAccounts(newIds);
    updateDataForAccounts(updateIds);

    TRACE(SOCIALD_DEBUG,
            QString(QLatin1String("successfully triggered sync of %1: %2 purged, %3 new, %4 updated accounts"))
            .arg(SyncService::dataType(m_dataType)).arg(purgeIds.size()).arg(newIds.size()).arg(updateIds.size()));
}

void TwitterDataTypeSyncAdaptor::updateDataForAccounts(const QList<int> &accountIds)
{
    foreach (int accountId, accountIds) {
        Account *account = m_accountManager->account(accountId);
        if (!account) {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: existing account with id %1 couldn't be retrieved"))
                    .arg(accountId));
            continue;
        }
        if (account->status() == Account::Initialized || account->status() == Account::Synced) {
            signIn(account);
        } else {
            connect(account, SIGNAL(statusChanged()), this, SLOT(accountStatusChangeHandler()));
        }
    }
}

void TwitterDataTypeSyncAdaptor::accountStatusChangeHandler()
{
    Account *account = qobject_cast<Account*>(sender());
    if (account->status() == Account::Initialized || account->status() == Account::Synced)
    {
        // Not anymore interested about status changes of this account instance
        account->disconnect(this);
        signIn(account);
    }
}

void TwitterDataTypeSyncAdaptor::signOnError(const QString &err)
{
    Account *account = qobject_cast<Account*>(sender());
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: credentials for account with id %1 couldn't be retrieved:"))
          .arg(account->identifier()) << err);
    account->disconnect(this);
    changeStatus(SocialNetworkSyncAdaptor::Error);
}

void TwitterDataTypeSyncAdaptor::signOnResponse(const QVariantMap &data)
{
    QString oauthToken;
    QString oauthTokenSecret;
    Account *account = qobject_cast<Account*>(sender());
    int accountId = account->identifier();

    if (data.contains(QLatin1String("AccessToken"))) {
        oauthToken = data.value(QLatin1String("AccessToken")).toString();
    } else {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("signon response for account with id %1 contained no oauth token"))
                .arg(accountId));
    }

    if (data.contains(QLatin1String("TokenSecret"))) {
        oauthTokenSecret = data.value(QLatin1String("TokenSecret")).toString();
    } else {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("signon response for account with id %1 contained no oauth token secret"))
                .arg(accountId));
    }

    account->disconnect(this);
    if (!oauthToken.isEmpty() && !oauthTokenSecret.isEmpty()) {
        beginSync(accountId, oauthToken, oauthTokenSecret); // call the derived-class sync entrypoint.
    }
}

void TwitterDataTypeSyncAdaptor::errorHandler(QNetworkReply::NetworkError err)
{
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: %1 request with account %2 experienced error: %3"))
            .arg(SyncService::dataType(m_dataType)).arg(sender()->property("accountId").toInt()).arg(err));
    // the error is an incomprehensible enum value, but that doesn't matter to users.
    changeStatus(SocialNetworkSyncAdaptor::Error);
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
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: %1 request with account %2 experienced ssl errors: %3"))
            .arg(SyncService::dataType(m_dataType)).arg(sender()->property("accountId").toInt()).arg(sslerrs));
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
    // Twitter requires all requests to be signed with an authorization header.
    QString consumerSecret;
    QString oauthConsumerKey;
    if (!consumerKeyAndSecret(oauthConsumerKey, consumerSecret)) {
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
                         QUrl::toPercentEncoding(oauthConsumerKey));
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

    QString signingKey = QUrl::toPercentEncoding(consumerSecret) + QLatin1String("&")
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

QVariant TwitterDataTypeSyncAdaptor::parseReplyData(const QByteArray &replyData, bool *ok)
{
    QVariant parsed;
    QJsonDocument jsonDocument = QJsonDocument::fromJson(replyData);
    *ok = !jsonDocument.isEmpty();
    parsed = jsonDocument.toVariant();
    if (*ok && parsed.type() == QVariant::Map) {
        return parsed.toMap();
    } else if (*ok && parsed.type() == QVariant::List) {
        return parsed.toList();
    }

    *ok = false;
    return QVariantMap();
}

bool TwitterDataTypeSyncAdaptor::consumerKeyAndSecret(QString &consumerKey, QString &consumerSecret)
{
    char *cConsumerKey = NULL;
    char *cConsumerSecret = NULL;
    int ckSuccess = SailfishKeyProvider_storedKey("twitter", "twitter-sync", "consumer_key", &cConsumerKey);
    int csSuccess = SailfishKeyProvider_storedKey("twitter", "twitter-sync", "consumer_secret", &cConsumerSecret);

    if (ckSuccess != 0 || cConsumerKey == NULL || csSuccess != 0 || cConsumerSecret == NULL) {
        TRACE(SOCIALD_INFORMATION, QLatin1String("No valid OAuth2 keys found"));
        return false;
    }

    consumerKey = QLatin1String(cConsumerKey);
    consumerSecret = QLatin1String(cConsumerSecret);
    free(cConsumerKey);
    free(cConsumerSecret);
    return true;
}

void TwitterDataTypeSyncAdaptor::signIn(Account *account)
{
    // grab out a valid identity for the sync service.
    if (!account->isEnabledWithService("twitter-sync")) {
        TRACE(SOCIALD_INFORMATION,
              QString(QLatin1String("account with id %1 has no enabled sync service"))
              .arg(account->identifier()));
        return;
    }

    // Fetch consumer key and secret from keyprovider
    QString oauthConsumerKey;
    QString consumerSecret;
    if (!consumerKeyAndSecret(oauthConsumerKey, consumerSecret)) {
        return;
    }

    SignInParameters *sip = account->signInParameters("twitter-sync");
    sip->setParameter(QLatin1String("ConsumerKey"), oauthConsumerKey);
    sip->setParameter(QLatin1String("ConsumerSecret"), consumerSecret);
    sip->setParameter(QLatin1String("UiPolicy"), SignInParameters::NoUserInteractionPolicy);

    connect(account, SIGNAL(signInError(QString)), this, SLOT(signOnError(QString)));
    connect(account, SIGNAL(signInResponse(QVariantMap)), this, SLOT(signOnResponse(QVariantMap)));
    account->signIn("Jolla", "Jolla", sip);
}
