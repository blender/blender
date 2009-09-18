/**
 * BKE_softbody.h 
 *	
 * $Id: BKE_softbody.h 
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_SOFTBODY_H
#define BKE_SOFTBODY_H

struct Object;
struct Scene;
struct SoftBody;

typedef struct BodyPoint {
	float origS[3], origE[3], origT[3], pos[3], vec[3], force[3];
	float goal;
	float prevpos[3], prevvec[3], prevdx[3], prevdv[3]; /* used for Heun integration */
    float impdv[3],impdx[3];
    int nofsprings; int *springs;
	float choke,choke2,frozen;
	float colball;
	short flag;
	//char octantflag;
	float mass;
	float springweight;
} BodyPoint;

/* allocates and initializes general main data */
extern struct SoftBody	*sbNew(struct Scene *scene);

/* frees internal data and softbody itself */
extern void				sbFree(struct SoftBody *sb);

/* frees simulation data to reset simulation */
extern void				sbFreeSimulation(struct SoftBody *sb);

/* do one simul step, reading and writing vertex locs from given array */
extern void				sbObjectStep(struct Scene *scene, struct Object *ob, float framnr, float (*vertexCos)[3], int numVerts);

/* makes totally fresh start situation, resets time */
extern void				sbObjectToSoftbody(struct Object *ob);

/* links the softbody module to a 'test for Interrupt' function */
/* pass NULL to unlink again */
extern void             sbSetInterruptCallBack(int (*f)(void));

#endif

