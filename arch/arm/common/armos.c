/*  armos.c -- ARMulator OS interface:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
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
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/* This file contains a model of Demon, ARM Ltd's Debug Monitor,
including all the SWI's required to support the C library. The code in
it is not really for the faint-hearted (especially the abort handling
code), but it is a complete example. Defining NOOS will disable all the
fun, and definign VAILDATE will define SWI 1 to enter SVC mode, and SWI
0x11 to halt the emulator. */

//chy 2005-09-12 disable below line
//#include "config.h"
#include "ansidecl.h"

#include <time.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#ifndef O_RDONLY
#define O_RDONLY 0
#endif
#ifndef O_WRONLY
#define O_WRONLY 1
#endif
#ifndef O_RDWR
#define O_RDWR   2
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef __STDC__
#define unlink(s) remove(s)
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>		/* For SEEK_SET etc */
#endif

#ifdef __riscos
extern int _fisatty (FILE *);
#define isatty_(f) _fisatty(f)
#else
#ifdef __ZTC__
#include <io.h>
#define isatty_(f) isatty((f)->_file)
#else
#ifdef macintosh
#include <ioctl.h>
#define isatty_(f) (~ioctl ((f)->_file, FIOINTERACTIVE, NULL))
#else
#define isatty_(f) isatty (fileno (f))
#endif
#endif
#endif

#include "armdefs.h"
#include "armos.h"
#include "armemu.h"
#include "skyeye_swapendian.h"
#ifndef NOOS
#ifndef VALIDATE
/* #ifndef ASIM */
//chy 2005-09-12 disable below line
//#include "armfpe.h"
/* #endif */
#endif
#endif

#define DUMP_SYSCALL 0
#define dump(...) do { if (DUMP_SYSCALL) printf(__VA_ARGS__); } while(0)
//#define debug(...)			printf(__VA_ARGS__);
#define debug(...)			;

extern unsigned ARMul_OSHandleSWI (ARMul_State * state, ARMword number);

#ifndef FOPEN_MAX
#define FOPEN_MAX 64
#endif

/***************************************************************************\
*                          OS private Information                           *
\***************************************************************************/

unsigned arm_dyncom_SWI(ARMul_State * state, ARMword number)
{
	ARMul_OSHandleSWI(state, number);
}

mmap_area_t *mmap_global = NULL;

static int translate_open_mode[] = {
	O_RDONLY,		/* "r"   */
	O_RDONLY + O_BINARY,	/* "rb"  */
	O_RDWR,			/* "r+"  */
	O_RDWR + O_BINARY,	/* "r+b" */
	O_WRONLY + O_CREAT + O_TRUNC,	/* "w"   */
	O_WRONLY + O_BINARY + O_CREAT + O_TRUNC,	/* "wb"  */
	O_RDWR + O_CREAT + O_TRUNC,	/* "w+"  */
	O_RDWR + O_BINARY + O_CREAT + O_TRUNC,	/* "w+b" */
	O_WRONLY + O_APPEND + O_CREAT,	/* "a"   */
	O_WRONLY + O_BINARY + O_APPEND + O_CREAT,	/* "ab"  */
	O_RDWR + O_APPEND + O_CREAT,	/* "a+"  */
	O_RDWR + O_BINARY + O_APPEND + O_CREAT	/* "a+b" */
};

static void
SWIWrite0 (ARMul_State * state, ARMword addr)
{
	ARMword temp;

	while ((temp = ARMul_ReadByte (state, addr++)) != 0)
		(void) fputc ((char) temp, stdout);
}

static void
WriteCommandLineTo (ARMul_State * state, ARMword addr)
{
	ARMword temp;
	char *cptr = state->CommandLine;
	if (cptr == NULL)
		cptr = "\0";
	do {
		temp = (ARMword) * cptr++;
		ARMul_WriteByte (state, addr++, temp);
	}
	while (temp != 0);
}

static void
SWIopen (ARMul_State * state, ARMword name, ARMword SWIflags)
{
	char dummy[2000];
	int flags;
	int i;

	for (i = 0; (dummy[i] = ARMul_ReadByte (state, name + i)); i++);

	/* Now we need to decode the Demon open mode */
	flags = translate_open_mode[SWIflags];

	/* Filename ":tt" is special: it denotes stdin/out */
	if (strcmp (dummy, ":tt") == 0) {
		if (flags == O_RDONLY)	/* opening tty "r" */
			state->Reg[0] = 0;	/* stdin */
		else
			state->Reg[0] = 1;	/* stdout */
	}
	else {
		state->Reg[0] = (int) open (dummy, flags, 0666);
	}
}

static void
SWIread (ARMul_State * state, ARMword f, ARMword ptr, ARMword len)
{
	int res;
	int i;
	char *local = malloc (len);

	if (local == NULL) {
		fprintf (stderr,
			 "sim: Unable to read 0x%ulx bytes - out of memory\n",
			 len);
		return;
	}

	res = read (f, local, len);
	if (res > 0)
		for (i = 0; i < res; i++)
			ARMul_WriteByte (state, ptr + i, local[i]);
	free (local);
	//state->Reg[0] = res == -1 ? -1 : len - res;
	state->Reg[0] = res;
}

