/**
 * BKE_softbody.h 
 *	
 * $Id: BKE_softbody.h 
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef BKE_SOFTBODY_H
#define BKE_SOFTBODY_H

typedef struct BodyPoint {
	float origS[3], origE[3], origT[3], pos[3], vec[3], force[3];
	float weight, goal;
	float prevpos[3], prevvec[3], prevdx[3], prevdv[3]; /* used for Heun integration */
    int nofsprings; int *springs;
	float contactfrict;
} BodyPoint;

typedef struct BodySpring {
	int v1, v2;
	float len, strength;
} BodySpring;

struct Object;
struct SoftBody;

/* allocates and initializes general main data */
extern struct SoftBody	*sbNew(void);

/* frees internal data and softbody itself */
extern void				sbFree(struct SoftBody *sb);

/* go one step in simulation, copy result in vertexCos for meshes, or
 * directly for lattices.
 */
extern void				sbObjectStep(struct Object *ob, float framnr, float (*vertexCos)[3]);

/* makes totally fresh start situation, resets time */
extern void				sbObjectToSoftbody(struct Object *ob);

/* resets all motion and time */
extern void				sbObjectReset(struct Object *ob);

#endif

