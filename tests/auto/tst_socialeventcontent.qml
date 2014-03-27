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

ApplicationWindow {
    id: window
    deviceOrientation: Orientation.Portrait
    initialPage: page

    Page {
        id: page

        SocialEventContent {
            id: socialEventContent
            parentPage: page
            timestamp: "2014-03-27T11:39:37.732Z"
        }
    }

    TestCase {
        name: "SocialEventContent"
        when: windowShown

        function test_socialEventContent() {
            compare(socialEventContent.parentPage, page)
            compare(socialEventContent.avatar, "")
            compare(socialEventContent.fallbackAvatar, "")
            compare(socialEventContent.source, "")
            verify(socialEventContent.timestamp.toString !== "")
            compare(socialEventContent.body, "")
            compare(socialEventContent.description, "")
            compare(socialEventContent.title, "")
            compare(socialEventContent.startTime,"")
            compare(socialEventContent.endTime, "")
            compare(socialEventContent.location, "")
            compare(socialEventContent.fullRowSocialButtons.length, 0)
            compare(socialEventContent.connectedToNetwork, false)
            compare(socialEventContent.rsvpStatus, "")
            compare(socialEventContent.personsGoing, "")
            compare(socialEventContent.eventImageSource, "")
        }
    }
}
