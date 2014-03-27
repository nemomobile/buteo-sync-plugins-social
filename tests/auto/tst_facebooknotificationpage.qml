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
import "eventfeed"

ApplicationWindow {
    id: window
    deviceOrientation: Orientation.Portrait
    initialPage: notificationPage

    FacebookNotificationPage {
        id: notificationPage
        model: QtObject {
            property string object: "Test"
        }
    }

    TestCase {
        name: "FacebookNotificationPage"
        when: windowShown

        function test_facebookNotificationPage() {
            compare(notificationPage.nodeIdentifier, "Test")
            compare(notificationPage.subviewModel, null)
            compare(notificationPage.allowLike, true)
            compare(notificationPage.allowComment, true)
            compare(notificationPage.connectedToNetwork, false)
            compare(notificationPage.readyToPopulate, false)
            compare(notificationPage.account, null)
            compare(notificationPage.facebook, null)
            compare(notificationPage.facebookMe, null)
            compare(notificationPage.facebookUser, null)
        }
    }
}
