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

    property bool positioningCalled
    property bool syncCalled

    initialPage: Page {
        id: initPage
        SocialMediaAccountDelegate {
            id: accountDelegate
            feedPage: Page {
                id: page
                property int unseenPostCount
                property bool connectedToNetwork
                function positionViewAtBeginning() {
                    window.positioningCalled = true
                }
                function sync() {
                    window.syncCalled = true
                }
            }
        }
    }

    SignalSpy {
        id: clickedSpy
        target: accountDelegate
        signalName: "clicked"
    }
    TestEvent { id: testEvent }

    TestCase {
        name: "SocialMediaAccountDelegate"
        when: windowShown

        function test_accountDelegate() {
            compare(accountDelegate.width, initPage.width)
            compare(accountDelegate.height, accountDelegate.width * .2)
            compare(accountDelegate.subviewModel, null)
            compare(accountDelegate.text, "")
            compare(accountDelegate.iconSource, "")

            compare(accountDelegate.unseenPostCount, 0)
            page.unseenPostCount = 5
            compare(accountDelegate.unseenPostCount, 5)
        }

        function test_methods() {
            window.positioningCalled = false
            window.syncCalled = false

            testEvent.mouseClick(accountDelegate, accountDelegate.width / 2, accountDelegate.height / 2, Qt.LeftButton, 0, 0)
            compare(window.positioningCalled, true)
            compare(window.syncCalled, false)
            verify(page === pageStack.currentPage)

            accountDelegate.sync()
            compare(window.syncCalled, true)
        }
    }
}
