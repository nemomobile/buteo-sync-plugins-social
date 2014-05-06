Name:       sociald
Summary:    Syncs device data from social services
Version:    0.1.37
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
BuildRequires:  pkgconfig(accounts-qt5) >= 1.13
BuildRequires:  pkgconfig(socialcache) >= 0.0.46
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
systemctl-user try-restart msyncd.service || :


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
systemctl-user try-restart msyncd.service || :


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
systemctl-user try-restart msyncd.service || :


%package facebook-notifications
Summary:    Provides notification synchronisation with Facebook
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  nemo-qml-plugin-notifications-qt5-devel
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
systemctl-user try-restart msyncd.service || :



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
systemctl-user try-restart msyncd.service || :



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
systemctl-user try-restart msyncd.service || :


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
systemctl-user try-restart msyncd.service || :


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
systemctl-user try-restart msyncd.service || :



%package twitter-notifications
Summary:    Provides notification synchronisation with Twitter
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions)
BuildRequires:  nemo-qml-plugin-notifications-qt5-devel
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
%{_datadir}/lipstick/notificationcategories/x-nemo.social.twitter.retweet.conf
%{_datadir}/lipstick/notificationcategories/x-nemo.social.twitter.follower.conf
%{_datadir}/translations/lipstick-jolla-home-twitter-notif_eng_en.qm

%pre twitter-notifications
rm -f /home/nemo/.cache/msyncd/sync/client/twitter-notifications.xml
rm -f /home/nemo/.cache/msyncd/sync/twitter.Notifications.xml

%post twitter-notifications
systemctl-user try-restart msyncd.service || :


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
systemctl-user try-restart msyncd.service || :


%package onedrive-signon
Summary:    Provides signon credentials refreshing with OneDrive
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description onedrive-signon
Provides signon credentials refreshing with OneDrive

%files onedrive-signon
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/onedrive-signon-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libonedrive-signon-client.so
%config %{_sysconfdir}/buteo/profiles/client/onedrive-signon.xml
%config %{_sysconfdir}/buteo/profiles/sync/onedrive.Signon.xml

%pre onedrive-signon
rm -f /home/nemo/.cache/msyncd/sync/client/onedrive-signon.xml
rm -f /home/nemo/.cache/msyncd/sync/onedrive.Signon.xml

%post onedrive-signon
systemctl-user try-restart msyncd.service || :


%package vk-posts
Summary:    Provides post synchronisation with VK
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions)
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description vk-posts
Provides post synchronisation with VK

%files vk-posts
%{_datadir}/lipstick/notificationcategories/x-nemo.social.vk.statuspost.conf
#%{_datadir}/translations/lipstick-jolla-home-vk_eng_en.qm
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/vk-posts-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libvk-posts-client.so
%config %{_sysconfdir}/buteo/profiles/client/vk-posts.xml
%config %{_sysconfdir}/buteo/profiles/sync/vk.Posts.xml

%pre vk-posts
rm -f /home/nemo/.cache/msyncd/sync/client/vk-posts.xml
rm -f /home/nemo/.cache/msyncd/sync/vk.Posts.xml

%post vk-posts
systemctl-user restart msyncd.service || :

%package dropbox-images
Summary:    Provides image synchronisation with Dropbox
License:    LGPLv2.1
Group:      System/Libraries
Requires: %{name} = %{version}-%{release}

%description dropbox-images
Provides image synchronisation with Dropbox

%files dropbox-images
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/dropbox-images-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libdropbox-images-client.so
%config %{_sysconfdir}/buteo/profiles/client/dropbox-images.xml
%config %{_sysconfdir}/buteo/profiles/sync/dropbox.Images.xml

%pre dropbox-images
rm -f /home/nemo/.cache/msyncd/sync/client/dropbox-images.xml
rm -f /home/nemo/.cache/msyncd/sync/dropbox.Images.xml

%post dropbox-images
systemctl-user try-restart msyncd.service || :

%package onedrive-images
Summary:    Provides image synchronisation with OneDrive
License:    LGPLv2.1
Group:      System/Libraries
Requires: %{name} = %{version}-%{release}

%description onedrive-images
Provides image synchronisation with OneDrive

%files onedrive-images
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/onedrive-images-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libonedrive-images-client.so
%config %{_sysconfdir}/buteo/profiles/client/onedrive-images.xml
%config %{_sysconfdir}/buteo/profiles/sync/onedrive.Images.xml

%pre onedrive-images
rm -f /home/nemo/.cache/msyncd/sync/client/onedrive-images.xml
rm -f /home/nemo/.cache/msyncd/sync/onedrive.Images.xml

%post onedrive-images
systemctl-user try-restart msyncd.service || :



%package onedrive-backup
Summary:    Provides backup-blob synchronization for OneDrive
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description onedrive-backup
Provides backup-blob synchronization for OneDrive

%files onedrive-backup
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/onedrive-backup-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libonedrive-backup-client.so
%config %{_sysconfdir}/buteo/profiles/client/onedrive-backup.xml
%config %{_sysconfdir}/buteo/profiles/sync/onedrive.Backup.xml

%pre onedrive-backup
rm -f /home/nemo/.cache/msyncd/sync/client/onedrive-backup.xml
rm -f /home/nemo/.cache/msyncd/sync/onedrive.Backup.xml

%post onedrive-backup
systemctl-user try-restart msyncd.service || :



%package dropbox-backup
Summary:    Provides backup-blob synchronization for Dropbox
License:    LGPLv2.1
Group:      System/Libraries
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description dropbox-backup
Provides backup-blob synchronization for Dropbox

%files dropbox-backup
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/dropbox-backup-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libdropbox-backup-client.so
%config %{_sysconfdir}/buteo/profiles/client/dropbox-backup.xml
%config %{_sysconfdir}/buteo/profiles/sync/dropbox.Backup.xml

%pre dropbox-backup
rm -f /home/nemo/.cache/msyncd/sync/client/dropbox-backup.xml
rm -f /home/nemo/.cache/msyncd/sync/dropbox.Backup.xml

%post dropbox-backup
systemctl-user try-restart msyncd.service || :



%package vk-notifications
Summary:    Provides notification synchronisation with VK
License:    TBD
Group:      System/Libraries
BuildRequires:  nemo-qml-plugin-notifications-qt5-devel
BuildRequires:  qt5-qttools-linguist
Requires: %{name} = %{version}-%{release}

%description vk-notifications
Provides notification synchronisation with VK

%files vk-notifications
#out-of-process-plugin form:
/usr/lib/buteo-plugins-qt5/oopp/vk-notifications-client
#in-process-plugin form:
#/usr/lib/buteo-plugins-qt5/libvk-notifications-client.so
%config %{_sysconfdir}/buteo/profiles/client/vk-notifications.xml
%config %{_sysconfdir}/buteo/profiles/sync/vk.Notifications.xml
%{_datadir}/lipstick/notificationcategories/x-nemo.social.vk.notification.conf

%pre vk-notifications
rm -f /home/nemo/.cache/msyncd/sync/client/vk-notifications.xml
rm -f /home/nemo/.cache/msyncd/sync/vk.Notifications.xml

%post vk-notifications
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
systemctl-user try-restart msyncd.service || :

