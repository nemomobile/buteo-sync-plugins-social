import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.TextLinking 1.0

Column {
    property alias text: body.plainText
    property alias time: time.text
    anchors {
        left: parent.left
        right: parent.right
    }

    LinkedText {
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
