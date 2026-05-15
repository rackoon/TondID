/*
  helper functions for RemoteID javascript
*/

// helper function for cross-origin requests
function createCORSRequest(method, url) {
    var xhr = new XMLHttpRequest();
    if ("withCredentials" in xhr) {
        // XHR for Chrome/Firefox/Opera/Safari.
        xhr.open(method, url, true);
    } else if (typeof XDomainRequest != "undefined") {
        // XDomainRequest for IE.
        xhr = new XDomainRequest();
        xhr.open(method, url);
    } else {
        // CORS not supported.
        xhr = null;
    }
    return xhr;
}


/*
  fill variables in a page from json
*/
function page_fill_json_value(json) {
    for (var v in json) {
        var element = document.getElementById(v);
        if (element) {
            if (element.type == "checkbox") {
                element.checked = json[v] == "1" || json[v] == 1 || json[v] === true;
            } else {
                element.value = json[v];
            }
        }
    }
    if (typeof qaUpdateEnabledState == "function") {
        qaUpdateEnabledState();
    }
}

/*
  fill html in a page from json
*/
function page_fill_json_html(json) {
    for (var v in json) {
        var element = document.getElementById(v);
        if (element) {
            element.innerHTML = json[v];
        } else if (v == "STATUS:BOARD_ID") {
	    if(typeof page_fill_json_html.run_once == 'undefined' ) {
		//run this code only once to avoid updating these fields
                if (json[v] == "3") {
                    document.getElementById("logo").src="images/TondID_logo.svg";
                    document.getElementById("logo").alt="TondID";
                    document.getElementById("STATUS:BOARD").innerText = "BlueMark db200";
                    document.getElementById("documentation").innerHTML = "<ul><li><a href='https://download.bluemark.io/db200.pdf'>db200 series manual</a></li><li><a href='https://ardupilot.org/ardupilot/index.html'>ArduPilot Project</a></li><li><a href='https://github.com/ArduPilot/ArduRemoteID'>ArduRemoteID Project</a></li><li><a href='https://ardupilot.org/plane/docs/common-remoteid.html'>ArduPilot RemoteID Documentation</a></li><li><a href='https://www.opendroneid.org/'>OpenDroneID Website</a></li></ul>";
                    document.body.style.background = "#fafafa";
                } else if (json[v] == "4") {
                    document.getElementById("logo").src="images/TondID_logo.svg";
                    document.getElementById("logo").alt="TondID";
                    document.getElementById("STATUS:BOARD").innerText = "BlueMark db110";
                    document.getElementById("documentation").innerHTML = "<ul><li><a href='https://download.bluemark.io/db110.pdf'>db110 manual</a></li><li><a href='https://ardupilot.org/ardupilot/index.html'>ArduPilot Project</a></li><li><a href='https://github.com/ArduPilot/ArduRemoteID'>ArduRemoteID Project</a></li><li><a href='https://ardupilot.org/plane/docs/common-remoteid.html'>ArduPilot RemoteID Documentation</a></li><li><a href='https://www.opendroneid.org/'>OpenDroneID Website</a></li></ul>";
                    document.body.style.background = "#fafafa";
                } else if (json[v] == "10") {
                    document.getElementById("logo").src="images/TondID_logo.svg";
                    document.getElementById("logo").alt="TondID";
                    document.getElementById("STATUS:BOARD").innerText = "BlueMark db210pro";
                    document.getElementById("documentation").innerHTML = "<ul><li><a href='https://download.bluemark.io/db200.pdf'>db210pro manual</a></li><li><a href='https://ardupilot.org/ardupilot/index.html'>ArduPilot Project</a></li><li><a href='https://github.com/ArduPilot/ArduRemoteID'>ArduRemoteID Project</a></li><li><a href='https://ardupilot.org/plane/docs/common-remoteid.html'>ArduPilot RemoteID Documentation</a></li><li><a href='https://www.opendroneid.org/'>OpenDroneID Website</a></li></ul>";
                } else if (json[v] == "8") {
                    document.getElementById("logo").src="images/TondID_logo.svg";
                    document.getElementById("logo").alt="TondID";
                    document.getElementById("STATUS:BOARD").innerText = "BlueMark db202mav";
                    document.getElementById("documentation").innerHTML = "<ul><li><a href='https://download.bluemark.io/db200.pdf'>db200 series manual</a></li><li><a href='https://ardupilot.org/ardupilot/index.html'>ArduPilot Project</a></li><li><a href='https://github.com/ArduPilot/ArduRemoteID'>ArduRemoteID Project</a></li><li><a href='https://ardupilot.org/plane/docs/common-remoteid.html'>ArduPilot RemoteID Documentation</a></li><li><a href='https://www.opendroneid.org/'>OpenDroneID Website</a></li></ul>";
                    document.body.style.background = "#fafafa";
                } else if (json[v] == "9") {
                    document.getElementById("logo").src="images/TondID_logo.svg";
                    document.getElementById("logo").alt="TondID";
                    document.getElementById("STATUS:BOARD").innerText = "BlueMark db203can";
                    document.getElementById("documentation").innerHTML = "<ul><li><a href='https://download.bluemark.io/db200.pdf'>db200 series manual</a></li><li><a href='https://ardupilot.org/ardupilot/index.html'>ArduPilot Project</a></li><li><a href='https://github.com/ArduPilot/ArduRemoteID'>ArduRemoteID Project</a></li><li><a href='https://ardupilot.org/plane/docs/common-remoteid.html'>ArduPilot RemoteID Documentation</a></li><li><a href='https://www.opendroneid.org/'>OpenDroneID Website</a></li></ul>";
                    document.body.style.background = "#fafafa";
                } else if (json[v] == "11") {
                    document.getElementById("logo").src="images/TondID_logo.svg";
                    document.getElementById("logo").alt="TondID";
                    document.getElementById("STATUS:BOARD").innerText = "Holybro RemoteID";
                    document.getElementById("documentation").innerHTML = "<ul><li><a href='https://ardupilot.org/ardupilot/index.html'>ArduPilot Project</a></li><li><a href='https://github.com/ArduPilot/ArduRemoteID'>ArduRemoteID Project</a></li><li><a href='https://ardupilot.org/plane/docs/common-remoteid.html'>ArduPilot RemoteID Documentation</a></li><li><a href='https://www.opendroneid.org/'>OpenDroneID Website</a></li></ul>";
                } else if (json[v] == "12") {
                    document.getElementById("logo").src="images/TondID_logo.svg";
                    document.getElementById("logo").alt="TondID";
                    document.getElementById("STATUS:BOARD").innerText = "CUAV RemoteID";
                    document.getElementById("documentation").innerHTML = "<ul><li><a href='https://ardupilot.org/ardupilot/index.html'>ArduPilot Project</a></li><li><a href='https://github.com/ArduPilot/ArduRemoteID'>ArduRemoteID Project</a></li><li><a href='https://ardupilot.org/plane/docs/common-remoteid.html'>ArduPilot RemoteID Documentation</a></li><li><a href='https://www.opendroneid.org/'>OpenDroneID Website</a></li></ul>";
                } else {
                    document.getElementById("logo").src="images/TondID_logo.svg";
                    document.getElementById("logo").alt="TondID";

                    if (json[v] == "1") {
                        document.getElementById("STATUS:BOARD").innerText = "ESP32S3_DEV";
                    } else if (json[v] == "2") {
                        document.getElementById("STATUS:BOARD").innerText = "ESP32C3_DEV";
                    } else if (json[v] == "5") {
                        document.getElementById("STATUS:BOARD").innerText = "JW_TBD";
                    } else if (json[v] == "6") {
                        document.getElementById("STATUS:BOARD").innerText = "mRo-RID";
                    } else {
                        document.getElementById("STATUS:BOARD").innerText = "unknown:" + json[v];
                    }

                    document.getElementById("documentation").innerHTML = "<ul><li><a href='https://ardupilot.org/ardupilot/index.html'>ArduPilot Project</a></li><li><a href='https://github.com/ArduPilot/ArduRemoteID'>ArduRemoteID Project</a></li><li><a href='https://ardupilot.org/plane/docs/common-remoteid.html'>ArduPilot RemoteID Documentation</a></li><li><a href='https://www.opendroneid.org/'>OpenDroneID Website</a></li></ul>";
                }
            }
            run_once = 1;
        }
    }
}

