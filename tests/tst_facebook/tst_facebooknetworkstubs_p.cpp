/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "networkstubs_p.h"
#include "socialdnetworkaccessmanager_p.h"

#include <QDateTime>
#include <QTimer>

static QByteArray createFacebookMeData(const QString &generator)
{
    // if the request is for https://graph.facebook.com/me

    QDateTime currDt = QDateTime::currentDateTime();
    QString currDtStr = currDt.toString(Qt::ISODate);

    QString dataStr = QString(QLatin1String(
        "{\n"
        "  \"id\": \"123456789\",\n"
        "  \"name\": \"Test Person\",\n"
        "  \"first_name\": \"Test\",\n"
        "  \"last_name\": \"Person\",\n"
        "  \"link\": \"https://www.facebook.com/test.person.1/\",\n"
        "  \"username\": \"test.person.1\",\n"
        "  \"quotes\": \"This is fake test data!\",\n"
        "  \"gender\": \"male\",\n"
        "  \"timezone\": 10,\n"
        "  \"locale\": \"en_US\",\n"
        "  \"verified\": true,\n"
        "  \"updated_time\": \"%1\"\n"
        "}")).arg(currDtStr);

    return dataStr.toUtf8();
}

static QByteArray createFacebookMeNotificationsData(const QString &generator)
{
    // if the request is for https://graph.facebook.com/me/notifications

    QDateTime currDt = QDateTime::currentDateTime();
    QString currDtStr = currDt.toString(Qt::ISODate);

    QString dataStr = QString(QLatin1String(
        "{\n"
        "  \"data\": [\n"
        "    {\n"
        "      \"id\": \"notif_123456789_444433321\",\n"
        "      \"from\": {\n"
        "        \"name\": \"Example Entity\",\n"
        "        \"id\": \"987654321\"\n"
        "      },\n"
        "      \"to\": {\n"
        "        \"name\": \"Test Person\",\n"
        "        \"id\": \"123456789\"\n"
        "      },\n"
        "      \"created_time\": \"%1\",\n"
        "      \"updated_time\": \"%2\",\n"
        "      \"title\": \"Example Entity commented on a link that you're tagged in: \"testing\"\",\n"
        "      \"link\": \"http://www.facebook.com/test.person.1/posts/1?comment_id=2&offset=0&total_comments=1\",\n"
        "      \"application\": {\n"
        "        \"name\": \"Feed Comments\",\n"
        "        \"id\": \"19675640871\"\n"
        "      },\n"
        "      \"unread\": 0,\n"
        "      \"object\": null\n"
        "    },\n"
        "    {\n"
        "      \"id\": \"notif_123456789_444433322\",\n"
        "      \"from\": {\n"
        "        \"name\": \"Example Entity\",\n"
        "        \"id\": \"987654321\"\n"
        "      },\n"
        "      \"to\": {\n"
        "        \"name\": \"Test Person\",\n"
        "        \"id\": \"123456789\"\n"
        "      },\n"
        "      \"created_time\": \"2013-05-10T10:43:45+0000\",\n"
        "      \"updated_time\": \"2013-05-10T10:43:45+0000\",\n"
        "      \"title\": \"Example Entity likes your link: \"Another test post\"\",\n"
        "      \"link\": \"http://www.facebook.com/test.person.1/posts/3\",\n"
        "      \"application\": {\n"
        "        \"name\": \"Likes\",\n"
        "        \"id\": \"2409997254\"\n"
        "      },\n"
        "      \"unread\": 0,\n"
        "      \"object\": null\n"
        "    }\n"
        "   ]\n"
        "  \"paging\": {\n"
        "    \"previous\": \"https://graph.facebook.com/123456789/notifications?include_read=1&limit=5000&since=444433321&__paging_token=notif_123456789_444433321\",\n"
        "    \"next\": \"https://graph.facebook.com/123456789/notifications?include_read=1&limit=5000&until=444433322&__paging_token=notif_123456789_444433322\"\n"
        "  },\n"
        "  \"summary\": [\n"
        "  ]\n"
        "}")).arg(currDtStr).arg(currDtStr);

    return dataStr.toUtf8();
}

