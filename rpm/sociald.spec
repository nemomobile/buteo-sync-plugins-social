Name:       sociald
Summary:    Syncs device data from social services
Version:    0.0.91
Release:    1
Group:      System/Libraries
License:    TBD
URL:        https://bitbucket.org/jolla/base-sociald
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(mlite5)
BuildRequires:  pkgconfig(buteosyncfw5) >= 0.6.33
BuildRequires:  pkgconfig(libsignon-qt5)
BuildRequires:  pkgconfig(sailfishaccounts) >= 0.0.65
BuildRequires:  pkgconfig(socialcache) >= 0.0.23
BuildRequires:  pkgconfig(libsailfishkeyprovider)
BuildRequires:  qt5-qttools-linguist
Requires: buteo-syncfw-qt5-msyncd
Obsoletes: buteo-sync-plugins-google-simple <= 0.0.2
Provides: buteo-sync-plugins-google-simple
Requires: systemd
Requires(post): systemd

%description
A Buteo plugin which provides data synchronization with various social services.

%files
%defattr(-,root,root,-)
/usr/lib/buteo-plugins-qt5/libsociald-client.so
%config %{_sysconfdir}/buteo/profiles/client/sociald.xml
%config %{_sysconfdir}/buteo/profiles/sync/sociald.All.xml


%package eventfeed-shared
Summary:    Provides shared components for the lipstick events feed
License:    TBD
Group:      System/Libraries
Requires: %{name} = %{version}-%{release}
Requires: sailfish-components-textlinking
Requires: nemo-qml-plugin-notifications-qt5
Requires: nemo-qml-plugin-social-qt5 >= 0.0.12
Requires: nemo-qml-plugin-connectivity

%description eventfeed-shared
Provides shared components for the lipstick events feed