/*
  fetch a URL, calling a callback
*/
function ajax_get_callback(url, callback) {
    var xhr = createCORSRequest("GET", url);
    xhr.onload = function() {
        callback(xhr.responseText);
    }
    xhr.send();
}

/*
  fetch a URL, calling a callback for binary data
*/
function ajax_get_callback_binary(url, callback) {
    var xhr = createCORSRequest("GET", url);
    xhr.onload = function() {
        console.log("got response length " + xhr.response.byteLength);
        callback(xhr.response);
    }
    xhr.responseType = "arraybuffer";
    xhr.send();
}

/*
  poll a URL, calling a callback
*/
function ajax_poll(url, callback, refresh_ms=1000) {
    function again() {
        setTimeout(function() { ajax_poll(url, callback, refresh_ms); }, refresh_ms);
    }
    var xhr = createCORSRequest("GET", url);
    xhr.onload = function() {
        if (callback(xhr.responseText)) {
            again();
        }
    }
    xhr.onerror = function() {
        again();
    }
    xhr.timeout = 3000;
    xhr.ontimeout = function() {
        again();
    }
    xhr.send();
}


/*
  poll a json file and fill document IDs at the given rate
*/
function ajax_json_poll(url, callback, refresh_ms=1000) {
    function do_callback(responseText) {
        try {
            var json = JSON.parse(responseText);
            return callback(json);
        } catch(e) {
            return true;
        }
        /* on bad json keep going */
        return true;
    }
    ajax_poll(url, do_callback, refresh_ms);
}