static void
SWIwrite (ARMul_State * state, ARMword f, ARMword ptr, ARMword len)
{
	int res;
	ARMword i;
	char *local = malloc (len);

	if (local == NULL) {
		fprintf (stderr,
			 "sim: Unable to write 0x%lx bytes - out of memory\n",
			 (int) len);
		return;
	}

	for (i = 0; i < len; i++)
		local[i] = ARMul_ReadByte (state, ptr + i);

	res = write (f, local, len);
	//state->Reg[0] = res == -1 ? -1 : len - res;
	state->Reg[0] = res;
	free (local);
}

static void
SWIflen (ARMul_State * state, ARMword fh)
{
	ARMword addr;

	if (fh == 0 || fh > FOPEN_MAX) {
		state->Reg[0] = -1L;
		return;
	}

	addr = lseek (fh, 0, SEEK_CUR);

	state->Reg[0] = lseek (fh, 0L, SEEK_END);
	(void) lseek (fh, addr, SEEK_SET);

}

/***************************************************************************\
* The emulator calls this routine when a SWI instruction is encuntered. The *
* parameter passed is the SWI number (lower 24 bits of the instruction).    *
\***************************************************************************/
//static int brk_static =  0x00082008 + 0x000bbf38;
static int brk_static =  0x10000000;
unsigned
ARMul_OSHandleSWI (ARMul_State * state, ARMword number)
{
	number &= 0xfffff;
	ARMword addr, temp;

	switch (number) {
	case SWI_Read:
		SWIread (state, state->Reg[0], state->Reg[1], state->Reg[2]);
		return TRUE;

	case SWI_GetUID32:
		state->Reg[0] = getuid();
		return TRUE;

	case SWI_GetGID32:
		state->Reg[0] = getgid();
		return TRUE;

	case SWI_GetEUID32:
		state->Reg[0] = geteuid();
		return TRUE;

	case SWI_GetEGID32:
		state->Reg[0] = getegid();
		return TRUE;

	case SWI_Write:
		SWIwrite (state, state->Reg[0], state->Reg[1], state->Reg[2]);
		return TRUE;

	case SWI_Open:
		SWIopen (state, state->Reg[0], state->Reg[1]);
		return TRUE;

	case SWI_Close:
		state->Reg[0] = close (state->Reg[0]);
		return TRUE;

	case SWI_Seek:{
			/* We must return non-zero for failure */
			state->Reg[0] =
				lseek (state->Reg[0], state->Reg[1],
					     SEEK_SET);
			return TRUE;
		}

	case SWI_ExitGroup:
	case SWI_Exit:
		exit(0);
		return TRUE;

	case SWI_Times:{
		time_t now;
		time(&now);
		bus_write(32, state->Reg[0], now);
		state->Reg[0] = now;

		return TRUE;
		}

	case SWI_Brk:
		if(state->Reg[0]){
			brk_static = state->Reg[0];
			state->Reg[0] = 0;
		} else
			state->Reg[0] = brk_static;
		return TRUE;

	case SWI_Break:
		state->Emulate = FALSE;
		return TRUE;

	case SWI_Mmap:{
		int addr = state->Reg[0];
		int len = state->Reg[1];
		int prot = state->Reg[2];
		int flag = state->Reg[3];
		int fd = state->Reg[4];
		int offset = state->Reg[5];
		mmap_area_t *area = new_mmap_area(addr, len);
		state->Reg[0] = area->bank.addr;
		//printf("syscall %d mmap(0x%x,%x,0x%x,0x%x,%d,0x%x) = 0x%x\n",\
				SWI_Mmap, addr, len, prot, flag, fd, offset, state->Reg[0]);
		return TRUE;
	}

	case SWI_Munmap:
		state->Reg[0] = 0;
		return TRUE;

	case SWI_Breakpoint:
		//chy 2005-09-12 change below line
		//state->EndCondition = RDIError_BreakpointReached;
		//printf ("SKYEYE: in armos.c : should not come here!!!!\n");
		state->EndCondition = 0;
		/*modified by ksh to support breakpoiont*/
		state->Emulate = STOP;
		return (TRUE);
#if 0
	case SWI_Clock:
		/* return number of centi-seconds... */
		state->Reg[0] =
#ifdef CLOCKS_PER_SEC
			(CLOCKS_PER_SEC >= 100)
			? (ARMword) (clock () / (CLOCKS_PER_SEC / 100))
			: (ARMword) ((clock () * 100) / CLOCKS_PER_SEC);
#else
			/* presume unix... clock() returns microseconds */
			(ARMword) (clock () / 10000);
#endif
		return (TRUE);

	case SWI_Time:
		state->Reg[0] = (ARMword) time (NULL);
		return (TRUE);
	case SWI_Flen:
		SWIflen (state, state->Reg[0]);
		return (TRUE);

#endif
	default:
		return (FALSE);
	}
}

/**
 * @brief For mmap syscall.A mmap_area is a memory bank. Get from ppc.
 */
