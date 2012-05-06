/*
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

/** \file blender/render/intern/raytrace/rayobject.cpp
 *  \ingroup render
 */


#include <assert.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_material_types.h"

#include "rayintersection.h"
#include "rayobject.h"
#include "raycounter.h"
#include "render_types.h"

/* RayFace
 *
 * note we force always inline here, because compiler refuses to otherwise
 * because function is too long. Since this is code that is called billions
 * of times we really do want to inline. */

MALWAYS_INLINE RayObject* rayface_from_coords(RayFace *rayface, void *ob, void *face,
                                              float *v1, float *v2, float *v3, float *v4)
{
	rayface->ob = ob;
	rayface->face = face;

	copy_v3_v3(rayface->v1, v1);
	copy_v3_v3(rayface->v2, v2);
	copy_v3_v3(rayface->v3, v3);

	if (v4) {
		copy_v3_v3(rayface->v4, v4);
		rayface->quad = 1;
	}
	else {
		rayface->quad = 0;
	}

	return RE_rayobject_unalignRayFace(rayface);
}

MALWAYS_INLINE void rayface_from_vlak(RayFace *rayface, ObjectInstanceRen *obi, VlakRen *vlr)
{
	rayface_from_coords(rayface, obi, vlr, vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->v4 ? vlr->v4->co : NULL);

	if (obi->transform_primitives) {
		mul_m4_v3(obi->mat, rayface->v1);
		mul_m4_v3(obi->mat, rayface->v2);
		mul_m4_v3(obi->mat, rayface->v3);

		if (RE_rayface_isQuad(rayface))
			mul_m4_v3(obi->mat, rayface->v4);
	}
}

RayObject* RE_rayface_from_vlak(RayFace *rayface, ObjectInstanceRen *obi, VlakRen *vlr)
{
	return rayface_from_coords(rayface, obi, vlr, vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->v4 ? vlr->v4->co : 0);
}

/* VlakPrimitive */

RayObject* RE_vlakprimitive_from_vlak(VlakPrimitive *face, struct ObjectInstanceRen *obi, struct VlakRen *vlr)
{
	face->ob = obi;
	face->face = vlr;

	return RE_rayobject_unalignVlakPrimitive(face);
}

/* Checks for ignoring faces or materials */

MALWAYS_INLINE int vlr_check_intersect(Isect *is, ObjectInstanceRen *obi, VlakRen *vlr)
{
	/* for baking selected to active non-traceable materials might still
	 * be in the raytree */
	if (!(vlr->flag & R_TRACEBLE))
		return 0;

	/* I know... cpu cycle waste, might do smarter once */
	if (is->mode==RE_RAY_MIRROR)
		return !(vlr->mat->mode & MA_ONLYCAST);
	else
		return (is->lay & obi->lay);
}

MALWAYS_INLINE int vlr_check_intersect_solid(Isect *UNUSED(is), ObjectInstanceRen* UNUSED(obi), VlakRen *vlr)
{
	/* solid material types only */
	if (vlr->mat->material_type == MA_TYPE_SURFACE)
		return 1;
	else
		return 0;
}

MALWAYS_INLINE int vlr_check_bake(Isect *is, ObjectInstanceRen* obi, VlakRen *UNUSED(vlr))
{
	return (obi->obr->ob != is->userdata) && (obi->obr->ob->flag & SELECT);
}

/* Ray Triangle/Quad Intersection */

