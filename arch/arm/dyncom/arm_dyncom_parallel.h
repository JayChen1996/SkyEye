/* 
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
 * 03/27/2011   Michael.Kang  <blackfin.kang@gmail.com>
 */

#ifndef __ARM_DYNCOM_PARALLEL_H__
#define __ARM_DYNCOM_PARALLEL_H__
#include "arm_dyncom_run.h"
void init_compiled_queue(cpu_t* cpu);
int launch_compiled_queue(cpu_t* cpu, uint32_t pc);
int launch_compiled_queue_dyncom(cpu_t* cpu, uint32_t pc);

typedef enum{
	PURE_INTERPRET = 0,
	PURE_DYNCOM,
	HYBRID,
	FAST_INTERPRET,
	MAX_RUNNING_MODE,
}running_mode_t;

extern running_mode_t running_mode;
#ifdef __cplusplus
 extern "C" {
#endif

int
do_mode_option (skyeye_option_t * this_option, int num_params,
	       const char *params[]);
void clear_translated_cache(addr_t phys_addr);
#ifdef __cplusplus
}
#endif

#endif

