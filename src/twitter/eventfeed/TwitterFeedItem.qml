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
        anchors {
            left: item.avatar.right
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.paddingMedium
        }

        SocialMediaPreviewRow {
            imageList: item.imageList
        }

        Item {
            width: parent.width
            height: retweeterField.height
            visible: retweeterField.text.length > 0

            Image {
                id: retweeterIcon
                source: "image://theme/icon-s-retweet"
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                id: retweeterField
                anchors {
                    left: retweeterIcon.right
                    leftMargin: Theme.paddingSmall
                    right: parent.right
                }
                elide: Text.ElideRight
                opacity: .6
                text: model.retweeter
                font.pixelSize: Theme.fontSizeExtraSmall
            }
        }

        Label {
            width: parent.width
            elide: Text.ElideRight
            opacity: .6
            text: "@" + model.screenName
            font.pixelSize: Theme.fontSizeExtraSmall
        }

        LinkedText {
            width: parent.width
            maximumLineCount: 4
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            shortenUrl: true
            color: item.pressed ? Theme.highlightColor : Theme.primaryColor
            plainText: model.body
        }

        Label {
            width: parent.width
            maximumLineCount: 1
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeExtraSmall
            text: item.formattedTime
        }
    }
}