MALWAYS_INLINE int isec_tri_quad(float start[3], float dir[3], RayFace *face, float uv[2], float *lambda)
{
	float co1[3], co2[3], co3[3], co4[3];
	float t0[3], t1[3], x[3], r[3], m[3], u, v, divdet, det1, l;
	int quad;

	quad= RE_rayface_isQuad(face);

	copy_v3_v3(co1, face->v1);
	copy_v3_v3(co2, face->v2);
	copy_v3_v3(co3, face->v3);

	copy_v3_v3(r, dir);

	/* intersect triangle */
	sub_v3_v3v3(t0, co3, co2);
	sub_v3_v3v3(t1, co3, co1);

	cross_v3_v3v3(x, r, t1);
	divdet= dot_v3v3(t0, x);

	sub_v3_v3v3(m, start, co3);
	det1= dot_v3v3(m, x);
	
	if (divdet != 0.0f) {
		divdet= 1.0f/divdet;
		v= det1*divdet;

		if (v < RE_RAYTRACE_EPSILON && v > -(1.0f+RE_RAYTRACE_EPSILON)) {
			float cros[3];

			cross_v3_v3v3(cros, m, t0);
			u= divdet*dot_v3v3(cros, r);

			if (u < RE_RAYTRACE_EPSILON && (v + u) > -(1.0f+RE_RAYTRACE_EPSILON)) {
				l= divdet*dot_v3v3(cros, t1);

				/* check if intersection is within ray length */
				if (l > -RE_RAYTRACE_EPSILON && l < *lambda) {
					uv[0]= u;
					uv[1]= v;
					*lambda= l;
					return 1;
				}
			}
		}
	}

	/* intersect second triangle in quad */
	if (quad) {
		copy_v3_v3(co4, face->v4);
		sub_v3_v3v3(t0, co3, co4);
		divdet= dot_v3v3(t0, x);

		if (divdet != 0.0f) {
			divdet= 1.0f/divdet;
			v = det1*divdet;
			
			if (v < RE_RAYTRACE_EPSILON && v > -(1.0f+RE_RAYTRACE_EPSILON)) {
				float cros[3];

				cross_v3_v3v3(cros, m, t0);
				u= divdet*dot_v3v3(cros, r);
	
				if (u < RE_RAYTRACE_EPSILON && (v + u) > -(1.0f+RE_RAYTRACE_EPSILON)) {
					l= divdet*dot_v3v3(cros, t1);
					
					if (l >- RE_RAYTRACE_EPSILON && l < *lambda) {
						uv[0]= u;
						uv[1]= -(1.0f + v + u);
						*lambda= l;
						return 2;
					}
				}
			}
		}
	}

	return 0;
}

/* Simpler yes/no Ray Triangle/Quad Intersection */

MALWAYS_INLINE int isec_tri_quad_neighbour(float start[3], float dir[3], RayFace *face)
{
	float co1[3], co2[3], co3[3], co4[3];
	float t0[3], t1[3], x[3], r[3], m[3], u, v, divdet, det1;
	int quad;

	quad= RE_rayface_isQuad(face);

	copy_v3_v3(co1, face->v1);
	copy_v3_v3(co2, face->v2);
	copy_v3_v3(co3, face->v3);

	negate_v3_v3(r, dir); /* note, different than above function */

	/* intersect triangle */
	sub_v3_v3v3(t0, co3, co2);
	sub_v3_v3v3(t1, co3, co1);

	cross_v3_v3v3(x, r, t1);
	divdet= dot_v3v3(t0, x);

	sub_v3_v3v3(m, start, co3);
	det1= dot_v3v3(m, x);
	
	if (divdet != 0.0f) {
		divdet= 1.0f/divdet;
		v= det1*divdet;

		if (v < RE_RAYTRACE_EPSILON && v > -(1.0f+RE_RAYTRACE_EPSILON)) {
			float cros[3];

			cross_v3_v3v3(cros, m, t0);
			u= divdet*dot_v3v3(cros, r);

			if (u < RE_RAYTRACE_EPSILON && (v + u) > -(1.0f+RE_RAYTRACE_EPSILON))
				return 1;
		}
	}

	/* intersect second triangle in quad */
	if (quad) {
		copy_v3_v3(co4, face->v4);
		sub_v3_v3v3(t0, co3, co4);
		divdet= dot_v3v3(t0, x);

		if (divdet != 0.0f) {
			divdet= 1.0f/divdet;
			v = det1*divdet;
			
			if (v < RE_RAYTRACE_EPSILON && v > -(1.0f+RE_RAYTRACE_EPSILON)) {
				float cros[3];

				cross_v3_v3v3(cros, m, t0);
				u= divdet*dot_v3v3(cros, r);
	
				if (u < RE_RAYTRACE_EPSILON && (v + u) > -(1.0f+RE_RAYTRACE_EPSILON))
					return 2;
			}
		}
	}

	return 0;
}

/* RayFace intersection with checks and neighbor verifaction included,
 * Isect is modified if the face is hit. */

