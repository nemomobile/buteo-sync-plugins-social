import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.TextLinking 1.0

Item {
    id: container

    property variant imageList
    property string mediaName
    property string mediaCaption
    property string mediaDescription
    property real imageSize: width / 3
    property int imageCount: imageList ? imageList.length : 0

    width: parent.width
    height: childrenRect.height
    visible: imageCount > 0

    Row {
        anchors {
            left: parent.left
            right: parent.right
        }

        Repeater {
            id: repeater
            model: container.imageList
            delegate: Image {
                width: container.imageSize
                height: container.imageSize
                sourceSize {
                    width: container.imageSize
                    height: container.imageSize
                }
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                visible: index < container.imageCount
                source: visible ? modelData.url : ""
            }
        }
    }

    Label {
        width: container.imageSize * 2 - Theme.paddingSmall
        x: container.imageSize + Theme.paddingMedium
        y: Theme.paddingMedium
        visible: container.imageCount === 1
        text: container.mediaName
        font.pixelSize: Theme.fontSizeExtraSmall
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        opacity: .6
    }
}
