import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.socialcache 1.0

Page {
    id: page

    property alias listModel: _listView.model
    property alias listDelegate: _listView.delegate
    property alias socialNetwork: syncHelper.socialNetwork
    property string headerTitle
    property SilicaListView listView: _listView

    signal refreshTime

    onVisibleChanged: {
        if (visible && status === PageStatus.Active) {
            page.refreshTime()
        }
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            page.refreshTime()
        }
    }

    SyncHelper {
        id: syncHelper
        socialNetwork: SocialSync.Twitter
        dataType: SocialSync.Posts
        onLoadingChanged: {
            if (!loading && page.listModel) {
                page.listModel.refresh()
            }
        }
    }

    SilicaListView {
        id: _listView
        anchors.fill: parent
        cacheBuffer: Screen.height * 4
        header: PageHeader {
            title: page.headerTitle
        }

        VerticalScrollDecorator {}
    }

    function sync() {
        syncHelper.sync()
    }

    function positionViewAtBeginning() {
         _listView.positionViewAtBeginning()
    }

    Component.onCompleted: sync()
}
