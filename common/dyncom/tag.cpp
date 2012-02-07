/**
 * @file tag.cpp
 *
 * Do a depth search of all reachable code and associate
 * every reachable instruction with flags that indicate
 * instruction type (branch,call,ret, ...), flags
 * (conditional, ...) and code flow information (branch
 * target, ...)
 *
 * @author OS Center,TsingHua University (Ported from libcpu)
 * @date 11/11/2010
 */
#include "skyeye_dyncom.h"
#include "skyeye_mm.h"
#include "dyncom/tag.h"
#include "dyncom/defines.h"
#include "dyncom/basicblock.h"
#include "sha1.h"
#include <vector>
using namespace std;
/*
 * TODO: on architectures with constant instruction sizes,
 * this shouldn't waste extra tag data for every byte of
 * code memory, but have one tag per instruction location.
 */

static uint32_t block_entry;

static const char *
get_temp_dir()
{
#ifdef _WIN32
	static char pathname[MAX_PATH];
	if (GetTempPathA(sizeof(pathname), pathname))
		return pathname;
#endif
	return "/tmp/";
}
static bool
is_tag_level2_table_allocated(cpu_t *cpu, addr_t addr)
{
	uint32_t level1_offset = TAG_LEVEL1_OFFSET(addr);
	return cpu->dyncom_engine->tag_table[level1_offset];
}

static bool
is_tag_level3_table_allocated(cpu_t *cpu, addr_t addr)
{
	uint32_t level1_offset = TAG_LEVEL1_OFFSET(addr);
	uint32_t level2_offset = TAG_LEVEL2_OFFSET(addr);
	return cpu->dyncom_engine->tag_table[level1_offset][level2_offset];
}

/**
 * @brief initialize tag level2 table
 *
 * @param cpu CPU core structure
 * @param addr address to tag
 */
static void
init_tag_level2_table(cpu_t *cpu, addr_t addr)
{
	tag_t **tag = (tag_t**)skyeye_mm_zero(TAG_LEVEL2_TABLE_SIZE * sizeof(tag_t *));
	memset(tag, 0, TAG_LEVEL2_TABLE_SIZE * sizeof(tag_t *));

	uint32_t level1_offset = TAG_LEVEL1_OFFSET(addr);
	cpu->dyncom_engine->tag_table[level1_offset] = tag;
}

/**
 * @brief initialize tag level3 table
 *
 * @param cpu CPU core structure
 * @param addr address to tag
 */
static void
init_tag_level3_table(cpu_t *cpu, addr_t addr)
{
	addr_t nitems, i;

	nitems = TAG_LEVEL3_TABLE_SIZE;

	cpu->dyncom_engine->tag = (tag_t*)skyeye_mm_zero(nitems * sizeof(tag_t) * 2);
	for (i = 0; i < nitems; i++) {
		cpu->dyncom_engine->tag[i] = TAG_UNKNOWN;
		cpu->dyncom_engine->tag[i + TAG_LEVEL3_TABLE_SIZE] = 0;
	}

	uint32_t level1_offset = TAG_LEVEL1_OFFSET(addr);
	uint32_t level2_offset = TAG_LEVEL2_OFFSET(addr);
	cpu->dyncom_engine->tag_table[level1_offset][level2_offset] = cpu->dyncom_engine->tag;

		#if 0
	if (!(cpu->flags_codegen & CPU_CODEGEN_TAG_LIMIT)) {
		/* calculate hash of code */
		SHA1_CTX ctx;
		SHA1Init(&ctx);
		//SHA1Update(&ctx, &cpu->RAM[cpu->code_start - 0x50000000], cpu->code_end - cpu->code_start);

		int32_t offset;
               printf("cpu->code_start : %x\n", cpu->code_start);
               if (cpu->code_start > 0x60000000) {
                       offset = cpu->code_start - 0x60000000;
               } else
                       offset = cpu->code_start - 0x50000000;

               SHA1Update(&ctx, &cpu->RAM[offset], cpu->code_end -cpu->code_start);


		SHA1Final(cpu->code_digest, &ctx);
		char ascii_digest[256];
		char cache_fn[256];
		ascii_digest[0] = 0;
		int j; 
		for (j=0; j<20; j++)
			sprintf(ascii_digest+strlen(ascii_digest), "%02x", cpu->code_digest[j]);
		LOG("Code Digest: %s\n", ascii_digest);
		sprintf(cache_fn, "%slibcpu-%s.entries", get_temp_dir(), ascii_digest);
		
		cpu->file_entries = NULL;
		FILE *f;
		if ((f = fopen(cache_fn, "r"))) {
			LOG("info: entry cache found.\n");
			while(!feof(f)) {
				addr_t entry = 0;
				for (i = 0; i < 4; i++) {
					entry |= fgetc(f) << (i*8);
				}
				tag_start(cpu, entry);
			}
			fclose(f);
		} else {
			LOG("info: entry cache NOT found.\n");
		}
		
		if (!(cpu->file_entries = fopen(cache_fn, "a"))) {
			printf("error appending to cache file!\n");
			exit(1);
		}
		#endif
//	}
}

/**
 * @brief check integrity of tag array memory. Allocate memory on demand.
 *
 * @param cpu CPU core structure
 * @param addr address to tag
 */
static void
check_tag_memory_integrity(cpu_t *cpu, addr_t addr)
{
	if (!is_tag_level2_table_allocated(cpu, addr)) {
		init_tag_level2_table(cpu, addr);
	}
	if (!is_tag_level3_table_allocated(cpu, addr)) {
		init_tag_level3_table(cpu, addr);
	}
}

/* check instruction at the address is translated or not. */
bool
is_translated_code(cpu_t *cpu, addr_t addr)
{
	if (!is_tag_level2_table_allocated(cpu, addr)) {
//		init_tag_level2_table(cpu, addr);
		return false;
	}
	if (!is_tag_level3_table_allocated(cpu, addr)) {
//		init_tag_level3_table(cpu, addr);
		return false;
	}
	uint32_t level1_offset = TAG_LEVEL1_OFFSET(addr);
	uint32_t level2_offset = TAG_LEVEL2_OFFSET(addr);
	uint32_t level3_offset = TAG_LEVEL3_OFFSET(addr);
	tag_t tag = cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset];
	if (tag & TAG_TRANSLATED) {
		return true;
	} else
		return false;
}

