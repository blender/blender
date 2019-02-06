/*
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
 */

#ifndef __BKE_FLUIDSIM_H__
#define __BKE_FLUIDSIM_H__

/** \file \ingroup bke
 */

struct Depsgraph;
struct FluidsimSettings;
struct MVert;
struct Object;
struct Scene;

/* old interface */

void initElbeemMesh(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *ob,
                    int *numVertices, float **vertices,
                    int *numTriangles, int **triangles,
                    int useGlobalCoords, int modifierIndex);

/* bounding box & memory estimate */
void fluid_get_bb(struct MVert *mvert, int totvert, float obmat[4][4],
                  float start[3], float size[3]);

void fluid_estimate_memory(struct Object *ob, struct FluidsimSettings *fss, char *value);

#endif
