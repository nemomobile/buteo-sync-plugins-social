Name:       sociald
Summary:    Syncs device data from social services
Version:    0.1.6
Release:    1
Group:      System/Libraries
License:    LGPLv2.1
URL:        https://bitbucket.org/jolla/base-sociald
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(mlite5)
BuildRequires:  pkgconfig(buteosyncfw5) >= 0.6.36
BuildRequires:  pkgconfig(libsignon-qt5)
BuildRequires:  pkgconfig(accounts-qt5)
BuildRequires:  pkgconfig(socialcache) >= 0.0.24
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
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/sociald-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libsociald-client.so
%config %{_sysconfdir}/buteo/profiles/client/sociald.xml
%config %{_sysconfdir}/buteo/profiles/sync/sociald.All.xml


%package facebook-calendars
Summary:    Provides calendar synchronisation with Facebook
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  pkgconfig(libmkcal-qt5)
BuildRequires:  pkgconfig(libkcalcoren-qt5)
Requires: %{name} = %{version}-%{release}

%description facebook-calendars
Provides calendar synchronisation with Facebook

%files facebook-calendars
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/facebook-calendars-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libfacebook-calendars-client.so
%config %{_sysconfdir}/buteo/profiles/client/facebook-calendars.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Calendars.xml

%pre facebook-calendars
rm -f /home/nemo/.cache/msyncd/sync/client/facebook-calendars.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Calendars.xml

%post facebook-calendars
systemctl-user restart msyncd.service || :


%package facebook-contacts
Summary:    Provides contact synchronisation with Facebook
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions)
Requires: %{name} = %{version}-%{release}

%description facebook-contacts
Provides contact synchronisation with Facebook

%files facebook-contacts
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/facebook-contacts-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libfacebook-contacts-client.so
%config %{_sysconfdir}/buteo/profiles/client/facebook-contacts.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Contacts.xml

%pre facebook-contacts
rm -f /home/nemo/.cache/msyncd/sync/client/facebook-contacts.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Contacts.xml

%post facebook-contacts
systemctl-user restart msyncd.service || :


%package facebook-images
Summary:    Provides image synchronisation with Facebook
License:    LGPLv2.1
Group:      System/Libraries
Requires: %{name} = %{version}-%{release}

%description facebook-images
Provides image synchronisation with Facebook

%files facebook-images
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/facebook-images-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libfacebook-images-client.so
%config %{_sysconfdir}/buteo/profiles/client/facebook-images.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Images.xml

%pre facebook-images
rm -f /home/nemo/.cache/msyncd/sync/client/facebook-images.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Images.xml

%post facebook-images
systemctl-user restart msyncd.service || :


%package facebook-notifications
Summary:    Provides notification synchronisation with Facebook
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  nemo-qml-plugin-notifications-qt5-devel
BuildRequires:  libmeegotouchevents-qt5-devel
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description facebook-notifications
Provides notification synchronisation with Facebook

%files facebook-notifications
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/facebook-notifications-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libfacebook-notifications-client.so
%config %{_sysconfdir}/buteo/profiles/client/facebook-notifications.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Notifications.xml
%{_datadir}/lipstick/notificationcategories/x-nemo.social.facebook.notification.conf

%pre facebook-notifications
rm -f /home/nemo/.cache/msyncd/sync/client/facebook-notifications.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Notifications.xml

%post facebook-notifications
systemctl-user restart msyncd.service || :


%package facebook-posts
Summary:    Provides post synchronisation with Facebook
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions)
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description facebook-posts
Provides post synchronisation with Facebook

%files facebook-posts
%{_datadir}/lipstick/notificationcategories/x-nemo.social.facebook.statuspost.conf
####out-of-process-plugin form:
###/usr/lib/buteo-plugins-qt5/oopp/facebook-posts-client
####in-process-plugin form:
###/usr/lib/buteo-plugins-qt5/libfacebook-posts-client.so
###%config %{_sysconfdir}/buteo/profiles/client/facebook-posts.xml
###%config %{_sysconfdir}/buteo/profiles/sync/facebook.Posts.xml

%pre facebook-posts
rm -f /home/nemo/.cache/msyncd/sync/client/facebook-posts.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Posts.xml

%post facebook-posts
systemctl-user restart msyncd.service || :


%package facebook-signon
Summary:    Provides signon credentials refreshing with Facebook
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description facebook-signon
Provides signon credentials refreshing with Facebook

