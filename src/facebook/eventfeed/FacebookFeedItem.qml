import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.TextLinking 1.0
import "shared"

SocialMediaFeedItem {
    id: item
    height: Math.max(content.height, avatar.height) + Theme.paddingMedium * 3
    width: parent.width

    property variant imageList
    property int likeCount
    property int commentCount

    Column {
        id: content
        anchors {
            left: item.avatar.right
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.paddingMedium
        }

        SocialMediaPreviewRow {
            id: mediaRow
            imageList: item.imageList
            mediaName: model.attachmentName
            mediaCaption: model.attachmentCaption
            mediaDescription: model.attachmentDescription
            connectedToNetwork: item.connectedToNetwork
        }

        LinkedText {
            anchors.right: parent.right
            width: parent.width
            visible: text.length > 0
            maximumLineCount: 3
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
            maximumLineCount: 3
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
            font.pixelSize: Theme.fontSizeExtraSmall
            text: model.name + " \u2022 " + formattedTime
        }
    }
}
