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
    property int accountCount
    property int accountIndex

    onModelChanged: {
        nodeIdentifier = container.model.metaData["nodeId"]
        twitter.consumerKey = container.model.metaData["consumerKey"]
        twitter.consumerSecret = container.model.metaData["consumerSecret"]
        retweeter = container.model.metaData["retweeter"]

        accountCount = container.model.metaData["accountIdCount"]
        account.identifier = container.model.metaData["accountId0"]
        replyField.avatar = container.model.metaData["profilePicture0"]
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
        property bool retweeting
        property bool favoriting
        property bool replying
        property bool replied
        socialNetwork: twitter
        onStatusChanged: {
            if (status == SocialNetwork.Idle) {
                if (retweeting) {
                    retweeting = false
                    reload()
                    return
                }
                if (favoriting) {
                    favoriting = false
                    reload()
                    return
                }
                if (replying) {
                    replying = false
                    replied = true
                    reload()
                    replyField.clear()
                    return
                }

                infoLabel.updateInfoLabel()
            } else if (status == SocialNetwork.Error) {
                // This happens when a RT, favorite or reply failed to be posted
                if (retweeting) {
                    //% "Failed to Retweet this Tweet"
                    infoLabel.text = qsTrId("lipstick-jolla-home-twitter-la-retweet-failure")
                    return
                }

                if (favoriting) {
                    //% "Failed to favorite this Tweet"
                    infoLabel.text = qsTrId("lipstick-jolla-home-twitter-la-favorite-failure")
                    return
                }

                if (replying) {
                    //% "Failed to reply to this Tweet"
                    infoLabel.text = qsTrId("lipstick-jolla-home-twitter-la-reply-failure")
                    return
                }
            }
        }
    }

    SilicaFlickable {
        id: flickable
        anchors.fill: parent
        contentHeight: column.height

        SocialAccountPullDownMenu {
            pageContainer: container.pageContainer
            metaData: container.model.metaData
            onCurrentAccountChanged: account.identifier = currentAccount
            onCurrentAccountIndexChanged: replyField.avatar = model.metaData["profilePicture" + currentAccountIndex]
            //% "Select account"
            selectAccountString: qsTrId("lipstick-jolla-home-la-select-account")
            //% "Change to %1"
            changeToAccountString: qsTrId("lipstick-jolla-home-la-change-to-account")
            //% "Account: %1"
            accountString: qsTrId("lipstick-jolla-home-la-account-name")
        }

        Column {
            id: column
            width: flickable.width

            SocialContent {
                avatar: container.model.icon
                source: container.model.title
                timestamp: model.timestamp
                body: model.body
                // The commented code below might still be useful, to display the
                // "follow" button if we finally decide to keep it.
                /*
                belowAvatar: MouseArea {
                    id: followButton
                    anchors.fill: parent

                    Rectangle {
                        id: followButtonBackground
                        anchors {
                            fill: followButtonLabel
                            margins: -Theme.paddingSmall
                        }
                        color: !followButton.pressed ? Theme.rgba(Theme.primaryColor, 0.2)
                                                     : Theme.rgba(Theme.highlightColor, 0.3)
                    }

                    Label {
                        id: followButtonLabel
                        anchors.centerIn: parent
                        font.pixelSize: Theme.fontSizeExtraSmall
                        //% "Follow"
                        text: qsTrId("lipstick-jolla-home-twitter-la-follow")
                        color: !followButton.pressed ? Theme.primaryColor : Theme.highlightColor
                    }
                }
                */
                belowAvatar: MouseArea {
                    id: favoriteButton
                    property bool favorited: tweet.favorited
                    enabled: tweet.status == Twitter.Idle && twitter.credentialsReady
                    anchors.centerIn: parent
                    width: icon.width + 2 * Theme.paddingLarge
                    height: icon.width + 2 * Theme.paddingLarge
                    onClicked: {
                        tweet.favoriting = true
                        if (favorited) {
                            tweet.unfavorite()
                        } else {
                            tweet.favorite()
                        }
                    }
                    Image {
                        id: icon
                        anchors.centerIn: parent
                        opacity: favoriteButton.enabled ? 1 : 0.5
                        source: !favoriteButton.favorited ? "image://theme/icon-m-favorite"
                                                          : "image://theme/icon-m-favorite-selected"
                    }
                }
                socialButtons: Item {
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
                        onClicked: replyField.forceActiveFocus()
                    }
                }
            }

            SocialMediaRow {
                id: mediaRow
                imageList: container.model.imageList
            }

            SocialInfoLabel {
                id: infoLabel
                function updateInfoLabel() {
                    //% "%n retweets"
                    var retweet = qsTrId("lipstick-jolla-home-twitter-retweets", tweet.retweetCount)
                    //% "%n favourited"
                    var favourited = qsTrId("lipstick-jolla-home-twitter-favourited", tweet.favoriteCount)
                    if (tweet.retweetCount > 0 && tweet.favoriteCount > 0) {
                        //% "%1 and %2"
                        text = qsTrId("lipstick-jolla-home-twitter-retweets-favourited-link").arg(retweet).arg(favourited)
                    } else if (tweet.retweetCount > 0 && tweet.favoriteCount == 0) {
                        text = retweet
                    } else if (tweet.retweetCount == 0 && tweet.favoriteCount > 0) {
                        text = favourited
                    } else {
                        text = ""
                    }

                    if (tweet.replied) {
                        if (text.length > 0) {
                            text += "\n"
                        }
                        //% "You replied to this Tweet"
                        text += qsTrId("lipstick-jolla-home-twitter-replied-uploaded")
                    }
                }
            }

            SocialReplyField {
                id: replyField
                enabled: !tweet.replying && twitter.credentialsReady
                //% "Write a reply"
                placeholderText: qsTrId("lipstick-jolla-home-twitter-ph-write-reply")
                allowComment: tweet.status == Facebook.Idle
                onEnterKeyClicked: {
                    tweet.uploadReply("@" + tweet.user.screenName + " " + text)
                    tweet.replying = true
                }
            }
        }
    }

}
