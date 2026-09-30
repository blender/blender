/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "util/aligned_malloc.h"
#include "util/guarded_allocator.h"

#ifdef WITH_BLENDER_GUARDEDALLOC
#  include "../../guardedalloc/MEM_guardedalloc.h"
#endif

#include <cassert>

/* Adopted from Libmv. */

#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
/* Needed for memalign on Linux and _aligned_alloc on Windows. */
#  ifdef FREE_WINDOWS
/* Make sure _aligned_malloc is included. */
#    ifdef __MSVCRT_VERSION__
#      undef __MSVCRT_VERSION__
#    endif
#    define __MSVCRT_VERSION__ 0x0700
#  endif /* FREE_WINDOWS */
#  include <malloc.h>
#else
/* Apple's `malloc` is 16-byte aligned, and does not have `malloc.h`, so include
 * `stdlib` instead.
 */
#  include <cstdlib>
#endif

#ifdef _WIN32
#  include "util/windows.h"
#else
#  include <sys/mman.h>
#endif

CCL_NAMESPACE_BEGIN

void *util_aligned_malloc(const size_t size, const int alignment)
{
  void *mem = nullptr;
#ifdef WITH_BLENDER_GUARDEDALLOC
  mem = MEM_new_uninitialized_aligned(size, alignment, "Cycles Aligned Alloc");
#elif defined(_WIN32)
  mem = _aligned_malloc(size, alignment);
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  if (posix_memalign(&mem, alignment, size)) {
    /* Non-zero means allocation error
     * either no allocation or bad alignment value. */
    mem = nullptr;
  }
#else /* This is for Linux. */
  mem = memalign(alignment, size);
#endif
  if (mem) {
    util_guarded_mem_alloc(size);
  }
  return mem;
}

void *util_page_aligned_malloc(const size_t size)
{
  void *mem = nullptr;
#ifdef _WIN32
  static const auto VirtualAlloc2 = reinterpret_cast<PVOID (*)(
      HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER *, ULONG)>(
      GetProcAddress(GetModuleHandleW(L"KernelBase.dll"), "VirtualAlloc2"));
  if (VirtualAlloc2 != nullptr) {
    MEM_ADDRESS_REQUIREMENTS address_requirements = {};
    address_requirements.Alignment = GetLargePageMinimum();

    MEM_EXTENDED_PARAMETER extended_param = {};
    extended_param.Type = MemExtendedParameterAddressRequirements;
    extended_param.Pointer = &address_requirements;

    mem = VirtualAlloc2(
        nullptr, nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, &extended_param, 1);
  }
  else {
    mem = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  }
#else
  mem = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED) {
    return nullptr;
  }
#endif
  if (mem) {
    util_guarded_mem_alloc(size);
  }
  return mem;
}

void util_aligned_free(void *ptr, const size_t size)
{
  if (ptr) {
    util_guarded_mem_free(size);
  }
#if defined(WITH_BLENDER_GUARDEDALLOC)
  if (ptr != nullptr) {
    MEM_delete_void(ptr);
  }
#elif defined(_WIN32)
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

void util_page_aligned_free(void *ptr, const size_t size)
{
  if (ptr) {
    util_guarded_mem_free(size);
  }
#ifdef _WIN32
  VirtualFree(ptr, 0, MEM_RELEASE);
#else
  munmap(ptr, size);
#endif
}

CCL_NAMESPACE_END
