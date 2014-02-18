import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import "shared"

SilicaFlickable {
    id: view

    property bool connectedToNetwork
    property date timestamp
    property string body
    property string source
    property string posterId
    property string imageSource
    property string targetName
    property alias description: descriptionLabel.text
    property alias model: repeater.model

    // Signal relays
    signal forceReplyFieldActiveFocus()
    signal clearReplyField()

    anchors.fill: parent
    contentHeight: contentColumn.height

    Column {
        id: contentColumn
        width: parent.width

        SocialContent {
            id: socialContent
            connectedToNetwork: view.connectedToNetwork
            avatar: view.posterId !== "" ? "https://graph.facebook.com/"+view.posterId+"/picture?width=200&height=200" : ""
            source: view.source
            subSource: view.targetName !== "" ? "> " + view.targetName : ""
            body: view.body
            timestamp: view.timestamp
            fullRowSocialButtons: Item {
                width: parent.width
                height: childrenRect.height

                SocialButton {
                    connectedToNetwork: view.connectedToNetwork
                    anchors.left: parent.left
                    enabled: !container.likesModel.busy && container.allowLike
                    onClicked: container.toggleLike()
                    icon: "image://theme/icon-m-like?"
                          + (down ? Theme.highlightColor : Theme.primaryColor)

                    //: Press button to unlike
                    //% "Unlike"
                    text: container.liked ? qsTrId("lipstick-jolla-home-facebook-la-unlike")
                                            //% "Like"
                                            : qsTrId("lipstick-jolla-home-facebook-la-like")
                }

                SocialButton {
                    id: commentButton
                    connectedToNetwork: view.connectedToNetwork
                    anchors.right: parent.right
                    enabled: !container.commentsModel.busy && container.allowComment
                    icon: "image://theme/icon-m-chat?"
                          + (down ? Theme.highlightColor : Theme.primaryColor)

                    //: Press button to write facebook comment
                    //% "Comment"
                    text: qsTrId("lipstick-jolla-home-facebook-la-comment")
                    onClicked: view.forceReplyFieldActiveFocus()
                }
            }
        }

        Rectangle {
            width: parent.width
            height: descriptionLabel.visible ? Math.max(mediaImage.height, descriptionLabel.height + Theme.paddingMedium) : mediaImage.height
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.rgba(Theme.highlightColor, 0) }
                GradientStop { position: 1; color: Theme.rgba(Theme.highlightColor, 0.05) }
            }

            SocialImage {
                id: mediaImage

                property int maxWidth: descriptionLabel.visible ? parent.width / 2 : parent.width

                visible: source.toString() !== ""
                width: visible ? Math.min(sourceSize.width, maxWidth) : 0
                height: visible ? width * (sourceSize.height / sourceSize.width) : 0
                source: view.imageSource
                connectedToNetwork: view.connectedToNetwork
            }

            SocialLabel {
                id: descriptionLabel
                anchors {
                    left: mediaImage.right
                    leftMargin: Theme.paddingLarge
                    right: parent.right
                    rightMargin: Theme.paddingLarge
                }
                maxOpacity: .6
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.highlightColor
            }
        }

        SocialInfoLabel { text: container.error !== "" ? container.likers + "\n" + container.error : container.likers }

        BackgroundItem {
            id: loadPreviousButton
            width: parent.width
            height: previousLabel.height + Theme.paddingLarge
            visible: container.likesModel.node === null ? false
                                : (container.likesModel.node.commentsCount > container.commentsModel.count
                                                    && container.commentsModel.hasPrevious)
            opacity: visible ? 1.0 : 0
            onClicked: container.commentsModel.loadPrevious()

            Behavior on opacity { FadeAnimation {} }
            Behavior on height { FadeAnimation { property: "height" } }

            Label {
                id: previousLabel
                anchors {
                    left: parent.left
                    leftMargin: Theme.paddingMedium
                    right: parent.right
                    rightMargin: Theme.paddingMedium
                }
                //: Load previous comments
                //% "Load previous comments"
                text: qsTrId("lipstick-jolla-home-facebook-la-load-previous-comments")
                color: loadPreviousButton.highlighted ? Theme.highlightColor
                                                      : Theme.primaryColor
            }
        }

        Column {
            width: parent.width
            spacing: Theme.paddingLarge
            Repeater {
                id: repeater

                SocialComment {
                    width: view.width
                    avatar: "http://graph.facebook.com/"+ model.contentItem.from.objectIdentifier + "/picture"
                    message: model.contentItem.message
                    footer: model.contentItem.from.objectName + " \u2022 "
                            + Format.formatDate(new Date(model.contentItem.createdTime).toISOString(), Formatter.DurationElapsed)
                    extraVisible: model.contentItem.likeCount > 0
                    //: Number of Facebook likes for a comment
                    //% "%n like(s)"
                    extra: qsTrId("lipstick-jolla-home-facebook-la-number_of_likes_for_comment", model.contentItem.likeCount)
                    connectedToNetwork: view.connectedToNetwork
                }
            }
        }

        SocialReplyField {
            id: replyField
            connectedToNetwork: view.connectedToNetwork
            visible: connectedToNetwork
            displayMargins: container.commentsModel.count > 0
            //: Label indicating text field is used for entering a comment to Facebook post
            //% "Comment"
            label: qsTrId("lipstick-jolla-home-facebook-la-comment")
            avatar: container.facebookMe ? container.facebookMe.avatar : ""
            //: Write a Facebook comment
            //% "Write a comment"
            placeholderText: qsTrId("lipstick-jolla-home-facebook-ph-write-comment")
            allowComment: !container.commentsModel.busy && container.allowComment
            onEnterKeyClicked: {
                if (replyField.text.length > 0) {
                    container.uploadComment(replyField.text)
                }
                replyField.close()
            }

            Connections {
                target: view
                onForceReplyFieldActiveFocus: replyField.forceActiveFocus()
                onClearReplyField: replyField.clear()
            }
        }
    }

    VerticalScrollDecorator {}
}
