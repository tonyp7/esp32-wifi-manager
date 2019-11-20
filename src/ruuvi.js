function save_config()
{
	console.log("save_config");

	use_http = $("#use_http")[0].checked;
	http_server = $("#http_server").val();
	http_user = $("#http_user").val();
	http_pass = $("#http_pass").val();

	use_mqtt = $("#use_mqtt")[0].checked;
	mqtt_server = $("#mqtt_server").val();
	mqtt_user = $("#mqtt_user").val();
	mqtt_pass = $("#mqtt_pass").val();

	use_filtering = $("#use_filtering")[0].checked;
	coordinates = $("#coordinates").val();

	var data = {};
	data.use_mqtt = use_mqtt;
	data.mqtt_server = mqtt_server;
	data.mqtt_user = mqtt_user;
	data.mqtt_pass = mqtt_pass;
	data.use_http = use_http;
	data.http_server = http_server;
	data.http_user = http_user;
	data.http_pass = http_pass;
	data.use_filtering = use_filtering;
	data.coordinates = coordinates;

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
