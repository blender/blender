/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/system.c
 *  \ingroup bli
 */

#include <stdio.h>
#include <stdlib.h>

#include "BLI_system.h"

/* for backtrace */
#if defined(__linux__) || defined(__APPLE__)
#  include <execinfo.h>
#elif defined(_MSV_VER)
#  include <DbgHelp.h>
#endif

int BLI_cpu_support_sse2(void)
{
#if defined(__x86_64__) || defined(_M_X64)
	/* x86_64 always has SSE2 instructions */
	return 1;
#elif defined(__GNUC__) && defined(i386)
	/* for GCC x86 we check cpuid */
	unsigned int d;
	__asm__(
	    "pushl %%ebx\n\t"
	    "cpuid\n\t"
	    "popl %%ebx\n\t"
		: "=d" (d)
		: "a" (1));
	return (d & 0x04000000) != 0;
#elif (defined(_MSC_VER) && defined(_M_IX86))
	/* also check cpuid for MSVC x86 */
	unsigned int d;
	__asm {
		xor     eax, eax
		inc eax
		push ebx
		cpuid
		pop ebx
		mov d, edx
	}
	return (d & 0x04000000) != 0;
#else
	return 0;
#endif
}

/**
 * Write a backtrace into a file for systems which support it.
 */
void BLI_system_backtrace(FILE *fp)
{
	/* ------------- */
	/* Linux / Apple */
#if defined(__linux__) || defined(__APPLE__)

#define SIZE 100
	void *buffer[SIZE];
	int nptrs;
	char **strings;
	int i;

	/* include a backtrace for good measure */
	nptrs = backtrace(buffer, SIZE);
	strings = backtrace_symbols(buffer, nptrs);
	for (i = 0; i < nptrs; i++) {
		fputs(strings[i], fp);
		fputc('\n', fp);
	}

	free(strings);
#undef SIZE

	/* -------- */
	/* Windows  */
#elif defined(_MSC_VER)

	(void)fp;
#if 0
#define MAXSYMBOL 256
	unsigned short	i;
	void *stack[SIZE];
	unsigned short nframes;
	SYMBOL_INFO	*symbolinfo;
	HANDLE process;

	process = GetCurrentProcess();

	SymInitialize(process, NULL, true);

	nframes = CaptureStackBackTrace(0, SIZE, stack, NULL);
	symbolinfo = MEM_callocN(sizeof(SYMBOL_INFO) + MAXSYMBOL * sizeof(char), "crash Symbol table");
	symbolinfo->MaxNameLen = MAXSYMBOL - 1;
	symbolinfo->SizeOfStruct = sizeof(SYMBOL_INFO);

	for (i = 0; i < nframes; i++) {
		SymFromAddr(process, ( DWORD64 )( stack[ i ] ), 0, symbolinfo);

		fprintf(fp, "%u: %s - 0x%0X\n", nframes - i - 1, symbolinfo->Name, symbolinfo->Address);
	}

	MEM_freeN(symbolinfo);
#undef MAXSYMBOL
#endif

	/* ------------------ */
	/* non msvc/osx/linux */
#else
	(void)fp;
#endif

}
/* end BLI_system_backtrace */