static QByteArray createFacebookMeHomeData(const QString &generator)
{
    // if the request is for https://graph.facebook.com/me/home

    QDateTime currDt = QDateTime::currentDateTime();
    QString currDtStr = currDt.toString(Qt::ISODate);

    QString dataStr = QString(QLatin1String(
        "{\n"
        "  \"data\": [\n"
        "    {\n"
        "      \"id\": \"987654321_55555566666\",\n"
        "      \"from\": {\n"
        "        \"name\": \"Example Entity\",\n"
        "        \"id\": \"987654321\"\n"
        "      },\n"
        "      \"message\": \"Another test status\",\n"
        "      \"actions\": [\n"
        "        {\n"
        "          \"name\": \"Comment\",\n"
        "          \"link\": \"https://www.facebook.com/987654321/posts/55555566666\"\n"
        "        },\n"
        "        {\n"
        "          \"name\": \"Like\",\n"
        "          \"link\": \"https://www.facebook.com/987654321/posts/55555566666\"\n"
        "        }\n"
        "      ],\n"
        "      \"privacy\": {\n"
        "        \"value\": \"\"\n"
        "      },\n"
        "      \"type\": \"status\",\n"
        "      \"status_type\": \"mobile_status_update\",\n"
        "      \"application\": {\n"
        "        \"name\": \"Facebook for iPad\",\n"
        "        \"namespace\": \"fbipad_\",\n"
        "        \"id\": \"173847642670370\"\n"
        "      },\n"
        "      \"created_time\": \"%1\",\n"
        "      \"updated_time\": \"%2\",\n"
        "      \"likes\": {\n"
        "        \"data\": [\n"
        "          {\n"
        "            \"name\": \"Test Person\",\n"
        "            \"id\": \"123456789\"\n"
        "          }\n"
        "        ],\n"
        "        \"count\": 1\n"
        "      }\n"
        "    },\n"
        "    {\n"
        "      \"id\": \"123456789_55555566667\",\n"
        "      \"from\": {\n"
        "        \"name\": \"Test Person\",\n"
        "        \"id\": \"123456789\"\n"
        "      },\n"
        "      \"message\": \"Sharing this video for testing\",\n"
        "      \"picture\": \"http://www.test.com/picture.png\",\n"
        "      \"link\": \"http://www.test.com/video.flv\",\n"
        "      \"source\": \"http://www.test.com/video.flv\",\n"
        "      \"name\": \"Test Video\",\n"
        "      \"description\": \"This video is a test video\",\n"
        "      \"icon\": \"https://fbstatic-a.akamaihd.net/rsrc.php/v2/yj/r/v2OnaTyTQZE.gif\",\n"
        "      \"actions\": [\n"
        "        {\n"
        "          \"name\": \"Comment\",\n"
        "          \"link\": \"https://www.facebook.com/123456789/posts/55555566667\"\n"
        "        },\n"
        "        {\n"
        "          \"name\": \"Like\",\n"
        "          \"link\": \"https://www.facebook.com/123456789/posts/55555566667\"\n"
        "        }\n"
        "      ],\n"
        "      \"privacy\": {\n"
        "        \"value\": \"\"\n"
        "      },\n"
        "      \"type\": \"video\",\n"
        "      \"status_type\": \"shared_story\",\n"
        "      \"created_time\": \"%1\",\n"
        "      \"updated_time\": \"%2\"\n"
        "    }\n"
        "  \"paging\": {\n"
        "    \"previous\": \"https://graph.facebook.com/123456789/home?limit=25&since=1368495811\",\n"
        "    \"next\": \"https://graph.facebook.com/123456789/home?limit=25&until=1368435194\"\n"
        "  }\n"
        "}")).arg(currDtStr).arg(currDtStr);

    return dataStr.toUtf8();
}

