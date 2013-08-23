import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.contacts 1.0
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import "shared"

Page {
    id: container

    property variant model
    property string nodeIdentifier
    property string retweeter

    onModelChanged: {
        nodeIdentifier = container.model.metaData["nodeId"]
        twitter.consumerKey = container.model.metaData["consumerKey"]
        twitter.consumerSecret = container.model.metaData["consumerSecret"]
        retweeter = container.model.metaData["retweeter"]
    }

    Account {
        id: account
        function performSignIn() {
            if (status == Account.Initialized && identifier != -1) {
                // Reset token
                twitter.oauthToken = ""
                twitter.oauthTokenSecret = ""

                // Sign in, and get credentials.
                var params = signInParameters("twitter-sync")
                params.setParameter("ConsumerKey", twitter.consumerKey)
                params.setParameter("ConsumerSecret", twitter.consumerSecret)
                params.setParameter("UiPolicy", SignInParameters.NoUserInteractionPolicy)
                signIn("Jolla", "Jolla", params)
            }
        }
        identifier: -1

        onStatusChanged: performSignIn()
        onIdentifierChanged: performSignIn()

        onSignInResponse: {
            var accessTok = data["AccessToken"]
            if (accessTok != "") {
                twitter.oauthToken = accessTok
            }
            var tokenSec = data["TokenSecret"]
            if (tokenSec != "") {
                twitter.oauthTokenSecret = tokenSec
            }
        }
    }

    Twitter {
        id: twitter
        onInitializedChanged: populateIfInitialized()
        onConsumerKeyChanged: populateIfInitialized()
        onConsumerSecretChanged: populateIfInitialized()
        onOauthTokenChanged: populateIfInitialized()
        onOauthTokenSecretChanged: populateIfInitialized()
        property bool credentialsReady

        // Seems that creating a propert with 4 checks is buggy,
        // so we do it as a function
        function checkCredentialsReady() {
            if (initialized && (consumerKey.length > 0)
                && (consumerSecret.length > 0) && (oauthToken.length > 0)
                && (oauthTokenSecret.length > 0)) {
                credentialsReady = true
            } else {
                credentialsReady = false
            }
        }

        function populateIfInitialized() {
            checkCredentialsReady()
            if (credentialsReady) {
                twitterReplies.repopulate()
            }
        }
    }

    SocialNetworkModel {
        id: twitterReplies
        filters: [ TwitterConversationFilter{} ]
        socialNetwork: twitter
        nodeIdentifier: container.nodeIdentifier
        onNodeChanged: {
            if (node == null) {
                return
            }

            if (view.state == "") {
                view.state = "loadingModel"
            }
        }

        onStatusChanged: {
            if (twitterReplies.status == SocialNetwork.Idle) {
                if (view.state == "loadingModel"
                    || view.state == "reloadingModel") {
                    view.state = "idle"
                }
            }
        }
    }

    Connections {
        target: twitterReplies.node
        onStatusChanged: {
            if (twitterReplies.node.status == SocialNetwork.Idle) {
                if (view.state == "favoriting" || view.state == "retweeting") {
                    view.state = "idle"
                } else if (view.state == "replying") {
                    view.state = "reloadingModel"
                } else if (view.state == "reloadingNode") {
                    view.state = "idle"
                }
            } else if (twitterReplies.node.status == SocialNetwork.Error) {
                // We simply display an error in the status, but
                // continue the flow to the "idle" state.
                if (view.state == "favoriting") {
                    view.state = "idle"
                    //% "Failed to favorite"
                    view.error = qsTrId("lipstick-jolla-home-twitter-error-fail-to-favorite")
                } else if (view.state == "retweeting") {
                    view.state = "idle"
                    //% "Failed to retweet"
                    view.error = qsTrId("lipstick-jolla-home-twitter-error-fail-to-retweet")
                } else if (view.state == "replying") {
                    view.state = "reloadingModel"
                    //% "Failed to reply"
                    view.error = qsTrId("lipstick-jolla-home-twitter-error-fail-to-reply")
                }
            }
        }
    }

    Formatter {
        id: formatter
    }

    SilicaListView {
        id: view
        property string info
        property bool favorited
        property bool retweeted
        property string error
        onErrorChanged: updateInfoLabelAndProperties()

        // Signal relays
        signal forceReplyFieldActiveFocus()
        signal clearReplyField()

        function updateInfoLabelAndProperties() {
            info = ""

            var tweet = twitterReplies.node

            //% "%n retweets"
            var retweet = qsTrId("lipstick-jolla-home-twitter-retweets", tweet.retweetCount)
            //% "%n favourited"
            var favourited = qsTrId("lipstick-jolla-home-twitter-favourited", tweet.favoriteCount)
            if (tweet.retweetCount > 0 && tweet.favoriteCount > 0) {
                //% "%1 and %2"
                info += qsTrId("lipstick-jolla-home-twitter-retweets-favourited-link").arg(retweet).arg(favourited)
            } else if (tweet.retweetCount > 0 && tweet.favoriteCount == 0) {
                info += retweet
            } else if (tweet.retweetCount == 0 && tweet.favoriteCount > 0) {
                info += favourited
            }

            if (view.error.length > 0) {
                if (info.length > 0) {
                    info += "\n"
                }
                info += view.error
            }

            // Update favorited
            favorited = tweet.favorited
            retweeted = tweet.retweeted
        }

        anchors.fill: parent
        model: twitterReplies
        spacing: Theme.paddingLarge

        // These states are used to manage the complex flow in NQPS
        states: [
            State { name: "loadingModel" },         // Step between not loaded and loaded, used to display info before replies.
            State { name: "idle" },                 // Default state
            State { name: "reloadingModel" },       // After replying, the model is being reloaded
            State { name: "replying" },             // Start a reply operation
            State { name: "favoriting" },           // Start a favoriting operation
            State { name: "retweeting" }            // Start a retweeting operation
        ]

        transitions: [
            // Transition from non-initialized state to idle
            // (we should display the info label when
            // the view is initialized)
            Transition {
                from: ""
                to: "loadingModel"
                ScriptAction {
                    script: view.updateInfoLabelAndProperties()
                }
            },
            Transition {
                from: "reloadingModel"
                ScriptAction {
                    script: view.clearReplyField()
                }
            },
            Transition {
                from: "replying"
                to: "reloadingModel"
                ScriptAction {
                    script: twitterReplies.loadNext()
                }
            },
            Transition {
                from: "favoriting"
                to: "idle"
                ScriptAction {
                    script: view.updateInfoLabelAndProperties()
                }
            },
            Transition {
                from: "retweeting"
                to: "idle"
                ScriptAction {
                    script: view.updateInfoLabelAndProperties()
                }
            },
            // We reset the error field when
            // we do anything
            Transition {
                from: "idle"
                ScriptAction {
                    script: view.error = ""
                }
            }

        ]

        header: Column {
            id: column
            width: view.width

            SocialContent {
                avatar: container.model.icon
                source: container.model.title
                timestamp: model.timestamp
                body: model.body
                fullRowSocialButtons: Item {
                    anchors {
                        left: parent.left
                        right: parent.right
                    }
                    height: childrenRect.height

                    Item {
                        anchors {
                            left: parent.left
                            right: favorite.left
                            verticalCenter: parent.verticalCenter
                        }
                        height: childrenRect.height

                        SocialButton {
                            anchors {
                                left: parent.left
                                right: parent.horizontalCenter
                            }
                            enabled: view.state == "idle" && !view.retweeted
                            onClicked: {
                                view.state = "retweeting"
                                twitterReplies.node.uploadRetweet()
                            }
                            icon: "image://theme/icon-m-sync"
                            //% "Retweet"
                            text: !view.retweeted ? qsTrId("lipstick-jolla-home-twitter-la-retweet")
                                                    //% "Retweeted"
                                                  : qsTrId("lipstick-jolla-home-twitter-la-retweeted")
                        }

                        SocialButton {
                            anchors {
                                left: parent.horizontalCenter
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                            }
                            enabled: view.state == "idle"
                            icon: "image://theme/icon-m-chat"
                            //% "Reply"
                            text: qsTrId("lipstick-jolla-home-twitter-la-reply")
                            onClicked: view.forceReplyFieldActiveFocus()
                        }
                    }

                    MouseArea {
                        id: favorite
                        enabled: view.state == "idle"
                        anchors {
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                        }
                        width: icon.width
                        height: icon.width + 2 * Theme.paddingLarge
                        onClicked: {
                            view.state = "favoriting"
                            if (view.favorited) {
                                twitterReplies.node.unfavorite()
                            } else {
                                twitterReplies.node.favorite()
                            }
                        }
                        Image {
                            id: icon
                            anchors.centerIn: parent
                            opacity: favorite.enabled ? 1 : 0.5
                            source: !view.favorited ? "image://theme/icon-m-favorite"
                                                    : "image://theme/icon-m-favorite-selected"
                        }
                    }
                }
            }

            SocialMediaRow {
                id: mediaRow
                imageList: container.model.imageList
            }

            // Label for retweet
            Item {
                visible: container.retweeter.length > 0
                height: retweetLabel.text.length > 0 ? (retweetLabel.height + Theme.paddingLarge)
                                                     : 0
                anchors {
                    left: parent.left
                    right: parent.right
                }

                Image {
                    id: retweetIcon
                    anchors {
                        left: parent.left
                        leftMargin: Theme.paddingLarge
                        verticalCenter: retweetLabel.verticalCenter
                    }
                    source: "image://theme/icon-s-retweet"
                }

                Label {
                    id: retweetLabel
                    anchors {
                        left: retweetIcon.right
                        leftMargin: Theme.paddingMedium
                        right: parent.right
                        rightMargin: Theme.paddingLarge
                        bottom: parent.bottom
                    }
                    text: container.retweeter
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.highlightColor
                }
            }

            SocialInfoLabel {
                id: infoLabel
                text: view.info
            }
        }
        footer: SocialReplyField {
            id: replyField
            enabled: view.state == "idle"
            avatar: socialAccountPullDown.avatar
            displayMargins: twitterReplies.count > 0
            //% "Write a reply"
            placeholderText: qsTrId("lipstick-jolla-home-twitter-ph-write-reply")
            allowComment: view.state == "idle"
            onEnterKeyClicked: {
                view.state = "replying"
                twitterReplies.node.uploadReply("@" + twitterReplies.node.user.screenName + " " + text)
            }

            Connections {
                target: view
                onForceReplyFieldActiveFocus: replyField.forceActiveFocus()
                onClearReplyField: replyField.clear()
            }
        }

        delegate: SocialComment {
            width: view.width
            avatar: model.contentItem.user.profileImageUrlHttps
            message: model.contentItem.text
            footer: model.contentItem.user.name + " \u2022 "
                    + formatter.formatDate(model.contentItem.createdAt, Formatter.DurationElapsed)
            extraVisible: false
        }

        VerticalScrollDecorator {}

        SocialAccountPullDownMenu {
            id: socialAccountPullDown
            property string avatar
            pageContainer: container.pageContainer
            metaData: container.model.metaData
            onCurrentAccountChanged: {
                view.state = ""
                account.identifier = currentAccount
            }
            onCurrentAccountIndexChanged: avatar = model.metaData["profilePicture" + currentAccountIndex]
            //% "Select account"
            selectAccountString: qsTrId("lipstick-jolla-home-la-select-account")
            //% "Change to %1"
            changeToAccountString: qsTrId("lipstick-jolla-home-la-change-to-account")
            //% "Account: %1"
            accountString: qsTrId("lipstick-jolla-home-la-account-name")

            // We should not set the identifier of account when we are syncing or signin in
            switchEnabled: account.status != Account.SigningIn
                           && account.status != Account.SyncInProgress
        }
    }

}
