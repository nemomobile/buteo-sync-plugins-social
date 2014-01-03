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

    SocialImage {
        id: image
        width: Theme.itemSizeLarge
        height: Theme.itemSizeSmall

        fillMode: Image.PreserveAspectFit
        sourceSize.width: width
        sourceSize.height: height
    }

    TestCase {
        name: "SocialImage"
        when: windowShown

        function test_image() {
            compare(image.source, "")
            compare(image.placeholderSource, "")
            compare(image.connectedToNetwork, false)
            compare(image.fillMode, Image.PreserveAspectFit)
            compare(image.sourceSize.width, image.width)
            compare(image.sourceSize.height, image.height)
        }
    }
}
