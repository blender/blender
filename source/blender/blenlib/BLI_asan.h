/*
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

#pragma once

/* Clang defines this. */
#ifndef __has_feature
#  define __has_feature(x) 0
#endif

#if (defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)) && !defined(_MSC_VER)
#  include "sanitizer/asan_interface.h"
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
