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

    SocialMediaRow {
        id: mediaRow
        width: parent.width
        imageList: images
    }

    ListModel {
        property int length: count
        id: images
    }

    TestCase {
        name: "SocialMediaRow"
        when: windowShown

        function test_socialMediaRow() {
            compare(mediaRow.imageList, images)
            compare(mediaRow.mediaName, "")
            compare(mediaRow.mediaCaption, "")
            compare(mediaRow.mediaDescription, "")
            compare(mediaRow.mediaUrl, "")
            compare(mediaRow.connectedToNetwork, false)
        }
    }
}
