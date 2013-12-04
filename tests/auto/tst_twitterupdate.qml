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
        source: "eventfeed/twitter-update.qml"
    }

    TestCase {
        name: "TwitterUpdate"
        when: windowShown

        function test_twitterUpdate() {
            verify(loader.item !== null)
            verify(loader.item.signInParams.parameters["ConsumerKey"] !== "")
            verify(loader.item.signInParams.parameters["ConsumerSecrect"] !== "")
        }
    }
}
