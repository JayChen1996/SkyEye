#ifndef __ARM_DYNCOM_RUN__
#define __ARM_DYNCOM_RUN__

#include "skyeye_dyncom.h"
extern void arm_dyncom_run(cpu_t* cpu);
extern void arm_dyncom_init(arm_core_t* core);
extern void switch_mode(arm_core_t *core, uint32_t mode);
extern void arch_arm_undef(cpu_t *cpu, BasicBlock *bb, uint32_t instr);
extern uint32_t is_int_in_interpret(cpu_t *cpu);

/* FIXME, we temporarily think thumb instruction is always 16 bit */
static inline uint32 GET_INST_SIZE(arm_core_t* core){
	return core->TFlag? 2 : 4;
}

#endif

