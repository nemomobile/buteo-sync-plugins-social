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
import "eventfeed"

Item {
    width: 100; height: 100

    Item {
        id: model
        property string timestamp
        property string icon
        property string retweeter
        property string name
        property string screenName
        property string body
    }

    TwitterFeedItem {
        id: feedItem
        width: parent.width
        height: Theme.itemSizeMedium * 2
    }

    TestCase {
        name: "TwitterFeedItem"
        when: windowShown

        function test_twitterFeedItem() {
            compare(feedItem.imageList, undefined)
            compare(feedItem.likeCount, 0)
            compare(feedItem.commentCount, 0)
            compare(feedItem.retweetCount, 0)
        }
    }
}
