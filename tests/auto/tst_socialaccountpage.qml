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
    initialPage: accountPage

    SocialAccountPage {
        id: accountPage
        accounts: createAccounts()

        function createAccounts() {
            var accArray = new Array
            for (var i = 0; i < 100; i++) {
                accArray.push({"name": "Account" + i})
            }
            return accArray
        }
    }

    SignalSpy {
        id: clickedSpy
        target: accountPage
        signalName: "indexSelected"
    }
    TestEvent { id: testEvent }

    TestCase {
        name: "SocialAccountPage"
        when: windowShown

        function test_accountPage() {
            verify(accountPage === pageStack.currentPage)
            compare(accountPage.currentIndex, 0)
            verify(accountPage.accounts !== null)
            compare(accountPage.headerText, "")

            clickedSpy.clear()
            testEvent.mouseClick(accountPage, accountPage.width / 2, accountPage.height / 2, Qt.LeftButton, 0, 0)
            compare(clickedSpy.count, 1)
            verify(accountPage.currentIndex > 0)
        }
    }
}
