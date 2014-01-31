import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.lipstick 0.1
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import "shared"

Page {
    id: container

    property Item subviewModel
    property variant model
    property string retweeter
    property bool connectedToNetwork
    property string avatarSource
    property string fallbackAvatarSource

    property Account account
    property QtObject twitterUser
    property SocialNetworkModel twitterReplies

    allowedOrientations: Lipstick.compositor.eventsWindowOrientation

    onModelChanged: retweeter = model.retweeter

    Component.onCompleted: view.checkContinueLoading()

    Connections {
        target: container.twitterUser
        onStatusChanged: view.checkContinueLoading()
    }

    Connections {
        target: container.twitterReplies
        onStatusChanged: view.checkContinueLoading()
    }

    onTwitterUserChanged: view.checkContinueLoading()
    onTwitterRepliesChanged: view.checkContinueLoading()

    Connections {
        target: container.twitterReplies.node
        onStatusChanged: {
            if (container.twitterReplies.node.status === SocialNetwork.Idle) {
                if (view.state === "favoriting"
                     || view.state === "retweeting"
                     || view.state === "unretweeting") {
                    view.state = "idle"
                } else if (view.state === "replying") {
                    view.state = "idle"
                }
            } else if (container.twitterReplies.node.status === SocialNetwork.Error) {
                // We simply display an error in the status, but
                // continue the flow to the "idle" state.
                if (view.state === "favoriting") {
                    view.state = "idle"
                    //% "Failed to favorite"
                    view.error = qsTrId("lipstick-jolla-home-twitter-error-fail-to-favorite")
                } else if (view.state === "retweeting") {
                    view.state = "idle"
                    //% "Failed to retweet"
                    view.error = qsTrId("lipstick-jolla-home-twitter-error-fail-to-retweet")
                } else if (view.state === "unretweeting") {
                    view.state = "idle"
                    //% "Failed to remove the retweet"
                    view.error = qsTrId("lipstick-jolla-home-twitter-error-fail-to-unretweet")
                } else if (view.state === "replying") {
                    view.state = "idle"
                    //% "Failed to reply"
                    view.error = qsTrId("lipstick-jolla-home-twitter-error-fail-to-reply")
                }
            }
        }
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

        function checkContinueLoading() {
            if (view.state !== "") {
                if (view.state === "loadingModel"
                      && container.twitterReplies.ready
                      && container.twitterUser.status === SocialNetwork.Idle) {
                    view.state = "idle"
                    view.updateInfoLabelAndProperties()
                }
                return
            }

            if (container.twitterUser
                  && container.twitterReplies
                  && container.twitterUser.status === SocialNetwork.Idle) {
                if (!container.twitterReplies.ready) {
                    view.state = "loadingModel"
                } else {
                    view.state = "idle"
                    view.updateInfoLabelAndProperties()
                }
            }
        }

        function updateInfoLabelAndProperties() {
            info = ""

            var tweet = container.twitterReplies.node

            if (!tweet) {
                return
            }

            //% "%n retweets"
            var retweet = qsTrId("lipstick-jolla-home-twitter-retweets", tweet.retweetCount)
            //% "%n favourited"
            var favourited = qsTrId("lipstick-jolla-home-twitter-favourited", tweet.favoriteCount)
            if (tweet.retweetCount > 0 && tweet.favoriteCount > 0) {
                //% "%1 and %2"
                info += qsTrId("lipstick-jolla-home-twitter-retweets-favourited-link").arg(retweet).arg(favourited)
            } else if (tweet.retweetCount > 0 && tweet.favoriteCount === 0) {
                info += retweet
            } else if (tweet.retweetCount === 0 && tweet.favoriteCount > 0) {
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
        model: container.twitterReplies
        spacing: Theme.paddingLarge

        // These states are used to manage the complex flow in NQPS
        states: [
            State { name: "loadingModel" },         // Step between not loaded and loaded, used to display info before replies.
            State { name: "idle" },                 // Default state
            State { name: "replying" },             // Start a reply operation
            State { name: "favoriting" },           // Start a favoriting operation
            State { name: "retweeting" },           // Start a retweeting operation
            State { name: "unretweeting" }          // Start a retweet cancelling operation
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
                from: "replying"
                ScriptAction {
                    script: {
                        view.clearReplyField()
                        container.twitterReplies.loadNext()
                    }
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
            Transition {
                from: "unretweeting"
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
                connectedToNetwork: container.connectedToNetwork
                avatar: container.avatarSource
                fallbackAvatar: container.fallbackAvatarSource
                source: container.model.name
                subSource: "@" + container.model.screenName
                parentPage: container
                timestamp: model.timestamp
                body: model.body
                fullRowSocialButtons: Item {
                    anchors {
                        left: parent.left
                        right: parent.right
                    }
                    height: favorite.height

                    SocialButton {
                        id: retweetButton
                        connectedToNetwork: container.connectedToNetwork
                        anchors.verticalCenter: parent.verticalCenter
                        enabled: view.state === "idle"
                        onClicked: {
                            if (!view.retweeted) {
                                view.state = "retweeting"
                                container.twitterReplies.node.uploadRetweet()
                            } else {
                                view.state = "unretweeting"
                                container.twitterReplies.node.removeRetweet()
                            }
                        }
                        icon: "image://theme/icon-m-sync?"
                              + (down ? Theme.highlightColor : Theme.primaryColor)

                        //% "Retweet"
                        text: !view.retweeted ? qsTrId("lipstick-jolla-home-twitter-la-retweet")
                                                //% "Remove retweet"
                                              : qsTrId("lipstick-jolla-home-twitter-la-unretweet")
                    }

                    SocialButton {
                        connectedToNetwork: container.connectedToNetwork
                        anchors {
                            left: retweetButton.right
                            leftMargin: Theme.paddingMedium
                            verticalCenter: parent.verticalCenter
                        }
                        enabled: view.state === "idle"
                        icon: "image://theme/icon-m-chat?"
                              + (down ? Theme.highlightColor : Theme.primaryColor)

                        //: A button for activating twitter reply field
                        //% "Reply"
                        text: qsTrId("lipstick-jolla-home-twitter-la-reply")
                        onClicked: view.forceReplyFieldActiveFocus()
                    }

                    MouseArea {
                        id: favorite

                        property bool down: pressed && containsMouse

                        enabled: view.state === "idle"
                        anchors {
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                        }
                        width: icon.width
                        height: icon.width + 2 * Theme.paddingLarge
                        onClicked: {
                            view.state = "favoriting"
                            if (view.favorited) {
                                container.twitterReplies.node.unfavorite()
                            } else {
                                container.twitterReplies.node.favorite()
                            }
                        }
                        Image {
                            id: icon
                            anchors.centerIn: parent
                            opacity: favorite.enabled ? 1 : 0.5
                            source: (!view.favorited ? "image://theme/icon-m-favorite?"
                                                     : "image://theme/icon-m-favorite-selected?")
                                    + (favorite.down ? Theme.highlightColor : Theme.primaryColor)
                            asynchronous: true
                        }
                    }
                }
            }

            SocialMediaRow {
                id: mediaRow
                imageList: container.model.images
                connectedToNetwork: container.connectedToNetwork
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
                    asynchronous: true
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
                    font.pixelSize: Theme.fontSizeSmall
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
            connectedToNetwork: container.connectedToNetwork
            visible: connectedToNetwork
            enabled: view.state === "idle"
            avatar: container.twitterUser ? container.twitterUser.profileImageUrlHttps : ""
            //: Label indicating text field is used for entering a reply to Twitter post
            //% "Reply (%0)"
            label: qsTrId("lipstick-jolla-home-twitter-la-reply-field").arg(text.length)
            displayMargins: container.twitterReplies.count > 0
            //: Write twitter reply
            //% "Write a reply"
            placeholderText: qsTrId("lipstick-jolla-home-twitter-ph-write-reply")
            allowComment: view.state === "idle"
            errorHighlight: text.length > 140
            onEnterKeyClicked: {
                if (text.length > 0) {
                    view.state = "replying"
                    container.twitterReplies.node.uploadReply("@" + container.twitterReplies.node.user.screenName + " " + replyField.text)
                }
                replyField.close()
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
                    + Format.formatDate(model.contentItem.createdAt, Format.DurationElapsed)
            extraVisible: false
            connectedToNetwork: container.connectedToNetwork
        }

        VerticalScrollDecorator {}

        SocialAccountPullDownMenu {
            id: socialAccountPullDown
            pageContainer: container.pageContainer
            onCurrentAccountChanged: container.account.setIdentifiers(currentAccount)
            //% "Select account"
            selectAccountString: qsTrId("lipstick-jolla-home-la-select-account")
            //% "Change to %1"
            changeToAccountString: qsTrId("lipstick-jolla-home-la-change-to-account")
            //% "Account: %1"
            accountString: qsTrId("lipstick-jolla-home-la-account-name")

            // We should not set the identifier of account when we are syncing or signin in
            switchEnabled: account.status !== Account.SigningIn
                             && account.status !== Account.SyncInProgress
            serviceName: "twitter"
        }
    }
}
