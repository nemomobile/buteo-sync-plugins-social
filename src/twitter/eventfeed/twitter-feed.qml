import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Accounts 1.0
import org.nemomobile.social 1.0
import org.nemomobile.socialcache 1.0
import "shared"

SocialMediaFeedPage {
    id: page

    property QtObject twitterUser
    property bool _needToPerformSignIn
    property string _nextNodeIdentifier
    property bool accountAndModelsReady: connectedToNetwork
                                           && account.status === Account.Synced
                                           && twitter.credentialsReady

    configKey: "/sailfish/events_view/twitter"
    timestampRole: 4 //TwitterPostsModel.Timestamp
    listModel: TwitterPostsModel {
        id: twitterPostsModel
    }
    socialNetwork: SocialSync.Twitter
    syncNotifications: true
    //: Twitter service name
    //% "Twitter"
    headerTitle: qsTrId("lipstick-jolla-home-la-twitter")
    listDelegate: TwitterFeedItem {
        id: feedItem
        connectedToNetwork: page.connectedToNetwork
        width: ListView.view.width
        imageList: model.images
        onClicked: {
            var rv = null
            try {
                twitter.consumerKey = model.consumerKey
                twitter.consumerSecret = model.consumerSecret
                page._nextNodeIdentifier = model.twitterId
                twitter.nodeIdentifier = ""
                twitterReplies.clean()

                rv = pageStack.push(Qt.resolvedUrl("TwitterPostPage.qml"), {
                                        "model": model,
                                        "subviewModel": subviewModel,
                                        "account": account,
                                        "twitterUser": Qt.binding(function() { return page.twitterUser }),
                                        "twitterReplies": twitterReplies,
                                        "connectedToNetwork": Qt.binding(function() { return page.connectedToNetwork })
                                    }, false)
            } catch (error) {
                console.log(error)
            }
        }
        Component.onCompleted: feedItem.refreshTimeCount = Qt.binding(function() { return page.refreshTimeCount })
    }

    onConnectedToNetworkChanged: {
        if (_needToPerformSignIn) {
            account.performSignIn()
        }
    }

    onAccountAndModelsReadyChanged: {
        if (accountAndModelsReady) {
            twitter.nodeIdentifier = page._nextNodeIdentifier
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
            if (status == Account.Initialized && identifier != -1) {
                // Reset token
                twitter.oauthToken = ""
                twitter.oauthTokenSecret = ""
                twitter.userId = ""

                // Sign in, and get credentials.
                var params = signInParameters("twitter-sync")
                params.setParameter("ConsumerKey", twitter.consumerKey)
                params.setParameter("ConsumerSecret", twitter.consumerSecret)
                params.setParameter("UiPolicy", SignInParameters.NoUserInteractionPolicy)
                signIn("Jolla", "Jolla", params)
            }
        }

        function setIdentifiers(accountId) {
            if (accountId !== identifier) {
                twitter.nodeIdentifier = ""
                identifier = accountId
                performSignIn()
            } else {
                twitter.nodeIdentifier = page._nextNodeIdentifier
            }
        }

        onIdentifierChanged: performSignIn()
        onStatusChanged: performSignIn()
        onErrorChanged: console.log("Twitter account error: " + error + "\n")

        onSignInResponse: {
            var accessTok = data["AccessToken"]
            if (accessTok !== "") {
                twitter.oauthToken = accessTok
            }
            var tokenSec = data["TokenSecret"]
            if (tokenSec !== "") {
                twitter.oauthTokenSecret = tokenSec
            }
            var userId = data["UserId"]
            if (userId !== "") {
                twitter.userId = userId
            }
        }
    }

    Twitter {
        id: twitter

        property string userId
        property bool credentialsReady
        property string nodeIdentifier

        onInitializedChanged: populateIfInitialized()
        onOauthTokenChanged: populateIfInitialized()
        onOauthTokenSecretChanged: populateIfInitialized()
        onUserIdChanged: populateIfInitialized()

        // Seems that creating a property with 5 checks is buggy,
        // so we do it as a function
        function checkCredentialsReady() {
            if (initialized
                  && consumerKey.length > 0
                  && consumerSecret.length > 0
                  && oauthToken.length > 0
                  && userId.length > 0
                  && oauthTokenSecret.length > 0) {
                credentialsReady = true
            } else {
                credentialsReady = false
            }
        }

        function populateIfInitialized() {
            checkCredentialsReady()
            if (page.connectedToNetwork && credentialsReady) {
                // Currently TwitterUser will fail if identifier is changed.
                // Work around by creating new user for each account.
                if (page.twitterUser) {
                    page.twitterUser.destroy()
                }
                page.twitterUser = twitterUserComponent.createObject()
            }
        }
    }

    Component {
        id: twitterUserComponent

        TwitterUser {
            socialNetwork: twitter
            identifier: twitter.userId
            onErrorChanged: console.log("TwitterUser error: " + error + "\n")
            onErrorMessageChanged: console.log("TwitterUser errorMessage: " + errorMessage + "\n")
        }
    }

    SocialNetworkModel {
        id: twitterReplies

        property bool ready: status === SocialNetwork.Idle && nodeIdentifier.length > 0

        filters: [ TwitterConversationFilter{} ]
        socialNetwork: twitter
        nodeIdentifier: twitter.nodeIdentifier

        onErrorChanged: console.log("Twitter network model error: " + error + "\n")
        onErrorMessageChanged: console.log("Twitter network model error message: " + errorMessage + "\n")
        onNodeIdentifierChanged: if (nodeIdentifier.length > 0) repopulate()
    }

    PullDownMenu {
        flickable: page.listView
        busy: page.updating

        MenuItem {
            //: Opens Twitter in browser
            //% "Go to Twitter"
            text: qsTrId("lipstick-jolla-home-me-go_to_twitter")
            onClicked: Qt.openUrlExternally("https://mobile.twitter.com")
        }

        MenuItem {
            //: Update social media feed
            //% "Update"
            text: qsTrId("lipstick-jolla-home-me-update_feed")
            onClicked: page.sync()
        }
    }
}
