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

/* this comes from the `reuse' library. copy any changes back to the source */

#ifndef _ODE_MEMORY_H_
#define _ODE_MEMORY_H_

#ifdef __cplusplus
extern "C" {
#endif

/* function types to allocate and free memory */
typedef void * dAllocFunction (int size);
typedef void * dReallocFunction (void *ptr, int oldsize, int newsize);
typedef void dFreeFunction (void *ptr, int size);

/* set new memory management functions. if fn is 0, the default handlers are
 * used. */
void dSetAllocHandler (dAllocFunction *fn);
void dSetReallocHandler (dReallocFunction *fn);
void dSetFreeHandler (dFreeFunction *fn);

/* get current memory management functions */
dAllocFunction *dGetAllocHandler ();
dReallocFunction *dGetReallocHandler ();
dFreeFunction *dGetFreeHandler ();

/* allocate and free memory. */
void * dAlloc (int size);
void * dRealloc (void *ptr, int oldsize, int newsize);
void dFree (void *ptr, int size);

/* when alloc debugging is turned on, this indicates that the given block of
 * alloc()ed memory should not be reported as "still in use" when the program
 * exits.
 */
void dAllocDontReport (void *ptr);

#ifdef __cplusplus
}
#endif

#endif
