import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: container
    property alias avatar: avatar.source
    property alias message: message.text
    property alias footer: footer.text
    property alias extra: extra.text
    property alias extraVisible: extra.visible
    property bool connectedToNetwork

    height: Math.max(message.height + footer.height + extra.height,
                     avatarPlaceholder.height)

    opacity: 0
    Component.onCompleted: opacity = 1
    Behavior on opacity { FadeAnimation {} }

    Rectangle {
        id: avatarPlaceholder
        width: Theme.iconSizeMedium
        height: Theme.iconSizeMedium
        color: Theme.highlightColor
        opacity: 0.1
    }

    SocialImage {
        id: avatar
        clip: true
        anchors.fill: avatarPlaceholder
        fillMode: Image.PreserveAspectCrop
        smooth: true
        connectedToNetwork: container.connectedToNetwork
    }

    Label {
        id: message
        anchors {
            left: avatar.right
            leftMargin: Theme.paddingMedium
            top: avatar.top
            right: parent.right
            rightMargin: Theme.paddingLarge
        }
        text: model.contentItem.message
        width: parent.width
        font.pixelSize: Theme.fontSizeSmall
        horizontalAlignment: Text.AlignLeft
        wrapMode: Text.Wrap
        color: Theme.highlightColor
    }

    Label {
        id: footer
        anchors {
            left: avatar.right
            leftMargin: Theme.paddingMedium
            top: message.bottom
            right: parent.right
            rightMargin: Theme.paddingLarge
        }
        opacity: 0.6
        color: Theme.highlightColor
        width: parent.width
        font.pixelSize: Theme.fontSizeExtraSmall
    }

    Label {
        id: extra
        anchors {
            left: avatar.right
            leftMargin: Theme.paddingMedium
            top: footer.bottom
            right: parent.right
            rightMargin: Theme.paddingLarge
        }
        opacity: 0.6
        color: Theme.highlightColor
        width: parent.width
        font.pixelSize: Theme.fontSizeExtraSmall
    }
}
