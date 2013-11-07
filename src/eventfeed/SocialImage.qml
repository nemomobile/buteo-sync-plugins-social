import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: container

    property url source
    property url placeholderSource
    property bool connectedToNetwork

    property alias fillMode: image.fillMode
    property alias sourceSize: image.sourceSize

    // ---------------------

    onConnectedToNetworkChanged: _setImageSource()
    onSourceChanged: _setImageSource()
    Component.onCompleted: _setImageSource()

    function _setImageSource() {
        if (container.connectedToNetwork || container.source.toString().indexOf("http") != 0) {
            image.source = container.source
        } else {
            image.source = container.placeholderSource
        }
    }

    Rectangle {
        id: placeholderRect
        visible: !image.visible
        color: Theme.highlightColor
        opacity: 0.06
        width: container.width
        height: container.height
    }

    Image {
        id: image
        visible: container.connectedToNetwork || image.source.toString().length > 0
        asynchronous: true
        width: container.width
        height: container.height
    }
}