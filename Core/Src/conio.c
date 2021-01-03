/*
 * conio.c
 *
 *  Created on: Dec 28, 2020
 *      Author: david.winant
 */
#include "main.h"
#include "conio.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

extern UART_HandleTypeDef huart3;


int getch (void)
{
	uint8_t		ch = 0;

	int status = HAL_UART_Receive (&huart3, &ch, 1, 0);

	if (status == HAL_TIMEOUT)
		return -1;
	return ch;
}


int putch (int c)
{
	uint8_t		ch = c;

	int status = HAL_UART_Transmit (&huart3, &ch, 1, 1000);

	if (status == HAL_TIMEOUT)
		return -1;
	return ch;
}

int await_key (void) {
	int c;

	while ((c = getch()) < 0) {
		HAL_Delay(10);
	}
	return c;
}


int getline (char* line, int maxlen)
{
	int 	pos = 0;

	if (maxlen < 2) return -1;

	while (1) {
		int c = await_key();
		if (c == 13) break;

		if (c == 8 || c == 127) {
			if (pos > 0) {
				pos -= 1;
				line[pos] = 0;
				putch (8); putch (' '); putch(8);
			}
		} else {
			//if (c < 'A' || c > 'z') output ("c of %d\r\n", c);
			line[pos++] = c;
			putch (c);
			if (pos >= maxlen-1) break;
		}
	}
	line[pos] = 0;
	return pos;
}
