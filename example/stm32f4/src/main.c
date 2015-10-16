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
	//Initializze ADC conv
	MX_ADC1_Init();

	//Initializze logical variable and memory areas
	memset(request, '\0',256);
	memset(response,'\0',256);
}
/* Loop function ------------------------------*/
void loop(){
	/*Local Logical Variable ------------------*/
	char buf = '\0';
	int spot = 0, p_code;
	Comand receivedcomand;
	//Reading cycle
	while(VCP_read(&buf,1) != 0){
		if(buf!='\n'){
			sprintf(&request[spot], "%c", buf );
			spot += 1;
		}
	}
	if(strlen(request) != 0){
		if(parsing(&receivedcomand) == 1) {/*TODO: gestione errori di ricezioni*/}
		execComand(receivedcomand);
	}
	//Reset the request string
	memset(request, '\0', strlen(request));
	//TODO: Letture periodiche
}
//Parsing the message and execute commands
int  parsing (Comand *received)
{
	/* Parsing Variable */
	jsmn_parser parser;
	jsmntok_t tokens[256];
	/*Use variable */
	char store[20];
	int t_length = 0;
	/*Initializze variable */
	received->name 	= 0;
	received->ID 	= 0;
	memset(store,"\0",20);
	/*Initializze parse */
	jsmn_init(&parser);
	//Parse the string
	int r = jsmn_parse(&parser, request, strlen(request), tokens, 256);
	// Error control
	if (r < 0) 	return 1;
	/* Assume the top-level element is an object */
	if (tokens[0].type != JSMN_OBJECT)return 1;
	/*Plays the string*/
	for (int i = 1; i < r; i++) {
		switch(tokens[i].type){
			case JSMN_STRING:
				t_length = tokens[i].end-tokens[i].start;
				strncpy(store,&request[tokens[i].start],t_length);

				if(strncmp(store,"command",t_length) == 0){
					memset(store,"\0",20);
					strncpy(store,&request[tokens[i+1].start],tokens[i+1].end-tokens[i+1].start);
					received->name = atoi(store);
				}else if(strncmp(store,"id",t_length) == 0){
					memset(store,"\0",t_length);
					strncpy(store,&request[tokens[i+1].start],tokens[i+1].end-tokens[i+1].start);
					received->ID = atoi(store);
				}else{
					//Quando mi arriva qualcosa di inateso
				}
				i++;
				break;
			case JSMN_PRIMITIVE: break;
			case JSMN_ARRAY: break;
		}
	}
	//Devo liberare le risorse so sicuro
	return 0;
}