/* In os simulation ,tag depth is 1, so we can find out the first instruction of jit function
   by checking tag attribute backward in tag table. If tag has TAG_ENTRY, it is start of jit.*/
addr_t find_bb_start(cpu_t *cpu, addr_t addr)
{
	uint32_t level1_offset = TAG_LEVEL1_OFFSET(addr);
	uint32_t level2_offset = TAG_LEVEL2_OFFSET(addr);
	uint32_t level3_offset = TAG_LEVEL3_OFFSET(addr);
	tag_t tag = cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset];
	int i = level3_offset;
	while (!(tag & TAG_ENTRY) && i) {
		i --;
		tag = cpu->dyncom_engine->tag_table[level1_offset][level2_offset][i];
	}
	return (level1_offset << TAG_LEVEL1_TABLE_SHIFT) |
		(level2_offset << TAG_LEVEL2_TABLE_SHIFT) |
		(i);
}
/**
 * @brief Determine an address is in code area or not
 *
 * @param cpu CPU core structure
 * @param a address
 *
 * @return true if in code area,false otherwise.
 */
bool
is_inside_code_area(cpu_t *cpu, addr_t a)
{
	//return (a >= cpu->dyncom_engine->code_start && a < cpu->dyncom_engine->code_end) | (a >= cpu->dyncom_engine->code1_start && a < a < cpu->dyncom_engine->code1_end);
	return (a >= cpu->dyncom_engine->code_start && a < cpu->dyncom_engine->code_end) | (a >= cpu->dyncom_engine->code1_start && a < cpu->dyncom_engine->code1_end);
}
/**
 * @brief Give a tag to an address
 *
 * @param cpu CPU core structure
 * @param a address to be tagged
 * @param t tag
 */
