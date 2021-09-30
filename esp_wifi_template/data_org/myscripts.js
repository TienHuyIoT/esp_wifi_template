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

function display_time() {
    var str_time;
    var str_date;

    var time = new Date();
    var hour = time.getHours();
    var min = time.getMinutes();
    var sec = time.getSeconds();
    hour = add_1zero_number(hour);
    min = add_1zero_number(min);
    sec = add_1zero_number(sec);
    str_time = hour + ":" + min + ":" + sec;

    var date = new Date();
    var year = date.getFullYear();
    var month = date.getMonth() + 1;
    var mday = date.getDate();
    month = add_1zero_number(month);
    mday = add_1zero_number(mday);
    str_date = mday + "/" + month + "/" + year;

    var t = setTimeout(display_time, 500);

    document.getElementById('TableInfo').rows[0].cells[2].innerHTML = str_date + " " + str_time + "";
}

var disconnect_timeout = setTimeout(2000);;

function probe_connection_status() {
    client.get('/get?param_wifi=device_info', function(response) {
        console.log(response);
        clearTimeout(disconnect_timeout);
        var tableif = document.getElementById('TableInfo');
        var connection_status = "Đang Kết Nối";
        tableif.rows[1].cells[2].innerHTML = connection_status;
        tableif.rows[1].cells[2].style.color = "#92D14F";
        var t = setTimeout(probe_connection_status, 10000);
        return;
    });

    disconnect_timeout = setTimeout(function() {
        var tableif = document.getElementById('TableInfo');
        var connection_status = "Đứt Kết Nối";
        tableif.rows[1].cells[2].innerHTML = connection_status;
        tableif.rows[1].cells[2].style.color = "red";
    }, 3000);
}

function add_1zero_number(_num) {
    if (_num < 10) { _num = "0" + _num }; // add zero in front of numbers < 10
    return _num;
}

/* [_num]: number need to convert
 * [_zero_num]: zero numbers need to add before _num
 */
function add_zero_number(_num, _zero_num) {
    var num_str = "";
    var _limit = 1;

    for (var i = 0; i < _zero_num; i++) {
        _limit *= 10;
    }

    for (var i = 0; i < _zero_num; i++) {
        if (_num < _limit) {
            num_str += '0';
            _limit /= 10;
        } else {
            break;
        }
    }
    num_str += _num;
    return num_str;
}

/* [n]: number 
 * [currency] : ""
 * volume_format(1000, ""));
 */
function volume_format(n, currency) {
    return currency + " " + n.toFixed(3).replace(/(\d)(?=(\d{3})+\.)/g, "$1,");
}

function probe_format(n, currency) {
    var str = currency + " " + n.toFixed(3).replace(/(\d)(?=(\d{3})+\.)/g, "$1,");
    str = str.slice(0, str.length - 4);
    return str;
}

/* [n]: number 
 * [currency] : ""
 * cash_format(1000, ""));
 */
function cash_format(n, currency) {
    var st = volume_format(n, currency);
    st = st.replace(/\./g, ",");
    return st;
}

function time_hhmmss_format(_time) {
    //hhmmss
    var t = _time.getHours() * 10000 + _time.getMinutes() * 100 + _time.getSeconds();
    return t;
}

function date_ddmmyy_format(_date) {
    //ddmmyy
    var d = _date.getDate() * 10000 + (_date.getMonth() + 1) * 100 + _date.getFullYear() % 100;
    return d;
}

function time_num2str(n) {
    var hour, min, sec;
    hour = Math.floor(n / 10000);
    min = Math.floor((n % 10000) / 100);
    sec = n % 100;

    var str_time;
    hour = add_1zero_number(hour);
    min = add_1zero_number(min);
    sec = add_1zero_number(sec);
    str_time = hour + ":" + min + ":" + sec;

    return str_time;
}

function date_num2str(n) {
    var mday, month, year;
    mday = Math.floor(n / 10000);
    month = Math.floor((n % 10000) / 100);
    year = n % 100;

    var str_date;
    month = add_1zero_number(month);
    mday = add_1zero_number(mday);
    str_date = mday + "/" + month + "/20" + year;

    return str_date;
}