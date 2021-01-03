/*
 * waves.h
 *
 *  Created on: Dec 26, 2020
 *      Author: david.winant
 */

#ifndef INC_WAVES_H_
#define INC_WAVES_H_

#include "main.h"
#include <stdint.h>

extern TIM_HandleTypeDef htim6;
extern DAC_HandleTypeDef hdac1;

void cosine_test (void);
void waveforms (void);

#ifndef NELEMENTS
#define NELEMENTS(t)		(sizeof(t) / sizeof(*(t)))
#endif

#endif /* INC_WAVES_H_ */
