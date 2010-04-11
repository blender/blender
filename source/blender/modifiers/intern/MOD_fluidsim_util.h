/**
 * $Id: BKE_fluidsim.h 26841 2010-02-12 13:34:04Z campbellbarton $
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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef MOD_FLUIDSIM_UTIL_H
#define MOD_FLUIDSIM_UTIL_H

struct Object;
struct Scene;
struct FluidsimModifierData;
struct DerivedMesh;

/* new fluid-modifier interface */
void fluidsim_init(struct FluidsimModifierData *fluidmd);
void fluidsim_free(struct FluidsimModifierData *fluidmd);

struct DerivedMesh *fluidsimModifier_do(struct FluidsimModifierData *fluidmd,
	struct Scene *scene, struct Object *ob, struct DerivedMesh *dm,
	int useRenderParams, int isFinalCalc);
	
#endif

