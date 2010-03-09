/**
 * blenlib/BKE_ipo.h (mar-2001 nzc)
 *	
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): 2008,2009  Joshua Leung (Animation Cleanup, Animation Systme Recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_IPO_H
#define BKE_IPO_H

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct Ipo;

void do_versions_ipos_to_animato(struct Main *main);

/* --------------------- xxx stuff ------------------------ */

void free_ipo(struct Ipo *ipo);

// xxx perhaps this should be in curve api not in anim api
void correct_bezpart(float *v1, float *v2, float *v3, float *v4);
	

#ifdef __cplusplus
};
#endif

#endif

