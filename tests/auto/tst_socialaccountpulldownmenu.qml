/****************************************************************************************
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: Antti Seppälä <antti.seppala@jollamobile.com>
** All rights reserved.
**
** This file is part of sociald package.
**
****************************************************************************************/

import QtTest 1.0
import QtQuick 2.0
import Sailfish.Silica 1.0
import "eventfeed/shared"

ApplicationWindow {
    id: window
    deviceOrientation: Orientation.Portrait

    Page {
        id: page

        SilicaListView {
            id: listView
            anchors.fill: parent
            model: ListModel {}

            Item {
                id: subviewModel

                function accountList(idString) {
                    var result = new Array
                    result.push({"identifier": 2, "displayName": "Test account"})
                    return result
                }
            }

            SocialAccountPullDownMenu {
                id: pullDownMenu
                pageContainer: page
                accountString: "Account %1"

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
        }
    }

    initialPage: page

    TestCase {
        name: "SocialAccountPullDownMenu"
        when: windowShown

        function test_pullDownMenu() {
            compare(pullDownMenu.currentAccount, 2)
            compare(pullDownMenu.currentAccountIndex, 0)
            compare(pullDownMenu.selectAccountString, "")
            compare(pullDownMenu.changeToAccountString, "")
            compare(pullDownMenu.accountString, "Account %1")
            compare(pullDownMenu.pageContainer, page)
            compare(pullDownMenu.serviceName, "")
            compare(pullDownMenu.switchEnabled, false)
            compare(pullDownMenu.link, "")
            compare(pullDownMenu.linkTitle, "")
        }
    }
}
