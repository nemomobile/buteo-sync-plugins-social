import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.TextLinking 1.0
import "shared"

SocialMediaFeedItem {
    id: item
    height: Math.max(content.height, avatar.height) + Theme.paddingMedium * 3
    width: parent.width

    property variant imageList

    Column {
        id: content
        x: item.avatar.width + Theme.paddingMedium
        width: parent.width - (item.avatar.width + Theme.paddingMedium*2)

        SocialMediaPreviewRow {
            imageSize: (Screen.width - item.avatar.width) / 3
            imageList: item.imageList
            connectedToNetwork: item.connectedToNetwork
        }

        Text {
            width: parent.width
            elide: Text.ElideRight
            opacity: .6
            text: model.name
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.primaryColor
        }

        LinkedText {
            width: parent.width
            maximumLineCount: 4
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            shortenUrl: true
            color: item.pressed ? Theme.highlightColor : Theme.primaryColor
            plainText: model.body
            visible: plainText !== ""
        }

        Text {
            width: parent.width
            maximumLineCount: 1
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeSmall
            text: item.formattedTime
        }
    }
}
