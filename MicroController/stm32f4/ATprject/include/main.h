
#include <stdio.h>
#include <USBD_CDC.h>
#include "diag/Trace.h"
#include "stm32f4_discovery.h"
#include "jsmn.h"
#include "usbd_core.h"
#include "usbd_cdc_if_template.h"
#include "usbd_desc.h"

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

//Function prototypes --------------------*/
void setup();
void loop();
uint8_t isLoop();
void finalize();

//Logical Variable -----------------------*/
uint8_t receiving_buffer;
uint8_t jstring[256];
char response[1024];

/* Parsing Variable -----------------------*/
jsmn_parser parser;
jsmntok_t tokens[256];

/* USB Handler ----------------------------*/
USBD_HandleTypeDef USBD_Device;


/* Constant define ------------------------*/

#define TESTING

