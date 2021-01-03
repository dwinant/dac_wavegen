/*
 * output.c
 *
 *  Created on: Dec 13, 2020
 *      Author: davew
 */
#include "main.h"
#include "output.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

extern UART_HandleTypeDef huart3;


void output_string (const char* s)
{
	int l = strlen(s);
	HAL_UART_Transmit (&huart3, (uint8_t*) s, l, 1000);
}


void output (const char* format, ...)
{
	static char outline[200];
	va_list		va;

	va_start (va, format);
	vsnprintf (outline, sizeof(outline), format, va);
	output_string (outline);
	va_end (va);
}
