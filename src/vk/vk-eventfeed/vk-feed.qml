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
    timestampRole: VKPostsModel.Timestamp
    listModel: VKPostsModel {
        id: vkPostsModel
    }

    //: VK service name
    //% "VK"
    headerTitle: qsTrId("lipstick-jolla-home-la-vk_service_name")

    listDelegate: VKFeedItem {
        id: feedItem
        connectedToNetwork: page.connectedToNetwork
        width: page.width
        imageList: model.images
        avatarSource: model.icon
        Component.onCompleted: feedItem.refreshTimeCount = Qt.binding(function() { return page.refreshTimeCount })
        onClicked: {
            var rv = null
            try {
                rv = pageStack.push(Qt.resolvedUrl("VKPostPage.qml"), {
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
}
