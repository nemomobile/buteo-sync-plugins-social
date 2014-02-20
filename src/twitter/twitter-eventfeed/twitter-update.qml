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
            nofityFailure()
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
            notifyFailure()
        }

        onStatusChanged: {
            if (status === Twitter.Idle && updater._posted) {
                updater.postRequestDone(updater)
            }
        }
    }

    function notifyFailure() {
        publishNotification("x-nemo.social.twitter.tweet",
                            "\"" + updater.statusUpdate + "\"",
                            //: Notification text for failure when posting a Twitter tweet.
                            //% "Failed to post a tweet"
                            qsTrId("lipstick-jolla-home-la-failed_to_post_tweet"))
        postRequestError(updater)
    }

    Component.onCompleted: {
        signInParams = updater.account.signInParameters("twitter-microblog")
        signInParams.setParameter("ConsumerKey", twitter.consumerKey)
        signInParams.setParameter("ConsumerSecret", twitter.consumerSecret)
        account.performSignIn()
    }
}
