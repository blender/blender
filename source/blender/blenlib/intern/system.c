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


#include "BLI_system.h"

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