static QByteArray createFacebookMeFriendsData(const QString &generator)
{
    // if the request is for https://graph.facebook.com/me/friends

    QString dataStr = QString(QLatin1String(
        "{\n"
        "  \"data\": [\n"
        "    {\n"
        "      \"updated_time\": \"2013-12-14T10:09:28+0000\",\n"
        "      \"link\": \"https://www.facebook.com/testfriendone\",\n"
        "      \"name\": \"Testfriend One\",\n"
        "      \"first_name\": \"Testfriend\",\n"
        "      \"last_name\": \"One\",\n"
        "      \"birthday\": \"01/01/2001\",\n"
        "      \"bio\": \"Fairly ordinary, average, test friend.\",\n"
        "      \"gender\": \"male\",\n"
        "      \"website\": \"http://friendone.test.com/\",\n"
        "      \"cover\": {\n"
        "          \"id\": \"123456\",\n"
        "          \"source\": \"https://fbcdn-sphotos-d-a.akamaihd.net/hphotos-ak-frc3/12345_123456123456_123456_n.jpg\",\n"
        "          \"offset_y\": 50\n"
        "      },\n"
        "      \"username\": \"testfriendone\",\n"
        "      \"id\": \"123456123456\",\n"
        "      \"picture\": {\n"
        "          \"data\": {\n"
        "              \"url\": \"https://fbcdn-profile-a.akamaihd.net/hprofile-ak-prn2/12345_123456123456_1234567_n.jpg\",\n"
        "              \"is_silhouette\": false\n"
        "          }\n"
        "      }\n"
        "    },\n"
        "    {\n"
        "      \"updated_time\": \"2013-12-14T10:09:28+0000\",\n"
        "      \"link\": \"https://www.facebook.com/testfriendtwo\",\n"
        "      \"name\": \"Test Friend Two\",\n"
        "      \"first_name\": \"Test\",\n"
        "      \"middle_name\": \"Friend\",\n"
        "      \"last_name\": \"Two\",\n"
        "      \"birthday\": \"01/01\",\n"
        "      \"bio\": \"Another ordinary, average, test friend.\",\n"
        "      \"gender\": \"female\",\n"
        "      \"username\": \"testfriendtwo\",\n"
        "      \"id\": \"7898778987\",\n"
        "      \"picture\": {\n"
        "          \"data\": {\n"
        "              \"url\": \"https://fbcdn-profile-a.akamaihd.net/hprofile-ak-prn2/78987_7898778987_7898789_n.jpg\",\n"
        "              \"is_silhouette\": false\n"
        "          }\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}"));

    return dataStr.toUtf8();
}

