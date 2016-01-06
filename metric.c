/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/*
 * \file
 *			Metric: Contain the functions that allow the duty cycle and metric computations and support processes.
 * \author
 * 			Eloi Garrido Barrabés
 */
#include "contiki.h"
#include "metric.h"
#include "../apps/energytrace/energytrace.h"
#include <stdio.h>
/*---------------------------------------------------------------------------*/
/* DEFINES */
/*---------------------------------------------------------------------------*/
/* VARIABLES AND CONSTANTS */
uint8_t node_duty_cycle;
node_energy_state_t node_energy_state;
uint16_t harvesting_rate = 0;
inline node_energy_state_t energy_state(void){
	if (remaining_energy > NS_ENERGY_HIGH) {
		return NS_HIGH;
	} else if (remaining_energy <= NS_ENERGY_HIGH && remaining_energy > NS_ENERGY_MID){
		return NS_MID;
	} else if (remaining_energy <= NS_ENERGY_MID && remaining_energy > NS_ENERGY_LOW){
		return NS_LOW;
	}
	return NS_ZERO;
}

/*---------------------------------------------------------------------------*/
// PROCESS(metric_process, "Metric main process");
// PROCESS_THREAD(&metric_process, ev, data){

// 	PROCESS_BEGIN();

// 	PROCESS_END();
// }
/*---------------------------------------------------------------------------*/
/* FUNCTIONS */
void compute_node_duty_cycle(void){
	switch(node_energy_state){
		case NS_HIGH:
			node_duty_cycle = DC_HIGH;
			break;
		case NS_MID:
			node_duty_cycle = DC_MID;
			break;
		case NS_LOW:
			node_duty_cycle = DC_LOW;
			break;
		default:
			node_duty_cycle = 0;
			break;
	}

}

uint8_t get_duty_cycle(void){
	return node_duty_cycle;
}

void compute_node_state(void){
	node_energy_state = energy_state();
}

node_energy_state_t get_node_state(void){
	return node_energy_state;
}

void compute_harvesting_rate(void){
	int indx;
	uint32_t acum = 0;
	for (indx = 0; indx < 5; indx++){
		acum += harvesting_rate_array[indx];
	}
	harvesting_rate = acum / 5;
}

uint16_t get_harvesting_rate(void){
	return harvesting_rate;
}