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
 * limitations under the License.
 */

#include "util/util_system.h"

#include "util/util_debug.h"
#include "util/util_logging.h"
#include "util/util_types.h"
#include "util/util_string.h"

#ifdef _WIN32
#  if(!defined(FREE_WINDOWS))
#    include <intrin.h>
#  endif
#  include "util_windows.h"
#elif defined(__APPLE__)
#  include <sys/sysctl.h>
#  include <sys/types.h>
#else
#  include <unistd.h>
#endif

CCL_NAMESPACE_BEGIN

int system_cpu_group_count()
{
#ifdef _WIN32
	util_windows_init_numa_groups();
	return GetActiveProcessorGroupCount();
#else
	/* TODO(sergey): Need to adopt for other platforms. */
	return 1;
#endif
}

int system_cpu_group_thread_count(int group)
{
	/* TODO(sergey): Need make other platforms aware of groups. */
#ifdef _WIN32
	util_windows_init_numa_groups();
	return GetActiveProcessorCount(group);
#elif defined(__APPLE__)
	(void)group;
	int count;
	size_t len = sizeof(count);
	int mib[2] = { CTL_HW, HW_NCPU };
	sysctl(mib, 2, &count, &len, NULL, 0);
	return count;
#else
	(void)group;
	return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

int system_cpu_thread_count()
{
	static uint count = 0;

	if(count > 0) {
		return count;
	}

	int max_group = system_cpu_group_count();
	VLOG(1) << "Detected " << max_group << " CPU groups.";
	for(int group = 0; group < max_group; ++group) {
		int num_threads = system_cpu_group_thread_count(group);
		VLOG(1) << "Group " << group
		        << " has " << num_threads << " threads.";
		count += num_threads;
	}

	if(count < 1) {
		count = 1;
	}

	return count;
}

unsigned short system_cpu_process_groups(unsigned short max_groups,
                                         unsigned short *groups)
{
#ifdef _WIN32
	unsigned short group_count = max_groups;
	if(!GetProcessGroupAffinity(GetCurrentProcess(), &group_count, groups)) {
		return 0;
	}
	return group_count;
#else
	(void) max_groups;
	(void) groups;
	return 0;
#endif
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
		brand = string_remove_trademark(brand);

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
	bool f16c;
	bool avx2;
	bool xop;
	bool fma3;
	bool fma4;
	bool bmi1;
	bool bmi2;
};

static CPUCapabilities& system_cpu_capabilities()
{
	static CPUCapabilities caps;
	static bool caps_init = false;

	if(!caps_init) {
		int result[4], num;

		memset(&caps, 0, sizeof(caps));

		__cpuid(result, 0);
		num = result[0];

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
				int edx; /* not used */
				/* actual opcode for xgetbv */
				__asm__ (".byte 0x0f, 0x01, 0xd0" : "=a" (xcr_feature_mask) , "=d" (edx) : "c" (0) );
#elif defined(_MSC_VER) && defined(_XCR_XFEATURE_ENABLED_MASK)
				xcr_feature_mask = (uint32_t)_xgetbv(_XCR_XFEATURE_ENABLED_MASK);  /* min VS2010 SP1 compiler is required */
#else
				xcr_feature_mask = 0;
#endif
				caps.avx = (xcr_feature_mask & 0x6) == 0x6;
			}

			caps.f16c = (result[2] & ((int)1 << 29)) != 0;

			__cpuid(result, 0x00000007);
			caps.bmi1 = (result[1] & ((int)1 << 3)) != 0;
			caps.bmi2 = (result[1] & ((int)1 << 8)) != 0;
			caps.avx2 = (result[1] & ((int)1 << 5)) != 0;
		}

		caps_init = true;
	}

	return caps;
}

bool system_cpu_support_sse2()
{
	CPUCapabilities& caps = system_cpu_capabilities();
	return DebugFlags().cpu.sse2 && caps.sse && caps.sse2;
}

bool system_cpu_support_sse3()
{
	CPUCapabilities& caps = system_cpu_capabilities();
	return DebugFlags().cpu.sse3 &&
	       caps.sse && caps.sse2 && caps.sse3 && caps.ssse3;
}

bool system_cpu_support_sse41()
{
	CPUCapabilities& caps = system_cpu_capabilities();
	return DebugFlags().cpu.sse41 &&
	       caps.sse && caps.sse2 && caps.sse3 && caps.ssse3 && caps.sse41;
}

bool system_cpu_support_avx()
{
	CPUCapabilities& caps = system_cpu_capabilities();
	return DebugFlags().cpu.avx &&
	       caps.sse && caps.sse2 && caps.sse3 && caps.ssse3 && caps.sse41 && caps.avx;
}

bool system_cpu_support_avx2()
{
	CPUCapabilities& caps = system_cpu_capabilities();
	return DebugFlags().cpu.avx2 &&
	       caps.sse && caps.sse2 && caps.sse3 && caps.ssse3 && caps.sse41 && caps.avx && caps.f16c && caps.avx2 && caps.fma3 && caps.bmi1 && caps.bmi2;
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
bool system_cpu_support_avx2()
{
	return false;
}

#endif

CCL_NAMESPACE_END

