import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.socialcache 1.0
import org.nemomobile.configuration 1.0

Page {
    id: page

    property alias listModel: _listView.model
    property alias listDelegate: _listView.delegate
    property alias socialNetwork: syncHelper.socialNetwork
    property string configKey
    property int timestampRole
    property int unseenPostCount
    property string headerTitle
    property SilicaListView listView: _listView
    property alias updating: syncHelper.loading

    signal refreshTime

    onVisibleChanged: {
        if (visible && status === PageStatus.Active) {
            page.refreshTime()
        }
    }

    Connections {
        target: page.listModel
        onCountChanged: page.listUpdated()
    }

    Timer {
        interval: 60000
        running: page.visible && page.status === PageStatus.Active
        repeat: true
        onTriggered: page.refreshTime()
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            page.unseenPostCount = 0
            page.refreshTime()
            page.setLastSeenTime()
        }
    }

    SyncHelper {
        id: syncHelper
        dataType: SocialSync.Posts
        onLoadingChanged: {
            if (!loading && page.listModel) {
                page.listModel.refresh()
            }
        }
    }

    SyncHelper {
        id: syncNotifications
        socialNetwork: syncHelper.socialNetwork
        dataType: SocialSync.Notifications
    }

    ConfigurationValue {
        id: _lastSeenTime
        key: page.configKey + "_last_seen_time"
    }

    SilicaListView {
        id: _listView
        anchors.fill: parent
        cacheBuffer: Screen.height
        header: PageHeader {
            title: page.headerTitle
        }

        VerticalScrollDecorator {}
    }

    function sync() {
        syncHelper.sync()
        if (socialNetwork === SocialSync.Twitter || socialNetwork === SocialSync.Facebook) {
            syncNotifications.sync()
        }
    }

    function positionViewAtBeginning() {
         _listView.positionViewAtBeginning()
    }

    function listUpdated() {
        if (!visible) {
            var result = 0
            var lastTimestamp = _lastSeenTime.value
            for (var i = 0; i < listModel.count; i++) {
                var rawTimestamp = listModel.getField(i, timestampRole).toString()
                var timestamp = new Date(rawTimestamp).getTime()
                if (timestamp <= lastTimestamp) {
                    break
                }
                result++
            }
            page.unseenPostCount = result
        } else if (status === PageStatus.Active) {
            setLastSeenTime()
        }
    }

    function setLastSeenTime() {
        if (listModel.count > 0) {
            var date = new Date()
            _lastSeenTime.value = date.getTime()
        }
    }

    Component.onCompleted: sync()
}
