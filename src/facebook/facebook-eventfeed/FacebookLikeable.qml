import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.social 1.0

FacebookNotificationPage {
    id: page

    property string likers
    property bool liked
    property QtObject likesModel: facebookLikes
    property string error

    Component.onCompleted: populateLikes()
    onReadyToPopulateChanged: populateLikes()
    onNodeIdentifierChanged: populateLikes()

    // We need (up to) first two people who liked
    // to display the "a, b and N others liked that"
    // string.
    SocialNetworkModel {
        id: facebookLikes

        property string reloadingStatus
        property bool busy: status === SocialNetwork.Busy || status === SocialNetwork.Initializing

        filters: ContentItemTypeFilter { type: Facebook.Like; limit: 3 }
        socialNetwork: page.facebook
        onNodeIdentifierChanged: repopulate()
        onErrorChanged: console.log("Facebook likes network model error: " + error + "\n")
        onErrorMessageChanged: console.log("Facebook likes network model error message: " + errorMessage + "\n")
        onNodeChanged: page.updateLikers()
        onCountChanged: page.updateLikers()
    }

    Connections {
        target: facebookLikes.node
        onStatusChanged: {
            if (!facebookLikes.node)
                return

            if (facebookLikes.node.status == SocialNetwork.Idle
                  && facebookLikes.reloadingStatus === "commenting") {
//                facebookComments.loadNext()
                facebookLikes.reloadingStatus = ""
            } else if (facebookLikes.node.status === SocialNetwork.Error) {
                // We simply display an error in the status, but
                // continue the flow to the "idle" state.
                if (facebookLikes.reloadingStatus === "liking") {
                    //: Facebook like operation failed.
                    //% "Failed to like"
                    page.error = !page.liked ? qsTrId("lipstick-jolla-home-facebook-error-fail-to-like")
                                               //% "Failed to remove like"
                                             : qsTrId("lipstick-jolla-home-facebook-error-fail-to-remove-like")
                } else if (facebookLikes.reloadingStatus === "commenting") {
                    //: Facebook comment operation failed.
                    //% "Failed to comment"
                    page.error = qsTrId("lipstick-jolla-home-facebook-error-fail-to-comment")
                }
            }
        }
    }

    Connections {
        target: page.facebookMe
        onNodeChanged: page.updateLikers()
    }

    function populateLikes() {
        if (readyToPopulate) {
            facebookLikes.nodeIdentifier = container.nodeIdentifier
        }
    }

    // Returns string formatted e.g. "You, Mike M and 3 others like this"
    function updateLikers() {
        likers = ""

        if (!facebookLikes.node)
            return

        // Not very pretty code but localization and how this message
        // is expressed requires quite many variations
        liked = facebookLikes.node.liked

        var myName = page.facebookMe.node ? page.facebookMe.node.name : ""
        var users = new Array
        for (var i = 0; i < facebookLikes.count; ++i) {
            if (facebookLikes.relatedItem(i).userName !== myName) {
                users.push(facebookLikes.relatedItem(i).userName)
            }
        }

        if (facebookLikes.count === 1) {
            if (liked) {
                //% "You like this"
                likers = qsTrId("lipstick-jolla-home-facebook-la-you-like-this")
            } else {
                //% "%1 likes this"
                likers = qsTrId("gallery-fb-la-one-friend-likes-this").arg(users[0])
            }
        } else if (facebookLikes.count === 2) {
            if (liked) {
                //% "You and %1 like this"
                likers = qsTrId("lipstick-jolla-home-facebook-la-you-and-another-friend-likes-this").arg(users[0])
            } else {
                //% "%1 and %2 like this"
                likers = qsTrId("lipstick-jolla-home-facebook-la-two-friend-likes-this").arg(users[0]).arg(users[1])
            }
        } else if (facebookLikes.count > 2) {
            if (liked) {
                //% "You, %1 and %2 others like this"
                likers = qsTrId("lipstick-jolla-home-facebook-la-you-and-multiple-friend-like-this").arg(users[0]).arg(facebookLikes.node.likesCount- 1)
            } else {
                //% "%1, %2 and %3 others like this"
                likers = qsTrId("lipstick-jolla-home-facebook-la-multiple-friend-like-this").arg(users[0]).arg(users[1]).arg(facebookLikes.node.likesCount - 2)
            }
        }
    }

    function uploadComment(comment) {
        if (facebookLikes.node) {
            facebookLikes.reloadingStatus = "commenting"
            facebookLikes.node.uploadComment(comment)
        }
    }

    function toggleLike() {
        if (facebookLikes.node) {
            facebookLikes.reloadingStatus = "liking"
            if (!liked) {
                facebookLikes.node.like()
            } else {
                facebookLikes.unlike()
            }
        }
    }
}