static QByteArray createFacebookMeAlbumsData(const QString &generator)
{
    // if the request is for https://graph.facebook.com/me/albums

    QDateTime currDt = QDateTime::currentDateTime();
    QString currDtStr = currDt.toString(Qt::ISODate);

    QString dataStr = QString(QLatin1String(
        "{\n"
        "  \"data\": [\n"
        "    {\n"
        "      \"id\": \"9999991111111\",\n"
        "      \"from\": {\n"
        "        \"name\": \"Test Person\",\n"
        "        \"id\": \"123456789\"\n"
        "      },\n"
        "      \"name\": \"Profile Pictures\",\n"
        "      \"link\": \"https://www.facebook.com/album.php?fbid=9999991111111&id=123456789&aid=1\",\n"
        "      \"cover_photo\": \"1111111111\",\n"
        "      \"privacy\": \"everyone\",\n"
        "      \"count\": 2,\n"
        "      \"type\": \"profile\",\n"
        "      \"created_time\": \"%1\",\n"
        "      \"updated_time\": \"%2\",\n"
        "      \"can_upload\": false\n"
        "    },\n"
        "    {\n"
        "      \"id\": \"9999992222222\",\n"
        "      \"from\": {\n"
        "        \"name\": \"Test Person\",\n"
        "        \"id\": \"123456789\"\n"
        "      },\n"
        "      \"name\": \"Timeline Photos\",\n"
        "      \"link\": \"https://www.facebook.com/album.php?fbid=9999992222222&id=123456789&aid=2\",\n"
        "      \"cover_photo\": \"22222222222\",\n"
        "      \"privacy\": \"everyone\",\n"
        "      \"count\": 4,\n"
        "      \"type\": \"wall\",\n"
        "      \"created_time\": \"%1\",\n"
        "      \"updated_time\": \"%2\",\n"
        "      \"can_upload\": false,\n"
        "      \"comments\": {\n"
        "        \"data\": [\n"
        "          {\n"
        "            \"id\": \"9999992222222_555555\",\n"
        "            \"from\": {\n"
        "              \"name\": \"Example Entity\",\n"
        "              \"id\": \"987654321\"\n"
        "            },\n"
        "            \"message\": \"This test album has a comment\",\n"
        "            \"can_remove\": true,\n"
        "            \"created_time\": \"%2\",\n"
        "            \"like_count\": 0,\n"
        "            \"user_likes\": false\n"
        "          }\n"
        "        ],\n"
        "        \"paging\": {\n"
        "          \"cursors\": {\n"
        "            \"after\": \"MQ==\",\n"
        "            \"before\": \"MQ==\"\n"
        "          }\n"
        "        }\n"
        "      }\n"
        "    },\n"
        "    {\n"
        "      \"id\": \"9999993333333\",\n"
        "      \"from\": {\n"
        "        \"name\": \"Test Person\",\n"
        "        \"id\": \"123456789\"\n"
        "      },\n"
        "      \"name\": \"Timeline Photos\",\n"
        "      \"privacy\": \"custom\",\n"
        "      \"type\": \"friends_walls\",\n"
        "      \"created_time\": \"%1\",\n"
        "      \"updated_time\": \"%2\",\n"
        "      \"can_upload\": false\n"
        "    },\n"
        "    {\n"
        "      \"id\": \"99999944444444\",\n"
        "      \"from\": {\n"
        "        \"name\": \"Test Person\",\n"
        "        \"id\": \"123456789\"\n"
        "      },\n"
        "      \"name\": \"More Testing\",\n"
        "      \"link\": \"https://www.facebook.com/album.php?fbid=99999944444444&id=123456789&aid=3\",\n"
        "      \"cover_photo\": \"44444444444\",\n"
        "      \"privacy\": \"friends\",\n"
        "      \"count\": 5,\n"
        "      \"type\": \"normal\",\n"
        "      \"created_time\": \"%1\",\n"
        "      \"updated_time\": \"%2\",\n"
        "      \"can_upload\": true\n"
        "    }\n"
        "  ],\n"
        "  \"paging\": {\n"
        "    \"cursors\": {\n"
        "      \"after\": \"MTA0MjQxNdUyOTc1NzY0\",\n"
        "      \"before\": \"NTAzNDQ4MdczMDU1MDk4\"\n"
        "    }\n"
        "  }\n"
        "}")).arg(currDtStr).arg(currDtStr);

    return dataStr.toUtf8();
}