%files facebook-signon
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/facebook-signon-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libfacebook-signon-client.so
%config %{_sysconfdir}/buteo/profiles/client/facebook-signon.xml
%config %{_sysconfdir}/buteo/profiles/sync/facebook.Signon.xml

%pre facebook-signon
rm -f /home/nemo/.cache/msyncd/sync/client/facebook-signon.xml
rm -f /home/nemo/.cache/msyncd/sync/facebook.Signon.xml

%post facebook-signon
systemctl-user restart msyncd.service || :



%package google-calendars
Summary:    Provides calendar synchronisation with Google
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  pkgconfig(libmkcal-qt5)
BuildRequires:  pkgconfig(libkcalcoren-qt5)
Requires: %{name} = %{version}-%{release}

%description google-calendars
Provides calendar synchronisation with Google

%files google-calendars
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/google-calendars-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libgoogle-calendars-client.so
%config %{_sysconfdir}/buteo/profiles/client/google-calendars.xml
%config %{_sysconfdir}/buteo/profiles/sync/google.Calendars.xml

%pre google-calendars
rm -f /home/nemo/.cache/msyncd/sync/client/google-calendars.xml
rm -f /home/nemo/.cache/msyncd/sync/google.Calendars.xml

%post google-calendars
systemctl-user restart msyncd.service || :


%package google-contacts
Summary:    Provides contact synchronisation with Google
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions) >= 0.1.58
Requires: %{name} = %{version}-%{release}

%description google-contacts
Provides contact synchronisation with Google

%files google-contacts
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/google-contacts-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libgoogle-contacts-client.so
%config %{_sysconfdir}/buteo/profiles/client/google-contacts.xml
%config %{_sysconfdir}/buteo/profiles/sync/google.Contacts.xml

%pre google-contacts
rm -f /home/nemo/.cache/msyncd/sync/client/google-contacts.xml
rm -f /home/nemo/.cache/msyncd/sync/google.Contacts.xml

%post google-contacts
systemctl-user restart msyncd.service || :


%package google-signon
Summary:    Provides signon credentials refreshing with Google
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description google-signon
Provides signon credentials refreshing with Google

%files google-signon
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/google-signon-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libgoogle-signon-client.so
%config %{_sysconfdir}/buteo/profiles/client/google-signon.xml
%config %{_sysconfdir}/buteo/profiles/sync/google.Signon.xml

%pre google-signon
rm -f /home/nemo/.cache/msyncd/sync/client/google-signon.xml
rm -f /home/nemo/.cache/msyncd/sync/google.Signon.xml

%post google-signon
systemctl-user restart msyncd.service || :



%package twitter-notifications
Summary:    Provides notification synchronisation with Twitter
License:    LGPLv2.1
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
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/twitter-notifications-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libtwitter-notifications-client.so
%config %{_sysconfdir}/buteo/profiles/client/twitter-notifications.xml
%config %{_sysconfdir}/buteo/profiles/sync/twitter.Notifications.xml
%{_datadir}/lipstick/notificationcategories/x-nemo.social.twitter.mention.conf
%{_datadir}/translations/lipstick-jolla-home-twitter-notif_eng_en.qm

%pre twitter-notifications
rm -f /home/nemo/.cache/msyncd/sync/client/twitter-notifications.xml
rm -f /home/nemo/.cache/msyncd/sync/twitter.Notifications.xml

%post twitter-notifications
systemctl-user restart msyncd.service || :


%package twitter-posts
Summary:    Provides post synchronisation with Twitter
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions)
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description twitter-posts
Provides post synchronisation with Twitter

%files twitter-posts
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/twitter-posts-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libtwitter-posts-client.so
%config %{_sysconfdir}/buteo/profiles/client/twitter-posts.xml
%config %{_sysconfdir}/buteo/profiles/sync/twitter.Posts.xml
%{_datadir}/lipstick/notificationcategories/x-nemo.social.twitter.tweet.conf

%pre twitter-posts
rm -f /home/nemo/.cache/msyncd/sync/client/twitter-posts.xml
rm -f /home/nemo/.cache/msyncd/sync/twitter.Posts.xml

%post twitter-posts
systemctl-user restart msyncd.service || :


%package ts-devel
Summary:    Translation source for sociald
License:    LGPLv2.1
Group:      System/Applications

%description ts-devel
Translation source for sociald

%files ts-devel
%defattr(-,root,root,-)
%{_datadir}/translations/source/lipstick-jolla-home-twitter-notif.ts

%package tests
Summary:    Automatable tests for sociald
License:    LGPLv2.1
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
%qmake5 "DEFINES+=OUT_OF_PROCESS_PLUGIN"
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

