//
// This file is part of the GNU ARM Eclipse distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

// ----------------------------------------------------------------------------
#include "main.h"

/* Main programm --------------------------*/
int main(){
	setup();
	//Main loop
	while(isLoop()){
		loop();
	}
	finalize();
}

/* Setup Function --------------------------*/
void setup(){
	HAL_Init();
	//Initializze USB device
	USBD_Init(&USBD_Device, &VCP_Desc, 0);
	USBD_RegisterClass(&USBD_Device, &USBD_CDC);
	USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_Template_fops);
	USBD_Start(&USBD_Device);
	HAL_Delay(4000);
	//Initializze board's led
	BSP_LED_Init(0);
	BSP_LED_Init(1);
	BSP_LED_Init(2);
	BSP_LED_Init(3);
	//Initializze logical variable and memory areas
	memset(response, '\0', sizeof response);
	jsmn_init(&parser);

}
/* Loop function ------------------------------*/
void loop(){

	char buf = '\0';
	int spot = 0;
	jsmnerr_t r;


	while(VCP_read(&buf,1) != 0){
	  BSP_LED_On(LED4);
	  if(buf == '\n'){
		  BSP_LED_Off(LED4);
		  r = jsmn_parse(&parser, response,strlen(response), tokens, 256);
		  for (int i=0;i<2;i++){
			  VCP_write(&tokens[i],sizeof(tokens[i]));
		  }

		  break;
	  }
	  sprintf(&response[spot], "%c", buf );
	  spot += 1;
	}

	BSP_LED_On(LED3);
}

uint8_t isLoop(){
	return 1;
}


void finalize(){
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
