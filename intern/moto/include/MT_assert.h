/**
* $Id$
* ***** BEGIN GPL LICENSE BLOCK *****
*
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
* Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
* The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): none yet.
*
* ***** END GPL LICENSE BLOCK *****
*/

#ifndef MT_ASSERT_H
#define MT_ASSERT_H

#include <signal.h>
#include <stdlib.h>
#include <assert.h>


// So it can be used from C
#ifdef __cplusplus
#define MT_CDECL extern "C"
#else
#define MT_CDECL
#endif

// Ask the user if they wish to abort/break, ignore, or ignore for good.
// file, line, predicate form the message to ask, *do_assert should be set
// to 0 to ignore.
// returns 1 to break, false to ignore
MT_CDECL int MT_QueryAssert(const char *file, int line, const char *predicate, int *do_assert);


#ifdef	NDEBUG
#define MT_assert(predicate) ((void)0)
#define BREAKPOINT() ((void)0)
#else 

// BREAKPOINT() will cause a break into the debugger
#if defined(__i386) && defined(__GNUC__)
// gcc on intel...
#define BREAKPOINT() \
asm("int $3")
#elif defined(_MSC_VER)
// Visual C++ (on Intel)
#define BREAKPOINT() \
{ _asm int 3 }
#elif defined(SIGTRAP)
// POSIX compatible...
#define BREAKPOINT() \
raise(SIGTRAP);
#else
// FIXME: Don't know how to do a decent break!
// Add some code for your cpu type, or get a posix
// system.
// abort instead
#define BREAKPOINT() \
abort();
#endif /* breakpoint */


#if defined(WIN32) && !defined(__GNUC__)
#define MT_assert(predicate) assert(predicate)
#else



// Abort the program if predicate is not true
#define MT_assert(predicate) 									\
{ 												\
	static int do_assert = 1; 								\
	if (!(predicate) && MT_QueryAssert(__FILE__, __LINE__, #predicate, &do_assert))		\
	{											\
		BREAKPOINT();									\
	}											\
}
#endif /* windows */

#endif /* NDEBUG */

#endif

