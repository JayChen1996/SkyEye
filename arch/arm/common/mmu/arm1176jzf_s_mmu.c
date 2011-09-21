/*
    arm1176jzf_s_mmu.c - ARM920T Memory Management Unit emulation.
    Copyright (C) 2003 Skyeye Develop Group
    for help please send mail to <skyeye-developer@lists.gro.clinux.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <assert.h>
#include <string.h>

#include "armdefs.h"
#include "bank_defs.h"

#if 0
fault_t
mmu_translate (ARMul_State *state, ARMword virt_addr, ARMword *phys_addr);
#endif
fault_t
mmu_translate (ARMul_State *state, ARMword virt_addr, ARMword *phys_addr, int *ap, int *sop);
/* This function encodes table 8-2 Interpreting AP bits,
   returning non-zero if access is allowed. */
static int
check_perms (ARMul_State *state, int ap, int read)
{
	int s, r, user;

	s = state->mmu.control & CONTROL_SYSTEM;
	r = state->mmu.control & CONTROL_ROM;
	/* chy 2006-02-15 , should consider system mode, don't conside 26bit mode */
//    printf("ap is %x, user is %x, s is %x, read is %x\n", ap, user, s, read);
//    printf("mode is %x\n", state->Mode);
	user = (state->Mode == USER32MODE) || (state->Mode == USER26MODE) || (state->Mode == SYSTEM32MODE);

	switch (ap) {
	case 0:
		return read && ((s && !user) || r);
	case 1:
		return !user;
	case 2:
		return read || !user;
	case 3:
		return 1;
	}
	return 0;
}

#if 0
fault_t
check_access (ARMul_State *state, ARMword virt_addr, tlb_entry_t *tlb,
	      int read)
{
	int access;

	state->mmu.last_domain = tlb->domain;
	access = (state->mmu.domain_access_control >> (tlb->domain * 2)) & 3;
	if ((access == 0) || (access == 2)) {
		/* It's unclear from the documentation whether this
		   should always raise a section domain fault, or if
		   it should be a page domain fault in the case of an
		   L1 that describes a page table.  In the ARM710T
		   datasheets, "Figure 8-9: Sequence for checking faults"
		   seems to indicate the former, while "Table 8-4: Priority
		   encoding of fault status" gives a value for FS[3210] in
		   the event of a domain fault for a page.  Hmm. */
		return SECTION_DOMAIN_FAULT;
	}
	if (access == 1) {
		/* client access - check perms */
		int subpage, ap;
#if 0
		switch (tlb->mapping) {
			/*ks 2004-05-09
			 *   only for XScale
			 *   Extend Small Page(ESP) Format
			 *   31-12 bits    the base addr of ESP
			 *   11-10 bits    SBZ
			 *   9-6   bits    TEX
			 *   5-4   bits    AP
			 *   3     bit     C
			 *   2     bit     B
			 *   1-0   bits    11
			 * */
		case TLB_ESMALLPAGE:	/* xj */
			subpage = 0;
			/* printf("TLB_ESMALLPAGE virt_addr=0x%x  \n",virt_addr ); */
			break;

		case TLB_TINYPAGE:
			subpage = 0;
			/* printf("TLB_TINYPAGE virt_addr=0x%x  \n",virt_addr ); */
			break;

		case TLB_SMALLPAGE:
			subpage = (virt_addr >> 10) & 3;
			break;
		case TLB_LARGEPAGE:
			subpage = (virt_addr >> 14) & 3;
			break;
		case TLB_SECTION:
			subpage = 3;
			break;
		default:
			assert (0);
			subpage = 0;	/* cleans a warning */
		}
		ap = (tlb->perms >> (subpage * 2 + 4)) & 3;
		if (!check_perms (state, ap, read)) {
			if (tlb->mapping == TLB_SECTION) {
				return SECTION_PERMISSION_FAULT;
			} else {
				return SUBPAGE_PERMISSION_FAULT;
			}
		}
#endif
	} else {			/* access == 3 */
		/* manager access - don't check perms */
	}
	return NO_FAULT;
}
#endif

#if 0
fault_t
mmu_translate (ARMul_State *state, ARMword virt_addr, ARMword *phys_addr)
#endif

/*  ap: AP bits value.
 *  sop: section or page description  0:section 1:page
 */