int execComand(Comand received){
	memset(response,'\0',256);
	switch(received.name){
		case C_ON :
			if(received.ID == 3){
				BSP_LED_On(LED3);
				sprintf(response,"{ \"opstatus\" : \"ok\ , \"id\" : %d }\n",received.ID);
				VCP_write(&response,256);
			}
			if(received.ID == 4){
				BSP_LED_On(LED4);
				sprintf(response,"{ \"opstatus\" : \"ok\ , \"id\" : %d }\n",received.ID);
				VCP_write(&response,256);
			}
			if(received.ID == 5){
				BSP_LED_On(LED5);
				sprintf(response,"{ \"opstatus\" : \"ok\ , \"id\" : %d }\n",received.ID);
				VCP_write(&response,256);
			}
			if(received.ID == 6){
				BSP_LED_On(LED6);
				sprintf(response,"{ \"opstatus\" : \"ok\ , \"id\" : %d }\n",received.ID);
			  VCP_write(&response,256);
			}
			break;
		case C_OFF :
			if(received.ID == 6) {
				BSP_LED_Off(LED6);
				sprintf(response,"{ \"opstatus\" : \"ok\ , \"id\" : %d }\n",received.ID);
				VCP_write(&response,256);
			}
			if(received.ID == 5){
				BSP_LED_Off(LED5);
				sprintf(response,"{ \"opstatus\" : \"ok\ , \"id\" : %d }\n",received.ID);
				VCP_write(&response,256);
			}
			if(received.ID == 4){
				BSP_LED_Off(LED4);
				sprintf(response,"{ \"opstatus\" : \"ok\ , \"id\" : %d }\n",received.ID);
				VCP_write(&response,256);
			}
			if(received.ID == 3) {
				BSP_LED_Off(LED3);
				sprintf(response,"{ \"opstatus\" : \"ok\ , \"id\" : %d }\n",received.ID);
				VCP_write(&response,256);
			}
			break;
		case C_READ :
			if(received.ID == 1){
				/*Initializzae local variable*/
				int16_t pos[3];
				/*Reading the board accelerometer */
				BSP_ACCELERO_GetXYZ(pos);
				/*Build JSON response */
				sprintf(response,"{ \"opstatus\" : \"ok\", \"measure\" : [ %d,%d,%d],\"type\" : \"accelerometer\" }\n ",pos[0],pos[1],pos[2]);
				//sprintf(response,"{ \"session\" : %d , \"opstatus\" : \"ok\", \"measure\" : [ %d,%d,%d],\"type\" : \"accelerometer\" }\n ",session,pos[0],pos[1],pos[2]);
				/*Send the json on usb*/
				VCP_write(&response,256);
			}else if (received.ID == 2){
				//Lettura da sensore di temperatura
				float temperature;
				//Start the conversion
				if(HAL_ADC_Start (&hadc1) != HAL_OK){
					//Gestire errore
				}
				while (HAL_ADC_GetState(&hadc1) == HAL_ADC_STATE_BUSY ){}
				//Processing the conversion
				temperature = HAL_ADC_GetValue(&hadc1); //Return the converted data
				temperature *= 3300;
				temperature /= 0xfff; //Reading in mV
				temperature /= 1000.0; //Reading in Volts
				temperature -= 0.760; // Subtract the reference voltage at 25°C
				temperature /= .0025; // Divide by slope 2.5mV

				temperature += 25.0; // Add the 25°C

				int d1 = temperature;            // Get the integer part (678).
				float f2 = temperature - d1;     // Get fractional part (678.0123 - 678 = 0.0123).
				int d2 = trunc(f2 * 10000);   // Turn into integer (123).
				// Print as parts, note that you need 0-padding for fractional bit.
				// Since d1 is 678 and d2 is 123, you get "678.0123".
				sprintf(response,"{ \"opstatus\" : \"ok\", \"measure\" : %d.%d4,\"type\" : \"temperature\" }\n ",d1,d2);
				//sprintf(response,"{ \"session\" : %d , \"opstatus\" : \"ok\", \"measure\" : %d.%d4,\"type\" : \"temperature\" }\n ",session,d1,d2);
				VCP_write(&response,256);
			}
			break;
		default:
			//Nel caso arrivi un comando non conosciuto
			sprintf(response,"{ \"opstatus\" : \"err\" , \"code\" : 1 }\n");
			VCP_write(&response,256);
			return 1;
			break;
	}
	memset(response,'\0',256);
	return 0;
}

uint8_t isLoop(){
	return 1;
}

/* GPIO initialization function */
void MX_GPIO_Init()
{
  /* GPIO Ports Clock Enable */
  __GPIOA_CLK_ENABLE();
  __GPIOB_CLK_ENABLE();
  __GPIOC_CLK_ENABLE();
}
/* ADC1 Initializzation function */
void MX_ADC1_Init(void)
{
	ADC_ChannelConfTypeDef sConfig;

	/**Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
	*/
	hadc1.Instance 						= ADC1;
	hadc1.Init.ClockPrescaler 			= ADC_CLOCKPRESCALER_PCLK_DIV8;
	hadc1.Init.Resolution 				= ADC_RESOLUTION12b;
	hadc1.Init.ScanConvMode 			= DISABLE;
	hadc1.Init.ContinuousConvMode 		= DISABLE;
	hadc1.Init.DiscontinuousConvMode 	= DISABLE;
	hadc1.Init.ExternalTrigConvEdge 	= ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc1.Init.DataAlign 				= ADC_DATAALIGN_RIGHT;
	hadc1.Init.NbrOfConversion 			= 1;
	hadc1.Init.DMAContinuousRequests 	= DISABLE;
	hadc1.Init.EOCSelection 			= EOC_SINGLE_CONV;
	HAL_ADC_Init(&hadc1);

	/**Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
	*/
	sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
	sConfig.Rank = 1;
	sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
	HAL_ADC_ConfigChannel(&hadc1, &sConfig);

}

void finalize(){
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
