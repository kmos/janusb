/***SERIAL_JS***/

var server = null;
if(window.location.protocol === 'http:')
	server = "http://" + window.location.hostname + ":8088/janus";
else
	server = "https://" + window.location.hostname + ":8089/janus";

var janus = null;
var serial = null;
var started = false;
var startled = false;
var startled3 = false;
var startled4 = false;
var startled5 = false;
var startled6 = false;


/***MESSAGGI JSON***/
var sens_a = { "command": 2, "id": 1 }; //read accelerometer
var sens_t = { "command": 2, "id": 2 }; //read temperature
var led3_on = { "command": 0, "id": 3 }; //on l3
var led3_off = { "command": 1, "id": 3 }; //off l3
var led4_on = { "command": 0, "id": 4 }; //on l4
var led4_off = { "command": 1, "id": 4 }; //off l4
var led5_on = { "command": 0, "id": 5 }; //on l5
var led5_off = { "command": 1, "id": 5 }; //off l5
var led6_on = { "command": 0, "id": 6 }; //on l6
var led6_off = { "command": 1, "id": 6 }; //off l6


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
					success: function() {
						// Attach to echo test plugin
						janus.attach(
							{
								plugin: "janus.plugin.serial",
								success: function(pluginHandle) {
									$('#details').remove();
									serial = pluginHandle;
									console.log("Plugin attached! (" + serial.getPlugin() + ", id=" + serial.getId() + ")");
									// Show features 
									$('#features').removeClass('hide').show();
									$('#clear').removeClass('hide').show();
									$('#sensordiv').removeClass('hide').show();
									$('#lediv').removeClass('hide').show();
									$('#texta').removeClass('hide').show();
									
				
									/*************CODICE INVIO JSON**************/
									//accelerometer
									$('#accel').click(function() {
										serial.send({"message": sens_a});
									});	
									
									//temperature
									$('#temp').click(function() {
										serial.send({"message": sens_t});
									});	

									//Led 3
									$('#l3').click(function(){
										startled3 = !startled3;			
										if(startled3){	
											$('#l3').html("ON").removeClass("btn-default").addClass("btn-warning");
											serial.send({"message": led3_on});
										}
										else{											
											$('#l3').html("OFF").removeClass("btn-warning").addClass("btn-default");
											serial.send({"message": led3_off});											
										}
									});
												
									//Led 4		
									$('#l4').click(function(){
										startled4 = !startled4;			
										if(startled4){
											$('#l4').html("ON").removeClass("btn-default").addClass("btn-success");
											serial.send({"message": led4_on});
										}
										else{				
											$('#l4').html("OFF").removeClass("btn-success").addClass("btn-default");
											serial.send({"message": led4_off});
										}
									});
										
									//Led 5		
									$('#l5').click(function(){
										startled5 = !startled5;			
										if(startled5){
											$('#l5').html("ON").removeClass("btn-default").addClass("btn-danger");
											serial.send({"message": led5_on});
										}
										else{
											$('#l5').html("OFF").removeClass("btn-danger").addClass("btn-default");
											serial.send({"message": led5_off});
										}
									});	
									
									//Led 6
									$('#l6').click(function(){
										startled6 = !startled6;			
										if(startled6){
											$('#l6').html("ON").removeClass("btn-default").addClass("btn-primary");
											serial.send({"message": led6_on});
										}
										else{				
											$('#l6').html("OFF").removeClass("btn-primary").addClass("btn-default");
											serial.send({"message": led6_off});
										}
									});	
      
									//Quando si preme "stop session".....
									$('#start').removeAttr('disabled').html("Stop Session")
										.click(function() {
											$(this).attr('disabled', true);
											janus.destroy();
										});
										
									//Tasto Clear	
									$('#clear').click(function(){
										$('#result').text('');
										$('#textacc').val('');
										$('#textemp').val('');
										
									});
								}, // end success pluginhandler 
								
								
								error: function(error) {
									console.log("  -- Error attaching plugin... " + error);
									bootbox.alert("Error attaching plugin... " + error);
								},
								
								onmessage: function(msg) {
									console.log(" ::: Got a message :::");
									console.log(JSON.stringify(msg));
									var text = JSON.stringify(msg);
									var obj = JSON.parse(text);
									var now = new Date(Date.now());
									var formatted = now.getHours() + ":" + now.getMinutes() + ":" + now.getSeconds();
									var tac="accelerometer";
									var tem="temperature";
									$('#result').append(formatted+" -> "+text+"\n");
									if(obj.type ==tac)
										$('#textacc').val(obj.measure);
									else if(obj.type ==tem)
										$('#textemp').val(obj.measure);
								
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

