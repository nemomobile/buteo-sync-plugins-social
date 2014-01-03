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
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import "eventfeed"

ApplicationWindow {
    id: window
    deviceOrientation: Orientation.Portrait
    initialPage: twitterPostPage

    property bool identifiersSet

    TwitterPostPage {
        id: twitterPostPage

        subviewModel: Item {
            function accountList(idString) {
                var result = new Array
                result.push({"identifier": 2, "displayName": "Test account"})
                return result
            }
        }
        model: QtObject {
            property string retweeter: "Test retweeter"
            property date timestamp
            property string body
            property string icon
            property string name
            property string screenName
            property QtObject accounts: QtObject {
                function indexOf(what) {
                    if (what === 2) {
                        return 0
                    }
                    return -1
                }
            }
            property var images: []
        }
        account: Account {
            function setIdentifiers(accountId) {
                window.identifiersSet = true
            }
        }
        twitterUser: TwitterUser {}
        twitterReplies: SocialNetworkModel {}
    }

    TestCase {
        name: "TwitterPostPage"
        when: windowShown

        function test_twitterPostPage() {
            compare(twitterPostPage.retweeter, "Test retweeter")
            compare(twitterPostPage.connectedToNetwork, false)
            compare(window.identifiersSet, true)
        }
    }
}
