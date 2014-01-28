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
    property int retweetCount

    Column {
        id: content
        x: item.avatar.width + Theme.paddingMedium
        width: parent.width - (item.avatar.width + Theme.paddingMedium*2)

        SocialMediaPreviewRow {
            imageSize: (Screen.width - item.avatar.width) / 3
            imageList: item.imageList
            connectedToNetwork: item.connectedToNetwork
        }

        Item {
            width: parent.width
            height: retweeterField.height
            visible: retweeterField.text.length > 0

            Image {
                id: retweeterIcon
                source: "image://theme/icon-s-retweet"
            }
            Text {
                id: retweeterField
                x: retweeterIcon.width + Theme.paddingSmall
                width: parent.width - (retweeterIcon.width + Theme.paddingSmall)
                elide: Text.ElideRight
                opacity: .6
                text: model.retweeter
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primaryColor
            }
        }

        Text {
            width: parent.width
            elide: Text.ElideRight
            opacity: .6
            text: model.name
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.primaryColor
        }

        Text {
            width: parent.width
            elide: Text.ElideRight
            opacity: .6
            text: "@" + model.screenName
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
