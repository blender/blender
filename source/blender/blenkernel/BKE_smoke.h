/**
 * BKE_smoke.h
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_SMOKE_H_
#define BKE_SMOKE_H_

typedef float (*bresenham_callback) (float *result, float *input, int res[3], int *pixel, float *tRay, float correct);

void smokeModifier_do(struct SmokeModifierData *smd, struct Scene *scene, struct Object *ob, struct DerivedMesh *dm, int useRenderParams, int isFinalCalc);

void smokeModifier_free (struct SmokeModifierData *smd);
void smokeModifier_reset(struct SmokeModifierData *smd);
void smokeModifier_reset_turbulence(struct SmokeModifierData *smd);
void smokeModifier_createType(struct SmokeModifierData *smd);

long long smoke_get_mem_req(int xres, int yres, int zres, int amplify);

#endif /* BKE_SMOKE_H_ */
