import QtQuick 2.0
import Sailfish.Silica 1.0
import "shared"

SocialAccountPullDownMenu {
    //: Opens Facebook notification in browser
    //% "Open in Facebook"
    linkTitle: qsTrId("lipstick-jolla-home-me-open_in_facebook")
    selectAccountString: qsTrId("lipstick-jolla-home-la-select-account")
    //% "Change to %1"
    changeToAccountString: qsTrId("lipstick-jolla-home-la-change-to-account")
    //% "Account: %1"
    accountString: qsTrId("lipstick-jolla-home-la-account-name")
    switchEnabled: false
    serviceName: "facebook"
}