fault_t
mmu_translate (ARMul_State *state, ARMword virt_addr, ARMword *phys_addr, int *ap, int *sop)
{
	{
		/* walk the translation tables */
		ARMword l1addr, l1desc;
		if (state->mmu.translation_table_ctrl && virt_addr << state->mmu.translation_table_ctrl >> (32 - state->mmu.translation_table_ctrl - 1)) {
			l1addr = state->mmu.translation_table_base1;
			l1addr = (((l1addr >> 14) << 14) | (virt_addr >> 18)) & ~3;
		} else {
			l1addr = state->mmu.translation_table_base0;
			l1addr = (((l1addr >> (14 - state->mmu.translation_table_ctrl)) << (14 - state->mmu.translation_table_ctrl)) | (virt_addr << state->mmu.translation_table_ctrl) >> (18 + state->mmu.translation_table_ctrl)) & ~3;
		}

		/* l1desc = mem_read_word (state, l1addr); */

		bus_read(32, l1addr, &l1desc);
        #if 0
        if (virt_addr == 0xc000d2bc) {
                printf("mmu_control is %x\n", state->mmu.translation_table_ctrl);
                printf("mmu_table_0 is %x\n", state->mmu.translation_table_base0);
                printf("mmu_table_1 is %x\n", state->mmu.translation_table_base1);
                printf("l1addr is %x l1desc is %x\n", l1addr, l1desc);
 //               exit(-1);
        }
        #endif
		switch (l1desc & 3) {
		case 0:
		case 3:
			/*
			 * according to Figure 3-9 Sequence for checking faults in arm manual,
			 * section translation fault should be returned here.
			 */
			{
				return SECTION_TRANSLATION_FAULT;
			}
		case 1:
			/* coarse page table */
			{
				ARMword l2addr, l2desc;


				l2addr = l1desc & 0xFFFFFC00;
				l2addr = (l2addr |
					  ((virt_addr & 0x000FF000) >> 10)) &
					~3;

				bus_read(32, l2addr, &l2desc);
				/* chy 2003-09-02 for xscale */
				*ap = (l2desc >> 4) & 0x3;
				*sop = 1;	/* page */

				switch (l2desc & 3) {
				case 0:
					return PAGE_TRANSLATION_FAULT;
					break;
				case 1:
					*phys_addr = (l2desc & 0xFFFF0000) | (virt_addr & 0x0000FFFF);
					break;
				case 2:
				case 3:
					*phys_addr = (l2desc & 0xFFFFF000) | (virt_addr & 0x00000FFF);
					break;

				}
			}
			break;
		case 2:
			/* section */

			*ap = (l1desc >> 10) & 3;
			*sop = 0; 	/* section */
            #if 0
            if (virt_addr == 0xc000d2bc) {
                    printf("mmu_control is %x\n", state->mmu.translation_table_ctrl);
                    printf("mmu_table_0 is %x\n", state->mmu.translation_table_base0);
                    printf("mmu_table_1 is %x\n", state->mmu.translation_table_base1);
                    printf("l1addr is %x l1desc is %x\n", l1addr, l1desc);
//                    printf("l2addr is %x l2desc is %x\n", l2addr, l2desc);
                    printf("ap is %x, sop is %x\n", *ap, *sop);
                    printf("mode is %d\n", state->Mode);
//                      exit(-1);
            }
            #endif

            if (l1desc & 0x30000)
				*phys_addr = (l1desc & 0xFF000000) | (virt_addr & 0x00FFFFFF);
			else
				*phys_addr = (l1desc & 0xFFF00000) | (virt_addr & 0x000FFFFF);
			break;
		}
	}
	return NO_FAULT;
}


static fault_t arm1176jzf_s_mmu_write (ARMul_State *state, ARMword va,
				  ARMword data, ARMword datatype);
static fault_t arm1176jzf_s_mmu_read (ARMul_State *state, ARMword va,
				 ARMword *data, ARMword datatype);

int
arm1176jzf_s_mmu_init (ARMul_State *state)
{
	state->mmu.control = 0x70;
	state->mmu.translation_table_base = 0xDEADC0DE;
	state->mmu.domain_access_control = 0xDEADC0DE;
	state->mmu.fault_status = 0;
	state->mmu.fault_address = 0;
	state->mmu.process_id = 0;

}

void
arm1176jzf_s_mmu_exit (ARMul_State *state)
{
}


