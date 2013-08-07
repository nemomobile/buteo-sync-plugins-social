import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.contacts 1.0
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import "shared"

Page {
    id: container

    property variant model

    property string accessToken
    property string myName
    property string nodeIdentifier
    property bool liked
    property string likers

    property string mediaName
    property string mediaCaption
    property string mediaDescription

    onModelChanged: {
        nodeIdentifier = container.model.metaData["nodeId"]
        mediaName = container.model.metaData["postAttachmentName"]
        mediaCaption = container.model.metaData["postAttachmentCaption"]
        mediaDescription = container.model.metaData["postAttachmentDescription"]
    }

    Account {
        identifier: container.model != null ? container.model.metaData["accountId"] : -1
        onStatusChanged: {
            if (status == Account.Initialized) {
                // Sign in, and get access token.
                var params = signInParameters("facebook-sync")
                console.debug(container.model.metaData["clientId"])
                params.setParameter("ClientId", container.model.metaData["clientId"])
                params.setParameter("UiPolicy", SignInParameters.NoUserInteractionPolicy)
                signIn("Jolla", "Jolla", params)
            }
        }

        onSignInResponse: {
            var accessTok = data["AccessToken"]
            if (accessTok != "") {
                container.accessToken = accessTok
            }
        }
    }

    // Returns true|false if user has liked this image
    function isLikedBySelf() {
        for (var i = 0; i < facebookLikes.count; i++) {
            if (facebookLikes.relatedItem(i).userName === myName) {
                return true
            }
        }
        return false
    }

    // Returns string formatted e.g. "You, Mike M and 3 others like this"
    function updateLikers() {
        // Not very pretty code but localization and how this message
        // is expressed requires quite many variations
        var likedBySelf = false
        var photoUserName = ""
        var users = new Array
        for (var i = 0; i < facebookLikes.count; i++) {
            if (facebookLikes.relatedItem(i).userName === myName) {
                likedBySelf = true
                photoUserName = facebookLikes.relatedItem(i).userName
            } else {
                users.push(facebookLikes.relatedItem(i).userName)
            }
        }

        if (facebookLikes.count == 1) {
            if (likedBySelf) {
                //% "You like this"
                return qsTrId("lipstick-jolla-home-facebook-la-you-like-this")
            } else {
                //% "%1 likes this"
                return qsTrId("gallery-fb-la-one-friend-likes-this").arg(users[0])
            }
        }
        if (facebookLikes.count == 2) {
            if (likedBySelf) {
                //% "You and %1 like this"
                return qsTrId("lipstick-jolla-home-facebook-la-you-and-another-friend-likes-this").arg(users[0])
            } else {
                //% "%1 and %2 like this"
                return qsTrId("lipstick-jolla-home-facebook-la-two-friend-likes-this").arg(users[0]).arg(users[1])
            }
        }
        if (facebookLikes.count > 2) {
            if (likedBySelf) {
                //% "You, %1 and %2 others like this"
                return qsTrId("lipstick-jolla-home-facebook-la-you-and-multiple-friend-like-this").arg(users[0]).arg(facebookLikes.node.likesCount- 1)
            } else {
                //% "%1, %2 and %3 others like this"
                return qsTrId("lipstick-jolla-home-facebook-la-multiple-friend-like-this").arg(users[0]).arg(users[1]).arg(facebookLikes.node.likesCount - 2)
            }
        }
        // Return an empty string for 0 likes
        return ""
    }

    Facebook {
        id: facebook
        accessToken: container.accessToken
        onInitializedChanged: populateIfInitialized()
        onAccessTokenChanged: populateIfInitialized()
        function populateIfInitialized() {
            if (initialized && accessToken.length > 0) {
                facebookMe.populate()
                facebookLikes.populate()
                facebookLikes.loading = true
                facebookComments.populate()
            }
        }
    }

    SocialNetworkModel {
        id: facebookMe
        filters: [ ContentItemTypeFilter { type: Facebook.UserPicture } ]
        socialNetwork: facebook
        nodeIdentifier: "me"
        onStatusChanged: {
            if (status == SocialNetwork.Idle) {
                container.myName = facebookMe.node.name
                container.liked = container.isLikedBySelf()
                container.likers = container.updateLikers()
            }
        }
    }

    // If you have a lot of likes, Facebook will provide
    // them as paginated. So it is not reliable to get
    // the likes by counting the number of elements in
    // this model.
    //
    // We still need (up to) the first 3 people who liked
    // that photo to display the "a, b and c liked that"
    // string. So we only need to retrieve 3 likes.
    SocialNetworkModel {
        id: facebookLikes
        property bool loading
        filters: ContentItemTypeFilter { type: Facebook.Like; limit: 3 }
        socialNetwork: facebook
        nodeIdentifier: container.nodeIdentifier
        onStatusChanged: {
            if (status == SocialNetwork.Idle) {
                facebookLikes.loading = false
                container.liked = container.isLikedBySelf()
                container.likers = container.updateLikers()
            }
        }
    }

    Connections {
        target: facebookLikes.node
        onStatusChanged:  {
            switch (facebookLikes.node.status) {
            case Facebook.Idle:
                facebookLikes.repopulate()
                break
            default:
                facebookLikes.loading = true
                break
            }
        }
    }

    SocialNetworkModel {
        id: facebookComments
        filters: [ ContentItemTypeFilter { type: Facebook.Comment } ]
        socialNetwork: facebook
        nodeIdentifier: container.nodeIdentifier
    }

    Formatter {
        id: formatter
    }

    SilicaListView {
        id: view
        anchors.fill: parent
        model: facebookComments
        spacing: Theme.paddingLarge

        header: Column {
            width: view.width

            // Contains the header, picture, body and social button
            // Includes a background
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

                // Contains the picture, body and social button
                Item {
                    anchors {
                        top: header.bottom
                        left: parent.left
                        right: parent.right
                        rightMargin: Theme.paddingLarge
                    }
                    height: childrenRect.height

                    Face {
                        id: face
                        icon: container.model.icon
                    }

                    Column {
                        anchors {
                            left: face.right
                            leftMargin: Theme.paddingMedium
                            right: parent.right
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
                                anchors.left: parent.left
                                enabled: !facebookLikes.loading && container.accessToken != ""
                                onClicked: {
                                    if (!container.liked) {
                                        facebookLikes.node.like()
                                    } else {
                                        facebookLikes.node.unlike()
                                    }
                                }
                                icon: "image://theme/icon-m-like"
                                //% "Unlike"
                                text: container.liked ? qsTrId("lipstick-jolla-home-facebook-la-unlike")
                                                        //% "Like"
                                                      : qsTrId("lipstick-jolla-home-facebook-la-like")

                            }

                            SocialButton {
                                anchors.right: parent.right
                                enabled: !facebookLikes.loading && container.accessToken != ""
                                icon: "image://theme/icon-m-chat"
                                //% "Comment"
                                text: qsTrId("lipstick-jolla-home-facebook-la-comment")
                                onClicked: {
                                    view.positionViewAtEnd()
                                    // TODO: give focus to commentField
                                }
                            }
                        }
                    }
                }
            }

            MediaRow {
                id: mediaRow
                imageList: container.model.imageList
                mediaName: container.mediaName
                mediaCaption: container.mediaCaption
                mediaDescription: container.mediaDescription
            }

            Item {
                anchors {
                    left: parent.left
                    right: parent.right
                }

                height: container.likers != "" ? (likesLabel.height + 2 * Theme.paddingLarge)
                                           : Theme.paddingLarge
                opacity: container.likers != "" ? 1 : 0

                Label {
                    id: likesLabel
                    anchors {
                        left: parent.left
                        leftMargin: Theme.paddingMedium
                        right: parent.right
                        rightMargin: Theme.paddingMedium
                        verticalCenter: parent.verticalCenter
                    }
                    text: container.likers
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                Behavior on opacity { FadeAnimation {} }
                Behavior on height { FadeAnimation {} }
            }
        }

        delegate: Item {
            id: commentDelegate

            width: view.width
            height: commentColumn.height

            opacity: 0
            Component.onCompleted: opacity = 1
            Behavior on opacity { FadeAnimation {} }

            Rectangle {
                id: avatarPlaceholder
                width: Theme.iconSizeMedium
                height: Theme.iconSizeMedium
                color: Theme.highlightColor
                opacity: 0.5
            }

            Image {
                id: avatar
                // Fetch the avatar from the constructed url
                source: "http://graph.facebook.com/"+ model.contentItem.from.objectIdentifier + "/picture"
                clip: true
                anchors.fill: avatarPlaceholder
                fillMode: Image.PreserveAspectCrop
                smooth: true
            }

            Column {
                id: commentColumn
                anchors {
                    left: avatar.right
                    leftMargin: Theme.paddingMedium
                    top: avatar.top
                    right: parent.right
                    rightMargin: Theme.paddingLarge
                }

                Label {
                    text: model.contentItem.message
                    width: parent.width
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignLeft
                    wrapMode: Text.Wrap
                }

                Label {
                    text: model.contentItem.from.objectName + " \u2022 "
                          + formatter.formatDate(model.contentItem.createdTime, Formatter.DurationElapsed)
                    color: Theme.secondaryColor
                    width: parent.width
                    font.pixelSize: Theme.fontSizeExtraSmall
                }

                Label {
                    visible: model.contentItem.likeCount > 0
                    //% "%n likes"
                    text: qsTrId("lipstick-jolla-home-facebook-la-number-of-likes-for-comment",
                                 model.contentItem.likeCount)
                    color: Theme.secondaryColor
                    width: parent.width
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
            }
        }

        footer: Item {
            width: parent.width
            height: commentContainer.height  + Theme.paddingMedium
                    + (view.count != 0 ? Theme.paddingLarge : 0)
            opacity: facebookComments.status == Facebook.Idle ? 1 : 0
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
                    source: facebookMe.node != null && facebookMe.node.picture != null ? facebookMe.node.picture.url : ""
                }

                TextField {
                    id: commentField
                    anchors {
                        left: commentAvatar.right
                        right: parent.right
                    }

                    //% "Write a comment"
                    placeholderText: qsTrId("lipstick-jolla-home-facebook-ph-write-comment")
                    enabled: facebookComments.status == Facebook.Idle

                    EnterKey.onClicked: {
                        facebookComments.node.uploadComment(comment.text)
                        facebookComments.repopulate()
                    }

                    Connections {
                        target: facebookComments
                        onStatusChanged: {
                            if (facebookComments.status == Facebook.Idle) {
                                commentField.text = ""
                            }
                        }
                    }
                }
            }
        }
    }
}
