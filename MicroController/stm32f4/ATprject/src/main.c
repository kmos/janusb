//
// This file is part of the GNU ARM Eclipse distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

// ----------------------------------------------------------------------------

#include <stdio.h>
#include <USBD_CDC.h>
#include "diag/Trace.h"
#include "stm32f4_discovery.h"


#include "usbd_core.h"
#include "usbd_cdc_if_template.h"
#include "usbd_desc.h"

// ----------------------------------------------------------------------------
//
// Standalone STM32F4 empty sample (trace via NONE).
//
// Trace support is enabled by adding the TRACE macro definition.
// By default the trace messages are forwarded to the NONE output,
// but can be rerouted to any device or completely suppressed, by
// changing the definitions required in system/src/diag/trace_impl.c
// (currently OS_USE_TRACE_ITM, OS_USE_TRACE_SEMIHOSTING_DEBUG/_STDOUT).
//

// ----- main() ---------------------------------------------------------------

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

void setup();
void loop();
uint8_t isLoop();
void finalize();
void button_pressed();

int interr=0;

USBD_HandleTypeDef USBD_Device;

Button_TypeDef bt;
ButtonMode_TypeDef btm;
uint8_t * receiving_buffer;

int main(){
	setup();
	while(isLoop()){
		loop();
	}
	finalize();
}

void setup(){
	HAL_Init();

	USBD_Init(&USBD_Device, &VCP_Desc, 0);

	USBD_RegisterClass(&USBD_Device, &USBD_CDC);
	USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_Template_fops);
	USBD_Start(&USBD_Device);
	HAL_Delay(4000);

	BSP_LED_Init(0);
	BSP_LED_Init(1);
	BSP_LED_Init(2);
	BSP_LED_Init(3);

	receiving_buffer = malloc((int)1);

//	bt = BUTTON_KEY;
//	btm = BUTTON_MODE_EXTI;
//	BSP_PB_Init(bt, btm);
}

void loop(){
 	if(VCP_read(receiving_buffer, 1)!=0){
		VCP_write("t", 1);
		if((*receiving_buffer)=='k'){
			BSP_LED_Toggle(LED3);
		}else{
			BSP_LED_Toggle(LED4);
		}
	}

}

uint8_t isLoop(){
	return 1;
}

void button_pressed(){
	interr++;
	BSP_LED_On(interr%4);
	BSP_LED_Off(interr%4-1);
	VCP_write("scrivo",6);


}

void finalize(){
	//Deinizializza
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
