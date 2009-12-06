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

/**
 * @file	GHOST_Debug.h
 * Macro's used in GHOST debug target.
 */

#ifndef _GHOST_DEBUG_H_
#define _GHOST_DEBUG_H_

#ifdef WIN32
	#ifdef _DEBUG
		#pragma warning (disable:4786) // suppress stl-MSVC debug info warning
		// #define GHOST_DEBUG
	#endif // _DEBUG
#endif // WIN32

#ifdef BF_GHOST_DEBUG 
	#define GHOST_DEBUG // spit ghost events to stdout
#endif // BF_GHOST_DEBUG 

#ifdef GHOST_DEBUG
	#include <iostream>
	#include <stdio.h> //for printf()
#endif // GHOST_DEBUG


#ifdef GHOST_DEBUG
	#define GHOST_PRINT(x) { std::cout << x; }
	//#define GHOST_PRINTF(x) { printf(x); }
#else  // GHOST_DEBUG
	#define GHOST_PRINT(x)
	//#define GHOST_PRINTF(x)
#endif // GHOST_DEBUG


#ifdef GHOST_DEBUG
	#define GHOST_ASSERT(x, info) { if (!(x)) {GHOST_PRINT("assertion failed: "); GHOST_PRINT(info); GHOST_PRINT("\n"); } }
#else  // GHOST_DEBUG
	#define GHOST_ASSERT(x, info)
#endif // GHOST_DEBUG

#endif // _GHOST_DEBUG_H_

