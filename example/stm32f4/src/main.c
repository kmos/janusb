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
	//Initializze board's led
	BSP_LED_Init(0);
	BSP_LED_Init(1);
	BSP_LED_Init(2);
	BSP_LED_Init(3);
	//Initializze board's accelerometer
    BSP_ACCELERO_Init();
	//Initializze USB device
	BSP_LED_On(LED3);
	USBD_Init(&USBD_Device, &VCP_Desc, 0);
	USBD_RegisterClass(&USBD_Device, &USBD_CDC);
	USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_Template_fops);
	USBD_Start(&USBD_Device);
	HAL_Delay(4000);
	BSP_LED_Off(LED3);
	//Initializze logical variable and memory areas
	memset(response, '\0',strlen(response));
}
/* Loop function ------------------------------*/
void loop(){
	/*Local Logical Variable ------------------*/
	char buf = '\0';
	int spot = 0, p_code;
	Comand receivedcomand;
	//Reading cycle
	while(VCP_read(&buf,1) != 0){
		if(buf == '\n'){
			p_code = parsing(&receivedcomand);
			execComand(receivedcomand);
			if(p_code == 1){/*ERRORE NEL PARSING DA GESTIRE*/ }
			break;
		}
	  sprintf(&response[spot], "%c", buf );
	  spot += 1;
	}
	memset(response, '\0', strlen(response));
	//Qua ci va il codice per le letture periodiche se ci riesco a implementarle.
}
//Parsing the message and execute commands
int  parsing (Comand *received)
{
	/* Parsing Variable */
	jsmn_parser parser;
	jsmntok_t tokens[256];
	/*Use variable */
	char *store;
	int t_length = 0;
	/*Initializze variable */
	memset(received->name,'\0',10);
	received->ID = 0;
	/*Initializze parse */
	jsmn_init(&parser);
	//Parse the string
	int r = jsmn_parse(&parser, response, strlen(response), tokens, 256);
	// Error control
	if (r < 0) 	return 1;
	/* Assume the top-level element is an object */
	if (tokens[0].type != JSMN_OBJECT)return 1;
	/*Plays the string*/
	for (int i = 1; i < r; i++) {
		switch(tokens[i].type){
			case JSMN_STRING:
				t_length = tokens[i].end-tokens[i].start;
				store = malloc(sizeof(char)*t_length);
				strncpy(store,&response[tokens[i].start],t_length);
				if(strncmp(store,"command",t_length) == 0){
					strncpy(received->name,&response[tokens[i+1].start],tokens[i+1].end-tokens[i+1].start);
				}else if(strncmp(store,"ID",t_length) == 0){
					memset(store,'\0',tokens[i+1].end-tokens[i+1].start);
					strncpy(store,&response[tokens[i+1].start],tokens[i+1].end-tokens[i+1].start);
					received->ID = atoi(store);
				}else{
					//Quando mi arriva qualcosa di inateso
				}
				free(store);
				i++;
				break;
			case JSMN_PRIMITIVE: break;
			case JSMN_ARRAY: break;

		}
	}
	//Devo liberare le risorse so sicuro
	return 0;
}

void execComand(Comand received){
	if(strcmp(received.name,"on") == 0){
		if(received.ID == 3) BSP_LED_On(LED3);
		if(received.ID == 4) BSP_LED_On(LED4);
		if(received.ID == 5) BSP_LED_On(LED5);
		if(received.ID == 6) BSP_LED_On(LED6);
	}else if(strcmp(received.name,"off") == 0){
		if(received.ID == 6) BSP_LED_Off(LED6);
		if(received.ID == 5) BSP_LED_Off(LED5);
		if(received.ID == 4) BSP_LED_Off(LED4);
		if(received.ID == 3) BSP_LED_Off(LED3);
	}else if(strcmp(received.name,"read") == 0) {
		if(received.ID == 1){
			/*Initializzae local variable*/
			int16_t pos[3];
			int i = 0;
			/*Reading the board accelerometer */
			BSP_ACCELERO_GetXYZ(pos);
			/*Build JSON response */
			memset(response,'\0',256);
			//buildAcceleroResponse(pos);
			/*Send the json on usb*/
			for (i=0;i<256;i++) VCP_write(&response[i],1);
		}else if (received.ID == 2){
			//Lettura da sensore di temperatura

		}else{
			//Nel caso arrivi un ID da cui non si può leggere
		}
	}else{
		//Nel caso arrivi un comando non conosciuto
	}
}

void buildAcceleroResponse(int16_t * pos)
{
	char *buf;
	strcat (response,"{ \"opstatus\" : \"ok\", \"measure\" : [");
	strcat (response,itoa(pos[0],buf,10));
	strcat (response,",");
	strcat (response,itoa(pos[1],buf,10));
	strcat (response,",");
	strcat (response,itoa(pos[2],buf,10));
	strcat (response,"], \"type\" : \"accelerometer\" }");
}

uint8_t isLoop(){
	return 1;
}


void finalize(){
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
