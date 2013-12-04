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

    initialPage: Page {
        id: page

        SocialReplyField {
            id: replyField
            width: parent.width
        }
    }

    TestCase {
        name: "SocialReplyField"
        when: windowShown

        function test_replyField() {
            compare(replyField.connectedToNetwork, false)
            compare(replyField.displayMargins, false)
            compare(replyField.placeholderText, "")
            compare(replyField.text, "")
            compare(replyField.errorHighlight, false)
            compare(replyField.label, "")
            compare(replyField.avatar, "")
            compare(replyField.allowComment, true)

            verify(replyField.height > 0)
            var oldHeight = replyField.height
            replyField.displayMargins = true
            compare(replyField.displayMargins, true)
            verify(replyField.height > oldHeight)
            oldHeight = replyField.height

            replyField.text = "Test text"
            compare(oldHeight, replyField.height)
            compare(replyField.text, "Test text")
            replyField.clear()
            compare(replyField.text, "")

            replyField.text = "Another test text"
            compare(replyField.text, "Another test text")
            replyField.close()
            compare(replyField.text, "")

            replyField.forceActiveFocus()
        }
    }
}
