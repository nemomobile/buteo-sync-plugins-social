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

ApplicationWindow {
    id: window
    deviceOrientation: Orientation.Portrait

    Loader {
        id: loader
        source: "eventfeed/twitter-delegate.qml"
    }

    TestCase {
        name: "SocialMediaAccountDelegate"
        when: windowShown

        function test_twitterAccountDelegate() {
            verify(loader.item !== null)
            compare(loader.item.iconSource, "image://theme/graphic-service-twitter")
            compare(loader.item.text, "lipstick-jolla-home-la-twitter_tweets")
            loader.item.unseenPostCount = 1
            compare(loader.item.text, "lipstick-jolla-home-la-new_twitter_tweets")
        }
    }
}