%files eventfeed-shared
%{_datadir}/lipstick/eventfeed/shared/*.qml
%{_datadir}/translations/sociald_eventfeed_eng_en.qm


%package facebook-calendars
Summary:    Provides calendar synchronisation with Facebook
License:    TBD
Group:      System/Libraries
BuildRequires:  pkgconfig(libmkcal-qt5)
BuildRequires:  pkgconfig(libkcalcoren-qt5)
Requires: %{name} = %{version}-%{release}

%description facebook-calendars
Provides calendar synchronisation with Facebook

%files facebook-calendars
/usr/lib/buteo-plugins-qt5/libfacebook-calendars-client.so
%config %{_sysconfdir}/buteo/profiles/client/facebook-calendars.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Calendars.xml

%pre facebook-calendars
rm -f /home/nemo/.cache/msyncd/sync/client/facebook-calendars.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Calendars.xml

%post facebook-calendars
systemctl-user restart msyncd.service || :


%package facebook-contacts
Summary:    Provides contact synchronisation with Facebook
License:    TBD
Group:      System/Libraries
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions)
Requires: %{name} = %{version}-%{release}

%description facebook-contacts
Provides contact synchronisation with Facebook

%files facebook-contacts
/usr/lib/buteo-plugins-qt5/libfacebook-contacts-client.so
%config %{_sysconfdir}/buteo/profiles/client/facebook-contacts.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Contacts.xml

%pre facebook-contacts
rm -f /home/nemo/.cache/msyncd/sync/client/facebook-contacts.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Contacts.xml

%post facebook-contacts
systemctl-user restart msyncd.service || :


%package facebook-images
Summary:    Provides image synchronisation with Facebook
License:    TBD
Group:      System/Libraries
Requires: %{name} = %{version}-%{release}

%description facebook-images
Provides image synchronisation with Facebook

%files facebook-images
/usr/lib/buteo-plugins-qt5/libfacebook-images-client.so
%config %{_sysconfdir}/buteo/profiles/client/facebook-images.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Images.xml

%pre facebook-images
rm -f /home/nemo/.cache/msyncd/sync/client/facebook-images.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Images.xml

%post facebook-images
systemctl-user restart msyncd.service || :


%package facebook-notifications
Summary:    Provides notification synchronisation with Facebook
License:    TBD
Group:      System/Libraries
BuildRequires:  nemo-qml-plugin-notifications-qt5-devel
BuildRequires:  libmeegotouchevents-qt5-devel
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description facebook-notifications
Provides notification synchronisation with Facebook

%files facebook-notifications
/usr/lib/buteo-plugins-qt5/libfacebook-notifications-client.so
%config %{_sysconfdir}/buteo/profiles/client/facebook-notifications.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Notifications.xml
%{_datadir}/lipstick/notificationcategories/x-nemo.social.facebook.notification.conf
%{_datadir}/translations/lipstick-jolla-home-facebook_eng_en.qm
%{_datadir}/lipstick/eventfeed/FacebookNotificationPage.qml
%{_datadir}/lipstick/eventfeed/FacebookGenericNotificationPage.qml
%{_datadir}/lipstick/eventfeed/FacebookCommentablePage.qml
%{_datadir}/lipstick/eventfeed/FacebookAccountMenu.qml
%{_datadir}/lipstick/eventfeed/FacebookListView.qml
%{_datadir}/lipstick/eventfeed/FacebookPostPage.qml
%{_datadir}/lipstick/eventfeed/FacebookEventPage.qml
%{_datadir}/lipstick/eventfeed/FacebookPicturePage.qml
%{_datadir}/lipstick/eventfeed/FacebookEvent.qml
%{_datadir}/lipstick/eventfeed/facebook-update.qml
%{_datadir}/lipstick/eventfeed/facebook-delegate.qml
%{_datadir}/lipstick/eventfeed/FacebookNotificationItem.qml
%{_datadir}/lipstick/eventfeed/facebook-feed.qml

%pre facebook-notifications
rm -f /home/nemo/.cache/msyncd/sync/client/facebook-notifications.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Notifications.xml

%post facebook-notifications
systemctl-user restart msyncd.service || :


%package facebook-posts
Summary:    Provides post synchronisation with Facebook
License:    TBD
Group:      System/Libraries
Requires:   %{name}-eventfeed-shared = %{version}-%{release}
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions)
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description facebook-posts
Provides post synchronisation with Facebook

%files facebook-posts
%{_datadir}/lipstick/notificationcategories/x-nemo.social.facebook.statuspost.conf
%{_datadir}/translations/lipstick-jolla-home-facebook_eng_en.qm
###/usr/lib/buteo-plugins-qt5/libfacebook-posts-client.so
###%config %{_sysconfdir}/buteo/profiles/client/facebook-posts.xml
###%config %{_sysconfdir}/buteo/profiles/sync/facebook.Posts.xml

%pre facebook-posts
rm -f /home/nemo/.cache/msyncd/sync/client/facebook-posts.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Posts.xml

%post facebook-posts
systemctl-user restart msyncd.service || :


%package google-calendars
Summary:    Provides calendar synchronisation with Google
License:    TBD
Group:      System/Libraries
BuildRequires:  pkgconfig(libmkcal-qt5)
BuildRequires:  pkgconfig(libkcalcoren-qt5)
Requires: %{name} = %{version}-%{release}

%description google-calendars
Provides calendar synchronisation with Google

%files google-calendars
/usr/lib/buteo-plugins-qt5/libgoogle-calendars-client.so
%config %{_sysconfdir}/buteo/profiles/client/google-calendars.xml
%config %{_sysconfdir}/buteo/profiles/sync/google.Calendars.xml

%pre google-calendars
rm -f /home/nemo/.cache/msyncd/sync/client/google-calendars.xml
rm -f /home/nemo/.cache/msyncd/sync/google.Calendars.xml

%post google-calendars
systemctl-user restart msyncd.service || :


%package google-contacts
Summary:    Provides contact synchronisation with Google
License:    TBD
Group:      System/Libraries
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions) >= 0.1.54
Requires: %{name} = %{version}-%{release}

%description google-contacts
Provides contact synchronisation with Google

%files google-contacts
/usr/lib/buteo-plugins-qt5/libgoogle-contacts-client.so
%config %{_sysconfdir}/buteo/profiles/client/google-contacts.xml
%config %{_sysconfdir}/buteo/profiles/sync/google.Contacts.xml

%pre google-contacts
rm -f /home/nemo/.cache/msyncd/sync/client/google-contacts.xml
rm -f /home/nemo/.cache/msyncd/sync/google.Contacts.xml

%post google-contacts
systemctl-user restart msyncd.service || :



%package twitter-notifications
Summary:    Provides notification synchronisation with Twitter
License:    TBD
Group:      System/Libraries
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions)
BuildRequires:  nemo-qml-plugin-notifications-qt5-devel
BuildRequires:  libmeegotouchevents-qt5-devel
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description twitter-notifications
Provides notification synchronisation with Twitter

%files twitter-notifications
/usr/lib/buteo-plugins-qt5/libtwitter-notifications-client.so
%config %{_sysconfdir}/buteo/profiles/client/twitter-notifications.xml
%config %{_sysconfdir}/buteo/profiles/sync/twitter.Notifications.xml
%{_datadir}/lipstick/notificationcategories/x-nemo.social.twitter.mention.conf

%pre twitter-notifications
rm -f /home/nemo/.cache/msyncd/sync/client/twitter-notifications.xml
rm -f /home/nemo/.cache/msyncd/sync/twitter.Notifications.xml

%post twitter-notifications
systemctl-user restart msyncd.service || :


%package twitter-posts
Summary:    Provides post synchronisation with Twitter
License:    TBD
Group:      System/Libraries
Requires:   %{name}-eventfeed-shared = %{version}-%{release}
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions)
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description twitter-posts
Provides post synchronisation with Twitter

%files twitter-posts
/usr/lib/buteo-plugins-qt5/libtwitter-posts-client.so
%config %{_sysconfdir}/buteo/profiles/client/twitter-posts.xml
%config %{_sysconfdir}/buteo/profiles/sync/twitter.Posts.xml
%{_datadir}/lipstick/notificationcategories/x-nemo.social.twitter.tweet.conf
%{_datadir}/lipstick/eventfeed/TwitterPostPage.qml
%{_datadir}/lipstick/eventfeed/twitter-update.qml
%{_datadir}/lipstick/eventfeed/twitter-delegate.qml
%{_datadir}/lipstick/eventfeed/TwitterFeedItem.qml
%{_datadir}/lipstick/eventfeed/twitter-feed.qml
%{_datadir}/translations/lipstick-jolla-home-twitter_eng_en.qm

%pre twitter-posts
rm -f /home/nemo/.cache/msyncd/sync/client/twitter-posts.xml
rm -f /home/nemo/.cache/msyncd/sync/twitter.Posts.xml

%post twitter-posts
systemctl-user restart msyncd.service || :


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
rm -f /home/nemo/.cache/msyncd/sync/sociald.facebook.Calendars.xml
rm -f /home/nemo/.cache/msyncd/sync/sociald.facebook.Contacts.xml
rm -f /home/nemo/.cache/msyncd/sync/sociald.facebook.Images.xml
rm -f /home/nemo/.cache/msyncd/sync/sociald.facebook.Notifications.xml
rm -f /home/nemo/.cache/msyncd/sync/sociald.twitter.Notifications.xml
rm -f /home/nemo/.cache/msyncd/sync/sociald.twitter.Posts.xml
rm -f /home/nemo/.cache/msyncd/sync/sociald.google.Calendars.xml
rm -f /home/nemo/.cache/msyncd/sync/sociald.google.Contacts.xml

%install
rm -rf %{buildroot}
%qmake5_install

%post
systemctl-user restart msyncd.service || :

