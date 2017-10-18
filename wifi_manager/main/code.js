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

var selectedSSID = "";

$(function() {

	$(".networks").on("click", "label", function() {
		$( ".networks" ).slideUp( "fast", function() {});
		$( ".login" ).slideDown( "fast", function() {});
		$( "#login-data" ).slideDown( "fast", function() {});
		$( "h2" ).text("Password for " + $(this).text());
		selectedSSID = $(this).text();
	});
	
	$( "#cancel" ).click(function() {
		$("#join").prop("disabled", false);
		
		$( "#login-data" ).slideUp( "fast", function() {});
		$( "#loading" ).slideUp( "slow", function() {});
		
		
		$( ".networks" ).slideDown( "fast", function() {});
		$( ".login" ).slideUp( "fast", function() {});
		$( "h2" ).text("Choose a network...");
	});
	
	$(document).on("click", "#join", function() {
		$("#join").prop("disabled", true);
		$( "#login-data" ).slideUp( "fast", function() {});
		$( "#loading" ).slideDown( "slow", function() {});
		$( "h2" ).text("Connecting to " + selectedSSID);
		
		
		var pwd = $("#pwd").val();
		$.post( "connect", { ssid: selectedSSID, password: pwd, ok: "true" })
		.done(function( data ) {
			//response code
			alert(data);
		})
		.fail(function() {
			//fail code
		});
		
		
	});
	
	
	
});


function rssi_to_icon(rssi){
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

function refresh_wifi(){
	$.getJSON( "ap.json", function( data ) {
		if(data.length > 0){
			//sort by wifi strength
			data.sort(function (a, b) {
				var x = a["rssi"]; var y = b["rssi"];
				return ((x < y) ? 1 : ((x > y) ? -1 : 0));
			});
			
			var h = "";
			var id = 1;
			data.forEach(function(e) {
				h += '<input id="{0}" name="wifi" type="radio" /><label class="{1}" for="{2}"><div class="{3}">{4}</div></label>'.format(id, rssi_to_icon(e.rssi), e.ssid ,e.auth==0?'':'pw',e.ssid);
				id++;
			});
			
			$( ".networks" ).html(h)
		}
	});
}