import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    property alias text: label.text

    width: parent.width
    height: label.text !== "" ? (label.height + 2 * Theme.paddingLarge) : Theme.paddingLarge
    opacity: label.text !== "" ? 1 : 0

    Label {
        id: label
        anchors {
            left: parent.left
            leftMargin: Theme.paddingLarge
            right: parent.right
            rightMargin: Theme.paddingLarge
            verticalCenter: parent.verticalCenter
        }
        font.pixelSize: Theme.fontSizeExtraSmall
        wrapMode: Text.WordWrap
        color: Theme.highlightColor
    }

    Behavior on opacity { FadeAnimation {} }
    Behavior on height { FadeAnimation { property: "height" } }
}
