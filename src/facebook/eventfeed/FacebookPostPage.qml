import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.contacts 1.0
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import Sailfish.TextLinking 1.0
import "shared"

Page {
    id: container

    property variant model
    property Item subviewModel

    property string accessToken
    property string nodeIdentifier

    property bool allowLike
    property bool allowComment

    onModelChanged: {
        nodeIdentifier = container.model.facebookId
        allowLike = container.model.allowLike
        allowComment = container.model.allowComment
    }

    Account {
        id: account
        function performSign() {
            if (status == Account.Initialized && identifier != -1) {
                container.accessToken = ""
                // Sign in, and get access token.
                var params = signInParameters("facebook-sync")
                params.setParameter("ClientId", container.model.clientId)
                params.setParameter("UiPolicy", SignInParameters.NoUserInteractionPolicy)
                signIn("Jolla", "Jolla", params)
            }
        }

        onStatusChanged: performSign()
        onIdentifierChanged: performSign()
        onSignInResponse: container.accessToken = data["AccessToken"]
    }

    Facebook {
        id: facebook
        accessToken: container.accessToken
        onInitializedChanged: populateIfInitialized()
        onAccessTokenChanged: populateIfInitialized()
        function populateIfInitialized() {
            if (initialized && accessToken.length > 0) {
                facebookMe.repopulate()
                facebookLikes.repopulate()
            }
        }
    }

    SocialNetworkModel {
        id: facebookMe
        filters: [ ContentItemTypeFilter { type: Facebook.UserPicture } ]
        socialNetwork: facebook
        nodeIdentifier: "me"
        onStatusChanged: view.checkContinueLoading()
    }

    // We need (up to) first two people who liked
    // to display the "a, b and N others liked that"
    // string.
    SocialNetworkModel {
        id: facebookLikes
        filters: ContentItemTypeFilter { type: Facebook.Like; limit: 3 }
        socialNetwork: facebook
        nodeIdentifier: container.nodeIdentifier
        onStatusChanged: {
            view.checkContinueLoading()
            if (status == SocialNetwork.Idle) {
                if (view.state === "reloadingLikes") {
                    view.state = "idle"
                }
            }
        }
    }

    Connections {
        target: facebookLikes.node
        onStatusChanged: {
            if (facebookLikes.node.status == SocialNetwork.Idle) {
                if (view.state === "liking") {
                    view.state = "reloadingLikes"
                } else if (view.state === "commenting") {
                    view.state = "reloadingComments"
                }
            } else if (facebookLikes.node.status == SocialNetwork.Error) {
                // We simply display an error in the status, but
                // continue the flow to the "idle" state.
                if (view.state === "liking") {
                    view.state = "reloadingLikes"
                    //% "Failed to like"
                    view.error = !view.liked ? qsTrId("lipstick-jolla-home-facebook-error-fail-to-like")
                                               //% "Failed to remove like"
                                             : qsTrId("lipstick-jolla-home-facebook-error-fail-to-remove-like")
                } else if (view.state === "commenting") {
                    view.state = "reloadingComments"
                    //% "Failed to comment"
                    view.error = qsTrId("lipstick-jolla-home-facebook-error-fail-to-comment")
                }
            }
        }
    }

    SocialNetworkModel {
        id: facebookComments
        property bool replying
        filters: [
            FacebookCommentFilter {
                retrieveMode: FacebookCommentFilter.RetrieveLatest
                limit: 20
            }
        ]
        socialNetwork: facebook
        nodeIdentifier: container.nodeIdentifier
        onStatusChanged: {
            if (facebookComments.status == SocialNetwork.Idle) {
                if (view.state === "loadingComments"
                    || view.state === "reloadingComments") {
                    view.state = "idle"
                }
            }
        }
    }

    SilicaListView {
        id: view
        property string likers
        property bool liked
        property string error
        onErrorChanged: updateLikers()

        // Signal relays
        signal forceReplyFieldActiveFocus()
        signal clearReplyField()

        // Check point, to continue loading when both facebookMe and facebookLikes
        // got loaded
        function checkContinueLoading() {
            if (view.state !== "") {
                return
            }

            if (facebookMe.status == SocialNetwork.Idle
                && facebookLikes.status == SocialNetwork.Idle) {
                view.state = "loadingComments"
            }
        }

        // Returns string formatted e.g. "You, Mike M and 3 others like this"
        function updateLikers() {
            likers = ""

            // Not very pretty code but localization and how this message
            // is expressed requires quite many variations
            liked = facebookLikes.node.liked

            var myName = facebookMe.node.name
            var photoUserName = ""
            var users = new Array
            for (var i = 0; i < facebookLikes.count; i++) {
                if (facebookLikes.relatedItem(i).userName === myName) {
                    photoUserName = facebookLikes.relatedItem(i).userName
                } else {
                    users.push(facebookLikes.relatedItem(i).userName)
                }
            }

            if (facebookLikes.count == 1) {
                if (liked) {
                    //% "You like this"
                    likers = qsTrId("lipstick-jolla-home-facebook-la-you-like-this")
                } else {
                    //% "%1 likes this"
                    likers = qsTrId("gallery-fb-la-one-friend-likes-this").arg(users[0])
                }
            } else if (facebookLikes.count == 2) {
                if (liked) {
                    //% "You and %1 like this"
                    likers = qsTrId("lipstick-jolla-home-facebook-la-you-and-another-friend-likes-this").arg(users[0])
                } else {
                    //% "%1 and %2 like this"
                    likers = qsTrId("lipstick-jolla-home-facebook-la-two-friend-likes-this").arg(users[0]).arg(users[1])
                }
            } else if (facebookLikes.count > 2) {
                if (liked) {
                    //% "You, %1 and %2 others like this"
                    likers = qsTrId("lipstick-jolla-home-facebook-la-you-and-multiple-friend-like-this").arg(users[0]).arg(facebookLikes.node.likesCount- 1)
                } else {
                    //% "%1, %2 and %3 others like this"
                    likers = qsTrId("lipstick-jolla-home-facebook-la-multiple-friend-like-this").arg(users[0]).arg(users[1]).arg(facebookLikes.node.likesCount - 2)
                }
            }

            if (view.error.length > 0) {
                if (likers.length > 0) {
                    likers += "\n"
                }
                likers += view.error
            }
        }

        anchors.fill: parent
        model: facebookComments
        spacing: Theme.paddingLarge

        // These states are used to manage the complex flow in NQPS
        states: [
            State { name: "loadingComments" },      // Step between not loaded and loaded, used to display likes before comments.
            State { name: "idle" },                 // Default state
            State { name: "reloadingLikes" },       // After liking the likes model is being reloaded
            State { name: "reloadingComments" },    // After commenting, the comments model is being reloaded
            State { name: "liking" },               // Start a liking operation
            State { name: "commenting" }            // Start a commenting operation
        ]

        transitions: [
            // Transition from non-initialized state to idle
            // (we should display the likers when
            // the view is initialized)
            Transition {
                from: ""
                to: "loadingComments"
                ScriptAction {
                    script: {
                        view.updateLikers()
                        facebookComments.repopulate()
                    }
                }
            },
            // Transition from a node being reloaded to idle
            // (we should display the likers when
            // the node is reloaded, after a like operation)
            Transition {
                from: "reloadingLikes"
                to: "idle"
                ScriptAction {
                    script: view.updateLikers()
                }
            },

            // Transition from a node being reloaded to idle
            // (we should display do some cleanups)
            Transition {
                from: "reloadingComments"
                to: "idle"
                ScriptAction {
                    script: view.clearReplyField()
                }
            },

            Transition {
                to: "reloadingLikes"
                ScriptAction {
                    script: facebookLikes.repopulate()
                }
            },

            Transition {
                to: "reloadingComments"
                ScriptAction {
                    script: facebookComments.loadNext()
                }
            }

        ]

        header: Column {
            width: view.width

            SocialContent {
                avatar: container.model.icon
                source: container.model.name
                timestamp: model.timestamp
                body: model.body
                fullRowSocialButtons: Item {
                    anchors {
                        left: parent.left
                        right: parent.right
                    }
                    height: childrenRect.height

                    SocialButton {
                        anchors.left: parent.left
                        enabled: view.state === "idle" && container.allowLike
                        onClicked: {
                            view.state = "liking"
                            if (!view.liked) {
                                facebookLikes.node.like()
                            } else {
                                facebookLikes.node.unlike()
                            }
                        }
                        icon: "image://theme/icon-m-like"
                        //: Press button to unlike
                        //% "Unlike"
                        text: view.liked ? qsTrId("lipstick-jolla-home-facebook-la-unlike")
                                           //% "Like"
                                         : qsTrId("lipstick-jolla-home-facebook-la-like")

                    }

                    SocialButton {
                        id: commentButton
                        anchors.right: parent.right
                        enabled: view.state === "idle" && container.allowComment
                        icon: "image://theme/icon-m-chat"
                        //: Press button to write facebook comment
                        //% "Comment"
                        text: qsTrId("lipstick-jolla-home-facebook-la-comment")
                        onClicked: view.forceReplyFieldActiveFocus()
                    }
                }
            }

            SocialMediaRow {
                id: mediaRow
                imageList: container.model.images
                mediaName: container.model.attachmentName
                mediaCaption: container.model.attachmentCaption
                mediaDescription: container.model.attachmentDescription
            }

            Item {
                width: 1
                height: Theme.paddingLarge
                visible: description.visible
            }

            LinkedText {
                id: description
                anchors {
                    left: parent.left
                    leftMargin: Theme.paddingSmall
                    right: parent.right
                    rightMargin: Theme.paddingSmall
                }
                visible: text.length > 0
                elide: Text.ElideRight
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                plainText: container.model.attachmentDescription
                shortenUrl: true
            }

            SocialInfoLabel { text: view.likers }

            BackgroundItem {
                id: loadPreviousButton
                anchors {
                    left: parent.left
                    right: parent.right
                }
                visible: facebookLikes.node == null ? false
                                                    : (facebookLikes.node.commentsCount > facebookComments.count
                                                         && facebookComments.hasPrevious)
                onClicked: {
                    view.state = "loadingComments"
                    facebookComments.loadPrevious()
                }

                Label {
                    anchors {
                        left: parent.left
                        leftMargin: Theme.paddingMedium
                        right: parent.right
                        rightMargin: Theme.paddingMedium
                        verticalCenter: parent.verticalCenter
                    }
                    //: Load previous comments
                    //% "Load previous comments"
                    text: qsTrId("lipstick-jolla-home-facebook-la-load-previous-comments")
                    color: loadPreviousButton.highlighted ? Theme.highlightColor
                                                          : Theme.primaryColor
                }
            }
        }

        delegate: SocialComment {
            width: view.width
            avatar: "http://graph.facebook.com/"+ model.contentItem.from.objectIdentifier + "/picture"
            message: model.contentItem.message
            footer: model.contentItem.from.objectName + " \u2022 "
                    + Format.formatDate(model.contentItem.createdTime, Formatter.DurationElapsed)
            extraVisible: model.contentItem.likeCount > 0
            //: Number of Facebook likes for a comment
            //% "%n like(s)"
            extra: qsTrId("lipstick-jolla-home-facebook-la-number_of_likes_for_comment", model.contentItem.likeCount)
        }

        footer: SocialReplyField {
            id: replyField
            enabled: view.state === "idle" && container.allowComment
            displayMargins: facebookComments.count > 0
            //: Label indicating text field is used for entering a comment to Facebook post
            //% "Comment"
            label: qsTrId("lipstick-jolla-home-facebook-la-comment")
            avatar: facebookMe.node != null && facebookMe.node.picture !== null ? facebookMe.node.picture.url : ""
            //: Write a Facebook comment
            //% "Write a comment"
            placeholderText: qsTrId("lipstick-jolla-home-facebook-ph-write-comment")
            allowComment: view.state === "idle"
            onEnterKeyClicked: {
                if (replyField.text.length > 0) {
                    facebookLikes.node.uploadComment(replyField.text)
                    view.state = "commenting"
                }
                replyField.close()
            }

            Connections {
                target: view
                onForceReplyFieldActiveFocus: replyField.forceActiveFocus()
                onClearReplyField: replyField.clear()
            }
        }

        VerticalScrollDecorator {}

        SocialAccountPullDownMenu {
            pageContainer: container.pageContainer
            onCurrentAccountChanged: account.identifier = currentAccount
            //% "Select account"
            selectAccountString: qsTrId("lipstick-jolla-home-la-select-account")
            //% "Change to %1"
            changeToAccountString: qsTrId("lipstick-jolla-home-la-change-to-account")
            //% "Account: %1"
            accountString: qsTrId("lipstick-jolla-home-la-account-name")

            // We should not set the identifier of account when we are syncing or signin in
            switchEnabled: account.status != Account.SigningIn
                             && account.status != Account.SyncInProgress
            serviceName: "facebook"
        }
    }
}
