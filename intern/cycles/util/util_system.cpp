/*
 * Copyright 2011, Blender Foundation.
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
 */

#include "util_system.h"
#include "util_types.h"

#ifdef _WIN32
#if(!defined(FREE_WINDOWS))
#include <intrin.h>
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <unistd.h>
#endif

CCL_NAMESPACE_BEGIN

int system_cpu_thread_count()
{
	static uint count = 0;

	if(count > 0)
		return count;

#ifdef _WIN32
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	count = (uint)info.dwNumberOfProcessors;
#elif defined(__APPLE__)
	size_t len = sizeof(count);
	int mib[2] = { CTL_HW, HW_NCPU };
	
	sysctl(mib, 2, &count, &len, NULL, 0);
#else
	count = (uint)sysconf(_SC_NPROCESSORS_ONLN);
#endif

	if(count < 1)
		count = 1;

	return count;
}

#if !defined(_WIN32) || defined(FREE_WINDOWS)
static void __cpuid(int data[4], int selector)
{
#ifdef __x86_64__
	asm("cpuid" : "=a" (data[0]), "=b" (data[1]), "=c" (data[2]), "=d" (data[3]) : "a"(selector));
#else
#ifdef __i386__
	asm("pushl %%ebx    \n\t"
		"cpuid          \n\t"
		"movl %%ebx, %1 \n\t"
		"popl %%ebx     \n\t" : "=a" (data[0]), "=r" (data[1]), "=c" (data[2]), "=d" (data[3]) : "a"(selector));
#else
	data[0] = data[1] = data[2] = data[3] = 0;
#endif
#endif
}
#endif

static void replace_string(string& haystack, const string& needle, const string& other)
{
	size_t i;

	while((i = haystack.find(needle)) != string::npos)
		haystack.replace(i, needle.length(), other);
}

string system_cpu_brand_string()
{
	char buf[48];
	int result[4];

	__cpuid(result, 0x80000000);

	if(result[0] >= (int)0x80000004) {
		__cpuid((int*)(buf+0), 0x80000002);
		__cpuid((int*)(buf+16), 0x80000003);
		__cpuid((int*)(buf+32), 0x80000004);

		string brand = buf;

		/* make it a bit more presentable */
		replace_string(brand, "(TM)", "");
		replace_string(brand, "(R)", "");

		size_t i;
		if((i = brand.find("  ")) != string::npos)
			brand = brand.substr(0, i);

		return brand;
	}

	return "Unknown CPU";
}

int system_cpu_bits()
{
	return (sizeof(void*)*8);
}

CCL_NAMESPACE_END

