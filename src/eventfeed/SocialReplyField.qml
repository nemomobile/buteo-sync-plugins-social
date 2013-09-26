import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: container
    property bool displayMargins
    property alias avatar: avatar.source
    property alias placeholderText: textField.placeholderText
    property alias text: textField.text
    property alias allowComment: textField.enabled
    property alias errorHighlight: textField.errorHighlight
    property alias label: textField.label
    signal enterKeyClicked()

    function forceActiveFocus() {
        textField.forceActiveFocus()
    }

    function close() {
        textField.focus = false
        textField.text = ""
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

    Rectangle {
        anchors.fill: avatar
        color: Theme.highlightColor
        opacity: 0.5
    }

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

        EnterKey.highlighted: text.length > 0
        EnterKey.iconSource: "image://theme/icon-m-enter-" + (text.length > 0 ? "accept" : "close")
        EnterKey.onClicked: {
            container.enterKeyClicked()
        }
    }
}
