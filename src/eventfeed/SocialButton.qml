import QtQuick 2.0
import Sailfish.Silica 1.0

MouseArea {
    id: container
    property alias icon: icon.source
    property alias text: label.text
    height: label.height + 2 * Theme.paddingLarge
    width: icon.width + Theme.paddingSmall + label.width

    Image {
        id: icon
        anchors.verticalCenter: parent.verticalCenter
        height: 32
        width: 32
        opacity: container.enabled ? 1 : 0.5
    }

    Label {
        id: label
        anchors {
            left: icon.right
            leftMargin: Theme.paddingSmall
            verticalCenter: parent.verticalCenter
        }
        color: container.pressed ? Theme.highlightColor : Theme.primaryColor
        opacity: container.enabled ? 1 : 0.5
        font.pixelSize: Theme.fontSizeSmall
    }
}
