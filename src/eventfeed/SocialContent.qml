import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: container
    property Page parentPage
    property alias avatar: socialAvatar.source
    property alias fallbackAvatar: socialAvatar.fallbackSource
    property string source
    property string subSource
    property date timestamp
    property alias body: body.text
    property alias socialButtons: socialButtonsContainer.children
    property alias fullRowSocialButtons: fullRowSocialButtonsContainer.children
    property bool connectedToNetwork

    height: childrenRect.height
    anchors {
        left: parent.left
        right: parent.right
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.highlightColor
        opacity: 0.1
    }

    Item {
        id: header
        anchors {
            left: parent.left
            right: parent.right
        }
        height: Theme.itemSizeLarge

        PageHeader {
            visible: subSource == ""
            title: container.source
        }

        Column {
            id: column

            property int _depth: parentPage && parentPage._depth ? parentPage._depth + 1 : 0

            visible: subSource != ""
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                right: parent.right
            }
            spacing: -Theme.paddingSmall

            Label {
                width: Math.min(implicitWidth, header.width - Theme.pageStackIndicatorWidth * column._depth
                                - 2 * Theme.paddingLarge)
                truncationMode: TruncationMode.Fade
                color: Theme.highlightColor
                anchors {
                    right: parent.right
                    rightMargin: Theme.paddingLarge
                }
                font {
                    pixelSize: Theme.fontSizeLarge
                    family: Theme.fontFamilyHeading
                }
                text: container.source
            }

            Label {
                width: Math.min(implicitWidth, header.width - Theme.pageStackIndicatorWidth * column._depth
                                - 2 * Theme.paddingLarge)
                truncationMode: TruncationMode.Fade
                color: Theme.highlightColor
                opacity: 0.6
                anchors {
                    right: parent.right
                    rightMargin: Theme.paddingLarge
                }
                font {
                    pixelSize: Theme.fontSizeSmall
                    family: Theme.fontFamilyHeading
                }
                text: container.subSource
            }
        }
    }

    SocialAvatar {
        id: socialAvatar
        anchors.top: header.bottom
        connectedToNetwork: container.connectedToNetwork
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

        time: Format.formatDate(container.timestamp, Formatter.DurationElapsed)
    }

    // This item is used to anchors social buttons and full-row social buttons content
    Item {
        id: mainContentMask
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: fullRowSocialButtonsContainer.childrenRect.height > 0 ? Math.max(body.height, socialAvatar.height)
                                                                      : body.height
    }

    Item {
        id: fullRowSocialButtonsContainer
        anchors {
            top: mainContentMask.bottom
            left: socialAvatar.left
            leftMargin: Theme.paddingLarge
            right: parent.right
            rightMargin: Theme.paddingLarge
        }
        height: childrenRect.height
    }

    Item {
        id: socialButtonsContainer
        anchors {
            top: mainContentMask.bottom
            left: socialAvatar.right
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.paddingLarge
        }
        height: childrenRect.height
    }
}
