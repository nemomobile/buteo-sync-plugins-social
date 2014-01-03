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

    SocialBody {
        id: body
        width: parent.width
    }

    TestCase {
        name: "SocialBody"
        when: windowShown

        function test_body() {
            compare(body.text, "")
            compare(body.time, "")
        }
    }
}
