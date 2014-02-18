import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.TextLinking 1.0

Column {
    property alias text: body.plainText
    property alias time: time.text

    width: parent.width

    LinkedText {
        id: body
        width: parent.width
        color: Theme.highlightColor
        wrapMode: Text.WordWrap
        font.pixelSize: Theme.fontSizeSmall
        shortenUrl: true
        visible: text.length > 0
        opacity: visible ? 1.0 : 0
        Behavior on opacity { FadeAnimation {} }
    }
    Label {
        id: time
        color: Theme.highlightColor
        opacity: 0.6
        font.pixelSize: Theme.fontSizeExtraSmall
    }

    Behavior on height { FadeAnimation {} }
}
