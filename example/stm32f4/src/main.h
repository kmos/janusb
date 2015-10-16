/*
 * main.h
 *
 *  Created on: 07 ott 2015
 *      Author: ntonjeta
 */

#ifndef MAIN_H_
#define MAIN_H_


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stm32f407xx.h"
#include "stm32f4xx_hal.h"
#include <USBD_CDC.h>
#include "diag/Trace.h"
#include "stm32f4_discovery.h"
#include "stm32f4_discovery_accelerometer.h"
#include "usbd_core.h"
#include "usbd_cdc_if_template.h"
#include "usbd_desc.h"
#include "jsmn.h"

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

/* Commenad Typedef -----------------------*/
typedef struct Comand{
	int name;
	int ID;
}Comand;

//Function prototypes --------------------*/
void setup();
void loop();
uint8_t isLoop();
void finalize();
int  parsing (Comand * received);
int execComand(Comand received);
static void MX_ADC1_Init(void);


//Logical Variable ------------------------*/
uint8_t receiving_buffer;
uint8_t jstring[256];
char response[256];
char request [256];

/* Session ID -----------------------------*/
uint64_t session;

/* USB Handler ----------------------------*/
USBD_HandleTypeDef USBD_Device;
/*ADC handler -----------------------------*/
ADC_HandleTypeDef hadc1;



/* Constant define ------------------------*/
#define TESTING
// Command
#define C_ON   0
#define C_OFF  1
#define C_READ 2
// Risposte
#define OK  0
#define ERR 1

///* Use parsing function -------------------*/
//static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
//	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
//			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
//		return 0;
//	}
//	return -1;
//}



#endif /* MAIN_H_ */
