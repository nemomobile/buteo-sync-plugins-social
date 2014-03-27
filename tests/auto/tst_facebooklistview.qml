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
    width: 100; height: 100

   FacebookListView {
        id: listView
        timestamp: "2014-03-27T11:39:37.732Z"
    }

    TestCase {
        name: "FacebookListView"
        when: windowShown

        function test_listView() {
            compare(listView.connectedToNetwork, false)
            verify(listView.timestamp.toString() !== "")
            compare(listView.body, "")
            compare(listView.source, "")
            compare(listView.posterId, "")
            compare(listView.imageSource, "")
            compare(listView.targetName, "")
            compare(listView.description, "")
        }
    }
}
