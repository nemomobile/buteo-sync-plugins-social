import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.socialcache 1.0
import "shared"

SocialMediaFeedPage {
    id: page
    configKey: "/sailfish/events_view/twitter"
    timestampRole: 4 //TwitterPostsModel.Timestamp
    listModel: TwitterPostsModel {
        id: twitterPostsModel
    }
    socialNetwork: SocialSync.Twitter
    //: Twitter service name
    //% "Twitter"
    headerTitle: qsTrId("lipstick-jolla-home-la-twitter")
    listDelegate: TwitterFeedItem {
        width: ListView.view.width
        imageList: model.images
        onClicked: {
            var rv = null
            try {
                rv = pageStack.push(Qt.resolvedUrl("TwitterPostPage.qml"), {
                                    "model": model, "subviewModel": subviewModel }, false)
            } catch (error) {
                console.log(error)
            }
        }
        Component.onCompleted: page.refreshTime.connect(formatTime)
    }

    PullDownMenu {
        flickable: page.listView
        busy: page.updating

        MenuItem {
            //: Opens Twitter in browser
            //% "Go to Twitter"
            text: qsTrId("lipstick-jolla-home-me-go_to_twitter")
            onClicked: Qt.openUrlExternally("https://mobile.twitter.com")
        }

        MenuItem {
            //: Update social media feed
            //% "Update"
            text: qsTrId("lipstick-jolla-home-me-update_feed")
            onClicked: page.sync()
        }
    }
}
