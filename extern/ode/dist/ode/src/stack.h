/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

/* this comes from the `reuse' library. copy any changes back to the source.

these stack allocation functions are a replacement for alloca(), except that
they allocate memory from a separate pool.

advantages over alloca():
  - consecutive allocations are guaranteed to be contiguous with increasing
    address.
  - functions can allocate stack memory that is returned to the caller,
    in other words pushing and popping stack frames is optional.

disadvantages compared to alloca():
  - less portable
  - slightly slower, although still orders of magnitude faster than malloc().
  - longjmp() and exceptions do not deallocate stack memory (but who cares?).

just like alloca():
  - using too much stack memory does not fail gracefully, it fails with a
    segfault.

*/


#ifndef _ODE_STACK_H_
#define _ODE_STACK_H_


#ifdef WIN32
#include "windows.h"
#endif


struct dStack {
  char *base;		// bottom of the stack
  int size;		// maximum size of the stack
  char *pointer;	// current top of the stack
  char *frame;		// linked list of stack frame ptrs
# ifdef WIN32		// stuff for windows:
  int pagesize;		//   - page size - this is ASSUMED to be a power of 2
  int committed;	//   - bytes committed in allocated region
#endif

  // initialize the stack. `max_size' is the maximum size that the stack can
  // reach. on unix and windows a `virtual' memory block of this size is
  // mapped into the address space but does not actually consume physical
  // memory until it is referenced - so it is safe to set this to a high value.

  void init (int max_size);


  // destroy the stack. this unmaps any virtual memory that was allocated.

  void destroy();


  // allocate `size' bytes from the stack and return a pointer to the allocated
  // memory. `size' must be >= 0. the returned pointer will be aligned to the
  // size of a long int.

  char * alloc (int size)
  {
    char *ret = pointer;
    pointer += ((size-1) | (sizeof(long int)-1) )+1;
#   ifdef WIN32
    // for windows we need to commit pages as they are required
    if ((pointer-base) > committed) {
      committed = ((pointer-base-1) | (pagesize-1))+1;	// round up to pgsize
      VirtualAlloc (base,committed,MEM_COMMIT,PAGE_READWRITE);
    }
#   endif
    return ret;
  }


  // return the address that will be returned by the next call to alloc()

  char *nextAlloc()
  {
    return pointer;
  }


  // push and pop the current size of the stack. pushFrame() saves the current
  // frame pointer on the stack, and popFrame() retrieves it. a typical
  // stack-using function will bracket alloc() calls with pushFrame() and
  // popFrame(). both functions return the current stack pointer - this should
  // be the same value for the two bracketing calls. calling popFrame() too
  // many times will result in a segfault.

  char * pushFrame()
  {
    char *newframe = pointer;
    char **addr = (char**) alloc (sizeof(char*));
    *addr = frame;
    frame = newframe;
    return newframe;

    /* OLD CODE
	*((char**)pointer) = frame;
	frame = pointer;
	char *ret = pointer;
	pointer += sizeof(char*);
	return ret;
    */
  }

  char * popFrame()
  {
    pointer = frame;
    frame = *((char**)pointer);
    return pointer;
  }
};


#endif