static fault_t
arm1176jzf_s_mmu_load_instr (ARMul_State *state, ARMword va, ARMword *instr)
{
	fault_t fault;
	int c;			/* cache bit */
	ARMword pa;		/* physical addr */
	ARMword perm;		/* physical addr access permissions */
	int ap, sop;

	static int debug_count = 0;	/* used for debug */

	d_msg ("va = %x\n", va);

	va = mmu_pid_va_map (va);
	if (MMU_Enabled) {
//            printf("MMU enabled.\n");
//            sleep(1);
            /* align check */
		if ((va & (WORD_SIZE - 1)) && MMU_Aligned) {
			d_msg ("align\n");
			return ALIGNMENT_FAULT;
		} else
			va &= ~(WORD_SIZE - 1);

		/* translate tlb */
		fault = mmu_translate (state, va, &pa, &ap, &sop);
		if (fault) {
			d_msg ("translate\n");
			return fault;
		}


		/* no tlb, only check permission */
		if (!check_perms(state, ap, 1)) {
			if (sop == 0) {
				return SECTION_PERMISSION_FAULT;
			} else {
				return SUBPAGE_PERMISSION_FAULT;
			}
		}

#if 0
		/*check access */
		fault = check_access (state, va, tlb, 1);
		if (fault) {
			d_msg ("check_fault\n");
			return fault;
		}
#endif
	}

	/*if MMU disabled or C flag is set alloc cache */
	if (MMU_Disabled) {
//            printf("MMU disabled.\n");
//            sleep(1);
		pa = va;
	}

	bus_read(32, pa, instr);

	return 0;
}

static fault_t
arm1176jzf_s_mmu_read_byte (ARMul_State *state, ARMword virt_addr, ARMword *data)
{
	/* ARMword temp,offset; */
	fault_t fault;
	fault = arm1176jzf_s_mmu_read (state, virt_addr, data, ARM_BYTE_TYPE);
	return fault;
}

static fault_t
arm1176jzf_s_mmu_read_halfword (ARMul_State *state, ARMword virt_addr,
			   ARMword *data)
{
	/* ARMword temp,offset; */
	fault_t fault;
	fault = arm1176jzf_s_mmu_read (state, virt_addr, data, ARM_HALFWORD_TYPE);
	return fault;
}

static fault_t
arm1176jzf_s_mmu_read_word (ARMul_State *state, ARMword virt_addr, ARMword *data)
{
	return arm1176jzf_s_mmu_read (state, virt_addr, data, ARM_WORD_TYPE);
}

static fault_t
arm1176jzf_s_mmu_read (ARMul_State *state, ARMword va, ARMword *data,
		  ARMword datatype)
{
	fault_t fault;
	ARMword pa, real_va, temp, offset;
	ARMword perm;		/* physical addr access permissions */
	int ap, sop;

	d_msg ("va = %x\n", va);

	va = mmu_pid_va_map (va);
	real_va = va;
	/* if MMU disabled, memory_read */
	if (MMU_Disabled) {
//            printf("MMU disabled cpu_id:%x addr:%x.\n", state->mmu.process_id, va);
//            sleep(1);

		/* *data = mem_read_word(state, va); */
		if (datatype == ARM_BYTE_TYPE)
			/* *data = mem_read_byte (state, va); */
			bus_read(8, va, data);
		else if (datatype == ARM_HALFWORD_TYPE)
			/* *data = mem_read_halfword (state, va); */
			bus_read(16, va, data);
		else if (datatype == ARM_WORD_TYPE)
			/* *data = mem_read_word (state, va); */
			bus_read(32, va, data);
		else {
			printf ("SKYEYE:1 arm1176jzf_s_mmu_read error: unknown data type %d\n", datatype);
			skyeye_exit (-1);
		}

		return 0;
	}
//    printf("MMU enabled.\n");
//    sleep(1);

	/* align check */
	if (((va & 3) && (datatype == ARM_WORD_TYPE) && MMU_Aligned) ||
	    ((va & 1) && (datatype == ARM_HALFWORD_TYPE) && MMU_Aligned)) {
		d_msg ("align\n");
		return ALIGNMENT_FAULT;
	}

	/* va &= ~(WORD_SIZE - 1); */

	/*translate va to tlb */
#if 0
	fault = mmu_translate (state, va, ARM920T_D_TLB (), &tlb);
#endif
	fault = mmu_translate (state, va, &pa, &ap, &sop);
	if (fault) {
		d_msg ("translate\n");
		printf("mmu read fault at %x\n", va);
		printf("fault is %d\n", fault);
		return fault;
	}
//    printf("va is %x pa is %x\n", va, pa);

	/* no tlb, only check permission */
	if (!check_perms(state, ap, 1)) {
		if (sop == 0) {
			return SECTION_PERMISSION_FAULT;
		} else {
			return SUBPAGE_PERMISSION_FAULT;
		}
	}
#if 0
	/*check access permission */
	fault = check_access (state, va, tlb, 1);
	if (fault)
		return fault;
#endif
		/* *data = mem_read_word(state, pa); */
	if (datatype == ARM_BYTE_TYPE) {
		/* *data = mem_read_byte (state, pa | (real_va & 3)); */
		bus_read(8, pa | (real_va & 3), data);
		/* bus_read(32, pa | (real_va & 3), data); */
	} else if (datatype == ARM_HALFWORD_TYPE) {
		/* *data = mem_read_halfword (state, pa | (real_va & 2)); */
		bus_read(16, pa | (real_va & 3), data);
		/* bus_read(32, pa | (real_va & 2), data); */
	} else if (datatype == ARM_WORD_TYPE)
		/* *data = mem_read_word (state, pa); */
		bus_read(32, pa, data);
	else {
		printf ("SKYEYE:2 arm1176jzf_s_mmu_read error: unknown data type %d\n", datatype);
		skyeye_exit (-1);
	}
#if 0
    if (state->pc == 0xc011a868) {
            printf("pa is %x value is %x size is %x\n", pa, data, datatype);
            printf("icounter is %lld\n", state->NumInstrs);
//            exit(-1);
    }
#endif

	return 0;
}


