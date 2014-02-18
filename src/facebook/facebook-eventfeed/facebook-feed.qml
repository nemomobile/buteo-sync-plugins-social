import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.socialcache 1.0
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import "shared"

SocialMediaFeedPage {
    id: page

    property string clientId
    property bool readyToPopulate: connectedToNetwork
                                     && account.status === Account.Synced
                                     && facebook.initialized
                                     && facebook.accessToken.length > 0
    property bool _needToPerformSignIn
    property string _nextUserId

    configKey: "/saifish/events_view/facebook_notifications"
    timestampRole: FacebookNotificationsModel.Timestamp
    listModel: FacebookNotificationsModel {
        id: facebookNotificationsModel
    }

    //: Facebook service name
    //% "Facebook notifications"
    headerTitle: qsTrId("lipstick-jolla-home-la-facebook_notifications")
    listDelegate: FacebookNotificationItem {
        id: feedItem
        connectedToNetwork: page.connectedToNetwork
        width: page.width
        Component.onCompleted: feedItem.refreshTimeCount = Qt.binding(function() { return page.refreshTimeCount })
        onClicked: {
            page.clientId = model.clientId

             switch (model.appId) {
                 case "Feed Comments":
                 case "Likes":
                 case "Wall":
                     var ids = model.object.split("_")
                     if ((ids.length <= 1 && model.appId !== "Likes")
                          || model.object === "") {
                         // Object is not given or this is combined notification
                         // (eg. "He, she and three others posted to your wall...")
                         // Open generic page instead
                         openGenericNotification(model, subviewModel)
                     } else {
                         openPost(model, subviewModel, ids[0])
                     }
                     break
                 case "Events":
                     openEvent(model, subviewModel)
                     break
                 case "Photos":
                     openPhoto(model, subviewModel)
                     break
                 default:
                     openGenericNotification(model, subviewModel)
                     break
             }
        }
    }
    socialNetwork: SocialSync.Facebook
    dataType: SocialSync.Notifications
    syncNotifications: true

    onConnectedToNetworkChanged: {
        if (_needToPerformSignIn) {
            account.performSignIn()
        }
    }

    onReadyToPopulateChanged: {
        if (readyToPopulate) {
            facebookUser.nodeIdentifier = page._nextUserId
        }
    }

    Account {
        id: account
        function performSignIn() {
            if (!page.connectedToNetwork) {
                page._needToPerformSignIn = true
                return
            }

            page._needToPerformSignIn = false
            if (status === Account.Initialized && identifier !== -1) {
                facebook.accessToken = ""
                // Sign in, and get access token.
                var params = signInParameters("facebook-sync")
                params.setParameter("ClientId", page.clientId)
                params.setParameter("UiPolicy", SignInParameters.NoUserInteractionPolicy)
                signIn("Jolla", "Jolla", params)
            }
        }

        function setIdentifiers(accountId) {
            if (accountId !== identifier) {
                identifier = accountId
                performSignIn()
            } else {
                if (page.readyToPopulate) {
                    facebookUser.nodeIdentifier = page._nextUserId
                }
            }
        }

        onIdentifierChanged: performSignIn()
        onStatusChanged: performSignIn()
        onErrorChanged: console.log("Facebook account error: " + error + "\n")

        onSignInResponse: {
            facebook.accessToken = data["AccessToken"]
            facebookMe.repopulate()
        }
    }

    Facebook {
        id: facebook
    }

    SocialNetworkModel {
        id: facebookMe

        property string avatar: node && node.picture ? node.picture.url : ""

        filters: [ ContentItemTypeFilter { type: Facebook.UserPicture } ]
        socialNetwork: facebook
        nodeIdentifier: "me"
        onErrorChanged: console.log("Facebook me network model error: " + error + "\n")
        onErrorMessageChanged: console.log("Facebook me network model error message: " + errorMessage + "\n")
    }

    SocialNetworkModel {
        id: facebookUser
        filters: [ ContentItemTypeFilter { type: Facebook.UserPicture } ]
        socialNetwork: facebook
        onNodeIdentifierChanged: if (nodeIdentifier !== "") repopulate()
        onErrorChanged: console.log("Facebook user network model error: " + error + "\n")
        onErrorMessageChanged: console.log("Facebook user network model error message: " + errorMessage + "\n")
    }

    PullDownMenu {
        flickable: page.listView
        busy: page.updating

        MenuItem {
            //: Opens Facebook in browser
            //% "Go to Facebook"
            text: qsTrId("lipstick-jolla-home-me-go_to_facebook")
            onClicked: Qt.openUrlExternally("https://m.facebook.com")
        }

        MenuItem {
            //: Update social media feed
            //% "Update"
            text: qsTrId("lipstick-jolla-home-me-update_feed")
            onClicked: page.sync()
        }
    }

    function openGenericNotification(model, subviewModel) {
        page._nextUserId = model.from
        if (page._nextUserId !== facebookUser.nodeIdentifier) {
            facebookUser.clean()
            facebookUser.nodeIdentifier = ""
        }

        try {
            rv = pageStack.push(Qt.resolvedUrl("FacebookGenericNotificationPage.qml"), {
                                    "model": model,
                                    "subviewModel": subviewModel,
                                    "account": account,
                                    "facebook": facebook,
                                    "facebookMe": facebookMe,
                                    "facebookUser": facebookUser,
                                    "connectedToNetwork": Qt.binding(function() { return page.connectedToNetwork }),
                                    "readyToPopulate": Qt.binding(function() { return page.readyToPopulate })
                                }, false)
        } catch (error) {
            console.log(error)
        }
    }

    function openPost(model, subviewModel, userId) {
        if (model.accounts[0] !== account.identifier) {
            // account is going to change, clear the old access token.
            facebook.accessToken = ""
        }

        page._nextUserId = userId
        if (page._nextUserId !== facebookUser.nodeIdentifier) {
            facebookUser.clean()
            facebookUser.nodeIdentifier = ""
        }

        var rv = null
        try {
            rv = pageStack.push(Qt.resolvedUrl("FacebookPostPage.qml"), {
                                    "model": model,
                                    "subviewModel": subviewModel,
                                    "account": account,
                                    "facebook": facebook,
                                    "facebookMe": facebookMe,
                                    "facebookUser": facebookUser,
                                    "connectedToNetwork": Qt.binding(function() { return page.connectedToNetwork }),
                                    "readyToPopulate": Qt.binding(function() { return page.readyToPopulate })
                                }, false)

        } catch (error) {
            console.log(error)
        }
    }

    function openPhoto(model, subviewModel) {
        try {
            rv = pageStack.push(Qt.resolvedUrl("FacebookPicturePage.qml"), {
                                    "model": model,
                                    "subviewModel": subviewModel,
                                    "account": account,
                                    "facebook": facebook,
                                    "facebookMe": facebookMe,
                                    "facebookUser": facebookUser,
                                    "connectedToNetwork": Qt.binding(function() { return page.connectedToNetwork }),
                                    "readyToPopulate": Qt.binding(function() { return page.readyToPopulate })
                                }, false)
        } catch (error) {
            console.log(error)
        }
    }

    function openEvent(model, subviewModel) {
        facebookUser.clean()
        facebookUser.nodeIdentifier = ""

        try {
            rv = pageStack.push(Qt.resolvedUrl("FacebookEventPage.qml"), {
                                    "model": model,
                                    "subviewModel": subviewModel,
                                    "account": account,
                                    "facebook": facebook,
                                    "facebookMe": facebookMe,
                                    "facebookUser": facebookUser,
                                    "bodyText": model.title,
                                    "connectedToNetwork": Qt.binding(function() { return page.connectedToNetwork }),
                                    "readyToPopulate": Qt.binding(function() { return page.readyToPopulate })
                                }, false)
        } catch (error) {
            console.log(error)
        }
    }
}
