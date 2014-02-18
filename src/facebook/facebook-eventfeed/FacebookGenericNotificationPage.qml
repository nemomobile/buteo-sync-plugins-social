import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import org.nemomobile.lipstick 0.1
import "shared"

FacebookNotificationPage {
    id: container

    SilicaListView {
        id: view
        anchors.fill: parent
        spacing: Theme.paddingLarge

        header: SocialContent {
            width: view.width
            connectedToNetwork: container.connectedToNetwork
            avatar: container.facebookUser
                      && container.facebookUser.node !== null
                      && container.facebookUser.node.picture !== null ? container.facebookUser.node.picture.url : ""
            timestamp: model.timestamp
            body: container.model.title
            fullRowSocialButtons: Item {
                 width: view.width
                 height: Theme.paddingLarge
            }
        }

        footer: Column {
            width: view.width

            Item {
                width: 1
                height: Theme.paddingLarge * 2
            }

            Button {
                visible: typeof container.model.link !== "undefined" && container.model.link.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                //: Opens Facebook notification in browser
                //% "Open in Facebook"
                text: qsTrId("lipstick-jolla-home-la-open_in_facebook")
                onClicked: Qt.openUrlExternally(container.model.link)
            }
        }

        VerticalScrollDecorator {}

        FacebookAccountMenu {
            pageContainer: container.pageContainer
            link: container.model.link
            onCurrentAccountChanged: container.account.setIdentifiers(currentAccount)
        }
    }
}