/*
  poll a json file and fill document IDs at the given rate
*/
function ajax_json_poll_fill(url, refresh_ms=1000) {
    function callback(json) {
        page_fill_json_html(json);
        return true;
    }
    ajax_json_poll(url, callback, refresh_ms);
}

/*
  poll a json file and fill input values
*/
function ajax_json_poll_fill_values(url, refresh_ms=1000) {
    function callback(json) {
        page_fill_json_value(json);
        page_fill_json_html(json);
        return true;
    }
    ajax_json_poll(url, callback, refresh_ms);
}


/*
  set a message in a div by id, with given color
*/
function set_message_color(id, color, message) {
    var element = document.getElementById(id);
    if (element) {
        element.innerHTML = '<b style="color:' + color + '">' + message + '</b>';
    }
}

function mockConfigUrl() {
    var params = new URLSearchParams();
    params.set("enable", document.getElementById("MOCK:ENABLE").value);
    params.set("home_lat", document.getElementById("MOCK:HOME_LAT").value);
    params.set("home_lon", document.getElementById("MOCK:HOME_LON").value);
    params.set("ghosts", document.getElementById("MOCK:GHOSTS").value);
    params.set("radius", document.getElementById("MOCK:RADIUS").value);
    params.set("speed", document.getElementById("MOCK:SPEED").value);
    params.set("uas_type", document.getElementById("MOCK:UAS_TYPE").value);
    params.set("uas_id", document.getElementById("MOCK:UAS_ID").value);
    params.set("self_id", document.getElementById("MOCK:SELF_ID").value);
    params.set("operator_id", document.getElementById("MOCK:OPERATOR_ID").value);
    return "/ajax/mock/set?" + params.toString();
}

function saveMockConfig() {
    ajax_get_callback(mockConfigUrl(), function(responseText) {
        try {
            page_fill_json_value(JSON.parse(responseText));
            set_message_color("MOCK:MESSAGE", "green", "Saved");
        } catch (e) {
            set_message_color("MOCK:MESSAGE", "red", "Save failed");
        }
    });
}

function qaConfigUrl() {
    var params = new URLSearchParams();
    params.set("enable", document.getElementById("QA:ENABLE").checked ? "1" : "0");
    params.set("uas_id_seed", document.getElementById("QA:UAS_ID_SEED").value);
    params.set("home_lat", document.getElementById("QA:HOME_LAT").value);
    params.set("home_lon", document.getElementById("QA:HOME_LON").value);
    params.set("alt_m", document.getElementById("QA:ALT_M").value);
    params.set("radius", document.getElementById("QA:RADIUS").value);
    params.set("speed", document.getElementById("QA:SPEED").value);
    params.set("heading_mode", document.getElementById("QA:HEADING_MODE").value);
    params.set("slot_count", document.getElementById("QA:SLOT_COUNT").value);
    params.set("lab_label", document.getElementById("QA:LAB_LABEL").value);
    params.set("lab_mac_override", document.getElementById("QA:LAB_MAC_OVERRIDE").value);
    return "/ajax/qa/set?" + params.toString();
}

