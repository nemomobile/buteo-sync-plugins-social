import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.contacts 1.0
import org.nemomobile.accounts 1.0
import org.nemomobile.signon 1.0
import org.nemomobile.social 1.0

Page {
    id: page

    property variant model

    property AccountManager accountManager: AccountManager { }
    property QtObject serviceAccount
    property string accessToken
    property string accountIdentifier
    property string myName
    property string nodeIdentifier
    property bool liked
    property string likers

    property ServiceAccountIdentity serviceAccountIdentity: ServiceAccountIdentity {
        identifier: serviceAccount.authData.identityIdentifier
        onStatusChanged: {
            if (serviceAccountIdentity.status == ServiceAccountIdentity.Initialized) {
                signIn(serviceAccount.authData.method, serviceAccount.authData.mechanism, serviceAccount.authData.parameters)
            }
        }

        onResponseReceived: {
            var token = data["AccessToken"]
            if (token != "") {
                page.accessToken = token
            }
        }
    }

    onModelChanged: {
        accountIdentifier = model.metaData["accountId"]
        nodeIdentifier = model.metaData["nodeId"]
    }

    onAccountIdentifierChanged: serviceAccount = accountManager.serviceAccount(accountIdentifier, "facebook-sync")

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
                return qsTrId("lipstick-jolla-home-facebook-la-you-and-multiple-friend-like-this").arg(users[0]).arg(users.length - 1)
            } else {
                //% "%1 and %2 and %3 others like this"
                return qsTrId("lipstick-jolla-home-facebook-la-multiple-friend-like-this").arg(users[0]).arg(users[1]).arg(users.length - 2)
            }
        }
        // Return an empty string for 0 likes
        return ""
    }

    Facebook {
        id: facebook
        accessToken: page.accessToken
        onInitializedChanged: populateIfInitialized()
        onAccessTokenChanged: populateIfInitialized()
        function populateIfInitialized() {
            if (initialized && accessToken.length > 0) {
                facebookMe.populate()
                facebookLikes.populate()
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
                page.myName = facebookMe.node.name
                page.liked = page.isLikedBySelf()
                page.likers = page.updateLikers()
            }
        }
    }

    SocialNetworkModel {
        id: facebookLikes
        filters: [ ContentItemTypeFilter { type: Facebook.Like } ]
        socialNetwork: facebook
        nodeIdentifier: page.nodeIdentifier
        onStatusChanged: {
            if (status == SocialNetwork.Idle) {
                page.liked = page.isLikedBySelf()
                page.likers = page.updateLikers()
            }
        }
    }

    SocialNetworkModel {
        id: facebookComments
        filters: [ ContentItemTypeFilter { type: Facebook.Comment } ]
        socialNetwork: facebook
        nodeIdentifier: page.nodeIdentifier
    }

    Connections {
        target: facebookComments.node
        onLikedChanged: facebookLikes.repopulate()
    }

    Formatter {
        id: formatter
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.highlightBackgroundColor
        opacity: 0.3
    }

    SilicaListView {
        id: commentsList
        spacing: Theme.paddingMedium
        anchors.fill: parent
        model: facebookComments

        header: Item {
            width: page.width
            height: content.height

            Rectangle {
                anchors {
                    fill: parent
                    bottomMargin: imageRow.height + (footer.text.length > 0 ? footer.height : Theme.paddingLarge)
                }
                color: "#33ffffff"

                MouseArea {
                    anchors.fill: parent
                    onClicked: model.clicked()
                }
            }

            Column {
                id: content
                width: parent.width

                Label {
                    text: model.sourceDisplayName
                    anchors {
                        right: parent.right
                        rightMargin: Theme.paddingLarge
                    }
                    color: Theme.highlightColor
                    font: Theme.fontFamilyHeading
                    verticalAlignment: Text.AlignVCenter
                    height: 4 * Theme.paddingLarge
                }

                Row {
                    width: parent.width
                    spacing: Theme.paddingMedium
                    Image {
                        id: face
                        width: Theme.itemSizeExtraLarge
                        height: Theme.itemSizeExtraLarge
                        sourceSize {
                            width: Theme.itemSizeExtraLarge
                            height: Theme.itemSizeExtraLarge
                        }
                        asynchronous: true
                        fillMode: Image.PreserveAspectCrop
                        source: {
                            if (model.icon == "") {
                                return model.icon
                            } else if (model.icon == 0) {
                                return model.icon
                            } else if (model.icon.indexOf("/") == 0) {
                                return "image://nemoThumbnail/" + model.icon
                            } else {
                                return "image://theme/" + model.icon
                            }
                        }
                    }

                    Column {
                        anchors.bottom: parent.bottom

                        Label {
                            text: model.body
                            color: Theme.highlightColor
                            wrapMode: Text.WordWrap
                            width: page.width - face.width - Theme.paddingLarge * 2
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        Label {
                            id: time
                            color: Theme.highlightColor
                            opacity: 0.6
                            text: formatter.formatDate(model.timestamp, Formatter.DurationElapsed)
                            font.pixelSize: Theme.fontSizeExtraSmall
                        }

                        MouseArea {
                            width: parent.width
                            height: like.height + Theme.paddingMedium
                            onClicked: {
                                if (!liked) {
                                    facebookComments.node.like()
                                } else {
                                    facebookComments.node.unlike()
                                }
                            }

                            Image {
                                source: "image://theme/icon-m-like"
                                anchors {
                                    right: like.left
                                    rightMargin: Theme.paddingSmall
                                }
                                height: 32
                                width: 32
                                opacity: 0.4
                            }

                            Label {
                                id: like
                                //% "Unlike"
                                text: liked ? qsTrId("lipstick-jolla-home-facebook-la-unlike") :
                                //% "Like"
                                qsTrId("lipstick-jolla-home-facebook-la-like")
                                anchors.right: parent.right
                                color: liked ? Theme.highlightColor : Theme.primaryColor
                            }
                        }
                    }
                }

                Item {
                    // Filler
                    width: 1
                    height: Theme.paddingLarge
                }

                Rectangle {
                    id: imageRow
                    width: parent.width
                    height: page.model.imageList.length > 0 ? Theme.itemSizeLarge + Theme.paddingLarge: 0
                    color: "#1affffff"
                    Row {
                        Repeater {
                            model: page.model.imageList
                            delegate: Image {
                                width: imageRow.height
                                height: imageRow.height
                                sourceSize {
                                    width: imageRow.height
                                    height: imageRow.height
                                }
                                asynchronous: true
                                fillMode: Image.PreserveAspectCrop
                                source: {
                                    if (page.model.imageList[index] == "") {
                                        return page.model.imageList[index]
                                    } else if (page.model.imageList[index].indexOf("http") == 0) {
                                        return page.model.imageList[index]
                                    } else if (page.model.imageList[index].indexOf("/") == 0) {
                                        return "image://nemoThumbnail/" + page.model.imageList[index]
                                    } else {
                                        return "image://theme/" + page.model.imageList[index]
                                    }
                                }
                            }
                        }
                    }
                    visible: page.model.imageList.length > 0
                }

                Label {
                    id: footer
                    text: page.likers
                    font.pixelSize: Theme.fontSizeExtraSmall
                    height: Theme.itemSizeSmall
                    verticalAlignment: Text.AlignVCenter
                    anchors.left: parent.left
                }

                Item {
                    // Filler
                    width: 1
                    height: page.likers.length > 0 ? 0 : Theme.paddingLarge
                }
            }
        }

        delegate: Item {
            id: commentDelegate
            property bool _showDelegate: commentsList.count

            width: commentsList.width
            height: commentFrom.paintedHeight
                    + commentText.paintedHeight
                    + (likeCount.visible ? likeText.paintedHeight : 0)
                    + 3 * Theme.paddingSmall

            opacity: _showDelegate ? 1 : 0
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
                source: _showDelegate ? "http://graph.facebook.com/"+ model.contentItem.from.objectIdentifier + "/picture" : ""
                clip: true
                anchors.fill: avatarPlaceholder
                fillMode: Image.PreserveAspectCrop
                smooth: true
            }

            Column {
                id: commentColumn
                spacing: Theme.paddingSmall
                anchors {
                    left: avatar.right
                    leftMargin: Theme.paddingMedium
                    top: avatar.top
                    right: parent.right
                    rightMargin: Theme.paddingLarge
                }

                Label {
                    id: commentText
                    text: _showDelegate ? model.contentItem.message : ""
                    width: parent.width
                    font.pixelSize: Theme.fontSizeExtraSmall
                    horizontalAlignment: Text.AlignLeft
                    wrapMode: Text.Wrap
                }

                Row {
                    spacing: Theme.paddingSmall

                    Label {
                        id: commentFrom
                        text: _showDelegate ? model.contentItem.from.objectName : ""
                        color: Theme.secondaryColor
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignTop
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }

                    Label {
                        id: createdTime
                        text: _showDelegate ? formatter.formatDate(model.contentItem.createdTime, Formatter.DurationElapsed) : ""
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }
            }

            Label {
                id: likeCount
                visible: _showDelegate ? model.contentItem.likeCount > 0 : ""
                text: model.contentItem.likeCount
                color: Theme.highlightColor
                horizontalAlignment: Text.AlignRight
                font.pixelSize: Theme.fontSizeExtraSmall
                anchors {
                    top: commentColumn.bottom
                    topMargin: Theme.paddingSmall
                    right: commentColumn.left
                    rightMargin: Theme.paddingMedium
                }
            }

            Label {
                id: likeText
                //: Number of likes for the comment
                //% "Like"
                property string like: qsTrId("lipstick-jolla-home-facebook-la-single-like-for-comment")
                //% "Likes"
                property string likes: qsTrId("lipstick-jolla-home-facebook-la-number-of-likes-for-comment")
                text: _showDelegate
                        ? model.contentItem.likeCount > 1 ? likes : like
                        : ""
                visible: likeCount.visible
                font.pixelSize: Theme.fontSizeExtraSmall
                anchors {
                    top: commentColumn.bottom
                    topMargin: Theme.paddingSmall
                    left: commentColumn.left
                }
            }
        }

        footer: Item {
            width: parent.width
            height: Theme.iconSizeMedium + Theme.paddingMedium

            Image {
                id: commentAvatar
                width: Theme.iconSizeMedium
                height: Theme.iconSizeMedium
                source: facebookMe.node.picture.url
            }

            TextField {
                id: comment
                anchors {
                    left: commentAvatar.right
                    leftMargin: Theme.paddingLarge + Theme.iconSizeMedium
                    right: parent.right
                }

                //% "Write a comment"
                placeholderText: qsTrId("lipstick-jolla-home-facebook-ph-write-comment")

                EnterKey.onClicked: {
                    facebookComments.node.uploadComment(comment.text)
                    text = ""
                    facebookComments.repopulate()
                }
            }
        }
    }
}
