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

#include "BLI_utildefines.h"
#include "BLI_system.h"
#include "BLI_string.h"

#include "MEM_guardedalloc.h"

/* for backtrace */
#if defined(__linux__) || defined(__APPLE__)
#  include <execinfo.h>
#elif defined(WIN32)
#  include <windows.h>
#  include <dbghelp.h>
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

#ifndef NDEBUG
#define MAXSYMBOL 256
#define SIZE 100
	unsigned short i;
	void *stack[SIZE];
	unsigned short nframes;
	SYMBOL_INFO *symbolinfo;
	HANDLE process;

	process = GetCurrentProcess();

	SymInitialize(process, NULL, TRUE);

	nframes = CaptureStackBackTrace(0, SIZE, stack, NULL);
	symbolinfo = MEM_callocN(sizeof(SYMBOL_INFO) + MAXSYMBOL * sizeof(char), "crash Symbol table");
	symbolinfo->MaxNameLen = MAXSYMBOL - 1;
	symbolinfo->SizeOfStruct = sizeof(SYMBOL_INFO);

	for (i = 0; i < nframes; i++) {
		SymFromAddr(process, (DWORD64)(stack[i]), 0, symbolinfo);

		fprintf(fp, "%u: %s - 0x%0X\n", nframes - i - 1, symbolinfo->Name, symbolinfo->Address);
	}

	MEM_freeN(symbolinfo);
#undef MAXSYMBOL
#undef SIZE
#else
	fprintf(fp, "Crash backtrace not supported on release builds\n");
#endif /* NDEBUG */
#else /* _MSC_VER */
	/* ------------------ */
	/* non msvc/osx/linux */
	(void)fp;
#endif

}
/* end BLI_system_backtrace */

/* NOTE: The code for CPU brand string is adopted from Cycles. */

#if !defined(_WIN32) || defined(FREE_WINDOWS)
static void __cpuid(int data[4], int selector)
{
#if defined(__x86_64__)
	asm("cpuid" : "=a" (data[0]), "=b" (data[1]), "=c" (data[2]), "=d" (data[3]) : "a"(selector));
#elif defined(__i386__)
	asm("pushl %%ebx    \n\t"
		"cpuid          \n\t"
		"movl %%ebx, %1 \n\t"
		"popl %%ebx     \n\t"
		: "=a" (data[0]), "=r" (data[1]), "=c" (data[2]), "=d" (data[3])
		: "a"(selector)
		: "ebx");
#else
	data[0] = data[1] = data[2] = data[3] = 0;
#endif
}
#endif

char *BLI_cpu_brand_string(void)
{
	char buf[48] = { 0 };
	int result[4] = { 0 };
	__cpuid(result, 0x80000000);
	if (result[0] >= (int)0x80000004) {
		__cpuid((int *)(buf + 0), 0x80000002);
		__cpuid((int *)(buf + 16), 0x80000003);
		__cpuid((int *)(buf + 32), 0x80000004);
		char *brand = BLI_strdup(buf);
		/* TODO(sergey): Make it a bit more presentable by removing trademark. */
		return brand;
	}
	return NULL;
}
