// We make use of this 'server' variable to provide the address of the
// REST Janus API. By default, in this example we assume that Janus is
// co-located with the web server hosting the HTML pages but listening
// on a different port (8088, the default for HTTP in Janus), which is
// why we make use of the 'window.location.hostname' base address. Since
// Janus can also do HTTPS, and considering we don't really want to make
// use of HTTP for Janus if your demos are served on HTTPS, we also rely
// on the 'window.location.protocol' prefix to build the variable, in
// particular to also change the port used to contact Janus (8088 for
// HTTP and 8089 for HTTPS, if enabled).
// In case you place Janus behind an Apache frontend (as we did on the
// online demos at http://janus.conf.meetecho.com) you can just use a
// relative path for the variable, e.g.:
//
// 		var server = "/janus";
//
// which will take care of this on its own.
//
//
// If you want to use the WebSockets frontend to Janus, instead, you'll
// have to pass a different kind of address, e.g.:
//
// 		var server = "ws://" + window.location.hostname + ":8188";
//
// Of course this assumes that support for WebSockets has been built in
// when compiling the gateway. WebSockets support has not been tested
// as much as the REST API, so handle with care!
//
//
// If you have multiple options available, and want to let the library
// autodetect the best way to contact your gateway (or pool of gateways),
// you can also pass an array of servers, e.g., to provide alternative
// means of access (e.g., try WebSockets first and, if that fails, fall
// back to plain HTTP) or just have failover servers:
//
//		var server = [
//			"ws://" + window.location.hostname + ":8188",
//			"/janus"
//		];
//
// This will tell the library to try connecting to each of the servers
// in the presented order. The first working server will be used for
// the whole session.
//
var server = null;
if(window.location.protocol === 'http:')
	server = "http://" + window.location.hostname + ":8088/janus";
else
	server = "https://" + window.location.hostname + ":8089/janus";

var janus = null;
var serial = null;
var started = false;
var startled = false;

/***MESSAGGI JSON***/
var sens_a = '{ "command": "read", "id": 1 }';
var sens_t = '{ "command": "read", "id": 2 }';
var led3_on = '{ "command": "on", "id": 3 }';
var led3_off = '{ "command": "off", "id": 3 }';
var led4_on = '{ "command": "on", "id": 4 }';
var led4_off = '{ "command": "off", "id": 4 }';
var led5_on = '{ "command": "on", "id": 5 }';
var led5_off = '{ "command": "off", "id": 5 }';
var led6_on = '{ "command": "on", "id": 6 }';
var led6_off = '{ "command": "off", "id": 6 }';


$(document).ready(function() {
	// Initialize the library (console debug enabled)
	Janus.init({debug: true, callback: function() {
		// Use a button to start the demo
		$('#start').click(function() {
			if(started)
				return;
			started = true;
			$(this).attr('disabled', true).unbind('click');
			// Make sure the browser supports WebRTC
			if(!Janus.isWebrtcSupported()) {
				bootbox.alert("No WebRTC support... ");
				return;
			}
			// Create session
			janus = new Janus(
				{
					server: server,
					// No "iceServers" is provided, meaning janus.js will use a default STUN server
					// Here are some examples of how an iceServers field may look like to support TURN
					// 		iceServers: [{url: "turn:yourturnserver.com:3478", username: "janususer", credential: "januspwd"}],
					// 		iceServers: [{url: "turn:yourturnserver.com:443?transport=tcp", username: "janususer", credential: "januspwd"}],
					// 		iceServers: [{url: "turns:yourturnserver.com:443?transport=tcp", username: "janususer", credential: "januspwd"}],
					// Should the Janus API require authentication, you can specify either the API secret or user token here too
					//		token: "mytoken",
					//	or
					//		apisecret: "serversecret",
					success: function() {
						// Attach to echo test plugin
						janus.attach(
							{
								plugin: "janus.plugin.serial",
								success: function(pluginHandle) {
									$('#details').remove();
									serial = pluginHandle;
									console.log("Plugin attached! (" + serial.getPlugin() + ", id=" + serial.getId() + ")");
									// Tasto led 
									
					startled=true;
				$('#led').click(function() {
			
			startled = !startled;			
			if(startled)
						$('#led').html("Led On").removeClass("btn-success").addClass("btn-danger");
			else
						$('#led').html("Led Off").removeClass("btn-danger").addClass("btn-success");
		var body = { "audio": true, "video": true };
		console.log("Sending message (" + JSON.stringify(body) + ")");
		serial.send({"message": body});
		   });	

/*************CODICE INVIO JSON**************/

		$('#accel').click(function() {
			
			$('#accel').html("Reading...").removeClass("btn-default").addClass("btn-primary");
			serial.send({"message": sens_a});
		   });	

		$('#temp').click(function() {

			$('#temp').html("Reading...").removeClass("btn-default").addClass("btn-primary");
			serial.send({"message": sens_t});
		   });	


		$('#l3').click(function(){
			if($('#l3').is(':checked')){
		
				window.alert("Led 3 ON");
				serial.send({"message": led3_on});
			}
			else {
				window.alert("Led 3 OFF");
				serial.send({"message": led3_on});
			}
		});			
		
		$('#l4').click(function(){
			if($('#l4').is(':checked')){
		
				window.alert("Led 4 ON");
				serial.send({"message": led4_on});
			}
			else {
				window.alert("Led 4 OFF");
				serial.send({"message": led4_on});
			}
		});	
		
		$('#l5').click(function(){
			if($('#l5').is(':checked')){
		
				window.alert("Led 5 ON");
				serial.send({"message": led5_on});
			}
			else {
				window.alert("Led 5 OFF");
				serial.send({"message": led5_on});
			}
		});	

		$('#l6').click(function(){
			if($('#l6').is(':checked')){
		
				window.alert("Led 6 ON");
				serial.send({"message": led6_on});
			}
			else {
				window.alert("Led 6 OFF");
				serial.send({"message": led6_on});
			}
		});	
      
							
									
									
									$('#start').removeAttr('disabled').html("Stop")
										.click(function() {
											$(this).attr('disabled', true);
											clearInterval(bitrateTimer);
											janus.destroy();
										});
								}, // end success pluginhandler
								
								
								error: function(error) {
									console.log("  -- Error attaching plugin... " + error);
									bootbox.alert("Error attaching plugin... " + error);
								},
								
							}); // end janus attach
					},
					error: function(error) {
						console.log(error);
						bootbox.alert(error, function() {
							window.location.reload();
						});
					},
					destroyed: function() {
						window.location.reload();
					}
				});
		});
	}});
});

