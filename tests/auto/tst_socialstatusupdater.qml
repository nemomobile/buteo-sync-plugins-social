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
import org.nemomobile.notifications 1.0 as SystemNotifications
import "eventfeed/shared"

Item {
    id: item
    width: 100; height: 100

    SocialStatusUpdater {
        id: statusUpdater
        function notifyFailure() {}
    }

    SystemNotifications.Notification {
        id: systemNotification
    }

    TestCase {
        name: "SocialStatusUpdater"
        when: windowShown

        function test_statusUpdater() {
            compare(statusUpdater.statusUpdate, "")
            compare(statusUpdater.accountId, 0)
            verify(statusUpdater.keyProvider !== null)
            verify(statusUpdater.account !== null)
        }

        function test_publish() {
            var timestamp = new Date().getTime()
            statusUpdater.publishNotification("x-nemo.social.twitter.tweet",
                                              "Test status update " + timestamp,
                                              "sociald: Testing notification")

            wait(100)
            compare(hasNotification(timestamp), true)
        }

        function hasNotification(timestamp) {
            var list = systemNotification.notifications()
            for (var i = 0; i < list.length; ++i) {
                var notif = list[i]
                if (notif.category === "x-nemo.social.twitter.tweet"
                     && notif.body === "Test status update " + timestamp
                     && notif.summary === "sociald: Testing notification") {
                    return true
                }
            }
            return false
        }
    }
}
