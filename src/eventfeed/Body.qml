import QtQuick 2.0
import Sailfish.Silica 1.0

Column {
    property alias text: body.text
    property alias time: time.text
    anchors {
        left: parent.left
        right: parent.right
    }

    Label {
        id: body
        anchors {
            left: parent.left
            right: parent.right
        }
        color: Theme.highlightColor
        wrapMode: Text.WordWrap
        font.pixelSize: Theme.fontSizeSmall
    }
    Label {
        id: time
        color: Theme.highlightColor
        opacity: 0.6
        font.pixelSize: Theme.fontSizeExtraSmall
    }
}
