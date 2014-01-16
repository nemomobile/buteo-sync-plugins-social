import QtQuick 2.0
import Sailfish.Silica 1.0
import "shared"
import org.nemomobile.dbus 1.0

/*
// We cannot override the onClicked() handler of the SMAD
// so instead, comment it out and provide an API-compatible
// replacement, which just loads the Facebook website.
SocialMediaAccountDelegate {
    id: delegate
    iconSource: "image://theme/graphic-service-facebook"
    //: New facebook posts
    //% "New posts"
    text: unseenPostCount > 0 ? qsTrId("lipstick-jolla-home-la-new_facebook_posts")
                                //: Facebook posts
                                //% "Posts"
                              : qsTrId("lipstick-jolla-home-la-facebook_posts")
}
*/

BackgroundItem {
    id: item

    width: parent.width
    height: Screen.width * .2

    property Page feedPage
    property Item subviewModel
    property alias text: label.text
    property alias iconSource: icon.source
    property int unseenPostCount: feedPage ? feedPage.unseenPostCount : 0

    Component.onCompleted: {
        iconSource = "image://theme/graphic-service-facebook"
        //: New facebook posts
        //% "New posts"
        text = Qt.binding(function() {
                return unseenPostCount > 0 ? qsTrId("lipstick-jolla-home-la-new_facebook_posts")
                        //: Facebook posts
                        //% "Posts"
                      : qsTrId("lipstick-jolla-home-la-facebook_posts") });
    }

    onClicked: {
        dbusBrowser.typedCall("openUrl", [{"type":"as", "value":["https://touch.facebook.com/"]}])
    }

    DBusInterface {
        id: dbusBrowser
        destination: "org.sailfishos.browser"
        path:        "/"
        iface:       "org.sailfishos.browser"
        busType:     DBusInterface.SessionBus
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

