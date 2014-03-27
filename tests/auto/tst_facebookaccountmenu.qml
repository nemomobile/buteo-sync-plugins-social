/****************************************************************************************
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: Antti Seppälä <antti.seppala@jollamobile.com>
** All rights reserved.
**
** This file is part of sociald package.
**
****************************************************************************************/

import QtTest 1.0
import QtQuick 2.0
import Sailfish.Silica 1.0
import "eventfeed"

ApplicationWindow {
    id: window
    deviceOrientation: Orientation.Portrait

    Item {
        id: subviewModel

        function accountList(idString) {
            var result = new Array
            result.push({"identifier": 2, "displayName": "Test account"})
            return result
        }
    }

    FacebookAccountMenu {
        id: accountMenu

        property QtObject model: QtObject {
            property QtObject accounts: QtObject {
                function indexOf(what) {
                    if (what === 2) {
                        return 0
                    }
                    return -1
                }
            }
        }
    }

    TestCase {
        name: "FacebookAccountMenu"
        when: windowShown

        function test_facebookAccountMenu() {
            compare(accountMenu.linkTitle, "lipstick-jolla-home-me-open_in_facebook")
            compare(accountMenu.selectAccountString, "lipstick-jolla-home-la-select-account")
            compare(accountMenu.changeToAccountString, "lipstick-jolla-home-la-change-to-account")
            compare(accountMenu.accountString, "lipstick-jolla-home-la-account-name")
            compare(accountMenu.switchEnabled, false)
            compare(accountMenu.serviceName, "facebook")
        }
    }
}
