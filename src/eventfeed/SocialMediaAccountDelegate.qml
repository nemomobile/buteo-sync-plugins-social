import QtQuick 2.0
import Sailfish.Silica 1.0

BackgroundItem {
    id: item
    width: parent.width
    height: width * .2

    property Page feedPage
    property Item subviewModel
    property alias text: label.text
    property alias iconSource: icon.source

    onClicked: {
        pageStack.push(feedPage)
        feedPage.positionViewAtBeginning()
    }

    Image {
        id: icon
        x: item.height
        width: height
        height: item.height
    }

    Label {
        id: label
        color: item.pressed ? Theme.highlightColor : Theme.primaryColor
        anchors {
            verticalCenter: parent.verticalCenter
            left: icon.right
            leftMargin: Theme.paddingLarge
        }
    }

    function sync() {
        feedPage.sync()
    }
}
