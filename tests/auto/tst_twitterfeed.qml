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
import org.nemomobile.socialcache 1.0

ApplicationWindow {
    id: window
    deviceOrientation: Orientation.Portrait

    Loader {
        id: loader
        source: "eventfeed/twitter-feed.qml"
    }

    TestCase {
        name: "TwitterFeed"
        when: windowShown

        function test_twitterFeed() {
            verify(loader.item !== null)
            compare(loader.item.configKey, "/sailfish/events_view/twitter")
            compare(loader.item.timestampRole, 4) //TwitterPostsModel.Timestamp
            verify(loader.item.listModel !== null)
            compare(loader.item.socialNetwork, SocialSync.Twitter)
            compare(loader.item.syncNotifications, true)
            compare(loader.item.headerTitle, "lipstick-jolla-home-la-twitter")
            compare(loader.item.accountAndModelsReady, false)
            compare(loader.item.twitterUser, null)
        }
    }
}
