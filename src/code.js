// save some bytes
const gel = (e) => document.getElementById(e);

const wifi = gel("wifi");
const connect = gel("connect");
const connect_manual = gel("connect_manual");
const connect_wait = gel("connect-wait");
const connect_details = gel("connect-details");

// First, checks if it isn't implemented yet.
if (!String.prototype.format) {
  String.prototype.format = function () {
    var args = arguments;
    return this.replace(/{(\d+)}/g, function (match, number) {
      return typeof args[number] != "undefined" ? args[number] : match;
    });
  };
}

function docReady(fn) {
  // see if DOM is already available
  if (
    document.readyState === "complete" ||
    document.readyState === "interactive"
  ) {
    // call on next available tick
    setTimeout(fn, 1);
  } else {
    document.addEventListener("DOMContentLoaded", fn);
  }
}

var apList = null;
var selectedSSID = "";
var refreshAPInterval = null;
var checkStatusInterval = null;

function stopCheckStatusInterval() {
  if (checkStatusInterval != null) {
    clearInterval(checkStatusInterval);
    checkStatusInterval = null;
  }
}

function stopRefreshAPInterval() {
  if (refreshAPInterval != null) {
    clearInterval(refreshAPInterval);
    refreshAPInterval = null;
  }
}

function startCheckStatusInterval() {
  checkStatusInterval = setInterval(checkStatus, 950);
}

function startRefreshAPInterval() {
  refreshAPInterval = setInterval(refreshAP, 2800);
}

docReady(async function () {
  gel("wifi-status").addEventListener(
    "click",
    function (e) {
      gel("wifi").style.display = "none";
      document.getElementById("connect-details").style.display = "block";
    },
    false
  );

  gel("manual_add").addEventListener(
    "click",
    function (e) {
      selectedSSID = e.target.innerText;

      gel("ssid-pwd").textContent = selectedSSID;
      gel("wifi").style.display = "none";
      gel("connect_manual").style.display = "block";
      gel("connect").style.display = "none";

      gel("connect-success").display = "none";
      gel("connect-fail").display = "none";
    },
    false
  );

  gel("wifi-list").addEventListener(
    "click",
    function (e) {
      selectedSSID = e.target.innerText;
      console.log("selected", selectedSSID);
      gel("ssid-pwd").textContent = selectedSSID;
      gel("connect").style.display = "block";
      gel("wifi").style.display = "none";
      // init_cancel();
    },
    false
  );

  function cancel(e) {
    selectedSSID = "";
    connect.style.display = "none";
    connect_manual.style.display = "none";
    wifi.style.display = "block";
  }

  gel("cancel").addEventListener("click", cancel, false);

  gel("manual_cancel").addEventListener("click", cancel, false);

  gel("join").addEventListener(
    "click",
    function (e) {
      performConnect();
    },
    false
  );

  gel("manual_join").addEventListener(
    "click",
    function (e) {
      performConnect($(this).data("connect"));
    },
    false
  );

  gel("ok-details").addEventListener(
    "click",
    function (e) {
      gel("connect-details").style.display = "none";
      gel("wifi").style.display = "block";
    },
    false
  );

  gel("ok-credits").addEventListener(
    "click",
    function (e) {
      gel("credits").style.display = "none";
      gel("app").style.display = "block";
    },
    false
  );

  gel("acredits").addEventListener(
    "click",
    function (e) {
      event.preventDefault();
      gel("app").style.display = "none";
      gel("credits").style.display = "block";
    },
    false
  );

  gel("ok-connect").addEventListener(
    "click",
    function (e) {
      gel("connect-wait").style.display = "none";
      gel("wifi").style.display = "block";
    },
    false
  );

  gel("disconnect").addEventListener(
    "click",
    function (e) {
      gel("connect-details-wrap").addClass("blur");
      gel("diag-disconnect").style.display = "block";
    },
    false
  );

  gel("no-disconnect").addEventListener(
    "click",
    function (e) {
      gel("diag-disconnect").style.display = "none";
      gel("connect-details-wrap").removeClass("blur");
    },
    false
  );

  gel("yes-disconnect").addEventListener("click", async function (e) {
    stopCheckStatusInterval();
    selectedSSID = "";

    document.getElementById("diag-disconnect").style.display = "none";
    gel("connect-details-wrap").removeClass("blur");

    const response = await fetch("/connect.json", {
      method: "DELETE",
      headers: {
        "Content-Type": "application/json",
        "X-Custom-ssid": selectedSSID,
        "X-Custom-pwd": pwd,
      },
      body: { timestamp: Date.now() },
    });

    startCheckStatusInterval();

    connect_details.style.display = "none";
    wifi.style.display = "block";
  });

  //first time the page loads: attempt get the connection status and start the wifi scan
  await refreshAP();
  startCheckStatusInterval();
  startRefreshAPInterval();
});

