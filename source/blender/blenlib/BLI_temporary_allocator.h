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

/** \file
 * \ingroup bli
 *
 * This allocation method assumes
 *   1. The allocations are short-lived.
 *   2. The total number of allocations is bound by a constant per thread.
 *
 * These two assumptions make it possible to cache and reuse relatively large buffers. They allow
 * to hand out buffers that are much larger than the requested size, without the fear of running
 * out of memory.
 *
 * The assumptions might feel a bit limiting at first, but hold true in many cases. For example,
 * many algorithms need to store temporary data. With this allocator, the allocation can become
 * very cheap for common cases.
 *
 * Many cpu-bound algorithms can benefit from being split up into several stages, whereby the
 * output of one stage is written into an array that is read by the next stage. This makes them
 * easier to debug, profile and optimize. Often a reason this is not done is that the memory
 * allocation might be expensive. The goal of this allocator is to make this a non-issue, by
 * reusing the same long buffers over and over again.
 *
 * All allocated buffers are 64 byte aligned, to make them as reusable as possible.
 * If the requested size is too large, there is a fallback to normal allocation. The allocation
 * overhead is probably very small in these cases anyway.
 *
 * The best way to use this allocator is to use one of the prepared containers like TemporaryVector
 * and TemporaryArray.
 */

#ifndef __BLI_TEMPORARY_ALLOCATOR_H__
#define __BLI_TEMPORARY_ALLOCATOR_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLI_TEMPORARY_BUFFER_ALIGNMENT 64

void *BLI_temporary_allocate(uint size);
void BLI_temporary_deallocate(void *buffer);

#ifdef __cplusplus
}
#endif

#endif /* __BLI_TEMPORARY_ALLOCATOR_H__ */