void
or_tag(cpu_t *cpu, addr_t a, tag_t t)
{
	/* NEW_PC_NONE is not a real address. Some branch/call address could not be known at translate-time*/
	if (a == NEW_PC_NONE) {
		return;
	}
	check_tag_memory_integrity(cpu, a);
	uint32_t level1_offset = TAG_LEVEL1_OFFSET(a);
	uint32_t level2_offset = TAG_LEVEL2_OFFSET(a);
	uint32_t level3_offset = TAG_LEVEL3_OFFSET(a);
	cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset] |= t;

	/* If tag is entry, set a counter to null */
	if ((cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset] & TAG_ENTRY))
		cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset + TAG_LEVEL3_TABLE_SIZE] = 0;
	/* Uncomment below if an entry needs to keep its entry point */
	//else
	//	cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset + TAG_LEVEL3_TABLE_SIZE] = block_entry;
}
void
xor_tag(cpu_t *cpu, addr_t a, tag_t t)
{
	/* NEW_PC_NONE is not a real address. Some branch/call address could not be known at translate-time*/
	if (a == NEW_PC_NONE) {
		return;
	}
	check_tag_memory_integrity(cpu, a);
	uint32_t level1_offset = TAG_LEVEL1_OFFSET(a);
	uint32_t level2_offset = TAG_LEVEL2_OFFSET(a);
	uint32_t level3_offset = TAG_LEVEL3_OFFSET(a);
	cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset] &= ~t;
}
void clear_tag(cpu_t *cpu, addr_t a)
{
	addr_t nitems, i;
	/* NEW_PC_NONE is not a real address. Some branch/call address could not be known at translate-time*/
	if (a == NEW_PC_NONE) {
		return;
	}
	check_tag_memory_integrity(cpu, a);
	uint32_t level1_offset = TAG_LEVEL1_OFFSET(a);
	uint32_t level2_offset = TAG_LEVEL2_OFFSET(a);
	uint32_t level3_offset = TAG_LEVEL3_OFFSET(a);
	cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset] = TAG_UNKNOWN;
	cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset + TAG_LEVEL3_TABLE_SIZE] = 0;
}
/**
 * @brief Clear specific tags
 *
 * @param cpu CPU core structure
 * @param a address to be tagged
 * @param mask bits of tags to be cleared (a ~ will be applied)
 */
void selective_clear_tag(cpu_t *cpu, addr_t a, uint32_t mask)
{
	addr_t nitems, i;
	/* NEW_PC_NONE is not a real address. Some branch/call address could not be known at translate-time*/
	if (a == NEW_PC_NONE) {
		return;
	}
	check_tag_memory_integrity(cpu, a);
	uint32_t level1_offset = TAG_LEVEL1_OFFSET(a);
	uint32_t level2_offset = TAG_LEVEL2_OFFSET(a);
	uint32_t level3_offset = TAG_LEVEL3_OFFSET(a);
	cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset] &= ~mask;
	/* Do not erase execution count. */
}
void clear_tag_page(cpu_t *cpu, addr_t a)
{
	addr_t nitems, i;
	/* NEW_PC_NONE is not a real address. Some branch/call address could not be known at translate-time*/
	if (a == NEW_PC_NONE) {
		return;
	}
	check_tag_memory_integrity(cpu, a);
	uint32_t level1_offset = TAG_LEVEL1_OFFSET(a);
	uint32_t level2_offset = TAG_LEVEL2_OFFSET(a);
	for (i = 0; i < TAG_LEVEL3_TABLE_SIZE; i++) {
		/* clear all instructions tag except TAG_ENTRY & TAG_TRANSLATED */
		cpu->dyncom_engine->tag_table[level1_offset][level2_offset][i] &= TAG_TRANSLATED | TAG_ENTRY;
	}
}
void clear_tag_table(cpu_t *cpu)
{
	addr_t i, j, k;
	for (i = 0; i < TAG_LEVEL1_TABLE_SIZE; i++)
		if (cpu->dyncom_engine->tag_table[i])
			for (j = 0; j < TAG_LEVEL2_TABLE_SIZE; j++)
				if (cpu->dyncom_engine->tag_table[i][j])
					for (k = 0; k < TAG_LEVEL3_TABLE_SIZE; k++) {
						cpu->dyncom_engine->tag_table[i][j][k] = TAG_UNKNOWN;
						cpu->dyncom_engine->tag_table[i][j][k + TAG_LEVEL3_TABLE_SIZE] = 0;
					}
}
/* access functions */
/**
 * @brief Get the tag of an address 
 *
 * @param cpu CPU core structure
 * @param a address
 *
 * @return tag of the address
 */
tag_t
get_tag(cpu_t *cpu, addr_t a)
{
	/* NEW_PC_NONE is not a real address. Some branch/call address could not be known at translate-time*/
	if (a == NEW_PC_NONE) {
		return TAG_UNKNOWN;
	}
	check_tag_memory_integrity(cpu, a);
	uint32_t level1_offset = TAG_LEVEL1_OFFSET(a);
	uint32_t level2_offset = TAG_LEVEL2_OFFSET(a);
	uint32_t level3_offset = TAG_LEVEL3_OFFSET(a);
	return cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset];
}
/**
 * @brief Get the tag of an address and add one to its execution 
 *        count
 *
 * @param cpu CPU core structure
 * @param a address 
 * @param counter pointer to counter 
 *
 * @return tag of the address
 */
