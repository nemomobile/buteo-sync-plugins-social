import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.TextLinking 1.0
import "shared"

SocialMediaFeedItem {
    id: item

    property string timestamp: model.timestamp
    property string formattedTime: refreshTimeCount ? Format.formatDate(timestamp, Format.DurationElapsed) : formattedTime
    property int refreshTimeCount: 1

    avatarSource: model.from !== "" ? "https://graph.facebook.com/"+model.from+"/picture?width=200&height=200" : ""
    height: Math.max(content.height, avatar.height) + Theme.paddingMedium * 3
    width: parent.width

    Column {
        id: content
        x: item.avatar.width + Theme.paddingMedium
        width: parent.width - (item.avatar.width + Theme.paddingMedium*2)

        LinkedText {
            width: parent.width
            visible: text.length > 0
            maximumLineCount: 3
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeSmall
            plainText: model.title
            shortenUrl: true
        }

        Text {
            id: timeField
            width: parent.width
            maximumLineCount: 1
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeExtraSmall
            text: formattedTime
        }
    }
}
