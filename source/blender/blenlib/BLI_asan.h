/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* Clang defines this. */
#ifndef __has_feature
#  define __has_feature(x) 0
#endif

#if (defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)) && \
    (!defined(_MSC_VER) || _MSC_VER > 1929) /* MSVC 2019 and below doesn't ship ASAN headers. */
#  include "sanitizer/asan_interface.h"
#  define WITH_ASAN
#else
/* Ensure return value is used. Just using UNUSED_VARS results in a warning. */
#  define ASAN_POISON_MEMORY_REGION(addr, size) (void)(0 && ((size) != 0 && (addr) != NULL))
#  define ASAN_UNPOISON_MEMORY_REGION(addr, size) (void)(0 && ((size) != 0 && (addr) != NULL))
#endif

/**
 * Mark a region of memory as "freed". When using address sanitizer, accessing the given memory
 * region will cause an use-after-poison error. This can be used to find errors when dealing with
 * uninitialized memory in custom containers.
 */
#define BLI_asan_poison(addr, size) ASAN_POISON_MEMORY_REGION(addr, size)

/**
 * Mark a region of memory as usable again.
 */
#define BLI_asan_unpoison(addr, size) ASAN_UNPOISON_MEMORY_REGION(addr, size)