tag_t
check_tag_execution(cpu_t *cpu, addr_t a, uint32_t *counter, uint32_t *entry)
{
	/* NEW_PC_NONE is not a real address. Some branch/call address could not be known at translate-time*/
	if (a == NEW_PC_NONE) {
		return TAG_UNKNOWN;
	}
	check_tag_memory_integrity(cpu, a);
	uint32_t level1_offset = TAG_LEVEL1_OFFSET(a);
	uint32_t level2_offset = TAG_LEVEL2_OFFSET(a);
	uint32_t level3_offset = TAG_LEVEL3_OFFSET(a);
	uint32_t tag = cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset];
	if (tag & TAG_ENTRY) {
		*counter = cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset + TAG_LEVEL3_TABLE_SIZE]++;
		return tag;
	} else if ((tag & TAG_CODE) && (entry == 0)) { /* condition on entry is to avoid infinite recursion */
		// uncomment if execution counter raises for each basic block instruction executed
		#if 0
		*entry = cpu->dyncom_engine->tag_table[level1_offset][level2_offset][level3_offset + TAG_LEVEL3_TABLE_SIZE];
		check_tag_execution(cpu, *entry, counter, entry);
		#endif
	}
	return tag;
}

/**
 * @brief Determine an address is code or not
 *
 * @param cpu CPU core structure
 * @param a address
 *
 * @return true if is code,false otherwise
 */
bool
is_code(cpu_t *cpu, addr_t a)
{
	return !!(get_tag(cpu, a) & TAG_CODE);
}
/**
 * @brief Determine an address is translated or not
 *
 * @param cpu CPU core structure
 * @param a address
 *
 * @return true if is translated,false otherwise
 */
bool
is_translated(cpu_t *cpu, addr_t a)
{
	return (get_tag(cpu, a) & TAG_TRANSLATED);
}

extern void disasm_instr(cpu_t *cpu, addr_t pc);