MALWAYS_INLINE int intersect_rayface(RayObject *hit_obj, RayFace *face, Isect *is)
{
	float dist, uv[2];
	int ok= 0;
	
	/* avoid self-intersection */
	if (is->orig.ob == face->ob && is->orig.face == face->face)
		return 0;
		
	/* check if we should intersect this face */
	if (is->check == RE_CHECK_VLR_RENDER) {
		if (vlr_check_intersect(is, (ObjectInstanceRen*)face->ob, (VlakRen*)face->face) == 0)
			return 0;
	}
	else if (is->check == RE_CHECK_VLR_NON_SOLID_MATERIAL) {
		if (vlr_check_intersect(is, (ObjectInstanceRen*)face->ob, (VlakRen*)face->face) == 0)
			return 0;
		if (vlr_check_intersect_solid(is, (ObjectInstanceRen*)face->ob, (VlakRen*)face->face) == 0)
			return 0;
	}
	else if (is->check == RE_CHECK_VLR_BAKE) {
		if (vlr_check_bake(is, (ObjectInstanceRen*)face->ob, (VlakRen*)face->face) == 0)
			return 0;
	}

	/* ray counter */
	RE_RC_COUNT(is->raycounter->faces.test);

	dist= is->dist;
	ok= isec_tri_quad(is->start, is->dir, face, uv, &dist);

	if (ok) {
	
		/* when a shadow ray leaves a face, it can be little outside the edges
		 * of it, causing intersection to be detected in its neighbor face */
		if (is->skip & RE_SKIP_VLR_NEIGHBOUR) {
			if (dist < 0.1f && is->orig.ob == face->ob) {
				VlakRen * a = (VlakRen*)is->orig.face;
				VlakRen * b = (VlakRen*)face->face;

				/* so there's a shared edge or vertex, let's intersect ray with
				 * face itself, if that's true we can safely return 1, otherwise
				 * we assume the intersection is invalid, 0 */
				if (a->v1==b->v1 || a->v2==b->v1 || a->v3==b->v1 || a->v4==b->v1 ||
				    a->v1==b->v2 || a->v2==b->v2 || a->v3==b->v2 || a->v4==b->v2 ||
				    a->v1==b->v3 || a->v2==b->v3 || a->v3==b->v3 || a->v4==b->v3 ||
				    (b->v4 && (a->v1==b->v4 || a->v2==b->v4 || a->v3==b->v4 || a->v4==b->v4)))
				{
					/* create RayFace from original face, transformed if necessary */
					RayFace origface;
					ObjectInstanceRen *ob= (ObjectInstanceRen*)is->orig.ob;
					rayface_from_vlak(&origface, ob, (VlakRen*)is->orig.face);

					if (!isec_tri_quad_neighbour(is->start, is->dir, &origface)) {
						return 0;
					}
				}
			}
		}

		RE_RC_COUNT(is->raycounter->faces.hit);

		is->isect= ok;	// which half of the quad
		is->dist= dist;
		is->u= uv[0]; is->v= uv[1];

		is->hit.ob   = face->ob;
		is->hit.face = face->face;
#ifdef RT_USE_LAST_HIT
		is->last_hit = hit_obj;
#endif
		return 1;
	}

	return 0;
}

/* Intersection */

int RE_rayobject_raycast(RayObject *r, Isect *isec)
{
	int i;

	RE_RC_COUNT(isec->raycounter->raycast.test);

	/* setup vars used on raycast */
	for (i=0; i<3; i++) {
		isec->idot_axis[i]		= 1.0f / isec->dir[i];
		
		isec->bv_index[2*i]		= isec->idot_axis[i] < 0.0 ? 1 : 0;
		isec->bv_index[2*i+1]	= 1 - isec->bv_index[2*i];
		
		isec->bv_index[2*i]		= i+3*isec->bv_index[2*i];
		isec->bv_index[2*i+1]	= i+3*isec->bv_index[2*i+1];
	}

#ifdef RT_USE_LAST_HIT	
	/* last hit heuristic */
	if (isec->mode==RE_RAY_SHADOW && isec->last_hit) {
		RE_RC_COUNT(isec->raycounter->rayshadow_last_hit.test);
		
		if (RE_rayobject_intersect(isec->last_hit, isec)) {
			RE_RC_COUNT(isec->raycounter->raycast.hit);
			RE_RC_COUNT(isec->raycounter->rayshadow_last_hit.hit);
			return 1;
		}
	}
#endif

#ifdef RT_USE_HINT
	isec->hit_hint = 0;
#endif

	if (RE_rayobject_intersect(r, isec)) {
		RE_RC_COUNT(isec->raycounter->raycast.hit);

#ifdef RT_USE_HINT
		isec->hint = isec->hit_hint;
#endif
		return 1;
	}

	return 0;
}

