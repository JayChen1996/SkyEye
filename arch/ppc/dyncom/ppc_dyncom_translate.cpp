#include <stdio.h>
#include <stdlib.h>
#include "llvm/Instructions.h"

#include "skyeye_dyncom.h"
#include "dyncom/dyncom_llvm.h"
#include "dyncom/frontend.h"
#include "ppc_dyncom.h"
#include "dyncom/tag.h"
#include "dyncom/basicblock.h"

using namespace llvm;

Value *
arch_powerpc_translate_cond(cpu_t *cpu, addr_t pc, BasicBlock *bb){
}

int arch_powerpc_tag_instr(cpu_t *cpu, addr_t pc, tag_t *tag, addr_t *new_pc, addr_t *next_pc){
}

int arch_powerpc_translate_instr(cpu_t *cpu, addr_t pc, BasicBlock *bb){
}
