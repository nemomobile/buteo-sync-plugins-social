import QtQuick 2.0
import Sailfish.Silica 1.0
import "shared"

SocialMediaAccountDelegate {
    id: delegate
    iconSource: "image://theme/graphic-service-twitter"
    feedPage: twitterFeedPage
    resources: TwitterFeedPage {
        id: twitterFeedPage
    }

    // TODO: Add indicator row as soon as libsocialcache provides them
}
