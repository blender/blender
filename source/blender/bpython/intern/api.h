/* $Id$ 
 *
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
 *
 *
 * This headerfile defines the API status.
 * Parts of the API can be compiled as dynamic module for testing -- 
 * see Makefile
 */

#undef EXPERIMENTAL  /* undefine this for release, please :-) */

/* Uncomment this if you want to have the new blender module
compiled static into Blender :*/
/*  #define SHAREDMODULE  -- put into Makefile */

/* API configuration -- define in Makefile */

#ifdef SHAREDMODULE 
	#define BLENDERMODULE _Blender
#elif defined(CURRENT_PYTHON_API)
	#define BLENDERMODULE Blender
#elif defined(FUTURE_PYTHON_API)
	#define BLENDERMODULE _Blender
#else // FALLBACK
	#define BLENDERMODULE Blender
#endif	

#define SUBMODULE(mod) (MODNAME(BLENDERMODULE) "." #mod)

/* this macro defines the init routine for dynamically loaded modules;
example:

void INITMODULE(BLENDERMODULE) -> void initBlender(void)
*/

#define _INITMODULE(x) init##x
#define INITMODULE(x) _INITMODULE(x)

/* MODNAME(MODULE) stringifies the module definition, example:
MODNAME(BLENDERMODULE) -> "_Blender"
*/

#define _MODNAME(x) #x
#define MODNAME(x) _MODNAME(x)

// module configuration -- TODO: this should be set later from the Makefile...
/* commented out by mein@cs.umn.edu default is non static now :)
#if defined(__FreeBSD__) || defined(__linux__) || defined (__sgi) || defined(__sparc) || defined(__sparc__) || defined (__OpenBSD__) 
#define STATIC_TEXTTOOLS 1
#endif
*/


#define USE_NMESH 1     // still use NMesh structure for <mesh object>.data
#define CLEAR_NAMESPACE // undefine this if you still want the old dirty global 
                        // namespace shared by ALL scripts. 


// experimental sh*t:
#ifdef EXPERIMENTAL
	#undef USE_NMESH
#endif

