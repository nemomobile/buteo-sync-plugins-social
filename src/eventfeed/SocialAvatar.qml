import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: container
    property string icon
    width: Theme.itemSizeExtraLarge
    height: Theme.itemSizeExtraLarge

    Rectangle {
        anchors.fill: parent
        color: Theme.highlightColor
        opacity: 0.5
    }

    Image {
        anchors.fill: parent
        sourceSize {
            width: container.width
            height: container.height
        }
        asynchronous: true
        fillMode: Image.PreserveAspectCrop
        source: {
            if (container.icon == "") {
                return container.icon
            } else if (container.icon.indexOf("http") == 0) {
                return container.icon
            } else if (container.icon.indexOf("/") == 0) {
                return "image://nemoThumbnail/" + container.icon
            } else {
                return "image://theme/" + container.icon
            }
        }
    }
}