static fault_t
arm1176jzf_s_mmu_write_byte (ARMul_State *state, ARMword virt_addr, ARMword data)
{
	return arm1176jzf_s_mmu_write (state, virt_addr, data, ARM_BYTE_TYPE);
}

static fault_t
arm1176jzf_s_mmu_write_halfword (ARMul_State *state, ARMword virt_addr,
			    ARMword data)
{
	return arm1176jzf_s_mmu_write (state, virt_addr, data, ARM_HALFWORD_TYPE);
}

static fault_t
arm1176jzf_s_mmu_write_word (ARMul_State *state, ARMword virt_addr, ARMword data)
{
	return arm1176jzf_s_mmu_write (state, virt_addr, data, ARM_WORD_TYPE);
}



static fault_t
arm1176jzf_s_mmu_write (ARMul_State *state, ARMword va, ARMword data,
		   ARMword datatype)
{
	int b;
	ARMword pa, real_va;
	ARMword perm;		/* physical addr access permissions */
	fault_t fault;
	int ap, sop;

#if 0
	/8 for sky_printk debugger.*/
	if (va == 0xffffffff) {
		putchar((char)data);
		return 0;
	}
	if (va == 0xBfffffff) {
		putchar((char)data);
		return 0;
	}
#endif

	d_msg ("va = %x, val = %x\n", va, data);
	va = mmu_pid_va_map (va);
	real_va = va;

	if (MMU_Disabled) {
		/* mem_write_word(state, va, data); */
		if (datatype == ARM_BYTE_TYPE)
			/* mem_write_byte (state, va, data); */
			bus_write(8, va, data);
		else if (datatype == ARM_HALFWORD_TYPE)
			/* mem_write_halfword (state, va, data); */
			bus_write(16, va, data);
		else if (datatype == ARM_WORD_TYPE)
			/* mem_write_word (state, va, data); */
			bus_write(32, va, data);
		else {
			printf ("SKYEYE:1 arm1176jzf_s_mmu_write error: unknown data type %d\n", datatype);
			skyeye_exit (-1);
		}

		return 0;
	}
	/*align check */
	/* if ((va & (WORD_SIZE - 1)) && MMU_Aligned){ */
	if (((va & 3) && (datatype == ARM_WORD_TYPE) && MMU_Aligned) ||
	    ((va & 1) && (datatype == ARM_HALFWORD_TYPE) && MMU_Aligned)) {
		d_msg ("align\n");
		return ALIGNMENT_FAULT;
	}
	va &= ~(WORD_SIZE - 1);
	/*tlb translate */
	fault = mmu_translate (state, va, &pa, &ap, &sop);
	if (fault) {
		d_msg ("translate\n");
		printf("mmu write fault at %x\n", va);
		return fault;
	}
//    printf("va is %x pa is %x\n", va, pa);

	/* no tlb, only check permission */
	if (!check_perms(state, ap, 0)) {
		if (sop == 0) {
			return SECTION_PERMISSION_FAULT;
		} else {
			return SUBPAGE_PERMISSION_FAULT;
		}
	}

#if 0
	/* tlb check access */
	fault = check_access (state, va, tlb, 0);
	if (fault) {
		d_msg ("check_access\n");
		return fault;
	}
#endif
#if 0
    if (pa <= 0x502860ff && (pa + 1 << datatype) > 0x502860ff) {
            printf("pa is %x value is %x size is %x\n", pa, data, datatype);
    }
#endif
#if 0
    if (state->pc == 0xc011a878) {
            printf("write pa is %x value is %x size is %x\n", pa, data, datatype);
            printf("icounter is %lld\n", state->NumInstrs);
            exit(-1);
    }
#endif
	if (datatype == ARM_BYTE_TYPE) {
		/* mem_write_byte (state,
				(pa | (real_va & 3)),
				data);
		*/
		bus_write(8, (pa | (real_va & 3)), data);

	} else if (datatype == ARM_HALFWORD_TYPE)
		/* mem_write_halfword (state,
				    (pa |
				     (real_va & 2)),
				    data);
		*/
		bus_write(16, (pa | (real_va & 3)), data);
	else if (datatype == ARM_WORD_TYPE)
		/* mem_write_word (state, pa, data); */
		bus_write(32, pa, data);
#if 0
    if (state->NumInstrs > 236403) {
            printf("write memory\n");
                printf("pa is %x value is %x size is %x\n", pa, data, datatype);
                printf("icounter is %lld\n", state->NumInstrs);
    }
#endif
		return 0;
}

