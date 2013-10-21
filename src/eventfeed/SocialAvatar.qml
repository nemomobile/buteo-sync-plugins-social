import QtQuick 2.0
import Sailfish.Silica 1.0

Image {
    id: image
    width: Theme.itemSizeExtraLarge
    height: Theme.itemSizeExtraLarge
    asynchronous: true
    fillMode: Image.PreserveAspectCrop
    sourceSize {
        width: image.width
        height: image.height
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.highlightColor
        opacity: 0.1
        visible: image.status !== Image.Ready
    }
}