static void save_startbb_addr(cpu_t *cpu, addr_t pc){
	if (is_start_of_basicblock(cpu, pc)){
		int cur_pos;
		cur_pos = cpu->dyncom_engine->cur_tagging_pos;
		vector<addr_t>::iterator i = cpu->dyncom_engine->startbb[cur_pos].begin();
		for(; i < cpu->dyncom_engine->startbb[cur_pos].end(); i++){

			if(*i == pc)
				break;
		}
		if(i == cpu->dyncom_engine->startbb[cur_pos].end())
			cpu->dyncom_engine->startbb[cur_pos].push_back(pc);

	}
}
static void
tag_recursive(cpu_t *cpu, addr_t pc, int level)
{
	int bytes;
	tag_t tag;
	addr_t new_pc, next_pc;

	if ((cpu->dyncom_engine->flags_codegen & CPU_CODEGEN_TAG_LIMIT)
	    && (level == LIMIT_TAGGING_DFS))
		return;
	if ((cpu->dyncom_engine->flags_codegen & CPU_CODEGEN_TAG_LIMIT)
	    && (level == 0))
	{
		LOG("tag start at %x\n", pc);
		/* save tag start address */
		cpu->dyncom_engine->tag_start = pc;
	}
	for(;;) {
		if(!cpu->mem_ops.is_inside_page(cpu, pc) && !is_user_mode(cpu))
			return;
		if (!is_inside_code_area(cpu, pc)){
			LOG("In %s pc = %x start = %x end = %x\n",
					__FUNCTION__, pc, cpu->dyncom_engine->code_start, cpu->dyncom_engine->code_end);
			return;
		}
		if (LOGGING) {
			LOG("%*s", level, "");
//			disasm_instr(cpu, pc);
		}

		/* clear tag when instruction re-transalted. */
		tag = get_tag(cpu, pc);

		bytes = cpu->f.tag_instr(cpu, pc, &tag, &new_pc, &next_pc);

		/* temporary fix: in case the previous instr at pc had changed,
		   we remove instr dependant tags. They will be set again anyway */
		selective_clear_tag(cpu, pc, TAG_BRANCH | TAG_CONDITIONAL | TAG_RET | TAG_STOP | TAG_CONTINUE | TAG_TRAP | TAG_MEMORY);
		or_tag(cpu, pc, tag | TAG_CODE);
#ifdef OPT_LOCAL_REGISTERS
#if 0
		if (is_inside_code_area(cpu, next_pc)){
			tag_t tmp_tag;
			addr_t tmp_newpc, tmp_nextpc;
			cpu->f.tag_instr(cpu, next_pc, &tmp_tag, &tmp_newpc, &tmp_nextpc);
			if(tmp_tag & TAG_SYSCALL){
				or_tag(cpu, pc, TAG_BEFORE_SYSCALL);
			}
			if(tag & TAG_SYSCALL){
				or_tag(cpu, next_pc, TAG_AFTER_SYSCALL);
			}
		}
#endif
#endif
		if ((tag & TAG_MEMORY) && !is_user_mode(cpu)) {
			or_tag(cpu, next_pc, TAG_AFTER_COND);
		}
		if (tag & (TAG_CONDITIONAL))
			or_tag(cpu, next_pc, TAG_AFTER_COND);

		if (tag & TAG_TRAP)	{
			/* regular trap - no code after it */
			if (!(cpu->dyncom_engine->flags_hint & (CPU_HINT_TRAP_RETURNS | CPU_HINT_TRAP_RETURNS_TWICE)))
				//return;
				break;
			/*
			 * client hints that a trap will likely return,
			 * so tag code after it (optimization for usermode
			 * code that makes syscalls)
			 */
			or_tag(cpu, next_pc, TAG_AFTER_TRAP);
			/*
			 * client hints that a trap will likely return
			 * - to the next instruction AND
			 * - to the instruction after that
			 * OpenBSD on M88K skips an instruction on a trap
			 * return if there was an error.
			 */
			if (cpu->dyncom_engine->flags_hint & CPU_HINT_TRAP_RETURNS_TWICE) {
				tag_t dummy1;
				addr_t next_pc2, dummy2;
				next_pc2 = next_pc + cpu->f.tag_instr(cpu, next_pc, &dummy1, &dummy2, &dummy2);
				or_tag(cpu, next_pc2, TAG_AFTER_TRAP);
				tag_recursive(cpu, next_pc2, level+1);
			}
		}

		if (tag & TAG_CALL) {
			/* tag subroutine, then continue with next instruction */
			or_tag(cpu, new_pc, TAG_SUBROUTINE);
			or_tag(cpu, next_pc, TAG_AFTER_CALL);
			tag_recursive(cpu, new_pc, level+1);
		}

		if (tag & (TAG_BRANCH)) {
			or_tag(cpu, new_pc, TAG_BRANCH_TARGET);
			tag_recursive(cpu, new_pc, level+1);
			if (!(tag & (TAG_CONDITIONAL)))
				//return;
				break;
		}

		if (is_translated(cpu, next_pc)) {
			or_tag(cpu, pc, tag | TAG_STOP | TAG_LAST_INST);
			//return;
		}
		if(cpu->mem_ops.is_page_start(pc) && !is_user_mode(cpu))
			or_tag(cpu, pc, tag | TAG_START_PAGE);
		if(cpu->mem_ops.is_page_end(pc) && !is_user_mode(cpu)){
			or_tag(cpu, pc, tag | TAG_STOP | TAG_END_PAGE);
			xor_tag(cpu, pc, TAG_CONTINUE);
			break;
		}
		if ((tag & TAG_EXCEPTION) && !is_user_mode(cpu)) {
			or_tag(cpu, next_pc, TAG_AFTER_EXCEPTION);
			xor_tag(cpu, pc, TAG_CONTINUE);
			break;
		}

		if (tag & (TAG_RET | TAG_STOP))	/* execution ends here, the follwing location is not reached */
			//return;
			break;
		save_startbb_addr(cpu, pc);
		pc = next_pc;
		/* save tag end address */
	}
	save_startbb_addr(cpu, pc);
	cpu->dyncom_engine->tag_end = pc;
	LOG("tag end at %x\n", pc);
    LOG("next pc is %x\n", next_pc);
}
/**
 * @brief Start tag from current pc.
 *
 * @param cpu CPU core structure
 * @param pc current address start tagging(physics pc)
 */
void
tag_start(cpu_t *cpu, addr_t pc)
{
	cpu->dyncom_engine->tags_dirty = true;

	/* for singlestep, we don't need this */
	if (cpu->dyncom_engine->flags_debug & (CPU_DEBUG_SINGLESTEP | CPU_DEBUG_SINGLESTEP_BB))
		return;

	/* initialize data structure on demand */
	check_tag_memory_integrity(cpu, pc);

	LOG("starting tagging at $%02llx\n", (unsigned long long)pc);

	if (!(cpu->dyncom_engine->flags_codegen & CPU_CODEGEN_TAG_LIMIT)) {
		int i;
		if (cpu->dyncom_engine->file_entries) {
			for (i = 0; i < 4; i++)
				fputc((pc >> (i*8))&0xFF, cpu->dyncom_engine->file_entries);
			fflush(cpu->dyncom_engine->file_entries);
		}
	}

	block_entry = pc;
	
	or_tag(cpu, pc, TAG_ENTRY); /* client wants to enter the guest code here */

	tag_recursive(cpu, pc, 0);
}