ARMword
arm1176jzf_s_mmu_mrc (ARMul_State *state, ARMword instr, ARMword *value)
{
	mmu_regnum_t creg = BITS (16, 19) & 0xf;
	int OPC_2 = BITS (5, 7) & 0x7;
	ARMword data;

	switch (creg) {
	case MMU_ID:
		if (OPC_2 == 0) {
			data = state->cpu->cpu_val;
		} else if (OPC_2 == 1) {
			/* Cache type:
			 * 000 0110 1 000 101 110 0 10 000 101 110 0 10
			 * */
			data = 0x0D172172;
		}
		break;
	case MMU_CONTROL:
		/*
		 * 6:3          should be 1.
		 * 11:10        should be 0
		 * */
		data = (state->mmu.control | 0x78) & 0xFFFFF3FF;;
		break;
	case MMU_TRANSLATION_TABLE_BASE:
#if 0
		data = state->mmu.translation_table_base;
#endif
		switch (OPC_2) {
		case 0:
			data = state->mmu.translation_table_base0;
			break;
		case 1:
			data = state->mmu.translation_table_base1;
			break;
		case 2:
			data = state->mmu.translation_table_ctrl;
			break;
		default:
			printf ("mmu_mrc read UNKNOWN - p15 c2 opcode2 %d\n", OPC_2);
			break;
		break;
		}
	case MMU_DOMAIN_ACCESS_CONTROL:
		data = state->mmu.domain_access_control;
		break;
	case MMU_FAULT_STATUS:
		/* OPC_2 = 0: data FSR value
		 * */
		if (OPC_2 == 0)
			data = state->mmu.fault_status;
		if (OPC_2 == 1)
			data = state->mmu.fault_statusi;
		break;
	case MMU_FAULT_ADDRESS:
		data = state->mmu.fault_address;
		break;
	case MMU_PID:
		data = state->mmu.process_id;
	default:
		printf ("mmu_mrc read UNKNOWN - reg %d\n", creg);
		data = 0;
		break;
	}
/*      printf("\t\t\t\t\tpc = 0x%08x\n", state->Reg[15]); */
	*value = data;
	return data;
}

