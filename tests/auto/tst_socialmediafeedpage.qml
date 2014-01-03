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
import org.nemomobile.configuration 1.0
import "eventfeed/shared"

ApplicationWindow {
    id: window
    deviceOrientation: Orientation.Portrait
    initialPage: feedPage

    property bool modelWasRefreshed

    SocialMediaFeedPage {
        id: feedPage
        listModel: _listModel
        configKey: "/testKey"
    }

    ListModel {
        id: _listModel

        signal modelUpdated()

        function getField(value1, value2) {
            return new Date().toJSON()
        }
        function refresh() {
            window.modelWasRefreshed = true
        }
    }

    ConfigurationValue {
        id: lastSeenTime
        key: feedPage.configKey + "_last_seen_time"
    }

    TestCase {
        name: "SocialMediaFeedPage"
        when: windowShown

        function test_feedPage() {
            compare(feedPage.listModel, _listModel)
            compare(feedPage.listDelegate, null)
            compare(feedPage.socialNetwork, 0)
            compare(feedPage.configKey, "/testKey")
            compare(feedPage.timestampRole, 0)
            compare(feedPage.headerTitle, "")
            verify(feedPage.listView !== null)
            compare(feedPage.updating, false)
            compare(feedPage.syncNotifications, false)
            compare(feedPage.connectedToNetwork, false)
            compare(feedPage.refreshTimeCount, 2)  // Status change increases this by one
            compare(window.modelWasRefreshed, true)
        }

        function test_timeUpdate() {
            _listModel.clear()
            var prevTime = lastSeenTime.value
            feedPage.setLastSeenTime()
            wait(50)
            compare(prevTime, lastSeenTime.value)

            _listModel.append({"test": "Test"})
            feedPage.setLastSeenTime()
            wait(50)
            verify(lastSeenTime.value > prevTime)

            compare(feedPage.unseenPostCount, 0)
            prevTime = lastSeenTime.value
            feedPage.listUpdated()
            wait(50)
            verify(lastSeenTime.value > prevTime)
            compare(feedPage.unseenPostCount, 0)

            feedPage.visible = false
            wait(50)
            feedPage.listUpdated()
            compare(feedPage.unseenPostCount, 1)

            // also trigger via modelUpdated signal
            feedPage.unseenPostCount = 0
            _listModel.modelUpdated()
            compare(feedPage.unseenPostCount, 1)
        }
    }
}
