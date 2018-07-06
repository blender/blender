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

#ifndef __UTIL_WINDOWS_H__
#define __UTIL_WINDOWS_H__

#ifdef _WIN32

#ifndef NOGDI
#  define NOGDI
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

CCL_NAMESPACE_BEGIN

#if _WIN32_WINNT < 0x0601
typedef WORD tGetActiveProcessorGroupCount();
typedef DWORD tGetActiveProcessorCount(WORD GroupNumber);
typedef BOOL tSetThreadGroupAffinity(HANDLE hThread,
                                     const GROUP_AFFINITY  *GroupAffinity,
                                     PGROUP_AFFINITY PreviousGroupAffinity);
typedef BOOL tGetProcessGroupAffinity(HANDLE  hProcess,
                                     PUSHORT GroupCount,
                                     PUSHORT GroupArray);

extern tGetActiveProcessorGroupCount *GetActiveProcessorGroupCount;
extern tGetActiveProcessorCount *GetActiveProcessorCount;
extern tSetThreadGroupAffinity *SetThreadGroupAffinity;
extern tGetProcessGroupAffinity *GetProcessGroupAffinity;
#endif

/* Make sure NUMA and processor groups API is initialized. */
void util_windows_init_numa_groups();

CCL_NAMESPACE_END

#endif  /* WIN32 */

#endif /* __UTIL_WINDOWS_H__ */
