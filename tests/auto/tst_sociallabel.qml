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
    id: item
    width: 100; height: 100

    SocialLabel {
        id: label
        text: "Testing"
    }

    FadeAnimation { id: fadeAnimation }

    TestCase {
        name: "SocialLabel"
        when: windowShown

        function test_socialLabel() {
            compare(label.text, "Testing")
            compare(label.width, item.width)
            compare(label.opacity, 1.0)
            compare(label.wrapMode, Text.Wrap)
            compare(label.elide, Text.ElideRight)
            compare(label.visible, true)

            label.text = ""
            compare(label.text, "")
            wait(fadeAnimation.duration + 50)
            compare(label.opacity, 0)
            compare(label.visible, false)
        }
    }
}
