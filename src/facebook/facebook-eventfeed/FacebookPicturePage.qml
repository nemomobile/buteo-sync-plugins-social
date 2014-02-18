import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import "shared"

FacebookCommentablePage {
    id: container

    Component.onCompleted: {
        if (readyToPopulate) {
            facebookPicture.nodeIdentifier = container.nodeIdentifier
        }
    }

    onReadyToPopulateChanged: {
        if (readyToPopulate) {
            facebookPicture.nodeIdentifier = container.nodeIdentifier
        }
    }

    SocialNetworkModel {
        id: facebookPicture

        property string imageName

        filters: [ ContentItemTypeFilter { type: Facebook.PhotoImage } ]
        socialNetwork: container.facebook
        onNodeIdentifierChanged: repopulate()
        onErrorChanged: console.log("Facebook photo network model error: " + error + "\n")
        onErrorMessageChanged: console.log("Facebook photo network model error message: " + errorMessage + "\n")
        onNodeChanged: {
            if (node) {
                view.posterId = node.from ? node.from.objectIdentifier : ""
                view.source = node.from ? node.from.objectName : ""
                view.body = node.name
                view.timestamp = node.createdTime
            }
        }
    }

    FacebookListView {
        id: view
        connectedToNetwork: container.connectedToNetwork
        model: container.commentsModel
        imageSource: facebookPicture.node ? facebookPicture.node.source : ""

        FacebookAccountMenu {
            link: container.model.link
            pageContainer: container.pageContainer
            onCurrentAccountChanged: container.account.setIdentifiers(currentAccount)
        }
    }
}
