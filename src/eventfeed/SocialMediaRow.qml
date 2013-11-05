import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.TextLinking 1.0

Item {
    id: container
    property variant imageList
    property string mediaName
    property string mediaCaption
    property string mediaDescription
    property string mediaUrl
    property bool connectedToNetwork

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

            delegate: SocialImage {
                width: repeater.imageSize
                height: repeater.imageSize
                sourceSize {
                    width: repeater.imageSize
                    height: repeater.imageSize
                }
                fillMode: Image.PreserveAspectCrop
                source: model.modelData.url
                connectedToNetwork: container.connectedToNetwork

                MouseArea {
                    id: imageMouseArea
                    anchors.fill: parent
                    enabled: mediaUrl != ""
                    onClicked: Qt.openUrlExternally(mediaUrl)
                }

                Rectangle {
                    anchors.fill: parent
                    visible: imageMouseArea.pressed
                    color: Theme.highlightColor
                    opacity: 0.3
                }
            }
        }

        Item {
            width: repeater.imageSize
            height: repeater.imageSize
            visible: container.imageList.length === 1

            Text {
                id: mediaName
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
                text: container.mediaName
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                opacity: .6
                color: mediaNameMouseArea.pressed ? Theme.highlightColor : Theme.primaryColor

                MouseArea {
                    id: mediaNameMouseArea
                    width: mediaName.paintedWidth
                    height: mediaName.paintedHeight
                    enabled: mediaUrl != ""
                    onClicked: Qt.openUrlExternally(mediaUrl)
                }
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
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: 1
                shortenUrl: true
            }
        }
    }
}
