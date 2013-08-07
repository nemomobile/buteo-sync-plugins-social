import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: container
    property bool displayMargins
    property alias avatar: avatar.source
    property alias placeholderText: textField.placeholderText
    property alias text: textField.text
    property alias allowComment: textField.enabled
    signal enterKeyClicked()

    function forceActiveFocus() {
        textField.forceActiveFocus()
    }

    function clear() {
        textField.text = ""
    }

    anchors {
        left: parent.left
        right: parent.right
    }
    height: childrenRect.height  + Theme.paddingMedium
            + (displayMargins ? Theme.paddingLarge : 0)

    Behavior on opacity { FadeAnimation {} }

    Image {
        id: avatar
        anchors {
            left: parent.left
            top: parent.top
            topMargin: container.displayMargins ? Theme.paddingLarge : 0
        }
        width: Theme.iconSizeMedium
        height: Theme.iconSizeMedium
    }

    TextField {
        id: textField
        anchors {
            top: parent.top
            topMargin: container.displayMargins ? Theme.paddingLarge : 0
            left: avatar.right
            right: parent.right
        }

        EnterKey.onClicked: {
            container.enterKeyClicked()
        }
    }
}