static ARMword
arm1176jzf_s_mmu_mcr (ARMul_State *state, ARMword instr, ARMword value)
{
	mmu_regnum_t creg = BITS (16, 19) & 0xf;
	int OPC_2 = BITS (5, 7) & 0x7;
	if (!strncmp (state->cpu->cpu_arch_name, "armv6", 5)) {
		switch (creg) {
		case MMU_CONTROL:
/*              printf("mmu_mcr wrote CONTROL      "); */
			state->mmu.control = (value | 0x78) & 0xFFFFF3FF;
			break;
		case MMU_TRANSLATION_TABLE_BASE:
			switch (OPC_2) {
				/* int i; */
				case 0:
#if 0
				/* TTBR0 */
					if (state->mmu.translation_table_ctrl & 0x7) {
						for (i = 0; i <= state->mmu.translation_table_ctrl; i++)
							state->mmu.translation_table_base0 &= ~(1 << (5 + i));
					}
#endif
					state->mmu.translation_table_base0 = (value);
					break;
				case 1:
#if 0
				/* TTBR1 */
					if (state->mmu.translation_table_ctrl & 0x7) {
						for (i = 0; i <= state->mmu.translation_table_ctrl; i++)
							state->mmu.translation_table_base1 &= 1 << (5 + i);
					}
#endif
					state->mmu.translation_table_base1 = (value);
					break;
				case 2:
				/* TTBC */
					state->mmu.translation_table_ctrl = value & 0x7;
					break;
				default:
					printf ("mmu_mcr wrote UNKNOWN - cp15 c2 opcode2 %d\n", OPC_2);
					break;
			}
			break;
		case MMU_DOMAIN_ACCESS_CONTROL:
		/* printf("mmu_mcr wrote DACR         "); */
			state->mmu.domain_access_control = value;
			break;

		case MMU_FAULT_STATUS:
			if (OPC_2 == 0)
				state->mmu.fault_status = value & 0xFF;
			if (OPC_2 == 1) {
				printf("set fault status instr\n");
			}
			break;
		case MMU_FAULT_ADDRESS:
			state->mmu.fault_address = value;
			break;

		case MMU_CACHE_OPS:
			break;
		case MMU_TLB_OPS:
			break;
		case MMU_CACHE_LOCKDOWN:
			/*
			 * FIXME: cache lock down*/
			break;
		case MMU_TLB_LOCKDOWN:
			/* FIXME:tlb lock down */
			break;
		case MMU_PID:
			/*0:24 should be zero. */
			state->mmu.process_id = value & 0xfe000000;
			break;

		default:
			printf ("mmu_mcr wrote UNKNOWN - reg %d\n", creg);
			break;
		}
	}
}

/* teawater add for arm2x86 2005.06.19------------------------------------------- */
static int
arm1176jzf_s_mmu_v2p_dbct (ARMul_State *state, ARMword virt_addr,
		      ARMword *phys_addr)
{
	fault_t fault;
	int ap, sop;

	ARMword perm;		/* physical addr access permissions */
	virt_addr = mmu_pid_va_map (virt_addr);
	if (MMU_Enabled) {

		/*align check */
		if ((virt_addr & (WORD_SIZE - 1)) && MMU_Aligned) {
			d_msg ("align\n");
			return ALIGNMENT_FAULT;
		} else
			virt_addr &= ~(WORD_SIZE - 1);

		/*translate tlb */
		fault = mmu_translate (state, virt_addr, phys_addr, &ap, &sop);
		if (fault) {
			d_msg ("translate\n");
			return fault;
		}

		/* permission check */
		if (!check_perms(state, ap, 1)) {
			if (sop == 0) {
				return SECTION_PERMISSION_FAULT;
			} else {
				return SUBPAGE_PERMISSION_FAULT;
			}
		}
#if 0
		/*check access */
		fault = check_access (state, virt_addr, tlb, 1);
		if (fault) {
			d_msg ("check_fault\n");
			return fault;
		}
#endif
	}

	if (MMU_Disabled) {
		*phys_addr = virt_addr;
	}

	return 0;
}

/* AJ2D-------------------------------------------------------------------------- */

/*arm1176jzf-s mmu_ops_t*/
mmu_ops_t arm1176jzf_s_mmu_ops = {
	arm1176jzf_s_mmu_init,
	arm1176jzf_s_mmu_exit,
	arm1176jzf_s_mmu_read_byte,
	arm1176jzf_s_mmu_write_byte,
	arm1176jzf_s_mmu_read_halfword,
	arm1176jzf_s_mmu_write_halfword,
	arm1176jzf_s_mmu_read_word,
	arm1176jzf_s_mmu_write_word,
	arm1176jzf_s_mmu_load_instr,
	arm1176jzf_s_mmu_mcr,
	arm1176jzf_s_mmu_mrc
/* teawater add for arm2x86 2005.06.19------------------------------------------- */
/*	arm1176jzf_s_mmu_v2p_dbct, */
/* AJ2D-------------------------------------------------------------------------- */
};
