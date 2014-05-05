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
    property bool connectedToNetwork
    property Account account
    property string accessToken
    property bool liked
    property string currentUserAvatar
    property string currentUserUid

    allowedOrientations: Lipstick.compositor.eventsWindowOrientation

    VKPost {
        id: vkPost
        accessToken: container.accessToken
        viewerId: container.currentUserUid
        onViewerIdChanged: loadIfReady()
        onAccessTokenChanged: loadIfReady()
        onCommentUploaded: loadComments()

        function loadIfReady() {
            if (accessToken !== "" && viewerId !== "") {
                load(model.vkId)
            }
        }
    }

    SilicaListView {
        id: view

        property string info

        // Signal relays
        signal forceReplyFieldActiveFocus()

        anchors.fill: parent
        model: vkPost.comments
        spacing: Theme.paddingLarge

        header: Column {
            id: column
            width: view.width

            SocialContent {
                connectedToNetwork: container.connectedToNetwork
                avatar: container.model.icon
                source: container.model.name
                parentPage: container
                timestamp: container.model.timestamp
                body: container.model.body
                fullRowSocialButtons: Item {
                    width: parent.width
                    height: childrenRect.height

                    SocialButton {
                        connectedToNetwork: container.connectedToNetwork
                        anchors.left: parent.left
                        icon: "image://theme/icon-m-like?"
                              + (down ? Theme.highlightColor : Theme.primaryColor)
                        //: Press button to unlike
                        //% "Unlike"
                        text: vkPost.userLikes ? qsTrId("lipstick-jolla-home-vk-la-unlike")
                                               //% "Like"
                                               : qsTrId("lipstick-jolla-home-vk-la-like")
                        onClicked: {
                            if (vkPost.userLikes) {
                                vkPost.unlike()
                            } else {
                                vkPost.like()
                            }
                        }
                    }

                    SocialButton {
                        id: commentButton
                        connectedToNetwork: container.connectedToNetwork
                        anchors.right: parent.right
                        enabled: vkPost.canPost
                        icon: "image://theme/icon-m-chat?"
                              + (down ? Theme.highlightColor : Theme.primaryColor)

                        //: Press button to write VK comment
                        //% "Comment"
                        text: qsTrId("lipstick-jolla-home-vk-la-comment")
                        onClicked: view.forceReplyFieldActiveFocus()
                    }
                }
            }

            SocialMediaRow {
                id: mediaRow
                imageList: container.model.images
                connectedToNetwork: container.connectedToNetwork
            }

            SocialInfoLabel {
                id: infoLabel
                //: Number of likes
                //% "%n like(s)"
                text: vkPost.likeCount > 0 ? qsTrId("lipstick-jolla-home-vk-la-likes_count", vkPost.likeCount) : ""
            }
        }
        footer: SocialReplyField {
            id: replyField
            connectedToNetwork: container.connectedToNetwork
            visible: connectedToNetwork
            avatar: container.currentUserAvatar
            //: Label indicating text field is used for entering a reply to VK post
            //% "Reply"
            label: qsTrId("lipstick-jolla-home-vk-la-reply-field")
            //: Write a reply to VK post
            //% "Write a reply"
            placeholderText: qsTrId("lipstick-jolla-home-vk-ph-write-reply")
            allowComment: vkPost.canPost
            onEnterKeyClicked: {
                if (text !== "") {
                    vkPost.postComment(text)
                }
                replyField.close()
            }

            Connections {
                target: view
                onForceReplyFieldActiveFocus: replyField.forceActiveFocus()
            }
        }

        delegate: SocialComment {
            property string timestamp: Format.formatDate(model.timestamp, Format.DurationElapsed)

            width: view.width
            avatar: model.icon
            message: model.text
            footer: model.name !== "" ? model.name + " \u2022 " + timestamp : timestamp
            extraVisible: model.likes > 0
            //: Number of VK likes for a comment
            //% "%n like(s)"
            extra: qsTrId("lipstick-jolla-home-vk-la-number_of_likes_for_comment", model.likes)
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
            serviceName: "vk"
        }
    }
}
