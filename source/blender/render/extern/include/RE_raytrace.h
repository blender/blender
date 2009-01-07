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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * RE_raytrace.h: ray tracing api, can be used independently from the renderer. 
 */

#ifndef RE_RAYTRACE_H
#define RE_RAYTRACE_H

/* ray types */
#define RE_RAY_SHADOW 0
#define RE_RAY_MIRROR 1
#define RE_RAY_SHADOW_TRA 2

/* spatial tree for raytracing acceleration */
typedef void RayTree;
/* abstraction of face type */
typedef void RayFace;

/* object numbers above this are transformed */
#define RE_RAY_TRANSFORM_OFFS 0x8000000

/* convert from pointer to index in array and back, with offset if the
 * instance is transformed */
#define RAY_OBJECT_SET(re, obi) \
	((obi == NULL)? 0: \
	((obi - (re)->objectinstance) + ((obi->flag & R_TRANSFORMED)? RE_RAY_TRANSFORM_OFFS: 0)))

#define RAY_OBJECT_GET(re, i) \
	((re)->objectinstance + ((i >= RE_RAY_TRANSFORM_OFFS)? i-RE_RAY_TRANSFORM_OFFS: i))


/* struct for intersection data */
typedef struct Isect {
	float start[3];			/* start+vec = end, in ray_tree_intersect */
	float vec[3];
	float end[3];			

	float labda, u, v;		/* distance to hitpoint, uv weights */

	RayFace *face;			/* face is where to intersect with */
	int ob;
	RayFace *faceorig;		/* start face */
	int oborig;
	RayFace *face_last;		/* for shadow optimize, last intersected face */
	int ob_last;

	short isect;			/* which half of quad */
	short mode;				/* RE_RAY_SHADOW, RE_RAY_MIRROR, RE_RAY_SHADOW_TRA */
	int lay;				/* -1 default, set for layer lamps */

	/* only used externally */
	float col[4];			/* RGBA for shadow_tra */

	/* octree only */
	RayFace *facecontr;
	int obcontr;
	float ddalabda;
	short faceisect;		/* flag if facecontr was done or not */

	/* custom pointer to be used in the RayCheckFunc */
	void *userdata;
} Isect;

/* function callbacks for face type abstraction */
typedef void (*RayCoordsFunc)(RayFace *face,
	float **v1, float **v2, float **v3, float **v4);
typedef int (*RayCheckFunc)(Isect *is, int ob, RayFace *face);
typedef float *(*RayObjectTransformFunc)(void *userdata, int ob);

/* tree building and freeing */
RayTree *RE_ray_tree_create(int ocres, int totface, float *min, float *max,
	RayCoordsFunc coordfunc, RayCheckFunc checkfunc,
	RayObjectTransformFunc transformfunc, void *userdata);
void RE_ray_tree_add_face(RayTree *tree, int ob, RayFace *face);
void RE_ray_tree_done(RayTree *tree);
void RE_ray_tree_free(RayTree *tree);

/* intersection with full tree and single face */
int RE_ray_tree_intersect(RayTree *tree, Isect *is);
int RE_ray_tree_intersect_check(RayTree *tree, Isect *is, RayCheckFunc check);
int RE_ray_face_intersection(Isect *is, RayObjectTransformFunc transformfunc,
	RayCoordsFunc coordsfunc);

/* retrieve the diameter of the tree structure, for setting intersection
   end distance */
float RE_ray_tree_max_size(RayTree *tree);

#endif /*__RE_RAYTRACE_H__*/

