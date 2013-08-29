import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: container
    property alias avatar: avatar.source
    property alias message: message.text
    property alias footer: footer.text
    property alias extra: extra.text
    property alias extraVisible: extra.visible

    height: commentColumn.height

    opacity: 0
    Component.onCompleted: opacity = 1
    Behavior on opacity { FadeAnimation {} }

    Rectangle {
        id: avatarPlaceholder
        width: Theme.iconSizeMedium
        height: Theme.iconSizeMedium
        color: Theme.highlightColor
        opacity: 0.5
    }

    Image {
        id: avatar
        clip: true
        anchors.fill: avatarPlaceholder
        fillMode: Image.PreserveAspectCrop
        smooth: true
    }

    Column {
        id: commentColumn
        anchors {
            left: avatar.right
            leftMargin: Theme.paddingMedium
            top: avatar.top
            right: parent.right
            rightMargin: Theme.paddingLarge
        }

        Label {
            id: message
            text: model.contentItem.message
            width: parent.width
            font.pixelSize: Theme.fontSizeSmall
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.Wrap
            color: Theme.highlightColor
        }

        Label {
            id: footer
            opacity: 0.6
            color: Theme.highlightColor
            width: parent.width
            font.pixelSize: Theme.fontSizeExtraSmall
        }

        Label {
            id: extra
            opacity: 0.6
            color: Theme.highlightColor
            width: parent.width
            font.pixelSize: Theme.fontSizeExtraSmall
        }
    }
}