async function performConnect(conntype) {
  //stop the status refresh. This prevents a race condition where a status
  //request would be refreshed with wrong ip info from a previous connection
  //and the request would automatically shows as succesful.
  stopCheckStatusInterval();

  //stop refreshing wifi list
  stopRefreshAPInterval();

  var pwd;
  if (conntype == "manual") {
    //Grab the manual SSID and PWD
    selectedSSID = gel("manual_ssid").value;
    pwd = gel("manual_pwd").value;
  } else {
    pwd = gel("pwd").value;
  }
  //reset connection
  gel("loading").style.display = "block";
  gel("connect-success").style.display = "none";
  gel("connect-fail").style.display = "none";

  gel("ok-connect").disabled = true;
  gel("ssid-wait").textContent = selectedSSID;
  connect.style.display = "none";
  connect_manual.style.display = "none";
  gel("connect-wait").style.display = "block";

  const response = await fetch("/connect.json", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      "X-Custom-ssid": selectedSSID,
      "X-Custom-pwd": pwd,
    },
    body: { timestamp: Date.now() },
  });

  //now we can re-set the intervals regardless of result
  startCheckStatusInterval();
  startRefreshAPInterval();
}

function rssiToIcon(rssi) {
  if (rssi >= -60) {
    return "w0";
  } else if (rssi >= -67) {
    return "w1";
  } else if (rssi >= -75) {
    return "w2";
  } else {
    return "w3";
  }
}

async function refreshAP(url = "/ap.json") {
  try {
    var res = await fetch(url);
    var access_points = await res.json();
    if (access_points.length > 0) {
      //sort by signal strength
      access_points.sort((a, b) => {
        var x = a["rssi"];
        var y = b["rssi"];
        return x < y ? 1 : x > y ? -1 : 0;
      });
      refreshAPHTML(access_points);
    }
  } catch (e) {
    console.info("Access points returned empty from /ap.json!");
  }
}

function refreshAPHTML(data) {
  var h = "";
  data.forEach(function (e, idx, array) {
    h += '<div class="ape{0}"><div class="{1}"><div class="{2}">{3}</div></div></div>'.format(
      idx === array.length - 1 ? "" : " brdb",
      rssiToIcon(e.rssi),
      e.auth == 0 ? "" : "pw",
      e.ssid
    );
    h += "\n";
  });

  gel("wifi-list").innerHTML = h;
}

async function checkStatus(url = "/status.json") {
  try {
    var response = await fetch(url);
    var data = await response.json();
    if (data && data.hasOwnProperty("ssid") && data["ssid"] != "") {
      if (data["ssid"] === selectedSSID) {
        //that's a connection attempt
        if (data["urc"] === 0) {
          //got connection
          document.querySelector("#connected-to div div div span").textContent =
            data["ssid"];
          document.querySelector("#connect-details h1").textContent =
            data["ssid"];
          gel("ip").textContent = data["ip"];
          gel("netmask").textContent = data["netmask"];
          gel("gw").textContent = data["gw"];
          gel("wifi-status").style.display = "none";

          //unlock the wait screen if needed
          gel("ok-connect").disabled = false;

          //update wait screen
          gel("loading").style.display = "none";
          gel("connect-success").style.display = "block";
          gel("connect-fail").style.display = "none";

          //           var link = gel("outbound_a_href_on_success");
          //           link.setAttribute("href", "http://" + data["ip"]);
        } else if (data["urc"] === 1) {
          //failed attempt
          document.querySelector("#connected-to div div div span").textContent =
            data["ssid"];
          document.querySelector("#connect-details h1").textContent =
            data["ssid"];
          gel("ip").textContent = "0.0.0.0";
          gel("netmask").textContent = "0.0.0.0";
          gel("gw").textContent = "0.0.0.0";

          //don't show any connection
          gel("wifi-status").display = "none";

          //unlock the wait screen
          gel("ok-connect").disabled = false;

          //update wait screen
          gel("loading").display = "none";
          gel("connect-fail").style.display = "block";
          gel("connect-success").style.display = "none";
        }
      } else if (data.hasOwnProperty("urc") && data["urc"] === 0) {
        console.log("Connected!");
        //ESP32 is already connected to a wifi without having the user do anything
        if (
          gel("wifi-status").style.display == "" ||
          gel("wifi-status").style.display == "none"
        ) {
          document.querySelector("#connected-to div div div span").textContent =
            data["ssid"];
          document.querySelector("#connect-details h1").textContent =
            data["ssid"];
          gel("ip").textContent = data["ip"];
          gel("netmask").textContent = data["netmask"];
          gel("gw").textContent = data["gw"];
          gel("wifi-status").style.display = "block";

          //           var link = gel("outbound_a_href");
          //           link.setAttribute("href", "http://" + data["ip"]);
        }
      }
    } else if (data && data.hasOwnProperty("urc") && data["urc"] === 2) {
      //that's a manual disconnect
      if (gel("wifi-status").style.display == "block") {
        gel("wifi-status").style.display == "none";
      }
    } else {
      console.info("Status returned empty from /status.json!");
    }
  } catch (e) {
    console.info("Was not able to fetch /status.json");
  }
}