int RE_rayobject_intersect(RayObject *r, Isect *i)
{
	if (RE_rayobject_isRayFace(r)) {
		return intersect_rayface(r, (RayFace*) RE_rayobject_align(r), i);
	}
	else if (RE_rayobject_isVlakPrimitive(r)) {
		//TODO optimize (useless copy to RayFace to avoid duplicate code)
		VlakPrimitive *face = (VlakPrimitive*) RE_rayobject_align(r);
		RayFace nface;
		rayface_from_vlak(&nface, face->ob, face->face);

		return intersect_rayface(r, &nface, i);
	}
	else if (RE_rayobject_isRayAPI(r)) {
		r = RE_rayobject_align(r);
		return r->api->raycast(r, i);
	}
	else {
		assert(0);
		return 0;
	}
}

/* Building */

void RE_rayobject_add(RayObject *r, RayObject *o)
{
	r = RE_rayobject_align(r);
	return r->api->add(r, o);
}

void RE_rayobject_done(RayObject *r)
{
	r = RE_rayobject_align(r);
	r->api->done(r);
}

void RE_rayobject_free(RayObject *r)
{
	r = RE_rayobject_align(r);
	r->api->free(r);
}

float RE_rayobject_cost(RayObject *r)
{
	if (RE_rayobject_isRayFace(r) || RE_rayobject_isVlakPrimitive(r)) {
		return 1.0f;
	}
	else if (RE_rayobject_isRayAPI(r)) {
		r = RE_rayobject_align(r);
		return r->api->cost(r);
	}
	else {
		assert(0);
		return 1.0f;
	}
}

/* Bounding Boxes */

void RE_rayobject_merge_bb(RayObject *r, float *min, float *max)
{
	if (RE_rayobject_isRayFace(r)) {
		RayFace *face = (RayFace*) RE_rayobject_align(r);
		
		DO_MINMAX(face->v1, min, max);
		DO_MINMAX(face->v2, min, max);
		DO_MINMAX(face->v3, min, max);
		if (RE_rayface_isQuad(face)) DO_MINMAX(face->v4, min, max);
	}
	else if (RE_rayobject_isVlakPrimitive(r)) {
		VlakPrimitive *face = (VlakPrimitive*) RE_rayobject_align(r);
		RayFace nface;
		rayface_from_vlak(&nface, face->ob, face->face);

		DO_MINMAX(nface.v1, min, max);
		DO_MINMAX(nface.v2, min, max);
		DO_MINMAX(nface.v3, min, max);
		if (RE_rayface_isQuad(&nface)) DO_MINMAX(nface.v4, min, max);
	}
	else if (RE_rayobject_isRayAPI(r)) {
		r = RE_rayobject_align(r);
		r->api->bb(r, min, max);
	}
	else
		assert(0);
}

/* Hints */

void RE_rayobject_hint_bb(RayObject *r, RayHint *hint, float *min, float *max)
{
	if (RE_rayobject_isRayFace(r) || RE_rayobject_isVlakPrimitive(r)) {
		return;
	}
	else if (RE_rayobject_isRayAPI(r)) {
		r = RE_rayobject_align(r);
		return r->api->hint_bb(r, hint, min, max);
	}
	else
		assert(0);
}

/* RayObjectControl */

int RE_rayobjectcontrol_test_break(RayObjectControl *control)
{
	if (control->test_break)
		return control->test_break(control->data);

	return 0;
}

void RE_rayobject_set_control(RayObject *r, void *data, RE_rayobjectcontrol_test_break_callback test_break)
{
	if (RE_rayobject_isRayAPI(r)) {
		r = RE_rayobject_align(r);
		r->control.data = data;
		r->control.test_break = test_break;
	}
}

