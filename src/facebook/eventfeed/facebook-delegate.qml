import QtQuick 2.0
import Sailfish.Silica 1.0
import "shared"

SocialMediaAccountDelegate {
    id: delegate
    iconSource: "image://theme/graphic-service-facebook"
    feedPage: facebookFeedPage
    //: New facebook posts
    //% "New posts"
    text: qsTrId("lipstick-jolla-home-la-new_facebook_posts")
    resources: FacebookFeedPage {
        id: facebookFeedPage
    }
}
