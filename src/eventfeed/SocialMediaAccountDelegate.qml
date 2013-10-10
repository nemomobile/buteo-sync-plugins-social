import QtQuick 2.0
import Sailfish.Silica 1.0

BackgroundItem {
    width: parent.width
    height: Theme.itemSizeLarge

    property Page feedPage
    property Item subviewModel
    property alias iconSource: icon.source

    onClicked: {
        pageStack.push(feedPage)
        feedPage.positionViewAtBeginning()
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.rgba(Theme.highlightColor, 0) }
            GradientStop { position: 1; color: Theme.rgba(Theme.highlightColor, 0.2) }
        }
    }

    Image {
        id: icon
        anchors {
            verticalCenter: parent.verticalCenter
            right: parent.right
            rightMargin: Theme.paddingLarge
        }
        width: parent.height / 2
        height: width
    }

    function sync() {
        feedPage.sync()
    }
}
