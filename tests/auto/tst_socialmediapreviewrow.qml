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
    id: item
    width: 100; height: 100

    SocialMediaPreviewRow {
        id: mediaRow
        width: parent.width
        imageList: images
    }

    ListModel {
        property int length: count
        id: images
    }

    TestCase {
        name: "SocialMediaPreviewRow"
        when: windowShown

        function test_socialMediaPreviewRow() {
            compare(mediaRow.imageList, images)
            compare(mediaRow.mediaName, "")
            compare(mediaRow.connectedToNetwork, false)
            compare(mediaRow.imageSize, item.width / 3)
            compare(mediaRow.imageCount, 0)

            images.append({"modelData": {"url": "Testimage1.png"}})
            images.append({"modelData": {"url": "Testimage2.png"}})
            images.append({"modelData": {"url": "Testimage3.png"}})
            images.append({"modelData": {"url": "Testimage4.png"}})
            compare(mediaRow.imageCount, 4)
        }
    }
}
