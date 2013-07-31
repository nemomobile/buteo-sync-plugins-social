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
        commentAvatar.source = container.model.metaData["profilePicture"]
    }

    Account {
        identifier: container.model != null ? container.model.metaData["accountId"] : -1
        onStatusChanged: {
            if (status == Account.Initialized) {
                // Sign in, and get credentials.
                var params = signInParameters("twitter-sync")
                params.setParameter("ConsumerKey", twitter.consumerKey)
                params.setParameter("ConsumerSecret", twitter.consumerSecret)
                params.setParameter("UiPolicy", 2) // NoUserInteractionPolicy - XXX TODO: define this in SailfishAccounts
                signIn("Jolla", "Jolla", params)
            }
        }

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
        property bool credentialsReady: false

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
                if (tweet.identifier != container.nodeIdentifier) {
                    tweet.identifier = container.nodeIdentifier
                } else {
                    if (tweet.status == SocialNetwork.Idle) {
                        tweet.reload()
                    }
                }
            }
        }
    }

    TwitterTweet {
        id: tweet
        property bool retweeting: false
        socialNetwork: twitter
        onStatusChanged: {
            if (status == SocialNetwork.Idle) {
                if (retweeting) {
                    retweeting = false
                    reload()
                    return
                }
                rtAndFavouritedContainer.updateRtAndFavourited()
            }
        }
    }

    Column {
        width: container.width
        Item {
            height: childrenRect.height
            anchors {
                left: parent.left
                right: parent.right
            }

            Rectangle {
                anchors.fill: parent
                color: Theme.highlightColor
                opacity: 0.1
            }

            PageHeader {
                id: header
                title: container.model.sourceDisplayName
            }

            Face {
                id: face
                anchors.top: header.bottom
                icon: container.model.icon
            }

            Column {
                anchors {
                    top: header.bottom
                    left: face.right
                    leftMargin: Theme.paddingMedium
                    right: parent.right
                    rightMargin: Theme.paddingLarge
                }

                Body {
                    text: model.body
                    time: formatter.formatDate(model.timestamp, Formatter.DurationElapsed)
                }

                Item {
                    anchors {
                        left: parent.left
                        right: parent.right
                    }
                    height: childrenRect.height

                    SocialButton {
                        property bool retweeted: tweet.retweeted
                        anchors.left: parent.left
                        enabled: tweet.status == Twitter.Idle && twitter.credentialsReady
                                 && !retweeted
                        onClicked: {
                            tweet.retweeting = true
                            tweet.uploadRetweet()
                        }
                        icon: "image://theme/icon-m-sync"
                        //% "Retweet"
                        text: !retweeted ? qsTrId("lipstick-jolla-home-twitter-la-retweet")
                                          //% "Retweeted"
                                          : qsTrId("lipstick-jolla-home-twitter-la-retweeted")
                    }

                    SocialButton {
                        anchors.right: parent.right
                        enabled: tweet.status == Twitter.Idle && twitter.credentialsReady
                        icon: "image://theme/icon-m-chat"
                        //% "Reply"
                        text: qsTrId("lipstick-jolla-home-twitter-la-reply")
                        onClicked: {
                            // TODO: give focus to commentField
                        }
                    }
                }
            }
        }

        MediaRow {
            id: mediaRow
            imageList: container.model.imageList
        }

        Item {
            id: rtAndFavouritedContainer
            function updateRtAndFavourited() {
                //% "%n retweets"
                var rt = qsTrId("lipstick-jolla-home-twitter-retweets", tweet.retweetCount)
                //% "%n favourited"
                var favourited = qsTrId("lipstick-jolla-home-twitter-favourited", tweet.favoriteCount)
                if (tweet.retweetCount > 0 && tweet.favoriteCount > 0) {
                    //% "%1 and %2"
                    rtAndFavourited = qsTrId("lipstick-jolla-home-twitter-rt-favourited-link").arg(rt).arg(favourited)
                } else if (tweet.retweetCount > 0 && tweet.favoriteCount <= 0) {
                    rtAndFavourited = rt
                } else if (tweet.retweetCount <= 0 && tweet.favoriteCount > 0) {
                    rtAndFavourited = favourited
                } else {
                    rtAndFavourited = ""
                }
            }

            property string rtAndFavourited: ""

            anchors {
                left: parent.left
                right: parent.right
            }

            height: rtAndFavourited != "" ? (rtAndFavouritedLabel.height + 2 * Theme.paddingLarge)
                                          : Theme.paddingLarge
            opacity: rtAndFavourited != "" ? 1 : 0

            Label {
                id: rtAndFavouritedLabel
                anchors {
                    left: parent.left
                    leftMargin: Theme.paddingMedium
                    right: parent.right
                    rightMargin: Theme.paddingMedium
                    verticalCenter: parent.verticalCenter
                }
                text: rtAndFavouritedContainer.rtAndFavourited
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Behavior on opacity { FadeAnimation {} }
            Behavior on height { FadeAnimation { property: "height" } }
        }

        Item {
            anchors {
                left: parent.left
                right: parent.right
            }

            height: commentContainer.height  + Theme.paddingMedium
            opacity: tweet.status == Twitter.Idle && twitter.credentialsReady ? 1 : 0
            Behavior on opacity { FadeAnimation {} }

            Item {
                id: commentContainer
                height: childrenRect.height
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    bottomMargin: Theme.paddingMedium
                }

                Image {
                    id: commentAvatar
                    width: Theme.iconSizeMedium
                    height: Theme.iconSizeMedium
                }

                TextField {
                    id: commentField
                    anchors {
                        left: commentAvatar.right
                        right: parent.right
                    }

                    //% "Write a reply"
                    placeholderText: qsTrId("lipstick-jolla-home-twitter-ph-write-reply")
    //                enabled: facebookComments.status == Facebook.Idle

                    EnterKey.onClicked: {
    //                    facebookComments.node.uploadComment(comment.text)
    //                    facebookComments.repopulate()
                    }

    //                Connections {
    //                    target: facebookComments
    //                    onStatusChanged: {
    //                        if (facebookComments.status == Facebook.Idle) {
    //                            commentField.text = ""
    //                        }
    //                    }
    //                }
                }
            }
        }
    }
}
