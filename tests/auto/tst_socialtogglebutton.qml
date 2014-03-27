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
import "eventfeed/shared"

Item {
    width: 200; height: 300

    SocialToggleButton {
        id: button
        width: parent.width
    }
    SignalSpy {
        id: clickedSpy
        target: button
        signalName: "clicked"
    }

    TestEvent { id: testEvent }

    TestCase {
        name: "SocialToggleButton"
        when: windowShown

        function test_toggleButton() {
            compare(button.text, "")
            verify(button.height > 2 * Theme.paddingLarge)
        }

        function test_click() {
            compare(button.down, false)

            clickedSpy.clear()

            testEvent.mousePress(button, button.width / 2, button.height / 2, Qt.LeftButton, 0, 0)
            compare(button.down, true)
            compare(clickedSpy.count, 0)

            testEvent.mouseRelease(button, button.width / 2, button.height / 2, Qt.LeftButton, 0, 0)
            compare(button.down, false)
            compare(clickedSpy.count, 1)

            compare(button.locked, false)
            button.locked = true
            compare(button.down, true)
        }
    }
}
