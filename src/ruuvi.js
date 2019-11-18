function save_config()
{
	console.log("save_config");

	use_http = $("#use_http")[0].checked;
	http_server = $("#http_server").val();
	http_user = $("#http_user").val();
	http_pass = $("#http_password").val();

	use_mqtt = $("#use_mqtt")[0].checked;
	mqtt_server = $("#mqtt_server").val();
	mqtt_user = $("#mqtt_user").val();
	mqtt_pass = $("#mqtt_password").val();

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

$(document).ready(function() {
	$("#save_config").on("click", function() {
		save_config();
	});
});
