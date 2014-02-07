Name:       sociald
Summary:    Syncs device data from social services
Version:    0.0.74
Release:    1
Group:      System/Applications
License:    TBD
URL:        https://bitbucket.org/jolla/base-sociald
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
# Qt5Gui requires -> hence adding over here.
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(mlite5)
BuildRequires:  pkgconfig(libsignon-qt5)
BuildRequires:  pkgconfig(libsailfishkeyprovider)
BuildRequires:  pkgconfig(sailfishaccounts) >= 0.0.45
BuildRequires:  pkgconfig(libmkcal-qt5)
BuildRequires:  pkgconfig(libkcalcoren-qt5)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions)
BuildRequires:  nemo-qml-plugin-notifications-qt5-devel
BuildRequires:  libmeegotouchevents-qt5-devel
BuildRequires:  buteo-syncfw-qt5-devel
BuildRequires:  pkgconfig(socialcache) >= 0.0.15
# for lupdate
BuildRequires:  qt5-qttools-linguist
Requires: sailfish-components-accounts-qt5 >= 0.0.43
Requires: sailfish-components-textlinking
Requires: nemo-qml-plugin-notifications-qt5
Requires: nemo-qml-plugin-social-qt5 >= 0.0.12
Requires: nemo-qml-plugin-connectivity
Requires: buteo-syncfw-qt5-msyncd
Requires: mkcal-qt5
Obsoletes: buteo-sync-plugins-google-simple <= 0.0.2
Provides: buteo-sync-plugins-google-simple

%description
A daemon process which provides data synchronization with various social services.

%files
%defattr(-,root,root,-)
/usr/lib/buteo-plugins-qt5/libsociald-client.so
%config %{_sysconfdir}/buteo/profiles/client/sociald.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Calendars.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Contacts.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Images.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Notifications.xml
%config %{_sysconfdir}/buteo/profiles/sync/twitter.Notifications.xml
%config %{_sysconfdir}/buteo/profiles/sync/twitter.Posts.xml
%config %{_sysconfdir}/buteo/profiles/sync/google.Calendars.xml
%config %{_sysconfdir}/buteo/profiles/sync/google.Contacts.xml
%{_datadir}/lipstick/notificationcategories/x-nemo.social.facebook.notification.conf
%{_datadir}/lipstick/notificationcategories/x-nemo.social.facebook.statuspost.conf
%{_datadir}/lipstick/notificationcategories/x-nemo.social.twitter.mention.conf
%{_datadir}/lipstick/notificationcategories/x-nemo.social.twitter.tweet.conf
%{_datadir}/translations/sociald_eng_en.qm
%{_datadir}/translations/lipstick-jolla-home-facebook_eng_en.qm
%{_datadir}/translations/lipstick-jolla-home-twitter_eng_en.qm
%{_datadir}/lipstick/eventfeed/FacebookPostPage.qml
%{_datadir}/lipstick/eventfeed/facebook-update.qml
%{_datadir}/lipstick/eventfeed/facebook-delegate.qml
%{_datadir}/lipstick/eventfeed/FacebookFeedItem.qml
%{_datadir}/lipstick/eventfeed/facebook-feed.qml
%{_datadir}/lipstick/eventfeed/TwitterPostPage.qml
%{_datadir}/lipstick/eventfeed/twitter-update.qml
%{_datadir}/lipstick/eventfeed/twitter-delegate.qml
%{_datadir}/lipstick/eventfeed/TwitterFeedItem.qml
%{_datadir}/lipstick/eventfeed/twitter-feed.qml
%{_datadir}/lipstick/eventfeed/shared/*.qml

%package ts-devel
Summary:    Translation source for sociald
License:    TBD
Group:      System/Applications

%description ts-devel
Translation source for sociald

%files ts-devel
%defattr(-,root,root,-)
%{_datadir}/translations/source/sociald.ts
%{_datadir}/translations/source/lipstick-jolla-home-facebook.ts
%{_datadir}/translations/source/lipstick-jolla-home-twitter.ts

%package tests
Summary:    Automatable tests for sociald
License:    TBD
Group:      System/Applications
BuildRequires:  pkgconfig(Qt5Test)
Requires:   qt5-qtdeclarative-devel-tools
Requires:   qt5-qtdeclarative-import-qttest

%description tests
Automatable tests for sociald

%files tests
%defattr(-,root,root,-)
/opt/tests/sociald/*

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
make %{?jobs:-j%jobs}

%pre
rm -f /home/nemo/.cache/msyncd/sync/client/sociald.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Calendars.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Contacts.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Images.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Notifications.xml
rm -f /home/nemo/.cache/msyncd/sync/twitter.Notifications.xml
rm -f /home/nemo/.cache/msyncd/sync/twitter.Posts.xml
rm -f /home/nemo/.cache/msyncd/sync/google.Contacts.xml

%install
rm -rf %{buildroot}
%qmake5_install

%post
su nemo -c "systemctl --user restart msyncd.service"
