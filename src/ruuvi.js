function save_config()
{
	console.log("save_config");

	var data = {};
	data.use_mqtt = $("#use_mqtt")[0].checked;
	data.mqtt_server = $("#mqtt_server").val();
	data.mqtt_user = $("#mqtt_user").val();
	data.mqtt_pass = $("#mqtt_pass").val();

	data.use_http = $("#use_http")[0].checked;
	data.http_server = $("#http_server").val();
	data.http_user = $("#http_user").val();
	data.http_pass = $("#http_pass").val();

	mqtt_port = parseInt($("#mqtt_port").val())
	if (mqtt_port == NaN) {
		mqtt_port = 0;
	}

	http_port = parseInt($("#http_port").val())
	if (http_port == NaN) {
		http_port = 0;
	}

	data.mqtt_port = mqtt_port;
	data.http_port = http_port;

	data.use_filtering = $("#use_filtering")[0].checked;
	data.coordinates = $("#coordinates").val();

	$.ajax({
		url: '/ruuvi.json',
		dataType: 'json',
		contentType : 'application/json',
		method: 'POST',
		cache: false,
		data: JSON.stringify(data)
	});
}

function get_config(){
	$.getJSON( "/ruuvi.json", function( data ) {
		if(data != null){
			for (var key in data) {
				var el = $("#"+key);
				if (el.length > 0) {
					if (el[0].type == "checkbox") {
						el[0].checked = data[key];
					} else {
						$("#" + key).val(data[key]);
					}
				}
			}
		}
	});
}

function showError(error) {
	switch(error.code) {
	  case error.PERMISSION_DENIED:
		msg ="Error: Geolocation not allowed."
		console.log(msg)
		alert(msg)
		break;
	  case error.POSITION_UNAVAILABLE:
		msg = "Location information is unavailable."
		console.log(msg)
		alert(msg)
		break;
	  case error.TIMEOUT:
		msg = "The request to get user location timed out."
		console.log(msg)
		alert(msg)
		break;
	  case error.UNKNOWN_ERROR:
		msg = "An unknown error occurred."
		console.log(msg)
		alert(msg)
		break;
	}
  }

function round(x) {
	var decimals = 5
	return Number.parseFloat(x).toFixed(decimals);
}

function handlePosition(position) {
	loc = round(position.coords.latitude) + "," + round(position.coords.longitude);
	//console.log(loc)
	$("#coordinates").val(loc)
}

function get_location() {
	if (navigator.geolocation) {
		navigator.geolocation.getCurrentPosition(handlePosition, showError);
	  } else {
		alert("Geolocation is not supported by this browser");
	  }
}

$(document).ready(function() {
	//get configuration from flash and fill the web page
	get_config();

	$("#save_config").on("click", function() {
		save_config();
	});

	$("#get_location").on("click", function() {
		get_location();
	})
});
