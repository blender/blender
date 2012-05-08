/*
 * BKE_fluidsim.h 
 *	
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_fluidsim.h
 *  \ingroup editors
 */

#ifndef __ED_FLUIDSIM_H__
#define __ED_FLUIDSIM_H__

struct Object;
struct FluidsimSettings;


/* allocates and initializes fluidsim data */
struct FluidsimSettings* fluidsimSettingsNew(struct Object *srcob);

/* frees internal data itself */
void fluidsimSettingsFree(struct FluidsimSettings* sb);

/* duplicate internal data */
struct FluidsimSettings* fluidsimSettingsCopy(struct FluidsimSettings* sb);

#endif /* __ED_FLUIDSIM_H__ */
