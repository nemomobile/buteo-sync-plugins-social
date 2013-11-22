import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.TextLinking 1.0

Item {
    id: container

    property var imageList
    property string mediaName
    property string mediaCaption
    property string mediaDescription
    property real imageSize: width / 3
    property int imageCount: imageList ? imageList.length : 0
    property bool connectedToNetwork

    width: parent.width
    height: childrenRect.height
    visible: imageCount > 0

    Row {
        width: parent.width

        Repeater {
            id: repeater
            model: container.imageList
            delegate: SocialImage {
                width: container.imageSize
                height: container.imageSize
                sourceSize {
                    width: container.imageSize
                    height: container.imageSize
                }
                fillMode: Image.PreserveAspectCrop
                visible: index < container.imageCount
                source: modelData.url
                connectedToNetwork: container.connectedToNetwork
            }
        }
    }

    Label {
        width: container.imageSize * 2 - Theme.paddingSmall
        x: container.imageSize + Theme.paddingMedium
        visible: container.imageCount === 1
        text: container.mediaName
        font.pixelSize: Theme.fontSizeSmall
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        opacity: .6
    }
}
