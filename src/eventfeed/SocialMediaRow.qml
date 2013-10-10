import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.TextLinking 1.0

Item {
    id: container
    property variant imageList
    property string mediaName
    property string mediaCaption
    property string mediaDescription

    anchors {
        left: parent.left
        right: parent.right
    }

    height: childrenRect.height
    visible: imageList.length > 0

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.rgba(Theme.highlightColor, 0) }
            GradientStop { position: 1; color: Theme.rgba(Theme.highlightColor, 0.05) }
        }
    }

    Row {
        anchors {
            left: parent.left
            right: parent.right
        }

        Repeater {
            id: repeater
            property real imageSize: container.imageList.length <= 2 ? container.width / 2
                                                                     : container.width / 4
            model: container.imageList

            delegate: Image {
                width: repeater.imageSize
                height: repeater.imageSize
                sourceSize {
                    width: repeater.imageSize
                    height: repeater.imageSize
                }
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                source: model.modelData.url
            }
        }

        Item {
            width: repeater.imageSize
            height: repeater.imageSize
            visible: container.imageList.length === 1

            LinkedText {
                anchors {
                    top: parent.top
                    topMargin: Theme.paddingMedium
                    left: parent.left
                    leftMargin: Theme.paddingMedium
                    right: parent.right
                    rightMargin: Theme.paddingMedium
                    bottom: caption.top
                    bottomMargin: Theme.paddingSmall
                }
                plainText: container.mediaName
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                shortenUrl: true
                opacity: .6
            }

            LinkedText {
                id: caption
                anchors {
                    bottom: parent.bottom
                    bottomMargin: Theme.paddingMedium
                    left: parent.left
                    leftMargin: Theme.paddingMedium
                    right: parent.right
                    rightMargin: Theme.paddingMedium
                }
                plainText: container.mediaCaption
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: 1
                shortenUrl: true
            }
        }
    }
}