function qaRandomHexOctet() {
    return Math.floor(Math.random() * 256).toString(16).toUpperCase().padStart(2, "0");
}

function qaRandomAlphaNum(length) {
    var chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    var out = "";
    for (var i = 0; i < length; i++) {
        out += chars[Math.floor(Math.random() * chars.length)];
    }
    return out;
}

function qaRandomizeSerial() {
    var input = document.getElementById("QA:UAS_ID_SEED");
    var prefix = (input.value || "").toUpperCase().replace(/[^A-Z0-9]/g, "");
    var targetLength = 16;
    input.value = prefix.length >= targetLength ? prefix.slice(0, targetLength) : prefix + qaRandomAlphaNum(targetLength - prefix.length);
}

function qaRandomizeMac() {
    var input = document.getElementById("QA:LAB_MAC_OVERRIDE");
    var base = (input.value || "").trim().replace(/:+$/g, "");
    var suffix = qaRandomHexOctet() + ":" + qaRandomHexOctet() + ":" + qaRandomHexOctet();
    input.value = base ? base + ":" + suffix : suffix;
}

function qaUpdateEnabledState() {
    var enabledElement = document.getElementById("QA:ENABLE");
    var enabled = enabledElement && enabledElement.checked;
    var cards = document.querySelectorAll(".qa-dependent");
    for (var i = 0; i < cards.length; i++) {
        cards[i].classList.toggle("qa-disabled", !enabled);
        var controls = cards[i].querySelectorAll("input, select, button");
        for (var j = 0; j < controls.length; j++) {
            controls[j].disabled = !enabled;
        }
    }
}

function saveQaConfig() {
    ajax_get_callback(qaConfigUrl(), function(responseText) {
        try {
            page_fill_json_value(JSON.parse(responseText));
            set_message_color("QA:MESSAGE", "green", "Saved");
        } catch (e) {
            set_message_color("QA:MESSAGE", "red", "Save failed");
        }
    });
}

function randomizeMockConfig() {
    ajax_get_callback("/ajax/mock/randomize", function(responseText) {
        try {
            page_fill_json_value(JSON.parse(responseText));
            set_message_color("MOCK:MESSAGE", "green", "Randomized");
        } catch (e) {
            set_message_color("MOCK:MESSAGE", "red", "Randomize failed");
        }
    });
}

function autoHomeFromIP() {
    function applyHome(lat, lon) {
        document.getElementById("MOCK:HOME_LAT").value = lat.toFixed(6);
        document.getElementById("MOCK:HOME_LON").value = lon.toFixed(6);
        saveMockConfig();
        set_message_color("MOCK:MESSAGE", "green", "IP home applied");
    }

    fetch("https://ipwho.is/")
        .then(function(response) { return response.json(); })
        .then(function(json) {
            if (json && json.success !== false && typeof json.latitude === "number" && typeof json.longitude === "number") {
                applyHome(json.latitude, json.longitude);
                return;
            }
            throw new Error("ipwho.is failed");
        })
        .catch(function() {
            fetch("https://ipapi.co/json/")
                .then(function(response) { return response.json(); })
                .then(function(json) {
                    if (json && json.latitude && json.longitude) {
                        applyHome(Number(json.latitude), Number(json.longitude));
                        return;
                    }
                    throw new Error("ipapi failed");
                })
                .catch(function() {
                    if (navigator.geolocation) {
                        navigator.geolocation.getCurrentPosition(function(position) {
                            applyHome(position.coords.latitude, position.coords.longitude);
                        }, function() {
                            set_message_color("MOCK:MESSAGE", "red", "No IP/geolocation result");
                        });
                    } else {
                        set_message_color("MOCK:MESSAGE", "red", "No IP/geolocation result");
                    }
                });
        });
}

function maybeAutoHomeFromIP() {
    var lat = parseFloat(document.getElementById("MOCK:HOME_LAT").value || "0");
    var lon = parseFloat(document.getElementById("MOCK:HOME_LON").value || "0");
    if (Math.abs(lat - 59.4370) < 0.0002 && Math.abs(lon - 24.7536) < 0.0002) {
        autoHomeFromIP();
    }
}

