var HttpClient = function() {
    this.get = function(aUrl, aCallback) {
        var anHttpRequest = new XMLHttpRequest();
        anHttpRequest.onreadystatechange = function() {
            if (anHttpRequest.readyState == 4 && anHttpRequest.status == 200)
                aCallback(anHttpRequest.responseText);
        }

        anHttpRequest.open("GET", aUrl, true);
        anHttpRequest.send(null);
    }
    this.post = function(aUrl, aRgs, aCallback) {
        var anHttpRequest = new XMLHttpRequest();
        anHttpRequest.onreadystatechange = function() {
            if (anHttpRequest.readyState == 4 && anHttpRequest.status == 200)
                aCallback(anHttpRequest.responseText);
        }

        anHttpRequest.open("POST", aUrl, true);
        anHttpRequest.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
        anHttpRequest.send(aRgs);
    }
    this.delect = function(aUrl, aRgs, aCallback) {
        var anHttpRequest = new XMLHttpRequest();
        anHttpRequest.onreadystatechange = function() {
            if (anHttpRequest.readyState == 4 && anHttpRequest.status == 200)
                aCallback(anHttpRequest.responseText);
        }

        anHttpRequest.open("DELETE", aUrl, true);
        anHttpRequest.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
        anHttpRequest.send(aRgs);
    }
}
var client = new HttpClient();

function time_sync_post() {
    var time_web = new Date().toString();
    console.log(time_web);
    client.post('/post', 'time_setting=' + time_web, function(response) {
        console.log(response);
    });
}