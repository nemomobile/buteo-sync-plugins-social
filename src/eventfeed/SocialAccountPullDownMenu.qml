import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Accounts 1.0

PullDownMenu {
    id: container
    property int currentAccount
    property int currentAccountIndex: -1
    property string selectAccountString
    property string changeToAccountString
    property string accountString
    property var pageContainer
    property string serviceName
    property bool switchEnabled

    // internal.index stores the index of the current account in
    // internal.account, while container.currentAccountIndex
    // is used to store the index for the metadata. If metadata
    // carries also information for (for example) avatar, that
    // are indexed, then container.currentAccountIndex contains
    // the good index, even if accounts got removed
    function refreshAccountList() {
        var subviewAccounts = subviewModel.accountList(serviceName)
        internal.accounts = []
        for (var i = 0; i < subviewAccounts.length; ++i) {
            var accountData = new Object
            var account = subviewAccounts[i]
            accountData["id"] = account.identifier
            accountData["name"] = account.displayName
            accountData["index"] = internal.accounts.length
            internal.accounts.push(accountData)
        }
        internal.accountCount = internal.accounts.length
        container.currentAccount = internal.accounts[0]["id"]
        container.currentAccountIndex = internal.accounts[0]["index"]
        internal.setCurrentAccountName()
    }
    resources: [
        QtObject {
            id: internal

            property int accountCount
            property var accounts
            property int index
            property int otherIndex
            property string currentAccountName

            onIndexChanged: setCurrentAccountName()

            function setIndex(index) {
                internal.index = index
                container.currentAccountIndex = internal.accounts[index]["index"]
                pageContainer.pop()
            }

            function setCurrentAccountName() {
                currentAccountName = internal.index < internal.accounts.length ? container.accountString.arg(internal.accounts[internal.index]["name"]) : ""
            }
        }
    ]

    MenuItem {
        text: {
            if (internal.accountCount > 2) {
                container.selectAccountString
            } else if (internal.accountCount == 2) {
                container.changeToAccountString.arg(internal.accounts[internal.index == 0 ? 1 : 0]["name"])
            } else {
                ""
            }
        }
        visible: internal.accountCount > 1
        enabled: container.switchEnabled

        onClicked: {
            if (internal.accountCount > 2) {
                var page = container.pageContainer.push(Qt.resolvedUrl("SocialAccountPage.qml"),
                                                        {"accounts": internal.accounts,
                                                         "currentIndex": internal.index,
                                                         "headerText": container.selectAccountString})
                page.indexSelected.connect(internal.setIndex)
            } else {
                internal.index = (internal.index == 0 ? 1 : 0)
                container.currentAccount = internal.accounts[internal.index]["id"]
                container.currentAccountIndex = internal.accounts[internal.index]["index"]
            }
        }
    }

    MenuLabel {
        text: internal.currentAccountName
    }

    Component.onCompleted: {
        refreshAccountList()
    }
}