static QByteArray createFacebookAlbumPhotosData(const QString &generator)
{
    // if the request is for https://graph.facebook.com/SOME_ALBUM_ID/photos

    QDateTime currDt = QDateTime::currentDateTime();
    QString currDtStr = currDt.toString(Qt::ISODate);

    QString dataStr = QString(QLatin1String(
        "{\n"
        "  \"data\": [\n"
        "    {\n"
        "      \"id\": \"44444444444444444\",\n"
        "      \"from\": {\n"
        "        \"name\": \"Test Person\",\n"
        "        \"id\": \"123456789\"\n"
        "      },\n"
        "      \"name\": \"Test Photo\",\n"
        "      \"picture\": \"https://fbcdn-photos-e-a.akamaihd.net/hphotos-ak-frc1/11111_222222222222222_3333333_s.jpg\",\n"
        "      \"source\": \"https://fbcdn-sphotos-e-a.akamaihd.net/hphotos-ak-frc1/11111_222222222222222_3333333_n.jpg\",\n"
        "      \"height\": 652,\n"
        "      \"width\": 652,\n"
        "      \"images\": [\n"
        "        {\n"
        "          \"height\": 652,\n"
        "          \"width\": 652,\n"
        "          \"source\": \"https://fbcdn-sphotos-e-a.akamaihd.net/hphotos-ak-frc1/11111_222222222222222_3333333_n.jpg\"\n"
        "        },\n"
        "        {\n"
        "          \"height\": 652,\n"
        "          \"width\": 652,\n"
        "          \"source\": \"https://fbcdn-sphotos-e-a.akamaihd.net/hphotos-ak-frc1/11111_222222222222222_3333333_n.jpg\"\n"
        "        },\n"
        "        {\n"
        "          \"height\": 600,\n"
        "          \"width\": 600,\n"
        "          \"source\": \"https://fbcdn-sphotos-e-a.akamaihd.net/hphotos-ak-frc1/s600x600/11111_222222222222222_3333333_n.jpg\"\n"
        "        },\n"
        "        {\n"
        "          \"height\": 480,\n"
        "          \"width\": 480,\n"
        "          \"source\": \"https://fbcdn-sphotos-e-a.akamaihd.net/hphotos-ak-frc1/s480x480/11111_222222222222222_3333333_n.jpg\"\n"
        "        },\n"
        "        {\n"
        "          \"height\": 320,\n"
        "          \"width\": 320,\n"
        "          \"source\": \"https://fbcdn-sphotos-e-a.akamaihd.net/hphotos-ak-frc1/s320x320/11111_222222222222222_3333333_n.jpg\"\n"
        "        },\n"
        "        {\n"
        "          \"height\": 180,\n"
        "          \"width\": 180,\n"
        "          \"source\": \"https://fbcdn-photos-e-a.akamaihd.net/hphotos-ak-frc1/11111_222222222222222_3333333_a.jpg\"\n"
        "        },\n"
        "        {\n"
        "          \"height\": 130,\n"
        "          \"width\": 130,\n"
        "          \"source\": \"https://fbcdn-photos-e-a.akamaihd.net/hphotos-ak-frc1/11111_222222222222222_3333333_s.jpg\"\n"
        "        },\n"
        "        {\n"
        "          \"height\": 130,\n"
        "          \"width\": 130,\n"
        "          \"source\": \"https://fbcdn-photos-e-a.akamaihd.net/hphotos-ak-frc1/s75x225/11111_222222222222222_3333333_s.jpg\"\n"
        "        }\n"
        "      ],\n"
        "      \"link\": \"https://www.facebook.com/photo.php?fbid=44444444444444444&set=a.222222222222222.5555.123456789&type=1\",\n"
        "      \"icon\": \"https://fbstatic-a.akamaihd.net/rsrc.php/v2/yz/r/StEh3RhPvjk.gif\",\n"
        "      \"created_time\": \"%1\",\n"
        "      \"updated_time\": \"%2\"\n"
        "    }\n"
        "  ],\n"
        "  \"paging\": {\n"
        "    \"cursors\": {\n"
        "      \"after\": \"MTA0MjQ0MdAyOTc1NTA5\",\n"
        "      \"before\": \"MTA0MjQxNdYyOTc1NzYz\"\n"
        "    }\n"
        "  }\n"
        "}")).arg(currDtStr).arg(currDtStr);

    return dataStr.toUtf8();
}

static QByteArray createFacebookAlbumPhotosContinuationData(const QString &generator)
{
    // if the request is for a continuation (page) of https://graph.facebook.com/SOME_ALBUM_ID/photos

    QString dataStr = QString(QLatin1String(
        "{\n"
        "    \"data\": [\n"
        "    ]\n"
        "    \"paging\": {\n"
        "    }\n"
        "}"));

    return dataStr.toUtf8();
}

QByteArray TestNetworkReply::generateData(const QUrl &requestUrl, const QString &generator)
{
    // we inspect the request url and based upon what we see there, generate some data.
    QString host = requestUrl.host();
    QString path = requestUrl.path();

    if (host == QLatin1String("graph.facebook.com")) {
        if (path == QLatin1String("/me")) {
            return createFacebookMeData(generator);
        } else if (path == QLatin1String("/me/notifications")) {
            return createFacebookMeNotificationsData(generator);
        } else if (path == QLatin1String("/me/home")) {
            return createFacebookMeHomeData(generator);
        } else if (path == QLatin1String("/me/friends")) {
            return createFacebookMeFriendsData(generator);
        } else if (path == QLatin1String("/me/albums")) {
            return createFacebookMeAlbumsData(generator);
        } else if (path.contains(QLatin1String("photos"))) {
            if (!path.contains(QLatin1String("next"))) {
                // initial request
                return createFacebookAlbumPhotosData(generator);
            }

            // else, continuation request
            return createFacebookAlbumPhotosContinuationData(generator);
        }
    }

    // no test data function exists for this host/path combination
    qWarning() << Q_FUNC_INFO << "no test data function exists for:" << host << path;
    return QByteArray();
}