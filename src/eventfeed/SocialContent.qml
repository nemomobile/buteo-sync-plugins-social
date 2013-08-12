import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: container
    property alias avatar: socialAvatar.icon
    property alias source: header.title
    property date timestamp
    property alias body: body.text
    property alias belowAvatar: belowAvatarContainer.children
    property alias socialButtons: socialButtonsContainer.children

    height: childrenRect.height
    anchors {
        left: parent.left
        right: parent.right
    }

    Formatter {
        id: formatter
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.highlightColor
        opacity: 0.1
    }

    PageHeader {
        id: header
    }

    SocialAvatar {
        id: socialAvatar
        anchors.top: header.bottom
    }

    SocialBody {
        id: body
        anchors {
            top: header.bottom
            left: socialAvatar.right
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.paddingLarge
        }

        time: formatter.formatDate(container.timestamp, Formatter.DurationElapsed)
    }

    // This item is used to anchors social buttons and below avatar content
    Item {
        id: mainContentMask
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: belowAvatarContainer.childrenRect.height > 0 ? Math.max(body.height, socialAvatar.height)
                                                             : body.height
    }

    Item {
        id: belowAvatarContainer
        anchors {
            left: socialAvatar.left
            right: socialAvatar.right
            top: mainContentMask.bottom
        }
        height: socialButtonsContainer.height
    }

    Item {
        id: socialButtonsContainer
        anchors {
            top: mainContentMask.bottom
            left: belowAvatarContainer.right
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.paddingLarge
        }
        height: childrenRect.height
    }
}
