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
    initialPage: page

    Page {
        id: page

        SocialContent {
            id: socialContent
            parentPage: page
            timestamp: "2001-01-01"
        }
    }

    TestCase {
        name: "SocialContent"
        when: windowShown

        function test_socialContent() {
            compare(socialContent.parentPage, page)
            compare(socialContent.avatar, "")
            compare(socialContent.fallbackAvatar, "")
            compare(socialContent.source, "")
            compare(socialContent.subSource, "")
            verify(socialContent.timestamp.toString !== "")
            compare(socialContent.body, "")
            compare(socialContent.socialButtons.length, 0)
            compare(socialContent.fullRowSocialButtons.length, 0)
            compare(socialContent.connectedToNetwork, false)
        }
    }
}
