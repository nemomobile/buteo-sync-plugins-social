import QtQuick 2.0
import org.nemomobile.social 1.0
import "shared"

SocialStatusUpdater {
    id: updater

    property bool _posted

    onSignInDataChanged: {
        var accessTok = signInData["AccessToken"]
        if (accessTok !== "") {
            twitter.oauthToken = accessTok
        }
        var tokenSec = signInData["TokenSecret"]
        if (tokenSec !== "") {
            twitter.oauthTokenSecret = tokenSec
        }
        if (twitter.oauthToken === "" || twitter.oauthTokenSecret === "") {
            updater.postRequestError(posted)
        }
    }

    Twitter {
        id: twitter

        property bool credentialsReady: initialized
                                          && consumerKey.length > 0
                                          && consumerSecret.length > 0
                                          && oauthToken.length > 0
                                          && oauthTokenSecret.length > 0

        consumerKey: keyProvider.storedKey("twitter", "twitter-microblog", "consumer_key")
        consumerSecret: keyProvider.storedKey("twitter", "twitter-microblog", "consumer_secret")

        onCredentialsReadyChanged: {
            updater._posted = true
            user.uploadTweet(updater.statusUpdate)
        }
    }

    TwitterUser {
        id: user
        socialNetwork: twitter

        onErrorChanged: {
            console.log("TwitterUser error: " + error + "\n")
            updater.postRequestError(updater)
        }

        onStatusChanged: {
            if (status === Twitter.Idle && updater._posted) {
                // success
                updater.postRequestDone(updater)
            }
        }
    }

    Component.onCompleted: {
        signInParams = updater.account.signInParameters("twitter-microblog")
        signInParams.setParameter("ConsumerKey", twitter.consumerKey)
        signInParams.setParameter("ConsumerSecret", twitter.consumerSecret)
        account.performSignIn()
    }
}
