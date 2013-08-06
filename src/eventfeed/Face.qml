import QtQuick 2.0
import Sailfish.Silica 1.0

Image {
    id: container
    property string icon
    width: Theme.itemSizeExtraLarge
    height: Theme.itemSizeExtraLarge
    sourceSize {
        width: Theme.itemSizeExtraLarge
        height: Theme.itemSizeExtraLarge
    }
    asynchronous: true
    fillMode: Image.PreserveAspectCrop
    source: {
        if (container.icon == "") {
            return container.icon
        } else if (container.icon == 0) {
            return container.icon
        } else if (container.icon.indexOf("/") == 0) {
            return "image://nemoThumbnail/" + container.icon
        } else {
            return "image://theme/" + container.icon
        }
    }
}
