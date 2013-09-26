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
    property int retweetCount

    //: Number of twitter comments
    //% "%n comment(s)"
    property string _nameFieldCommentsText: commentCount > 0 ? " \u2022 " + qsTrId("lipstick-jolla-home-la-tweet_comment_count", commentCount) : ""
    //: Number of twitter re-tweets
    //% "%n re-tweet(s)"
    property string _nameFieldRetweetsText: retweetCount > 0 ? " \u2022 " + qsTrId("lipstick-jolla-home-la-tweet_retweet_count", retweetCount) : ""

    Column {
        id: content
        anchors {
            left: item.avatar.right
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.paddingMedium
        }

        SocialMediaRow {
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
            }
        }

        Label {
            width: parent.width
            elide: Text.ElideRight
            opacity: .6
            text: "@" + model.screenName
        }

        LinkedText {
            width: parent.width
            maximumLineCount: 50
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            shortenUrl: true
            color: item.pressed ? Theme.highlightColor : Theme.primaryColor
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
            text: item.formattedTime + _nameFieldCommentsText + _nameFieldRetweetsText
        }

        Item {
            width: 1
            height: Theme.paddingLarge
        }
    }
}
