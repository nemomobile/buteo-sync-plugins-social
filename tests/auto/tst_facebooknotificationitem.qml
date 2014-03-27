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

Item {
    id: item
    width: Screen.width; height: Screen.height

    Item {
        id: model
        property string timestamp: "2014-03-27T11:39:37.732Z"
        property string title: "Testing"
        property string from
    }

    FacebookNotificationItem {
        id: notificationItem
    }

    TestCase {
        name: "FacebookNotificationItem"
        when: windowShown

        function test_facebookNotificationItem() {
            compare(notificationItem.refreshTimeCount, 1)
            compare(notificationItem.avatarSource, "")
            compare(notificationItem.timestamp, "2014-03-27T11:39:37.732Z")
            verify(notificationItem.formattedTime !== "")
            compare(notificationItem.width, item.width)

            model.from = "test"
            compare(notificationItem.avatarSource, "https://graph.facebook.com/test/picture?width=200&height=200")
            compare(notificationItem.height, notificationItem.avatar.height + Theme.paddingMedium * 3)
        }
    }
}
