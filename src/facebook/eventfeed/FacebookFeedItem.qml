import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.TextLinking 1.0
import "shared"

SocialMediaFeedItem {
    id: item
    height: Math.max(content.height, avatar.height) + Theme.paddingLarge
    width: parent.width

    property variant imageList
    property int likeCount
    property int commentCount

    //: Number of facebook likes
    //% "%n like(s)"
    property string _countFieldLikeCount: likeCount > 0 ? " \u2022 " + qsTrId("lipstick-jolla-home-la-facebook_like_count", likeCount) : ""
    //: Number of facebook comments
    //% "%n comment(s)"
    property string _countFieldCommentCount: commentCount > 0 ? qsTrId("lipstick-jolla-home-la-facebook_comment_count", commentCount) : ""

    Column {
        id: content
        anchors {
            left: item.avatar.right
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.paddingMedium
        }

        SocialMediaRow {
            id: mediaRow
            imageList: item.imageList
            mediaName: model.attachmentName
            mediaCaption: model.attachmentCaption
            mediaDescription: model.attachmentDescription
        }

        LinkedText {
            anchors.right: parent.right
            width: parent.width
            visible: text.length > 0
            maximumLineCount: 20
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeSmall
            plainText: model.attachmentDescription
            shortenUrl: true
        }

        LinkedText {
            id: bodyField
            width: parent.width
            maximumLineCount: 50
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            color: item.pressed ? Theme.highlightColor : Theme.primaryColor
            shortenUrl: true
            plainText: model.body
        }

        Label {
            id: nameField
            width: parent.width
            maximumLineCount: 3
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeSmall
            text: model.name + " \u2022 " + formattedTime
        }

        Label {
            id: countField
            width: parent.width
            maximumLineCount: 2
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            visible: text.length > 0
            color: Theme.highlightColor
            opacity: .6
            font.pixelSize: Theme.fontSizeSmall
            text:  _countFieldCommentCount + _countFieldLikeCount
        }

        Item {
            width: 1
            height: Theme.paddingLarge
        }
    }
}