static mmap_area_t* new_mmap_area(int sim_addr, int len){
	mmap_area_t *area = (mmap_area_t *)malloc(sizeof(mmap_area_t));
	if(area == NULL){
		printf("error ,failed %s\n",__FUNCTION__);
		exit(0);
	}
	memset(area, 0x0, sizeof(mmap_area_t));
	area->bank.addr = mmap_next_base;
	area->bank.len = len;
	area->bank.bank_write = mmap_mem_write;
	area->bank.bank_read = mmap_mem_read;
	area->bank.type = MEMTYPE_RAM;
	area->bank.objname = "mmap";
	addr_mapping(&area->bank);

	mmap_next_base = mmap_next_base + len + 4;

	area->mmap_addr = malloc(len);
	if(area->mmap_addr == NULL){
		printf("error mmap malloc\n");
		exit(0);
	}
	memset(area->mmap_addr, 0x0, len);
	area->next = NULL;
	if(mmap_global){
		area->next = mmap_global->next;
		mmap_global->next = area;
	}else{
		mmap_global = area;
	}
	return area;
}

static mmap_area_t *get_mmap_area(int addr){
	mmap_area_t *tmp = mmap_global;
	while(tmp){
		if ((tmp->bank.addr <= addr) && (tmp->bank.addr + tmp->bank.len > addr)){
			return tmp;
		}
		tmp = tmp->next;
	}
	printf("cannot get mmap area:addr=0x%x\n", addr);
	return NULL;
}

/**
 * @brief the mmap_area bank write function. Get from ppc.
 *
 * @param size size to write, 8/16/32
 * @param addr address to write
 * @param value value to write
 *
 * @return sucess return 1,otherwise 0.
 */
static char mmap_mem_write(short size, int addr, uint32_t value){
	mmap_area_t *area_tmp = get_mmap_area(addr);
	mem_bank_t *bank_tmp = &area_tmp->bank;
	int offset = addr - bank_tmp->addr;
	switch(size){
		case 8:{
			//uint8_t value_endian = value;
			uint8_t value_endian = (uint8_t)value;
			*(uint8_t *)&(((char *)area_tmp->mmap_addr)[offset]) = value_endian;
			debug("in %s,size=%d,addr=0x%x,value=0x%x\n",__FUNCTION__,size,addr,value_endian);
			break;
		}
		case 16:{
			//uint16_t value_endian = half_to_BE((uint16_t)value);
			uint16_t value_endian = ((uint16_t)value);
			*(uint16_t *)&(((char *)area_tmp->mmap_addr)[offset]) = value_endian;
			debug("in %s,size=%d,addr=0x%x,value=0x%x\n",__FUNCTION__,size,addr,value_endian);
			break;
		}
		case 32:{
			//uint32_t value_endian = word_to_BE((uint32_t)value);
			uint32_t value_endian = ((uint32_t)value);
			*(uint32_t *)&(((char *)area_tmp->mmap_addr)[offset]) = value_endian;
			debug("in %s,size=%d,addr=0x%x,value=0x%x\n",__FUNCTION__,size,addr,value_endian);
			break;
		}
		default:
			printf("invalid size %d\n",size);
			return 0;
	}
	return 1;
}

/**
 * @brief the mmap_area bank read function. Get from ppc.
 *
 * @param size size to read, 8/16/32
 * @param addr address to read
 * @param value value to read
 *
 * @return sucess return 1,otherwise 0.
 */
static char mmap_mem_read(short size, int addr, uint32_t * value){
	mmap_area_t *area_tmp = get_mmap_area(addr);
	mem_bank_t *bank_tmp = &area_tmp->bank;
	int offset = addr - bank_tmp->addr;
	switch(size){
		case 8:{
			//*(uint8_t *)value = *(uint8_t *)&(((uint8_t *)area_tmp->mmap_addr)[offset]);
			*value = *(uint8_t *)&(((uint8_t *)area_tmp->mmap_addr)[offset]);
			debug("in %s,size=%d,addr=0x%x,value=0x%x\n",__FUNCTION__,size,addr,*(uint32_t*)value);
			break;
		}
		case 16:{
			//*(uint16_t *)value = half_from_BE(*(uint16_t *)&(((uint8_t *)area_tmp->mmap_addr)[offset]));
			*value = (*(uint16_t *)&(((uint8_t *)area_tmp->mmap_addr)[offset]));
			debug("in %s,size=%d,addr=0x%x,value=0x%x\n",__FUNCTION__,size,addr,*(uint16_t*)value);
			break;
		}
		case 32:
			//*value = (uint32_t)word_from_BE(*(uint32_t *)&(((uint8_t *)area_tmp->mmap_addr)[offset]));
			*value = (uint32_t)(*(uint32_t *)&(((uint8_t *)area_tmp->mmap_addr)[offset]));
			debug("in %s,size=%d,addr=0x%x,value=0x%x\n",__FUNCTION__,size,addr,*(uint32_t*)value);
			break;
		default:
			printf("invalid size %d\n",size);
			return 0;
	}
	return 1;
}
