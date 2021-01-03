/*
 * conio.h
 *
 *  Created on: Dec 28, 2020
 *      Author: david.winant
 */

#ifndef INC_CONIO_H_
#define INC_CONIO_H_

int getch (void);
int putch (int c);

int await_key (void);

int getline (char* line, int maxlen);

#endif /* INC_CONIO_H_ */
