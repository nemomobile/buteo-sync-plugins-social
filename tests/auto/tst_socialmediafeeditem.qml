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

    Item {
        id: model
        property string timestamp
        property string icon
    }
    SocialMediaFeedItem {
        id: feedItem
        width: parent.width
        height: Theme.itemSizeMedium * 2
    }
    SignalSpy {
        id: clickedSpy
        target: feedItem
        signalName: "clicked"
    }
    TestEvent { id: testEvent }


    TestCase {
        name: "SocialMediaFeedItem"
        when: windowShown

        function test_feedItem() {
            compare(feedItem.connectedToNetwork, false)
            verify(feedItem.avatar !== null)
            compare(feedItem.avatar.width, Theme.itemSizeMedium)
            compare(feedItem.avatar.height, Theme.itemSizeMedium)
            compare(feedItem.avatar.source, "")
            compare(feedItem.avatar.connectedToNetwork, false)
            compare(feedItem.refreshTimeCount, 1)
            compare(feedItem.avatarSource, "")
            compare(feedItem.fallbackAvatarSource, "")

            feedItem.connectedToNetwork = true
            compare(feedItem.connectedToNetwork, true)
            compare(feedItem.avatar.connectedToNetwork, true)
        }

        function test_timeFormatting() {
            compare(feedItem.timestamp, "")
            compare(feedItem.formattedTime, "")

            var date = new Date()
            var timestamp = date.toJSON()
            model.timestamp = timestamp
            compare(feedItem.timestamp, timestamp)
            verify(feedItem.formattedTime !== "")

            var oldFormattedTime = feedItem.formattedTime
            date.setFullYear(date.getFullYear() + 1)
            timestamp = date.toJSON()
            model.timestamp = timestamp
            compare(feedItem.timestamp, timestamp)
            verify(feedItem.formattedTime !== oldFormattedTime)

            feedItem.refreshTimeCount = feedItem.refreshTimeCount + 1
        }

        function test_click() {
            clickedSpy.clear()
            testEvent.mouseClick(feedItem, feedItem.width / 2, feedItem.height / 2, Qt.LeftButton, 0, 0)
            compare(clickedSpy.count, 1)
        }
    }
}
