import QtQuick 2.0
import "shared"

SocialStatusUpdater {
    id: updater

    onSignInDataChanged: {
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

        var postData = "message="+updater.statusUpdate
        var url = "https://api.vk.com/method/wall.post?access_token=" + signInData["AccessToken"]
        doc.open("POST", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }

    function notifyFailure() {
        publishNotification("x-nemo.social.vk.statuspost",
                            "\"" + updater.statusUpdate + "\"",
                            //: Notification text for failure when posting VK status.
                            //% "Failed to post VK status"
                            qsTrId("lipstick-jolla-home-la-failed_to_post_vk_status"))
        postRequestDone(updater)
    }

    Component.onCompleted: {
        signInParams = updater.account.signInParameters("vk-microblog")
        signInParams.setParameter("ClientId", updater.keyProvider.storedKey("vk", "vk-microblog", "client_id"))
        signInParams.setParameter("ClientSecret", updater.keyProvider.storedKey("vk", "vk-microblog", "client_secret"))
        signInParams.setParameter("ResponseType", "token")
    }
}
