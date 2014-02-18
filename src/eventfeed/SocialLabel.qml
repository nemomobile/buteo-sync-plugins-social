import QtQuick 2.0
import Sailfish.Silica 1.0

Label {
    property real maxOpacity: 1.0

    width: parent.width
    visible: text.length > 0
    opacity: visible ? maxOpacity : 0
    wrapMode: Text.Wrap
    elide: Text.ElideRight

    Behavior on opacity { FadeAnimation {} }
}
