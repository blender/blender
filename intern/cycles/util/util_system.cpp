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

#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(_M_IX86)

struct CPUCapabilities {
	bool x64;
	bool mmx;
	bool sse;
	bool sse2;
	bool sse3;
	bool ssse3;
	bool sse41;
	bool sse42;
	bool sse4a;
	bool avx;
	bool xop;
	bool fma3;
	bool fma4;
};

bool system_cpu_support_optimized()
{
	static CPUCapabilities caps;
	static bool caps_init = false;

	if(!caps_init) {
		int result[4], num; //, num_ex;

		memset(&caps, 0, sizeof(caps));

		__cpuid(result, 0);
		num = result[0];

#if 0
		__cpuid(result, 0x80000000);
		num_ex = result[0];
#endif

		if(num >= 1) {
			__cpuid(result, 0x00000001);
			caps.mmx = (result[3] & ((int)1 << 23)) != 0;
			caps.sse = (result[3] & ((int)1 << 25)) != 0;
			caps.sse2 = (result[3] & ((int)1 << 26)) != 0;
			caps.sse3 = (result[2] & ((int)1 <<  0)) != 0;

			caps.ssse3 = (result[2] & ((int)1 <<  9)) != 0;
			caps.sse41 = (result[2] & ((int)1 << 19)) != 0;
			caps.sse42 = (result[2] & ((int)1 << 20)) != 0;

			caps.avx = (result[2] & ((int)1 << 28)) != 0;
			caps.fma3 = (result[2] & ((int)1 << 12)) != 0;
		}

#if 0
		if(num_ex >= 0x80000001) {
			__cpuid(result, 0x80000001);
			caps.x64 = (result[3] & ((int)1 << 29)) != 0;
			caps.sse4a = (result[2] & ((int)1 <<  6)) != 0;
			caps.fma4 = (result[2] & ((int)1 << 16)) != 0;
			caps.xop = (result[2] & ((int)1 << 11)) != 0;
		}
#endif

		caps_init = true;
	}

	/* optimization flags use these */
	return caps.sse && caps.sse2 && caps.sse3;
}

#else

bool system_cpu_support_optimized()
{
	return false;
}

#endif

CCL_NAMESPACE_END

