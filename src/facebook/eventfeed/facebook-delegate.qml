import QtQuick 2.0
import Sailfish.Silica 1.0
import "shared"

SocialMediaAccountDelegate {
    id: delegate
    iconSource: "image://theme/graphic-service-facebook"
    feedPage: facebookFeedPage
    resources: FacebookFeedPage {
        id: facebookFeedPage
    }

    Row {
        height: parent.height
        x: Theme.paddingLarge

/*
        // TODO: Activate indicators as soon as libsocial cache supports them
        SocialMediaIndicator {
            icon: "image://theme/icon-m-people"
        }
        SocialMediaIndicator {
            icon: "image://theme/icon-lock-social"
        }
        SocialMediaIndicator {
            icon: "image://theme/icon-m-document"
        } */
    }
}
