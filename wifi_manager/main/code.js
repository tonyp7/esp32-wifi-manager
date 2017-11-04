// First, checks if it isn't implemented yet.
if (!String.prototype.format) {
  String.prototype.format = function() {
    var args = arguments;
    return this.replace(/{(\d+)}/g, function(match, number) { 
      return typeof args[number] != 'undefined'
        ? args[number]
        : match
      ;
    });
  };
}

var apList = null;
var selectedSSID = "";
var refreshAPInterval = null; 
var checkStatusInterval = null;
var currentConnection = null;

$(function() {
	
	
	$("#wifi-status").on("click", ".ape", function() {
		$( "#wifi" ).slideUp( "fast", function() {});
		$( "#connect-details" ).slideDown( "fast", function() {});
	});
	
	
	$("#wifi-list").on("click", ".ape", function() {
		selectedSSID = $(this).text();
		$( "#ssid-pwd" ).text(selectedSSID);
		$( "#wifi" ).slideUp( "fast", function() {});
		$( "#connect" ).slideDown( "fast", function() {});
		
	});
	
	$("#cancel").on("click", function() {
		$( "#connect" ).slideUp( "fast", function() {});
		$( "#wifi" ).slideDown( "fast", function() {});
		
	});
	
	$("#ok-details").on("click", function() {
		$( "#connect-details" ).slideUp( "fast", function() {});
		$( "#wifi" ).slideDown( "fast", function() {});
		
	});
	
	$("#ok-credits").on("click", function() {
		$( "#credits" ).slideUp( "fast", function() {});
		$( "#app" ).slideDown( "fast", function() {});
		
	});
	
	$("#acredits").on("click", function(event) {
		event.preventDefault();
		$( "#app" ).slideUp( "fast", function() {});
		$( "#credits" ).slideDown( "fast", function() {});
	});
	
	$("#ok-connect").on("click", function() {
		$( "#connect-wait" ).slideUp( "fast", function() {});
		$( "#wifi" ).slideDown( "fast", function() {});
	});
	
	$("#disconnect").on("click", function() {
		$( "#connect-details-wrap" ).addClass('blur');
		$( "#diag-disconnect" ).slideDown( "fast", function() {});
	});
	
	$("#no-disconnect").on("click", function() {
		$( "#diag-disconnect" ).slideUp( "fast", function() {});
		$( "#connect-details-wrap" ).removeClass('blur');
	});
	
	$("#yes-disconnect").on("click", function() {
		$( "#diag-disconnect" ).slideUp( "fast", function() {});
		$( "#connect-details-wrap" ).removeClass('blur');
		
		$.ajax({
			url: '/connect',
			type: 'DELETE',
			success: function(result) {
				
			}
		});
	});
	
	
	
	
	
	$(document).on("click", "#join", function() {
		performConnect();
	});
	
	
	//first time the page loads: attempt get the connection status and start the wifi scan
	refreshAP();
	checkStatusInterval = setInterval(checkStatus, 950);
	refreshAPInterval = setInterval(refreshAP, 2800);


	
	
});


function performConnect(){
	
	//reset connection 
	$( "#loading" ).show();
	$( "#connect-success" ).hide();
	$( "#connect-fail" ).hide();
	
	$( "#ok-connect" ).prop("disabled",true);
	$( "#ssid-wait" ).text(selectedSSID);
	$( "#connect" ).slideUp( "fast", function() {});
	$( "#connect-wait" ).slideDown( "fast", function() {});
	
	//stop refreshing wifi list
	if(refreshAPInterval != null){
		clearInterval(refreshAPInterval);
		refreshAPInterval = null;
	}
	
	var xhr = new XMLHttpRequest();
	xhr.onload = function() {
		if(this.status == 200){
			//start refreshing every now and then the IP page
			//to see if the connection to the STA is made
		}
		else{
			//alert(this.responseText);
		}
	};
	
	var pwd = $("#pwd").val();
	xhr.open("POST", "/connect", true);
	xhr.setRequestHeader('Authorization', "\x02{0}\x03\x02{1}\x03".format(selectedSSID, pwd));
	xhr.send();
}



function rssiToIcon(rssi){
	if(rssi >= -60){
		return 'w0';
	}
	else if(rssi >= -67){
		return 'w1';
	}
	else if(rssi >= -75){
		return 'w2';
	}
	else{
		return 'w3';
	}
}


function refreshAP(){
	$.getJSON( "/ap.json", function( data ) {
		if(data.length > 0){
			//sort by signal strength
			data.sort(function (a, b) {
				var x = a["rssi"]; var y = b["rssi"];
				return ((x < y) ? 1 : ((x > y) ? -1 : 0));
			});
			apList = data;
			refreshAPHTML(apList);
			
		}
	});
}

function refreshAPHTML(data){
	var h = "";
	data.forEach(function(e, idx, array) {
		//<div class="ape brdb"><div class="w0"><div class="pw">a0308</div></div></div>
		h += '<div class="ape{0}"><div class="{1}"><div class="{2}">{3}</div></div></div>'.format(idx === array.length - 1?'':' brdb', rssiToIcon(e.rssi), e.auth==0?'':'pw',e.ssid);
		h += "\n";
	});
	
	$( "#wifi-list" ).html(h)
}

function checkStatus(){
	$.getJSON( "/status", function( data ) {
		if(data["ssid"]){
			//got connection
			currentConnection = data;
			$("#connected-to span").text(data["ssid"]);
			$("#connect-details h1").text(data["ssid"]);
			$("#ip").text(data["ip"]);
			$("#netmask").text(data["netmask"]);
			$("#gw").text(data["gw"]);
			
			$("#wifi-status").slideDown( "fast", function() {});
			
			//unlock the wait screen if needed
			$( "#ok-connect" ).prop("disabled",false);
			
			//update wait screen
			$( "#loading" ).hide();
			$( "#connect-success" ).show();
			$( "#connect-fail" ).hide();
		}
		else{
			//disconnected
			currentConnection = null;
			$("#connected-to span").text('');
			$("#connect-details h1").text('');
			$("#wifi-status").slideUp( "fast", function() {});
		}
	})
	.fail(function() {
		//don't do anything, the server might be down
	});


}


