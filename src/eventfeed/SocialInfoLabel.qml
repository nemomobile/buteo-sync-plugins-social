import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    property alias text: label.text
    anchors {
        left: parent.left
        right: parent.right
    }

    height: label.text != "" ? (label.height + 2 * Theme.paddingLarge) : Theme.paddingLarge
    opacity: label.text != "" ? 1 : 0

    Label {
        id: label
        anchors {
            left: parent.left
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.paddingMedium
            verticalCenter: parent.verticalCenter
        }
        font.pixelSize: Theme.fontSizeSmall
        wrapMode: Text.WordWrap
    }

    Behavior on opacity { FadeAnimation {} }
    Behavior on height { FadeAnimation { property: "height" } }
}
