import QtQuick 2.0
import "shared"

SocialStatusUpdater {
    id: updater

    onSignInDataChanged: {
        // FIXME: Done with XMLHttpRequest for now. Start using social API
        // as soon as it is available.
        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    updater.postRequestDone(updater)
                } else {
                    updater.notifyFailure()
                }
            }
        }

        var postData = "access_token=" +
                       signInData["AccessToken"] +
                       "&message="+updater.statusUpdate

        var url = "https://graph.facebook.com/me/feed"
        doc.open("POST", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }

    function notifyFailure() {
        publishNotification("x-nemo.social.facebook.statuspost",
                            "\"" + updater.statusUpdate + "\"",
                            //: Notification text for failure when posting facebook status.
                            //% "Failed to post Facebook status"
                            qsTrId("lipstick-jolla-home-la-failed_to_post_facebook_status"))
        postRequestDone(updater)
    }

    Component.onCompleted: {
        signInParams = updater.account.signInParameters("facebook-microblog")
        signInParams.setParameter("ClientId", updater.keyProvider.storedKey("facebook", "facebook-microblog", "client_id"))
        account.performSignIn()
    }
}
