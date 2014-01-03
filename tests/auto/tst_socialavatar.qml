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

    SocialAvatar {
        id: avatar
    }

    TestCase {
        name: "SocialAvatar"
        when: windowShown

        function test_avatar() {
            compare(avatar.width, Theme.itemSizeExtraLarge)
            compare(avatar.height, Theme.itemSizeExtraLarge)
            compare(avatar.fillMode, Image.PreserveAspectCrop)
            compare(avatar.sourceSize.width, avatar.width)
            compare(avatar.sourceSize.height, avatar.height)
        }
    }
}
