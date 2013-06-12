/*
 * Copyright (C) 2013 Jolla Ltd. <chris.adams@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "twitterdatatypesyncadaptor.h"
#include "twittersyncadaptor.h"
#include "trace.h"

#include <QtCore/QVariantMap>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QUuid>
#include <QtCore/qmath.h>

#include <QCryptographicHash>

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <qjson/parser.h>
#else
#include <QJsonDocument>
#endif

//libsailfishkeyprovider
#include <sailfishkeyprovider.h>

//libaccounts-qt
#include <Accounts/Manager>
#include <Accounts/Service>
#include <Accounts/Account>
#include <Accounts/AccountService>
#include <Accounts/AuthData>

//libsignon-qt
#include <SignOn/Identity>
#include <SignOn/SessionData>
#include <SignOn/AuthSession>

Q_DECLARE_METATYPE(SignOn::Identity*)

TwitterDataTypeSyncAdaptor::TwitterDataTypeSyncAdaptor(SyncService *parent, TwitterSyncAdaptor *tsa, SyncService::DataType dataType)
    : SocialNetworkSyncAdaptor(parent)
    , m_tsa(tsa)
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
        return;
    }

    // three stage process.
    // 1) if an account has been removed, we need to purge the data we retrieved with it
    // 2) if an account has been added, we need to pull data for the account
    // 3) for existing accounts, pull new data for the existing account

    QList<int> newIds, purgeIds, updateIds;
    m_tsa->checkAccounts(m_dataType, &newIds, &purgeIds, &updateIds);
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
        Accounts::Account *act = m_tsa->m_accountManager->account(accountId);
        if (!act) {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: existing account with id %1 couldn't be retrieved"))
                    .arg(accountId));
            continue;
        }

        // grab out a valid identity for the sync service.
        Accounts::ServiceList enabledSrvs = act->enabledServices();
        if (!enabledSrvs.size()) {
            TRACE(SOCIALD_INFORMATION,
                    QString(QLatin1String("account with id %1 has no enabled sync service"))
                    .arg(accountId));
            continue;
        }

        quint32 identityId = 0;
        Accounts::AccountService *asrv = 0;
        for (int i = 0; i < enabledSrvs.size(); ++i) {
            asrv = new Accounts::AccountService(act, enabledSrvs.at(i));
            if (!asrv) {
                continue;
            }
            identityId = asrv->authData().credentialsId();
            if (identityId != 0) {
                break;
            }

            asrv->deleteLater();
            asrv = 0;
        }

        if (identityId == 0) {
            TRACE(SOCIALD_INFORMATION,
                    QString(QLatin1String("account with id %1 has no valid credentials"))
                    .arg(accountId));
            continue;
        }

        SignOn::Identity *ident = SignOn::Identity::existingIdentity(identityId);
        if (!ident) {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: credentials for account with id %1 couldn't be retrieved"))
                    .arg(accountId));
            continue;
        }

        // we need the access token to perform requests.
        // set UiPolicy to NO_USER_INTERACTION because we don't want
        // to show any UI if we don't already have a token.
        Accounts::AuthData authData(asrv->authData());
        asrv->deleteLater();
        SignOn::AuthSession *session = ident->createSession(authData.method());
        QVariantMap sessionData = authData.parameters();
        sessionData.insert(QLatin1String("UiPolicy"), SignOn::NoUserInteractionPolicy);
        QVariant identVar = QVariant::fromValue<SignOn::Identity*>(ident);
        session->setProperty("ident", identVar);
        session->setProperty("accountId", accountId);
        connect(session, SIGNAL(error(SignOn::Error)), this, SLOT(signOnError(SignOn::Error)));
        connect(session, SIGNAL(response(SignOn::SessionData)), this, SLOT(signOnResponse(SignOn::SessionData)));
        session->process(sessionData, authData.mechanism());
    }
}

void TwitterDataTypeSyncAdaptor::signOnError(const SignOn::Error &err)
{
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession *>(sender());
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: credentials for account with id %1 couldn't be retrieved:"))
            .arg(session->property("accountId").toInt()) << err.message());
    SignOn::Identity *ident = session->property("ident").value<SignOn::Identity*>();
    ident->destroySession(session); // XXX: is this safe?  Does it deleteLater()?
    ident->deleteLater();
}

void TwitterDataTypeSyncAdaptor::signOnResponse(const SignOn::SessionData &sdata)
{
    QVariantMap data;
    QStringList sdpns = sdata.propertyNames();
    foreach (const QString &sdpn, sdpns) {
        data.insert(sdpn, sdata.getProperty(sdpn));
    }    

    QString oauthToken;
    QString oauthTokenSecret;
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession *>(sender());
    int accountId = static_cast<int>(session->property("accountId").toUInt());

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

    SignOn::Identity *ident = session->property("ident").value<SignOn::Identity*>();
    ident->destroySession(session); // XXX: is this safe?  Does it deleteLater()?
    ident->deleteLater();

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
    char *cConsumerKey = NULL;
    char *cConsumerSecret = NULL;
    int ckSuccess = SailfishKeyProvider_storedKey("twitter", "twitter-sync", "consumer_key", &cConsumerKey);
    int csSuccess = SailfishKeyProvider_storedKey("twitter", "twitter-sync", "consumer_secret", &cConsumerSecret);

    if (ckSuccess != 0 || cConsumerKey == NULL || csSuccess != 0 || cConsumerSecret == NULL) {
        qWarning() << Q_FUNC_INFO << "No valid OAuth2 keys found";
        return QString();
    }

    QString consumerSecret = QLatin1String(cConsumerSecret);
    QString oauthConsumerKey = QLatin1String(cConsumerKey);

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
    // format of created_at: "DDD MMM dd hh:mm:ss +tttt yyyy"
    QStringList parts = tdt.split(' ');
    if (parts.count() != 6) {
        return QDateTime(); // invalid
    }

    QString modTdt = parts.at(1) + QLatin1String(" ")
                   + parts.at(2) + QLatin1String(" ")
                   + parts.at(3) + QLatin1String(" ")
                   + parts.at(5);

    QDateTime retn = QDateTime::fromString(modTdt, "MMM dd hh:mm:ss yyyy");
    retn.setTimeSpec(Qt::UTC);
    return retn;
}

QVariant TwitterDataTypeSyncAdaptor::parseReplyData(const QByteArray &replyData, bool *ok)
{
    QVariant parsed;

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    QJson::Parser jsonParser;
    parsed = jsonParser.parse(replyData, ok);
#else
    QJsonDocument jsonDocument = QJsonDocument::fromJson(replyData);
    *ok = !doc.isEmpty();
    parsed = doc.toVariant();
#endif

    if (*ok && parsed.type() == QVariant::Map) {
        return parsed.toMap();
    } else if (*ok && parsed.type() == QVariant::List) {
        return parsed.toList();
    }

    *ok = false;
    return QVariantMap();
}
