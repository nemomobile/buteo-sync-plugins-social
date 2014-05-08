import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Accounts 1.0
import org.nemomobile.social 1.0
import org.nemomobile.socialcache 1.0
import com.jolla.settings.accounts 1.0
import "shared"

SocialMediaFeedPage {
    id: page

    property bool _needToPerformSignIn
    property string accessToken
    property string currentUserAvatar
    property string currentUserUid

    configKey: "/sailfish/events_view/vkontakte"
    timestampRole: VKFeedModel.Timestamp
    listModel: VKFeedModel {}
    sectionProperty: "type"

    //: VK service name
    //% "VK"
    headerTitle: qsTrId("lipstick-jolla-home-la-vk_service_name")

    listDelegate: VKFeedItem {
        id: feedItem        
        body: model.type === "post" ? model.body : notificationString(model.notificationType)
        connectedToNetwork: page.connectedToNetwork
        width: page.width
        imageList: model.images
        avatarSource: model.icon
        Component.onCompleted: feedItem.refreshTimeCount = Qt.binding(function() { return page.refreshTimeCount })
        onClicked: {
            if (model.type === "notification") {
                var parentObj = JSON.parse(model.parent)
                if (parentObj.post_type !== "undefined" && parentObj.post_type === "post") {
                    console.log("HERE WE ARE")
                    var rv = null
                    try {
                        rv = pageStack.push(Qt.resolvedUrl("VKPostPage.qml"), {
                                                "vkId": parentObj.id,
                                                "model": model,
                                                "subviewModel": subviewModel,
                                                "account": account,
                                                "accessToken": Qt.binding(function() { return page.accessToken }),
                                                "currentUserAvatar": Qt.binding(function() { return page.currentUserAvatar}),
                                                "currentUserUid": Qt.binding(function() { return page.currentUserUid}),
                                                "connectedToNetwork": Qt.binding(function() { return page.connectedToNetwork })
                                            }, false)
                    } catch (error) {
                        console.log(error)
                    }
                }
            } else {
                var rv = null
                try {
                    rv = pageStack.push(Qt.resolvedUrl("VKPostPage.qml"), {
                                            "vkId": model.vkId,
                                            "model": model,
                                            "subviewModel": subviewModel,
                                            "account": account,
                                            "accessToken": Qt.binding(function() { return page.accessToken }),
                                            "currentUserAvatar": Qt.binding(function() { return page.currentUserAvatar}),
                                            "currentUserUid": Qt.binding(function() { return page.currentUserUid}),
                                            "connectedToNetwork": Qt.binding(function() { return page.connectedToNetwork })
                                        }, false)
                } catch (error) {
                    console.log(error)
                }
            }
        }
    }
    socialNetwork: SocialSync.VK
    dataType: SocialSync.Posts

    StoredKeyProvider {
        id: keyProvider
    }

    Account {
        id: account
        function performSignIn() {
            if (!page.connectedToNetwork) {
                page._needToPerformSignIn = true
                return
            }

            console.log("performSignIn: identifier = " + identifier)

            page._needToPerformSignIn = false
            if (status === Account.Initialized && identifier != -1) {
                page.currentUserAvatar = ""

                // Sign in, and get credentials.
                var params = signInParameters("vk-sync")
                params.setParameter("ClientId", keyProvider.storedKey("vk", "vk-microblog", "client_id"))
                params.setParameter("ClientSecret", keyProvider.storedKey("vk", "vk-microblog", "client_secret"))
                params.setParameter("ResponseType", "token")
                params.setParameter("UiPolicy", SignInParameters.NoUserInteractionPolicy)
                signIn("Jolla", "Jolla", params)
            }
        }

        function setIdentifiers(accountId) {
            if (accountId !== identifier) {
                identifier = accountId
                performSignIn()
            }
        }

        onIdentifierChanged: performSignIn()
        onStatusChanged: performSignIn()
        onErrorChanged: console.log("VK account error: " + error + "\n")

        onSignInResponse: {
            page.accessToken = data["AccessToken"];
            page.loadUserData()
        }
    }

    PullDownMenu {
        flickable: page.listView
        busy: page.updating

        MenuItem {
            //: Opens VK in browser
            //% "Go to VK"
            text: qsTrId("lipstick-jolla-home-me-go_to_vk")
            onClicked: Qt.openUrlExternally("https://m.vk.com")
        }

        MenuItem {
            //: Update social media feed
            //% "Update"
            text: qsTrId("lipstick-jolla-home-me-update_feed")
            onClicked: page.sync()
        }
    }

    function loadUserData() {
        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    var data = JSON.parse(doc.responseText)
                    if (data.response.length > 0) {
                        var person = data.response[0]
                        page.currentUserAvatar = person.photo
                        page.currentUserUid = person.uid
                    }
                } else {
                    console.log("VK feed: failed to query current user profile")
                }
            }
        }

        var postData = "access_token=" + accessToken
        var url = "https://api.vk.com/method/getProfiles?fields=photo&access_token=" + accessToken
        doc.open("GET", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }

    function notificationString(type) {
        switch (type) {
        case "follow":
            //% "follows you"
            return qsTrId("lipstick-jolla-home-vk-la-notif_follow")
        case "friend_accepted":
            //% "is now your friend"
            return qsTrId("lipstick-jolla-home-vk-la-notif_friend_accepted")
        case "mention":
            //% "mentioned you"
            return qsTrId("lipstick-jolla-home-vk-la-notif_mention")
        case "mention_comments":
            //% "mentioned you"
            return qsTrId("lipstick-jolla-home-vk-la-notif_mention_comments")
        case "wall":
            //% "posted to your wall"
            return qsTrId("lipstick-jolla-home-vk-la-notif_wall")
        case "comment_post":
            //% "commented your post"
            return qsTrId("lipstick-jolla-home-vk-la-notif_comment_post")
        case "comment_photo":
            //% "commented your photo"
            return qsTrId("lipstick-jolla-home-vk-la-notif_comment_photo")
        case "comment_video":
            //% "commented your video"
            return qsTrId("lipstick-jolla-home-vk-la-notif_comment_video")
        case "reply_comment":
        case "reply_comment_photo":
        case "reply_comment_video":
            //% "replied to your comment"
            return qsTrId("lipstick-jolla-home-vk-la-notif_reply_comment")
        case "reply_topic":
            return ""
        case "like_post":
            //% "likes your post"
            return qsTrId("lipstick-jolla-home-vk-la-notif_like_post")
        case "like_comment":
            //% "likes your comment"
            return qsTrId("lipstick-jolla-home-vk-la-notif_like_comment")
        case "like_photo":
            //% "likes your photo"
            return qsTrId("lipstick-jolla-home-vk-la-notif_like_photo")
        case "like_video":
            //% "likes your video"
            return qsTrId("lipstick-jolla-home-vk-la-notif_like_video")
        case "like_comment_photo":
        case "like_comment_video":
        case "like_comment_topic":
        case "copy_post":
        case "copy_photo":
        case "copy_video":
        default:
           return ""
        }
    }

    function sectionHeader(section) {
        if (section === "post") {
            //% "Posts"
            return qsTrId("lipstick-jolla-home-vk-la-section_posts")
        } else if (section === "notification") {
            //% "Notifications"
            return qsTrId("lipstick-jolla-home-vk-la-section_notifications")
        }

        return ""
    }
}
