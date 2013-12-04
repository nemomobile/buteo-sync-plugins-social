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

Item {
    width: 100; height: 100

    SocialInfoLabel {
        id: label
        width: parent.width
        text: "Testing"
    }

    TestCase {
        name: "SocialInfoLabel"
        when: windowShown

        function test_infoLabel() {
            compare(label.text, "Testing")
            verify(label.height > 2 * Theme.paddingLarge)
            compare(label.opacity, 1.0)

            label.text = ""
            compare(label.text, "")
            wait(300) // wait until fade animation is over
            compare(label.height, Theme.paddingLarge)
            compare(label.opacity, 0)
        }
    }
}
