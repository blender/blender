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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_FLUIDSIM_H
#define BKE_FLUIDSIM_H

struct Object;
struct Scene;
struct FluidsimModifierData;
struct DerivedMesh;
struct MVert;

/* old interface */
struct FluidsimSettings *fluidsimSettingsNew(struct Object *srcob);

void initElbeemMesh(struct Scene *scene, struct Object *ob,
	int *numVertices, float **vertices,
	int *numTriangles, int **triangles,
	int useGlobalCoords, int modifierIndex);

/* new fluid-modifier interface */
void fluidsim_init(struct FluidsimModifierData *fluidmd);
void fluidsim_free(struct FluidsimModifierData *fluidmd);

struct DerivedMesh *fluidsim_read_cache(struct Object *ob, struct DerivedMesh *orgdm,
	struct FluidsimModifierData *fluidmd, int framenr, int useRenderParams);
void fluidsim_read_vel_cache(struct FluidsimModifierData *fluidmd, struct DerivedMesh *dm,
	char *filename);
struct DerivedMesh *fluidsimModifier_do(struct FluidsimModifierData *fluidmd,
	struct Scene *scene, struct Object *ob, struct DerivedMesh *dm,
	int useRenderParams, int isFinalCalc);

/* bounding box & memory estimate */
void fluid_get_bb(struct MVert *mvert, int totvert, float obmat[][4],
		 float start[3], float size[3]);

void fluid_estimate_memory(struct Object *ob, struct FluidsimSettings *fss, char *value);

#endif

