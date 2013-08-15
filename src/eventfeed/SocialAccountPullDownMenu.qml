import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Accounts 1.0

PullDownMenu {
    id: container
    property int currentAccount
    property int currentAccountIndex: -1
    property var metaData
    property string selectAccountString
    property string changeToAccountString
    property string accountString
    property var pageContainer
    onMetaDataChanged: refreshAccountList()

    function refreshAccountList() {
        internal.accounts = []
        internal.accountCount = container.metaData["accountIdCount"]
        for (var i = 0; i < internal.accountCount; i++) {
            var accountData = new Object
            accountData["id"] = container.metaData["accountId" + i]
            var account = accountManager.account(accountData["id"])
            accountData["name"] = account.displayName
            internal.accounts.push(accountData)
        }
        container.currentAccount = internal.accounts[0]["id"]
        container.currentAccountIndex = 0

        if (internal.accountCount == 2) {
            internal.otherIndex = 1
        }
    }
    resources: [
        QtObject {
            id: internal
            function setIndex(index) {
                container.currentAccountIndex = index
                pageContainer.pop()
            }
            property int accountCount
            property var accounts
            property int otherIndex
        },
        AccountManager {
            id: accountManager
        }
    ]

    MenuItem {
        text: {
            if (internal.accountCount > 2) {
                container.selectAccountString
            } else if (internal.accountCount == 2) {
                container.changeToAccountString.arg(internal.accounts[internal.otherIndex]["name"])
            } else {
                ""
            }
        }
        visible: internal.accountCount > 1

        onClicked: {
            if (internal.accountCount > 2) {
                var page = container.pageContainer.push(Qt.resolvedUrl("SocialAccountPage.qml"),
                                                        {"accounts": internal.accounts,
                                                         "currentIndex": container.currentAccountIndex,
                                                         "headerText": container.selectAccountString})
                page.indexSelected.connect(internal.setIndex)
                // TODO
            } else {
                container.currentAccountIndex = internal.otherIndex
                container.currentAccount = internal.accounts[container.currentAccountIndex]["id"]
                internal.otherIndex = (internal.otherIndex == 0 ? 1 : 0)
            }
        }
    }

    MenuLabel {
        text: container.accountString.arg(internal.accounts[container.currentAccountIndex]["name"])
    }
}
