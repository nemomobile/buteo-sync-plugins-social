import QtQuick 2.0
import Sailfish.Silica 1.0
import "shared"

SocialMediaAccountDelegate {
    id: delegate
    iconSource: "image://theme/graphic-service-vk"
    //: New VK posts
    //% "New posts"
    text: unseenPostCount > 0 ? qsTrId("lipstick-jolla-home-la-new_vk_posts")
                                //: VK posts
                                //% "Posts"
                              : qsTrId("lipstick-jolla-home-la-vk_posts")
}
