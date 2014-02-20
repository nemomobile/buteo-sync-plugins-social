import QtQuick 2.0
import Sailfish.Silica 1.0
import "shared"

SocialMediaAccountDelegate {
    id: delegate
    iconSource: "image://theme/graphic-service-twitter"
    //: New twitter tweets
    //% "New tweets"
    text: unseenPostCount > 0 ? qsTrId("lipstick-jolla-home-la-new_twitter_tweets")
                                //: Twitter tweets
                                //% "Tweets"
                              : qsTrId("lipstick-jolla-home-la-twitter_tweets")
}
