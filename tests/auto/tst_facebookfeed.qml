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
import org.nemomobile.socialcache 1.0

ApplicationWindow {
    id: window
    deviceOrientation: Orientation.Portrait

    Loader {
        id: loader
        source: "eventfeed/facebook-feed.qml"
    }

    TestCase {
        name: "FacebookFeed"
        when: windowShown

        function test_facebookFeed() {
            verify(loader.item !== null)
            compare(loader.item.configKey, "/saifish/events_view/facebook_notifications")
            compare(loader.item.timestampRole, FacebookNotificationsModel.Timestamp)
            verify(loader.item.listModel !== null)
            compare(loader.item.socialNetwork, SocialSync.Facebook)
            compare(loader.item.dataType, SocialSync.Notifications)
            compare(loader.item.syncNotifications, true)
            compare(loader.item.headerTitle, "lipstick-jolla-home-la-facebook_notifications")

            compare(loader.item.clientId, "")
            compare(loader.item.readyToPopulate, false)
        }
    }
}
