/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * @author	Maarten Gribnau
 * @date	June 1, 2001
 */

#ifndef _GHOST_DEBUG_H_
#define _GHOST_DEBUG_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
	#ifdef _DEBUG
		#pragma warning (disable:4786) // suppress stl-MSVC debug info warning
		#define GHOST_DEBUG
	#endif // _DEBUG
#else // WIN32
	#ifndef NDEBUG
		#define GHOST_DEBUG
	#endif // DEBUG
#endif // WIN32

#ifdef GHOST_DEBUG
	#include <iostream>
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

