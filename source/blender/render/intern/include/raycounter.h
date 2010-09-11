/**
 * $Id$
 *
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef RE_RAYCOUNTER_H
#define RE_RAYCOUNTER_H

#include "RE_raytrace.h"


#ifdef RE_RAYCOUNTER

/* #define RE_RC_INIT(isec, shi) (isec).count = re_rc_counter+(shi).thread */
#define RE_RC_INIT(isec, shi) (isec).raycounter = &((shi).raycounter)
void RE_RC_INFO (RayCounter *rc);
void RE_RC_MERGE(RayCounter *rc, RayCounter *tmp);
#define RE_RC_COUNT(var) (var)++

extern RayCounter re_rc_counter[];

#else

#	define RE_RC_INIT(isec,shi)
#	define RE_RC_INFO(rc)
#	define RE_RC_MERGE(dest,src)
#	define	RE_RC_COUNT(var)
		
#endif


#endif
