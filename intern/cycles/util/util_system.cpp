/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
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

		brand = string_strip(brand);

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

static CPUCapabilities& system_cpu_capabilities()
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

			caps.fma3 = (result[2] & ((int)1 << 12)) != 0;
			caps.avx = false;
			bool os_uses_xsave_xrestore = (result[2] & ((int)1 << 27)) != 0;
			bool cpu_avx_support = (result[2] & ((int)1 << 28)) != 0;

			if( os_uses_xsave_xrestore && cpu_avx_support) {
				// Check if the OS will save the YMM registers
				uint32_t xcr_feature_mask;
				#if defined(__GNUC__)
					int edx; // not used
					__asm__ (".byte 0x0f, 0x01, 0xd0" : "=a" (xcr_feature_mask) , "=d" (edx) : "c" (0) ); /* actual opcode for xgetbv */
				#elif defined(_MSC_VER) && defined(_XCR_XFEATURE_ENABLED_MASK)
					xcr_feature_mask = (uint32_t)_xgetbv(_XCR_XFEATURE_ENABLED_MASK);  /* min VS2010 SP1 compiler is required */
				#else
					xcr_feature_mask = 0;
				#endif
				caps.avx = (xcr_feature_mask & 0x6) == 0x6;
			}
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

	return caps;
}

bool system_cpu_support_sse2()
{
	CPUCapabilities& caps = system_cpu_capabilities();
	return caps.sse && caps.sse2;
}

bool system_cpu_support_sse3()
{
	CPUCapabilities& caps = system_cpu_capabilities();
	return caps.sse && caps.sse2 && caps.sse3 && caps.ssse3;
}

bool system_cpu_support_sse41()
{
	CPUCapabilities& caps = system_cpu_capabilities();
	return caps.sse && caps.sse2 && caps.sse3 && caps.ssse3 && caps.sse41;
}

bool system_cpu_support_avx()
{
	CPUCapabilities& caps = system_cpu_capabilities();
	return caps.sse && caps.sse2 && caps.sse3 && caps.ssse3 && caps.sse41 && caps.avx;
}
#else

bool system_cpu_support_sse2()
{
	return false;
}

bool system_cpu_support_sse3()
{
	return false;
}

bool system_cpu_support_sse41()
{
	return false;
}

bool system_cpu_support_avx()
{
	return false;
}

#endif

CCL_NAMESPACE_END

