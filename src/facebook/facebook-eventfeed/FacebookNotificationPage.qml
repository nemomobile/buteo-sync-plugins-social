import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import org.nemomobile.lipstick 0.1

Page {
    property string nodeIdentifier: model ? model.object : ""
    property variant model
    property Item subviewModel
    property bool allowLike: true
    property bool allowComment: true
    property bool connectedToNetwork
    property bool readyToPopulate
    property Account account
    property Facebook facebook
    property SocialNetworkModel facebookMe
    property SocialNetworkModel facebookUser

    allowedOrientations: Lipstick.compositor.eventsWindowOrientation
}
