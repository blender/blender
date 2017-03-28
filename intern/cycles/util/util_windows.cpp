/*
 * Copyright 2011-2016 Blender Foundation
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

#include "util/util_windows.h"

#ifdef _WIN32

CCL_NAMESPACE_BEGIN

#ifdef _M_X64
#  include <VersionHelpers.h>
#endif

#if _WIN32_WINNT < 0x0601
tGetActiveProcessorGroupCount *GetActiveProcessorGroupCount;
tGetActiveProcessorCount *GetActiveProcessorCount;
tSetThreadGroupAffinity *SetThreadGroupAffinity;
tGetProcessGroupAffinity *GetProcessGroupAffinity;
#endif

static WORD GetActiveProcessorGroupCount_stub()
{
	return 1;
}

static DWORD GetActiveProcessorCount_stub(WORD /*GroupNumber*/)
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}

static BOOL SetThreadGroupAffinity_stub(
        HANDLE /*hThread*/,
        const GROUP_AFFINITY  * /*GroupAffinity*/,
        PGROUP_AFFINITY /*PreviousGroupAffinity*/)
{
	return TRUE;
}

static BOOL GetProcessGroupAffinity_stub(HANDLE hProcess,
                                         PUSHORT GroupCount,
                                         PUSHORT GroupArray)
{
	if(*GroupCount < 1) {
		return FALSE;
	}
	*GroupCount = 1;
	GroupArray[0] = 0;
	return TRUE;
}

static bool supports_numa()
{
#ifndef _M_X64
	return false;
#else
	return IsWindows7OrGreater();
#endif
}

void util_windows_init_numa_groups()
{
	static bool initialized = false;
	if(initialized) {
		return;
	}
	initialized = true;
#if _WIN32_WINNT < 0x0601
	if(!supports_numa()) {
		/* Use stubs on platforms which doesn't have rean NUMA/Groups. */
		GetActiveProcessorGroupCount = GetActiveProcessorGroupCount_stub;
		GetActiveProcessorCount = GetActiveProcessorCount_stub;
		SetThreadGroupAffinity = SetThreadGroupAffinity_stub;
		GetProcessGroupAffinity = GetProcessGroupAffinity_stub;
		return;
	}
	HMODULE kernel = GetModuleHandleA("kernel32.dll");
#  define READ_SYMBOL(sym) sym = (t##sym*)GetProcAddress(kernel, #sym)
	READ_SYMBOL(GetActiveProcessorGroupCount);
	READ_SYMBOL(GetActiveProcessorCount);
	READ_SYMBOL(SetThreadGroupAffinity);
	READ_SYMBOL(GetProcessGroupAffinity);
#  undef READ_SUMBOL
#endif
}

CCL_NAMESPACE_END

#endif  /* _WIN32 */
