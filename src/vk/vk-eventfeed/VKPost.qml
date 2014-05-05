import QtQuick 2.0

Item {
    id: vkPost

    property string accessToken
    property string viewerId
    property string postId
    property string toId
    property string fromId
    property string timestamp
    property string text
    property bool friendsOnly
    property ListModel comments: _comments
    property bool canPost
    property int likeCount
    property bool userLikes
    property bool canLike
    property bool canPublish
    property int repostCount
    property bool userReposted
    property string type

    ListModel {
        id: _comments
    }
 
    signal loaded()
    signal commentUploaded()

    function clear() {
        likeCount = 0
        userLikes = false
        canLike = false
        canPublish = false 
        repostCount = 0
        userReposted = false
        _comments.clear()
    }

    function load(postIdentifier) {
        if (accessToken === "" || postIdentifier === postId)
            return

        clear() 
        postId = postIdentifier

  
        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    var data = JSON.parse(doc.responseText)
                    if (data.response === "undefined" || data.response.length === 0) {
                        console.log("VKPost: empty post")
                        return
                    }
                    var post = data.response[0]

                    vkPost.toId = post.to_id
                    vkPost.fromId = post.from_id
                    vkPost.timestamp = post.date
                    vkPost.text = post.text 
                    vkPost.type = post.post_type
                    vkPost.likeCount = post.likes.count
                    vkPost.userLikes = post.likes.user_likes
                    vkPost.canLike = post.likes.can_like
                    vkPost.canPublish = post.likes.can_publish
                    vkPost.repostCount = post.reposts.count
                    vkPost.userReposted = post.reposts.user_reposted
                    vkPost.canPost = post.comments.can_post 
                    if (post.comments.count > 0) {
                        loadComments()
                    }                   

                    vkPost.loaded()
                } else {
                    console.log("Failed to load VK post, error: " + doc.status)
                }
            }
        }

        var postData = "access_token=" + accessToken
        var url = "https://api.vk.com/method/wall.getById?posts=" + viewerId + "_" + postId + "&access_token=" + accessToken
        doc.open("GET", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }

 
    function loadComments() {        
        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    _comments.clear()

                    var users = new Array                                         
                    var data = JSON.parse(doc.responseText)
                    var commentArray = data.response

                    // the first item is the number of items in array, skip it
                    for (var i = 1; i < commentArray.length; ++i) {
                        var comment = commentArray[i]
                        if (users.indexOf(comment.from_id) === -1) {
                            users.push(comment.from_id)
                        }

                        var date = new Date(parseInt(comment.date) * 1000)
                        _comments.append({"commentId": comment.id,
                                          "userId": comment.from_id,
                                          "timestamp": date.toISOString(),
                                          "likes": comment.likes.count,
                                          "text": comment.text,
                                          "icon": "",
                                          "name": ""})
                    }

                    if (users.length > 0) {
                        updateUserData(users)
                    }
                } else {
                    console.log("Failed to load VK comments for post " + postId + ", error: " + doc.status)
                }
            }
        }

        var postData = "access_token=" + accessToken
        var url = "https://api.vk.com/method/wall.getComments?owner_id=" + viewerId + "&post_id=" + postId + "&need_likes=1&access_token=" + accessToken
        doc.open("GET", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)     
    }

    function updateUserData(userArray) {
        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    var data = JSON.parse(doc.responseText)
                    var users = new Array

                    for (var i = 0; i < data.response.length; ++i) {
                        var person = data.response[i]
                        users[person.uid] = person
                    }

                    for (i = 0; i < _comments.count; ++i) {
                        var comment = comments.get(i)
                        person = users[comment.userId]
                        if (person !== null) {
                            comment.name = person.first_name + " " + person.last_name
                            comment.icon = person.photo
                        }
                    }
                } else {
                    console.log("VKPost: failed to query user profiles")
                }
            }
        }

        var postData = "uids="
        for (var i = 0; i < userArray.length; ++i) {
            postData += userArray[i]
            if (i < userArray.length-1) {
                postData += ","
            }
        }

        var url = "https://api.vk.com/method/getProfiles?fields=photo&" + postData + "&access_token=" + accessToken
        doc.open("GET", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }

    function postComment(comment) {
        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    vkPost.commentUploaded()
                } else {
                    console.log("VKPost: uploading comment failed")
                }
            }
        }

        var postData = "text="+comment
        var url = "https://api.vk.com/method/wall.addComment?post_id=" + postId + "&access_token=" + accessToken
        doc.open("POST", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }

    function like() {
        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    vkPost.userLikes = true
                    var data = JSON.parse(doc.responseText)
                    vkPost.likeCount = data.response.likes
                } else {
                    console.log("VKPost: liking post failed")
                }
            }
        }

        var postData = "access_token=" + accessToken
        var url = "https://api.vk.com/method/wall.addLike?post_id=" + postId + "&access_token=" + accessToken
        doc.open("POST", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }

    function unlike() {
        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    vkPost.userLikes = false
                    var data = JSON.parse(doc.responseText)
                    vkPost.likeCount = data.response.likes
                } else {
                    console.log("VKPost: unliking post failed")
                }
            }
        }

        var postData = "access_token=" + accessToken
        var url = "https://api.vk.com/method/wall.deleteLike?post_id=" + postId + "&access_token=" + accessToken
        doc.open("POST", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }
}
