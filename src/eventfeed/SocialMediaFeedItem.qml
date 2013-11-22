import QtQuick 2.0
import Sailfish.Silica 1.0

BackgroundItem {
    id: item
    property bool connectedToNetwork
    property string timestamp: model.timestamp
    property string formattedTime: refreshTimeCount ? Format.formatDate(timestamp, Format.DurationElapsed) : formattedTime
    property SocialAvatar avatar: _avatar
    property int refreshTimeCount: 1

    SocialAvatar {
        id: _avatar
        source: model.icon
        width: Theme.itemSizeMedium
        height: Theme.itemSizeMedium
        connectedToNetwork: item.connectedToNetwork
    }
}
