import QtQuick 2.0
import Sailfish.Silica 1.0
import "shared"

SocialMediaAccountDelegate {
    id: delegate
    iconSource: "image://theme/graphic-service-facebook"
    //: New facebook posts
    //% "New posts"
    text: unseenPostCount > 0 ? qsTrId("lipstick-jolla-home-la-new_facebook_posts")
                                //: Facebook posts
                                //% "Posts"
                              : qsTrId("lipstick-jolla-home-la-facebook_posts")
}
