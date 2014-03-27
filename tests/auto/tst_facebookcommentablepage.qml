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
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import "eventfeed"

ApplicationWindow {
    id: window
    deviceOrientation: Orientation.Portrait
    initialPage: commentablePage

    Facebook { id: _facebook }

    FacebookCommentablePage {
        id: commentablePage
        facebook: _facebook
    }

    TestCase {
        name: "FacebookCommentablePage"
        when: windowShown

        function test_facebookCommentablePage() {
            compare(commentablePage.likers, "")
            compare(commentablePage.liked, false)
            verify(commentablePage.commentsModel !== null)
            verify(commentablePage.likesModel !== null)
            compare(commentablePage.error, "")

            compare(commentablePage.commentsModel.socialNetwork, _facebook)
            compare(commentablePage.likesModel.socialNetwork, _facebook)

            commentablePage.populateCommentsAndLikes()
            commentablePage.updateLikers()
            commentablePage.toggleLike()
        }
    }
}
