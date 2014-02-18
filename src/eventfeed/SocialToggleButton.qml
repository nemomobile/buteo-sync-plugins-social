import QtQuick 2.0
import Sailfish.Silica 1.0

MouseArea {
    id: container
    property alias text: label.text
    property bool down: (pressed && containsMouse) || locked
    property bool locked

    height: label.height + 2 * Theme.paddingLarge
    width: label.width

    Rectangle {
        anchors.fill: parent
        color: Theme.rgba(Theme.highlightColor, 0.15)
        visible: container.locked
    }

    Label {
        id: label
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        color: container.down ? Theme.highlightColor : Theme.primaryColor
        opacity: container.enabled ? 1 : 0.5
        font.pixelSize: Theme.fontSizeSmall
    }
}
