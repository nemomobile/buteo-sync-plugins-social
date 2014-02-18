import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import "shared"

FacebookCommentablePage {
    id: container

    Component.onCompleted: {
        if (readyToPopulate) {
            facebookPost.nodeIdentifier = container.nodeIdentifier
        }
    }

    onReadyToPopulateChanged: {
        if (readyToPopulate) {
            facebookPost.nodeIdentifier = container.nodeIdentifier
        }
    }

    SocialNetworkModel {
        id: facebookPost

        filters: ContentItemTypeFilter { type: Facebook.Like; limit: 3 }
        socialNetwork: container.facebook
        onNodeIdentifierChanged: repopulate()
        onErrorChanged: console.log("Facebook post network model error: " + error + "\n")
        onErrorMessageChanged: console.log("Facebook post network model error message: " + errorMessage + "\n")
        onNodeChanged: {
            if (node) {
                view.imageSource = node.source.toString() !== "" ? node.source : node.picture
                view.body = typeof node.message !== "undefined" && node.message !== "" ? node.message
                                                                                       : typeof node.name !== "undefined" ? node.name : ""
                view.posterId = node.from.objectIdentifier
                view.source = node.from.objectName
                view.targetName =  typeof node.to !== "undefined" && node.to.length > 0 ? node.to[0].objectName : ""
                view.description = typeof node.description !== "undefined" ? node.description : ""
                view.timestamp = node.createdTime
            }
        }
    }

    FacebookListView {
        id: view
        connectedToNetwork: container.connectedToNetwork
        model: container.commentsModel

        FacebookAccountMenu {
            pageContainer: container.pageContainer
            onCurrentAccountChanged: container.account.setIdentifiers(currentAccount)
            link: container.model.link
        }
    }
}
