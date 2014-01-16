import QtQuick 2.0
import Sailfish.Silica 1.0

BackgroundItem {
    id: item
    width: parent.width
    height: Screen.width * .2

    property Page feedPage
    property Item subviewModel
    property alias text: label.text
    property alias iconSource: icon.source
    property int unseenPostCount: feedPage ? feedPage.unseenPostCount : 0

    onClicked: {
        pageStack.push(feedPage)
        feedPage.positionViewAtBeginning()
    }

    Label {
        anchors {
            right: icon.left
            rightMargin: Theme.paddingLarge
            verticalCenter: parent.verticalCenter
        }
        color: item.pressed ? Theme.highlightColor : Theme.primaryColor
        text: item.unseenPostCount
        visible: item.unseenPostCount > 0
    }

    SocialImage {
        id: icon
        x: item.height
        width: height
        height: item.height
        connectedToNetwork: feedPage ? feedPage.connectedToNetwork : false
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
