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
    width: 100; height: 500

    QtObject {
        id: model
        property QtObject contentItem: Item {
            property string message: "Test message"
        }
    }

    SocialComment {
        id: comment
        width: parent.width
    }

    FadeAnimation { id: fadeAnimation }

    TestCase {
        name: "SocialComment"
        when: windowShown

        function test_comment() {
            wait(fadeAnimation.duration + 50)
            compare(comment.opacity, 1.0)
            compare(comment.avatar, "")
            compare(comment.message, "Test message")
            compare(comment.footer, "")
            compare(comment.extra, "")
            compare(comment.extraVisible, true)
            compare(comment.connectedToNetwork, false)
            verify(comment.height >= Theme.iconSizeMedium)
        }
    }
}
