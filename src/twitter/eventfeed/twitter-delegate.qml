import QtQuick 2.0
import Sailfish.Silica 1.0
import "shared"

SocialMediaAccountDelegate {
    id: delegate
    iconSource: "image://theme/graphic-service-twitter"
    feedPage: twitterFeedPage
    //: New twitter tweets
    //% "New tweets"
    text: qsTrId("lipstick-jolla-home-la-new_twitter_tweets")
    resources: TwitterFeedPage {
        id: twitterFeedPage
    }

    // TODO: Add indicator row as soon as libsocialcache provides them
}
