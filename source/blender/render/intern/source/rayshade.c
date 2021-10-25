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
 * The Original Code is Copyright (C) 1990-1998 NeoGeo BV.
 * All rights reserved.
 *
 * Contributors: 2004/2005 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/rayshade.c
 *  \ingroup render
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_lamp_types.h"

#include "BLI_blenlib.h"
#include "BLI_system.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_node.h"

#include "render_result.h"
#include "render_types.h"
#include "rendercore.h"
#include "renderdatabase.h"
#include "pixelshading.h"
#include "shading.h"
#include "volumetric.h"

#include "rayintersection.h"
#include "rayobject.h"
#include "raycounter.h"

#define RAY_TRA		1
#define RAY_INSIDE	2

#define DEPTH_SHADOW_TRA  10

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
static int test_break(void *data)
{
	Render *re = (Render *)data;
	return re->test_break(re->tbh);
}

static void RE_rayobject_config_control(RayObject *r, Render *re)
{
	if (RE_rayobject_isRayAPI(r)) {
		r = RE_rayobject_align(r);
		r->control.data = re;
		r->control.test_break = test_break;
	}
}

RayObject *RE_rayobject_create(int type, int size, int octree_resolution)
{
	RayObject * res = NULL;

	if (type == R_RAYSTRUCTURE_AUTO) {
		/* TODO */
		//if (detect_simd())
#ifdef __SSE__
		type = BLI_cpu_support_sse2()? R_RAYSTRUCTURE_SIMD_SVBVH: R_RAYSTRUCTURE_VBVH;
#else
		type = R_RAYSTRUCTURE_VBVH;
#endif
	}
	
#ifndef __SSE__
	if (type == R_RAYSTRUCTURE_SIMD_SVBVH || type == R_RAYSTRUCTURE_SIMD_QBVH) {
		puts("Warning: Using VBVH (SSE was disabled at compile time)");
		type = R_RAYSTRUCTURE_VBVH;
	}
#endif
	
		
	if (type == R_RAYSTRUCTURE_OCTREE) //TODO dynamic ocres
		res = RE_rayobject_octree_create(octree_resolution, size);
	else if (type == R_RAYSTRUCTURE_VBVH)
		res = RE_rayobject_vbvh_create(size);
	else if (type == R_RAYSTRUCTURE_SIMD_SVBVH)
		res = RE_rayobject_svbvh_create(size);
	else if (type == R_RAYSTRUCTURE_SIMD_QBVH)
		res = RE_rayobject_qbvh_create(size);
	else
		res = RE_rayobject_vbvh_create(size);	//Fallback
	
	return res;
}

static RayObject* rayobject_create(Render *re, int type, int size)
{
	RayObject * res = NULL;

	res = RE_rayobject_create(type, size, re->r.ocres);
	
	if (res)
		RE_rayobject_config_control(res, re);

	return res;
}

#ifdef RE_RAYCOUNTER
RayCounter re_rc_counter[BLENDER_MAX_THREADS];
#endif


void freeraytree(Render *re)
{
	ObjectInstanceRen *obi;
	
	if (re->raytree) {
		RE_rayobject_free(re->raytree);
		re->raytree = NULL;
	}
	if (re->rayfaces) {
		MEM_freeN(re->rayfaces);
		re->rayfaces = NULL;
	}
	if (re->rayprimitives) {
		MEM_freeN(re->rayprimitives);
		re->rayprimitives = NULL;
	}

	for (obi=re->instancetable.first; obi; obi=obi->next) {
		ObjectRen *obr = obi->obr;
		if (obr->raytree) {
			RE_rayobject_free(obr->raytree);
			obr->raytree = NULL;
		}
		if (obr->rayfaces) {
			MEM_freeN(obr->rayfaces);
			obr->rayfaces = NULL;
		}
		if (obi->raytree) {
			RE_rayobject_free(obi->raytree);
			obi->raytree = NULL;
		}
	}
	
#ifdef RE_RAYCOUNTER
	{
		const int num_threads = re->r.threads;
		RayCounter sum;
		memset(&sum, 0, sizeof(sum));
		int i;
		for (i=0; i<num_threads; i++)
			RE_RC_MERGE(&sum, re_rc_counter+i);
		RE_RC_INFO(&sum);
	}
#endif
}

static bool is_raytraceable_vlr(Render *re, VlakRen *vlr)
{
	/* note: volumetric must be tracable, wire must not */
	if ((re->flag & R_BAKE_TRACE) || (vlr->flag & R_TRACEBLE) || (vlr->mat->material_type == MA_TYPE_VOLUME))
		if (vlr->mat->material_type != MA_TYPE_WIRE)
			return 1;
	return 0;
}

static bool is_raytraceable(Render *re, ObjectInstanceRen *obi)
{
	int v;
	ObjectRen *obr = obi->obr;

	if (re->excludeob && obr->ob == re->excludeob)
		return 0;

	for (v=0;v<obr->totvlak;v++) {
		VlakRen *vlr = obr->vlaknodes[v>>8].vlak + (v&255);

		if (is_raytraceable_vlr(re, vlr))
			return 1;
	}

	return 0;
}


RayObject* makeraytree_object(Render *re, ObjectInstanceRen *obi)
{
	/*TODO
	 * out-of-memory safeproof
	 * break render
	 * update render stats */
	ObjectRen *obr = obi->obr;

	if (obr->raytree == NULL) {
		RayObject *raytree;
		RayFace *face = NULL;
		VlakPrimitive *vlakprimitive = NULL;
		int v;
		
		//Count faces
		int faces = 0;
		for (v=0;v<obr->totvlak;v++) {
			VlakRen *vlr = obr->vlaknodes[v>>8].vlak + (v&255);
			if (is_raytraceable_vlr(re, vlr))
				faces++;
		}
		
		if (faces == 0)
			return NULL;

		//Create Ray cast accelaration structure
		raytree = rayobject_create( re,  re->r.raytrace_structure, faces );
		if (  (re->r.raytrace_options & R_RAYTRACE_USE_LOCAL_COORDS) )
			vlakprimitive = obr->rayprimitives = (VlakPrimitive *)MEM_callocN(faces * sizeof(VlakPrimitive), "ObjectRen primitives");
		else
			face = obr->rayfaces = (RayFace *)MEM_callocN(faces * sizeof(RayFace), "ObjectRen faces");

		obr->rayobi = obi;
		
		for (v=0;v<obr->totvlak;v++) {
			VlakRen *vlr = obr->vlaknodes[v>>8].vlak + (v&255);
			if (is_raytraceable_vlr(re, vlr)) {
				if ((re->r.raytrace_options & R_RAYTRACE_USE_LOCAL_COORDS)) {
					RE_rayobject_add(raytree, RE_vlakprimitive_from_vlak(vlakprimitive, obi, vlr));
					vlakprimitive++;
				}
				else {
					RE_rayface_from_vlak(face, obi, vlr);
					RE_rayobject_add(raytree, RE_rayobject_unalignRayFace(face));
					face++;
				}
			}
		}
		RE_rayobject_done(raytree);

		/* in case of cancel during build, raytree is not usable */
		if (test_break(re))
			RE_rayobject_free(raytree);
		else
			obr->raytree= raytree;
	}

	if (obr->raytree) {
		if ((obi->flag & R_TRANSFORMED) && obi->raytree == NULL) {
			obi->transform_primitives = 0;
			obi->raytree = RE_rayobject_instance_create( obr->raytree, obi->mat, obi, obi->obr->rayobi );
		}
	}
	
	if (obi->raytree) return obi->raytree;
	return obi->obr->raytree;
}

static bool has_special_rayobject(Render *re, ObjectInstanceRen *obi)
{
	if ( (obi->flag & R_TRANSFORMED) && (re->r.raytrace_options & R_RAYTRACE_USE_INSTANCES) ) {
		ObjectRen *obr = obi->obr;
		int v, faces = 0;
		
		for (v=0;v<obr->totvlak;v++) {
			VlakRen *vlr = obr->vlaknodes[v>>8].vlak + (v&255);
			if (is_raytraceable_vlr(re, vlr)) {
				faces++;
				if (faces > 4)
					return 1;
			}
		}
	}
	return 0;
}
/*
 * create a single raytrace structure with all faces
 */
static void makeraytree_single(Render *re)
{
	ObjectInstanceRen *obi;
	RayObject *raytree;
	RayFace *face = NULL;
	VlakPrimitive *vlakprimitive = NULL;
	int faces = 0, special = 0;

	for (obi = re->instancetable.first; obi; obi = obi->next) {
		if (is_raytraceable(re, obi)) {
			ObjectRen *obr = obi->obr;

			if (has_special_rayobject(re, obi)) {
				special++;
			}
			else {
				int v;
				for (v = 0;v < obr->totvlak; v++) {
					VlakRen *vlr = obr->vlaknodes[v >> 8].vlak + (v&255);
					if (is_raytraceable_vlr(re, vlr)) {
						faces++;
					}
				}
			}
		}
	}
	
	if (faces + special == 0) {
		re->raytree = RE_rayobject_empty_create();
		return;
	}
	
	//Create raytree
	raytree = re->raytree = rayobject_create( re, re->r.raytrace_structure, faces+special );

	if ( (re->r.raytrace_options & R_RAYTRACE_USE_LOCAL_COORDS) ) {
		vlakprimitive = re->rayprimitives = (VlakPrimitive *)MEM_callocN(faces * sizeof(VlakPrimitive), "Raytrace vlak-primitives");
	}
	else {
		face = re->rayfaces	= (RayFace *)MEM_callocN(faces * sizeof(RayFace), "Render ray faces");
	}
	
	for (obi=re->instancetable.first; obi; obi=obi->next)
	if (is_raytraceable(re, obi)) {
		if (test_break(re))
			break;

		if (has_special_rayobject(re, obi)) {
			RayObject *obj = makeraytree_object(re, obi);

			if (test_break(re))
				break;

			if (obj)
				RE_rayobject_add(re->raytree, obj);
		}
		else {
			int v;
			ObjectRen *obr = obi->obr;
			
			if (obi->flag & R_TRANSFORMED) {
				obi->transform_primitives = 1;
			}

			for (v=0;v<obr->totvlak;v++) {
				VlakRen *vlr = obr->vlaknodes[v>>8].vlak + (v&255);
				if (is_raytraceable_vlr(re, vlr)) {
					if ((re->r.raytrace_options & R_RAYTRACE_USE_LOCAL_COORDS)) {
						RayObject *obj = RE_vlakprimitive_from_vlak( vlakprimitive, obi, vlr );
						RE_rayobject_add(raytree, obj);
						vlakprimitive++;
					}
					else {
						RE_rayface_from_vlak(face, obi, vlr);
						if ((obi->flag & R_TRANSFORMED)) {
							mul_m4_v3(obi->mat, face->v1);
							mul_m4_v3(obi->mat, face->v2);
							mul_m4_v3(obi->mat, face->v3);
							if (RE_rayface_isQuad(face))
								mul_m4_v3(obi->mat, face->v4);
						}

						RE_rayobject_add(raytree, RE_rayobject_unalignRayFace(face));
						face++;
					}
				}
			}
		}
	}
	
	if (!test_break(re)) {
		re->i.infostr = IFACE_("Raytree.. building");
		re->stats_draw(re->sdh, &re->i);

		RE_rayobject_done(raytree);
	}
}

void makeraytree(Render *re)
{
	float min[3], max[3], sub[3];
	int i;
	
	re->i.infostr = IFACE_("Raytree.. preparing");
	re->stats_draw(re->sdh, &re->i);

	/* disable options not yet supported by octree,
	 * they might actually never be supported (unless people really need it) */
	if (re->r.raytrace_structure == R_RAYSTRUCTURE_OCTREE)
		re->r.raytrace_options &= ~( R_RAYTRACE_USE_INSTANCES | R_RAYTRACE_USE_LOCAL_COORDS);

	makeraytree_single(re);

	if (test_break(re)) {
		freeraytree(re);

		re->i.infostr = IFACE_("Raytree building canceled");
		re->stats_draw(re->sdh, &re->i);
	}
	else {
		/* Calculate raytree max_size
		 * This is ONLY needed to kept a bogus behavior of SUN and HEMI lights */
		INIT_MINMAX(min, max);
		RE_rayobject_merge_bb(re->raytree, min, max);
		if (min[0] > max[0]) {  /* empty raytree */
			zero_v3(min);
			zero_v3(max);
		}
		for (i=0; i<3; i++) {
			/* TODO: explain why add top both min and max??? */
			min[i] += 0.01f;
			max[i] += 0.01f;
			sub[i] = max[i]-min[i];
		}

		re->maxdist = len_v3(sub);

		re->i.infostr = IFACE_("Raytree finished");
		re->stats_draw(re->sdh, &re->i);
	}

#ifdef RE_RAYCOUNTER
	memset(re_rc_counter, 0, sizeof(re_rc_counter));
#endif
}

/* 	if (shi->osatex)  */
static void shade_ray_set_derivative(ShadeInput *shi)
{
	float detsh, t00, t10, t01, t11;
	int axis1, axis2;

	/* find most stable axis to project */
	axis_dominant_v3(&axis1, &axis2, shi->facenor);

	/* compute u,v and derivatives */
	if (shi->obi->flag & R_TRANSFORMED) {
		float v1[3], v2[3], v3[3];

		mul_v3_m3v3(v1, shi->obi->nmat, shi->v1->co);
		mul_v3_m3v3(v2, shi->obi->nmat, shi->v2->co);
		mul_v3_m3v3(v3, shi->obi->nmat, shi->v3->co);

		/* same as below */
		t00= v3[axis1]-v1[axis1]; t01= v3[axis2]-v1[axis2];
		t10= v3[axis1]-v2[axis1]; t11= v3[axis2]-v2[axis2];
	}
	else {
		const float *v1= shi->v1->co;
		const float *v2= shi->v2->co;
		const float *v3= shi->v3->co;

		/* same as above */
		t00= v3[axis1]-v1[axis1]; t01= v3[axis2]-v1[axis2];
		t10= v3[axis1]-v2[axis1]; t11= v3[axis2]-v2[axis2];
	}

	detsh= 1.0f/(t00*t11-t10*t01);
	t00*= detsh; t01*=detsh; 
	t10*=detsh; t11*=detsh;
	
	shi->dx_u=  shi->dxco[axis1]*t11- shi->dxco[axis2]*t10;
	shi->dx_v=  shi->dxco[axis2]*t00- shi->dxco[axis1]*t01;
	shi->dy_u=  shi->dyco[axis1]*t11- shi->dyco[axis2]*t10;
	shi->dy_v=  shi->dyco[axis2]*t00- shi->dyco[axis1]*t01;
	
}

/* main ray shader */
void shade_ray(Isect *is, ShadeInput *shi, ShadeResult *shr)
{
	ObjectInstanceRen *obi = (ObjectInstanceRen *)is->hit.ob;
	VlakRen *vlr = (VlakRen *)is->hit.face;
	
	/* set up view vector */
	copy_v3_v3(shi->view, is->dir);

	/* render co */
	shi->co[0]= is->start[0]+is->dist*(shi->view[0]);
	shi->co[1]= is->start[1]+is->dist*(shi->view[1]);
	shi->co[2]= is->start[2]+is->dist*(shi->view[2]);
	
	normalize_v3(shi->view);

	shi->obi= obi;
	shi->obr= obi->obr;
	shi->vlr= vlr;
	shi->mat= vlr->mat;
	shade_input_init_material(shi);
	
	if (is->isect==2) 
		shade_input_set_triangle_i(shi, obi, vlr, 0, 2, 3);
	else
		shade_input_set_triangle_i(shi, obi, vlr, 0, 1, 2);

	shi->u= is->u;
	shi->v= is->v;
	shi->dx_u= shi->dx_v= shi->dy_u= shi->dy_v=  0.0f;

	if (shi->osatex)
		shade_ray_set_derivative(shi);
	shade_input_set_normals(shi);

	shade_input_set_shade_texco(shi);
	if (shi->mat->material_type == MA_TYPE_VOLUME) {
		if (ELEM(is->mode, RE_RAY_SHADOW, RE_RAY_SHADOW_TRA)) {
			shade_volume_shadow(shi, shr, is);
		}
		else {
			shade_volume_outside(shi, shr);
		}
	}
	else if (is->mode==RE_RAY_SHADOW_TRA) {
		/* temp hack to prevent recursion */
		if (shi->nodes==0 && shi->mat->nodetree && shi->mat->use_nodes) {
			ntreeShaderExecTree(shi->mat->nodetree, shi, shr);
			shi->mat= vlr->mat;		/* shi->mat is being set in nodetree */
		}
		else
			shade_color(shi, shr);
	}
	else {
		if (shi->mat->nodetree && shi->mat->use_nodes) {
			ntreeShaderExecTree(shi->mat->nodetree, shi, shr);
			shi->mat= vlr->mat;		/* shi->mat is being set in nodetree */
		}
		else {
			shade_material_loop(shi, shr);
		}
		
		/* raytrace likes to separate the spec color */
		sub_v3_v3v3(shr->diff, shr->combined, shr->spec);
		copy_v3_v3(shr->diffshad, shr->diff);
	}

}

static int refraction(float refract[3], const float n[3], const float view[3], float index)
{
	float dot, fac;

	copy_v3_v3(refract, view);
	
	dot = dot_v3v3(view, n);

	if (dot>0.0f) {
		index = 1.0f/index;
		fac= 1.0f - (1.0f - dot*dot)*index*index;
		if (fac <= 0.0f) return 0;
		fac= -dot*index + sqrtf(fac);
	}
	else {
		fac= 1.0f - (1.0f - dot*dot)*index*index;
		if (fac <= 0.0f) return 0;
		fac= -dot*index - sqrtf(fac);
	}

	refract[0]= index*view[0] + fac*n[0];
	refract[1]= index*view[1] + fac*n[1];
	refract[2]= index*view[2] + fac*n[2];

	return 1;
}

static void reflection_simple(float ref[3], float n[3], const float view[3])
{
	const float f1= -2.0f * dot_v3v3(n, view);
	madd_v3_v3v3fl(ref, view, n, f1);
}

/* orn = original face normal */
static void reflection(float ref[3], float n[3], const float view[3], const float orn[3])
{
	float f1;

	reflection_simple(ref, n, view);

	/* test phong normals, then we should prevent vector going to the back */
	f1= dot_v3v3(ref, orn);
	if (f1>0.0f) {
		f1+= 0.01f;
		ref[0]-= f1*orn[0];
		ref[1]-= f1*orn[1];
		ref[2]-= f1*orn[2];
	}
}

#if 0
static void color_combine(float *result, float fac1, float fac2, float col1[3], float col2[3])
{
	float col1t[3], col2t[3];
	
	col1t[0]= sqrt(col1[0]);
	col1t[1]= sqrt(col1[1]);
	col1t[2]= sqrt(col1[2]);
	col2t[0]= sqrt(col2[0]);
	col2t[1]= sqrt(col2[1]);
	col2t[2]= sqrt(col2[2]);

	result[0]= (fac1*col1t[0] + fac2*col2t[0]);
	result[0]*= result[0];
	result[1]= (fac1*col1t[1] + fac2*col2t[1]);
	result[1]*= result[1];
	result[2]= (fac1*col1t[2] + fac2*col2t[2]);
	result[2]*= result[2];
}
#endif

static float shade_by_transmission(Isect *is, ShadeInput *shi, ShadeResult *shr)
{
	float d;
	if (0 == (shi->mat->mode & MA_TRANSP))
		return -1;
	   
	if (shi->mat->tx_limit <= 0.0f) {
		d= 1.0f;
	}
	else {
		float p;

		/* shi.co[] calculated by shade_ray() */
		const float dx= shi->co[0] - is->start[0];
		const float dy= shi->co[1] - is->start[1];
		const float dz= shi->co[2] - is->start[2];
		d = sqrtf(dx * dx + dy * dy + dz * dz);
		if (d > shi->mat->tx_limit)
			d= shi->mat->tx_limit;

		p = shi->mat->tx_falloff;
		if (p < 0.0f) p= 0.0f;
		else if (p > 10.0f) p= 10.0f;

		shr->alpha *= powf(d, p);
		if (shr->alpha > 1.0f)
			shr->alpha= 1.0f;
	}

	return d;
}

static void ray_fadeout_endcolor(float col[3], ShadeInput *origshi, ShadeInput *shi, ShadeResult *shr, Isect *isec, const float vec[3])
{
	/* un-intersected rays get either rendered material color or sky color */
	if (origshi->mat->fadeto_mir == MA_RAYMIR_FADETOMAT) {
		copy_v3_v3(col, shr->combined);
	}
	else if (origshi->mat->fadeto_mir == MA_RAYMIR_FADETOSKY) {
		copy_v3_v3(shi->view, vec);
		normalize_v3(shi->view);
		
		shadeSkyView(col, isec->start, shi->view, NULL, shi->thread);
		shadeSunView(col, shi->view);
	}
}

static void ray_fadeout(Isect *is, ShadeInput *shi, float col[3], const float blendcol[3], float dist_mir)
{
	/* if fading out, linear blend against fade color */
	float blendfac;

	blendfac = 1.0f - len_v3v3(shi->co, is->start)/dist_mir;
	
	col[0] = col[0]*blendfac + (1.0f - blendfac)*blendcol[0];
	col[1] = col[1]*blendfac + (1.0f - blendfac)*blendcol[1];
	col[2] = col[2]*blendfac + (1.0f - blendfac)*blendcol[2];
}

/* the main recursive tracer itself
 * note: 'col' must be initialized */
static void traceray(ShadeInput *origshi, ShadeResult *origshr, short depth, const float start[3], const float dir[3], float col[4], ObjectInstanceRen *obi, VlakRen *vlr, int traflag)
{
	ShadeInput shi = {NULL};
	Isect isec;
	float dist_mir = origshi->mat->dist_mir;

	/* with high depth the number of rays can explode due to the path splitting
	 * in two each time, giving 2^depth rays. we need to be able to cancel such
	 * a render to avoid hanging, a better solution would be random picking
	 * between directions and russian roulette termination */
	if (R.test_break(R.tbh)) {
		zero_v4(col);
		return;
	}
	
	copy_v3_v3(isec.start, start);
	copy_v3_v3(isec.dir, dir);
	isec.dist = dist_mir > 0 ? dist_mir : RE_RAYTRACE_MAXDIST;
	isec.mode= RE_RAY_MIRROR;
	isec.check = RE_CHECK_VLR_RENDER;
	isec.skip = RE_SKIP_VLR_NEIGHBOUR;
	isec.hint = NULL;

	isec.orig.ob   = obi;
	isec.orig.face = vlr;
	RE_RC_INIT(isec, shi);
	
	/* database is in original view, obi->imat transforms current position back to original */
	RE_instance_rotate_ray(origshi->obi, &isec);

	if (RE_rayobject_raycast(R.raytree, &isec)) {
		ShadeResult shr= {{0}};
		float d= 1.0f;

		RE_instance_rotate_ray_restore(origshi->obi, &isec);
		
		/* for as long we don't have proper dx/dy transform for rays we copy over original */
		copy_v3_v3(shi.dxco, origshi->dxco);
		copy_v3_v3(shi.dyco, origshi->dyco);
		
		shi.mask= origshi->mask;
		shi.osatex= origshi->osatex;
		shi.depth= origshi->depth + 1;					/* only used to indicate tracing */
		shi.thread= origshi->thread;
		//shi.sample= 0; // memset above, so don't need this
		shi.xs= origshi->xs;
		shi.ys= origshi->ys;
		shi.do_manage= origshi->do_manage;
		shi.lay= origshi->lay;
		shi.passflag= SCE_PASS_COMBINED; /* result of tracing needs no pass info */
		shi.combinedflag= 0xFFFFFF;		 /* ray trace does all options */
		//shi.do_preview = false; // memset above, so don't need this
		shi.light_override= origshi->light_override;
		shi.mat_override= origshi->mat_override;
		
		shade_ray(&isec, &shi, &shr);
		/* ray has traveled inside the material, so shade by transmission */
		if (traflag & RAY_INSIDE)
			d= shade_by_transmission(&isec, &shi, &shr);
		
		if (depth>0) {
			float fr, fg, fb, f1;

			if ((shi.mat->mode_l & MA_TRANSP) && shr.alpha < 1.0f && (shi.mat->mode_l & (MA_ZTRANSP | MA_RAYTRANSP))) {
				float nf, f, refract[3], tracol[4];
				
				tracol[0]= shi.r;
				tracol[1]= shi.g;
				tracol[2]= shi.b;
				tracol[3]= col[3];	/* we pass on and accumulate alpha */

				if ((shi.mat->mode & MA_TRANSP) && (shi.mat->mode & MA_RAYTRANSP)) {
					/* don't overwrite traflag, it's value is used in mirror reflection */
					int new_traflag = traflag;
					
					if (new_traflag & RAY_INSIDE) {
						/* inside the material, so use inverse normal */
						float norm[3];
						norm[0]= - shi.vn[0];
						norm[1]= - shi.vn[1];
						norm[2]= - shi.vn[2];

						if (refraction(refract, norm, shi.view, shi.ang)) {
							/* ray comes out from the material into air */
							new_traflag &= ~RAY_INSIDE;
						}
						else {
							/* total internal reflection (ray stays inside the material) */
							reflection(refract, norm, shi.view, shi.vn);
						}
					}
					else {
						if (refraction(refract, shi.vn, shi.view, shi.ang)) {
							/* ray goes in to the material from air */
							new_traflag |= RAY_INSIDE;
						}
						else {
							/* total external reflection (ray doesn't enter the material) */
							reflection(refract, shi.vn, shi.view, shi.vn);
						}
					}
					traceray(origshi, origshr, depth-1, shi.co, refract, tracol, shi.obi, shi.vlr, new_traflag);
				}
				else
					traceray(origshi, origshr, depth-1, shi.co, shi.view, tracol, shi.obi, shi.vlr, 0);
				
				f= shr.alpha; f1= 1.0f-f;
				nf= (shi.mat->mode & MA_RAYTRANSP) ? d * shi.mat->filter : 0.0f;
				fr= 1.0f+ nf*(shi.r-1.0f);
				fg= 1.0f+ nf*(shi.g-1.0f);
				fb= 1.0f+ nf*(shi.b-1.0f);
				shr.diff[0]= f*shr.diff[0] + f1*fr*tracol[0];
				shr.diff[1]= f*shr.diff[1] + f1*fg*tracol[1];
				shr.diff[2]= f*shr.diff[2] + f1*fb*tracol[2];
				
				shr.spec[0] *=f;
				shr.spec[1] *=f;
				shr.spec[2] *=f;

				col[3]= f1*tracol[3] + f;
			}
			else {
				col[3]= 1.0f;
			}

			float f;
			if (shi.mat->mode_l & MA_RAYMIRROR) {
				f= shi.ray_mirror;
				if (f!=0.0f) f*= fresnel_fac(shi.view, shi.vn, shi.mat->fresnel_mir_i, shi.mat->fresnel_mir);
			}
			else f= 0.0f;
			
			if (f!=0.0f) {
				float mircol[4];
				float ref[3];
				
				reflection_simple(ref, shi.vn, shi.view);
				traceray(origshi, origshr, depth-1, shi.co, ref, mircol, shi.obi, shi.vlr, traflag);
			
				f1= 1.0f-f;

				/* combine */
				//color_combine(col, f*fr*(1.0f-shr.spec[0]), f1, col, shr.diff);
				//col[0]+= shr.spec[0];
				//col[1]+= shr.spec[1];
				//col[2]+= shr.spec[2];
				
				fr= shi.mirr;
				fg= shi.mirg;
				fb= shi.mirb;
		
				col[0]= f*fr*(1.0f-shr.spec[0])*mircol[0] + f1*shr.diff[0] + shr.spec[0];
				col[1]= f*fg*(1.0f-shr.spec[1])*mircol[1] + f1*shr.diff[1] + shr.spec[1];
				col[2]= f*fb*(1.0f-shr.spec[2])*mircol[2] + f1*shr.diff[2] + shr.spec[2];
			}
			else {
				col[0]= shr.diff[0] + shr.spec[0];
				col[1]= shr.diff[1] + shr.spec[1];
				col[2]= shr.diff[2] + shr.spec[2];
			}
			
			if (dist_mir > 0.0f) {
				float blendcol[3];
				
				/* max ray distance set, but found an intersection, so fade this color
				 * out towards the sky/material color for a smooth transition */
				ray_fadeout_endcolor(blendcol, origshi, &shi, origshr, &isec, dir);
				ray_fadeout(&isec, &shi, col, blendcol, dist_mir);
			}
		}
		else {
			col[0]= shr.diff[0] + shr.spec[0];
			col[1]= shr.diff[1] + shr.spec[1];
			col[2]= shr.diff[2] + shr.spec[2];
		}
		
	}
	else {
		ray_fadeout_endcolor(col, origshi, &shi, origshr, &isec, dir);
	}
	RE_RC_MERGE(&origshi->raycounter, &shi.raycounter);
}

/* **************** jitter blocks ********** */

/* calc distributed planar energy */

static void DP_energy(float *table, float vec[2], int tot, float xsize, float ysize)
{
	int x, y, a;
	float *fp, force[3], result[3];
	float dx, dy, dist, min;
	
	min= MIN2(xsize, ysize);
	min*= min;
	result[0]= result[1]= 0.0f;
	
	for (y= -1; y<2; y++) {
		dy= ysize*y;
		for (x= -1; x<2; x++) {
			dx= xsize*x;
			fp= table;
			for (a=0; a<tot; a++, fp+= 2) {
				force[0]= vec[0] - fp[0]-dx;
				force[1]= vec[1] - fp[1]-dy;
				dist= force[0]*force[0] + force[1]*force[1];
				if (dist < min && dist>0.0f) {
					result[0]+= force[0]/dist;
					result[1]+= force[1]/dist;
				}
			}
		}
	}
	vec[0] += 0.1f*min*result[0]/(float)tot;
	vec[1] += 0.1f*min*result[1]/(float)tot;
	/* cyclic clamping */
	vec[0]= vec[0] - xsize*floorf(vec[0]/xsize + 0.5f);
	vec[1]= vec[1] - ysize*floorf(vec[1]/ysize + 0.5f);
}

/* random offset of 1 in 2 */
static void jitter_plane_offset(float *jitter1, float *jitter2, int tot, float sizex, float sizey, float ofsx, float ofsy)
{
	float dsizex= sizex*ofsx;
	float dsizey= sizey*ofsy;
	float hsizex= 0.5f*sizex, hsizey= 0.5f*sizey;
	int x;
	
	for (x=tot; x>0; x--, jitter1+=2, jitter2+=2) {
		jitter2[0]= jitter1[0] + dsizex;
		jitter2[1]= jitter1[1] + dsizey;
		if (jitter2[0] > hsizex) jitter2[0]-= sizex;
		if (jitter2[1] > hsizey) jitter2[1]-= sizey;
	}
}

/* called from convertBlenderScene.c */
/* we do this in advance to get consistent random, not alter the render seed, and be threadsafe */
void init_jitter_plane(LampRen *lar)
{
	float *fp;
	int x, tot= lar->ray_totsamp;
	
	/* test if already initialized */
	if (lar->jitter) return;
	
	/* at least 4, or max threads+1 tables */
	if (BLENDER_MAX_THREADS < 4) x= 4;
	else x= BLENDER_MAX_THREADS+1;
	fp= lar->jitter= MEM_callocN(x*tot*2*sizeof(float), "lamp jitter tab");
	
	/* if 1 sample, we leave table to be zero's */
	if (tot>1) {
		/* set per-lamp fixed seed */
		RNG *rng = BLI_rng_new_srandom(tot);
		int iter=12;

		/* fill table with random locations, area_size large */
		for (x=0; x<tot; x++, fp+=2) {
			fp[0]= (BLI_rng_get_float(rng)-0.5f)*lar->area_size;
			fp[1]= (BLI_rng_get_float(rng)-0.5f)*lar->area_sizey;
		}
		
		while (iter--) {
			fp= lar->jitter;
			for (x=tot; x>0; x--, fp+=2) {
				DP_energy(lar->jitter, fp, tot, lar->area_size, lar->area_sizey);
			}
		}

		BLI_rng_free(rng);
	}
	/* create the dithered tables (could just check lamp type!) */
	jitter_plane_offset(lar->jitter, lar->jitter+2*tot, tot, lar->area_size, lar->area_sizey, 0.5f, 0.0f);
	jitter_plane_offset(lar->jitter, lar->jitter+4*tot, tot, lar->area_size, lar->area_sizey, 0.5f, 0.5f);
	jitter_plane_offset(lar->jitter, lar->jitter+6*tot, tot, lar->area_size, lar->area_sizey, 0.0f, 0.5f);
}

/* table around origin, -0.5*size to 0.5*size */
static float *give_jitter_plane(LampRen *lar, int thread, int xs, int ys)
{
	int tot;
	
	tot= lar->ray_totsamp;
			
	if (lar->ray_samp_type & LA_SAMP_JITTER) {
		/* made it threadsafe */
		
		if (lar->xold[thread]!=xs || lar->yold[thread]!=ys) {
			jitter_plane_offset(lar->jitter, lar->jitter+2*(thread+1)*tot, tot, lar->area_size, lar->area_sizey, BLI_thread_frand(thread), BLI_thread_frand(thread));
			lar->xold[thread]= xs; 
			lar->yold[thread]= ys;
		}
		return lar->jitter+2*(thread+1)*tot;
	}
	if (lar->ray_samp_type & LA_SAMP_DITHER) {
		return lar->jitter + 2*tot*((xs & 1)+2*(ys & 1));
	}
	
	return lar->jitter;
}


/* **************** QMC sampling *************** */

static void halton_sample(double *ht_invprimes, double *ht_nums, double *v)
{
	/* incremental halton sequence generator, from:
	 * "Instant Radiosity", Keller A. */
	unsigned int i;

	for (i = 0; i < 2; i++) {
		double r = fabs((1.0 - ht_nums[i]) - 1e-10);
		
		if (ht_invprimes[i] >= r) {
			double lasth;
			double h = ht_invprimes[i];
			
			do {
				lasth = h;
				h *= ht_invprimes[i];
			} while (h >= r);
			
			ht_nums[i] += ((lasth + h) - 1.0);
		}
		else
			ht_nums[i] += ht_invprimes[i];
		
		v[i] = (float)ht_nums[i];
	}
}

/* Generate Hammersley points in [0,1)^2
 * From Lucille renderer */
static void hammersley_create(double *out, int n)
{
	double p, t;
	int k, kk;

	for (k = 0; k < n; k++) {
		t = 0;
		for (p = 0.5, kk = k; kk; p *= 0.5, kk >>= 1) {
			if (kk & 1) {		/* kk mod 2 = 1		*/
				t += p;
			}
		}
	
		out[2 * k + 0] = (double)k / (double)n;
		out[2 * k + 1] = t;
	}
}

static struct QMCSampler *QMC_initSampler(int type, int tot)
{	
	QMCSampler *qsa = MEM_callocN(sizeof(QMCSampler), "qmc sampler");
	qsa->samp2d = MEM_callocN(2*sizeof(double)*tot, "qmc sample table");

	qsa->tot = tot;
	qsa->type = type;
	
	if (qsa->type==SAMP_TYPE_HAMMERSLEY) 
		hammersley_create(qsa->samp2d, qsa->tot);
		
	return qsa;
}

static void QMC_initPixel(QMCSampler *qsa, int thread)
{
	if (qsa->type==SAMP_TYPE_HAMMERSLEY) {
		/* hammersley sequence is fixed, already created in QMCSampler init.
		 * per pixel, gets a random offset. We create separate offsets per thread, for write-safety */
		qsa->offs[thread][0] = 0.5f * BLI_thread_frand(thread);
		qsa->offs[thread][1] = 0.5f * BLI_thread_frand(thread);
	}
	else { 	/* SAMP_TYPE_HALTON */
		
		/* generate a new randomized halton sequence per pixel
		 * to alleviate qmc artifacts and make it reproducible 
		 * between threads/frames */
		double ht_invprimes[2], ht_nums[2];
		double r[2];
		int i;
	
		ht_nums[0] = BLI_thread_frand(thread);
		ht_nums[1] = BLI_thread_frand(thread);
		ht_invprimes[0] = 0.5;
		ht_invprimes[1] = 1.0/3.0;
		
		for (i=0; i< qsa->tot; i++) {
			halton_sample(ht_invprimes, ht_nums, r);
			qsa->samp2d[2*i+0] = r[0];
			qsa->samp2d[2*i+1] = r[1];
		}
	}
}

static void QMC_freeSampler(QMCSampler *qsa)
{
	MEM_freeN(qsa->samp2d);
	MEM_freeN(qsa);
}

static void QMC_getSample(double *s, QMCSampler *qsa, int thread, int num)
{
	if (qsa->type == SAMP_TYPE_HAMMERSLEY) {
		s[0] = fmod(qsa->samp2d[2*num+0] + qsa->offs[thread][0], 1.0f);
		s[1] = fmod(qsa->samp2d[2*num+1] + qsa->offs[thread][1], 1.0f);
	}
	else { /* SAMP_TYPE_HALTON */
		s[0] = qsa->samp2d[2*num+0];
		s[1] = qsa->samp2d[2*num+1];
	}
}

/* phong weighted disc using 'blur' for exponent, centred on 0,0 */
static void QMC_samplePhong(float vec[3], QMCSampler *qsa, int thread, int num, float blur)
{
	double s[2];
	float phi, pz, sqr;
	
	QMC_getSample(s, qsa, thread, num);

	phi = s[0]*2*M_PI;
	pz = pow(s[1], blur);
	sqr = sqrtf(1.0f - pz * pz);

	vec[0] = (float)(cosf(phi)*sqr);
	vec[1] = (float)(sinf(phi)*sqr);
	vec[2] = 0.0f;
}

/* rect of edge lengths sizex, sizey, centred on 0.0,0.0 i.e. ranging from -sizex/2 to +sizey/2 */
static void QMC_sampleRect(float vec[3], QMCSampler *qsa, int thread, int num, float sizex, float sizey)
{
	double s[2];

	QMC_getSample(s, qsa, thread, num);
		
	vec[0] = (float)(s[0] - 0.5) * sizex;
	vec[1] = (float)(s[1] - 0.5) * sizey;
	vec[2] = 0.0f;
}

/* disc of radius 'radius', centred on 0,0 */
static void QMC_sampleDisc(float vec[3], QMCSampler *qsa, int thread, int num, float radius)
{
	double s[2];
	float phi, sqr;
	
	QMC_getSample(s, qsa, thread, num);
	
	phi = s[0]*2*M_PI;
	sqr = sqrt(s[1]);

	vec[0] = cosf(phi)*sqr* radius/2.0f;
	vec[1] = sinf(phi)*sqr* radius/2.0f;
	vec[2] = 0.0f;
}

/* uniform hemisphere sampling */
static void QMC_sampleHemi(float vec[3], QMCSampler *qsa, int thread, int num)
{
	double s[2];
	float phi, sqr;
	
	QMC_getSample(s, qsa, thread, num);
	
	phi = s[0]*2.0*M_PI;
	sqr = sqrt(s[1]);

	vec[0] = cosf(phi)*sqr;
	vec[1] = sinf(phi)*sqr;
	vec[2] = (float)(1.0 - s[1]*s[1]);
}

#if 0 /* currently not used */
/* cosine weighted hemisphere sampling */
static void QMC_sampleHemiCosine(float vec[3], QMCSampler *qsa, int thread, int num)
{
	double s[2];
	float phi, sqr;
	
	QMC_getSample(s, qsa, thread, num);
	
	phi = s[0]*2.f*M_PI;
	sqr = s[1]*sqrt(2-s[1]*s[1]);

	vec[0] = cos(phi)*sqr;
	vec[1] = sin(phi)*sqr;
	vec[2] = 1.f - s[1]*s[1];

}
#endif

/* called from convertBlenderScene.c */
void init_render_qmcsampler(Render *re)
{
	const int num_threads = re->r.threads;
	re->qmcsamplers= MEM_callocN(sizeof(ListBase)*num_threads, "QMCListBase");
	re->num_qmc_samplers = num_threads;
}

static QMCSampler *get_thread_qmcsampler(Render *re, int thread, int type, int tot)
{
	QMCSampler *qsa;

	/* create qmc samplers as needed, since recursion makes it hard to
	 * predict how many are needed */

	for (qsa=re->qmcsamplers[thread].first; qsa; qsa=qsa->next) {
		if (qsa->type == type && qsa->tot == tot && !qsa->used) {
			qsa->used = true;
			return qsa;
		}
	}

	qsa= QMC_initSampler(type, tot);
	qsa->used = true;
	BLI_addtail(&re->qmcsamplers[thread], qsa);

	return qsa;
}

static void release_thread_qmcsampler(Render *UNUSED(re), int UNUSED(thread), QMCSampler *qsa)
{
	qsa->used= 0;
}

void free_render_qmcsampler(Render *re)
{
	if (re->qmcsamplers) {
		QMCSampler *qsa, *next;
		int a;
		for (a = 0; a < re->num_qmc_samplers; a++) {
			for (qsa=re->qmcsamplers[a].first; qsa; qsa=next) {
				next= qsa->next;
				QMC_freeSampler(qsa);
			}

			re->qmcsamplers[a].first= re->qmcsamplers[a].last= NULL;
		}

		MEM_freeN(re->qmcsamplers);
		re->qmcsamplers= NULL;
	}
}

static int adaptive_sample_variance(int samples, const float col[3], const float colsq[3], float thresh)
{
	float var[3], mean[3];

	/* scale threshold just to give a bit more precision in input rather than dealing with 
	 * tiny tiny numbers in the UI */
	thresh /= 2;
	
	mean[0] = col[0] / (float)samples;
	mean[1] = col[1] / (float)samples;
	mean[2] = col[2] / (float)samples;

	var[0] = (colsq[0] / (float)samples) - (mean[0]*mean[0]);
	var[1] = (colsq[1] / (float)samples) - (mean[1]*mean[1]);
	var[2] = (colsq[2] / (float)samples) - (mean[2]*mean[2]);
	
	if ((var[0] * 0.4f < thresh) && (var[1] * 0.3f < thresh) && (var[2] * 0.6f < thresh))
		return 1;
	else
		return 0;
}

static int adaptive_sample_contrast_val(int samples, float prev, float val, float thresh)
{
	/* if the last sample's contribution to the total value was below a small threshold
	 * (i.e. the samples taken are very similar), then taking more samples that are probably 
	 * going to be the same is wasting effort */
	if (fabsf(prev / (float)(samples - 1) - val / (float)samples ) < thresh) {
		return 1;
	}
	else
		return 0;
}

static float get_avg_speed(ShadeInput *shi)
{
	float pre_x, pre_y, post_x, post_y, speedavg;
	
	pre_x = (shi->winspeed[0] == PASS_VECTOR_MAX)?0.0f:shi->winspeed[0];
	pre_y = (shi->winspeed[1] == PASS_VECTOR_MAX)?0.0f:shi->winspeed[1];
	post_x = (shi->winspeed[2] == PASS_VECTOR_MAX)?0.0f:shi->winspeed[2];
	post_y = (shi->winspeed[3] == PASS_VECTOR_MAX)?0.0f:shi->winspeed[3];
	
	speedavg = (sqrtf(pre_x * pre_x + pre_y * pre_y) + sqrtf(post_x * post_x + post_y * post_y)) / 2.0f;
	
	return speedavg;
}

/* ***************** main calls ************** */


static void trace_refract(float col[4], ShadeInput *shi, ShadeResult *shr)
{
	QMCSampler *qsa=NULL;
	int samp_type;
	int traflag=0;
	
	float samp3d[3], orthx[3], orthy[3];
	float v_refract[3], v_refract_new[3];
	float sampcol[4], colsq[4];
	
	float blur = pow3f(1.0f - shi->mat->gloss_tra);
	short max_samples = shi->mat->samp_gloss_tra;
	float adapt_thresh = shi->mat->adapt_thresh_tra;
	
	int samples=0;
	
	colsq[0] = colsq[1] = colsq[2] = 0.0;
	col[0] = col[1] = col[2] = 0.0;
	col[3]= shr->alpha;

	if (blur > 0.0f) {
		if (adapt_thresh != 0.0f) samp_type = SAMP_TYPE_HALTON;
		else samp_type = SAMP_TYPE_HAMMERSLEY;
			
		/* all samples are generated per pixel */
		qsa = get_thread_qmcsampler(&R, shi->thread, samp_type, max_samples);
		QMC_initPixel(qsa, shi->thread);
	}
	else
		max_samples = 1;
	

	while (samples < max_samples) {
		if (refraction(v_refract, shi->vn, shi->view, shi->ang)) {
			traflag |= RAY_INSIDE;
		}
		else {
			/* total external reflection can happen for materials with IOR < 1.0 */
			if ((shi->vlr->flag & R_SMOOTH)) 
				reflection(v_refract, shi->vn, shi->view, shi->facenor);
			else
				reflection_simple(v_refract, shi->vn, shi->view);

			/* can't blur total external reflection */
			max_samples = 1;
		}
		
		if (max_samples > 1) {
			/* get a quasi-random vector from a phong-weighted disc */
			QMC_samplePhong(samp3d, qsa, shi->thread, samples, blur);
						
			ortho_basis_v3v3_v3(orthx, orthy, v_refract);
			mul_v3_fl(orthx, samp3d[0]);
			mul_v3_fl(orthy, samp3d[1]);
				
			/* and perturb the refraction vector in it */
			add_v3_v3v3(v_refract_new, v_refract, orthx);
			add_v3_v3(v_refract_new, orthy);
			
			normalize_v3(v_refract_new);
		}
		else {
			/* no blurriness, use the original normal */
			copy_v3_v3(v_refract_new, v_refract);
		}
		
		sampcol[0]= sampcol[1]= sampcol[2]= sampcol[3]= 0.0f;

		traceray(shi, shr, shi->mat->ray_depth_tra, shi->co, v_refract_new, sampcol, shi->obi, shi->vlr, traflag);
	
		col[0] += sampcol[0];
		col[1] += sampcol[1];
		col[2] += sampcol[2];
		col[3] += sampcol[3];
		
		/* for variance calc */
		colsq[0] += sampcol[0]*sampcol[0];
		colsq[1] += sampcol[1]*sampcol[1];
		colsq[2] += sampcol[2]*sampcol[2];
		
		samples++;
		
		/* adaptive sampling */
		if (adapt_thresh < 1.0f && samples > max_samples/2) {
			if (adaptive_sample_variance(samples, col, colsq, adapt_thresh))
				break;
			
			/* if the pixel so far is very dark, we can get away with less samples */
			if ( (col[0] + col[1] + col[2])/3.0f/(float)samples < 0.01f )
				max_samples--;
		}
	}
	
	col[0] /= (float)samples;
	col[1] /= (float)samples;
	col[2] /= (float)samples;
	col[3] /= (float)samples;
	
	if (qsa)
		release_thread_qmcsampler(&R, shi->thread, qsa);
}

static void trace_reflect(float col[3], ShadeInput *shi, ShadeResult *shr, float fresnelfac)
{
	QMCSampler *qsa=NULL;
	int samp_type;
	
	float samp3d[3], orthx[3], orthy[3];
	float v_nor_new[3], v_reflect[3];
	float sampcol[4], colsq[4];
		
	float blur = pow3f(1.0f - shi->mat->gloss_mir);
	short max_samples = shi->mat->samp_gloss_mir;
	float adapt_thresh = shi->mat->adapt_thresh_mir;
	float aniso = 1.0f - shi->mat->aniso_gloss_mir;
	
	int samples=0;
	
	col[0] = col[1] = col[2] = 0.0;
	colsq[0] = colsq[1] = colsq[2] = 0.0;
	
	if (blur > 0.0f) {
		if (adapt_thresh != 0.0f) samp_type = SAMP_TYPE_HALTON;
		else samp_type = SAMP_TYPE_HAMMERSLEY;
			
		/* all samples are generated per pixel */
		qsa = get_thread_qmcsampler(&R, shi->thread, samp_type, max_samples);
		QMC_initPixel(qsa, shi->thread);
	}
	else
		max_samples = 1;
	
	while (samples < max_samples) {
				
		if (max_samples > 1) {
			/* get a quasi-random vector from a phong-weighted disc */
			QMC_samplePhong(samp3d, qsa, shi->thread, samples, blur);

			/* find the normal's perpendicular plane, blurring along tangents
			 * if tangent shading enabled */
			if (shi->mat->mode & (MA_TANGENT_V)) {
				cross_v3_v3v3(orthx, shi->vn, shi->tang);      // bitangent
				copy_v3_v3(orthy, shi->tang);
				mul_v3_fl(orthx, samp3d[0]);
				mul_v3_fl(orthy, samp3d[1]*aniso);
			}
			else {
				ortho_basis_v3v3_v3(orthx, orthy, shi->vn);
				mul_v3_fl(orthx, samp3d[0]);
				mul_v3_fl(orthy, samp3d[1]);
			}

			/* and perturb the normal in it */
			add_v3_v3v3(v_nor_new, shi->vn, orthx);
			add_v3_v3(v_nor_new, orthy);
			normalize_v3(v_nor_new);
		}
		else {
			/* no blurriness, use the original normal */
			copy_v3_v3(v_nor_new, shi->vn);
		}
		
		if ((shi->vlr->flag & R_SMOOTH)) 
			reflection(v_reflect, v_nor_new, shi->view, shi->facenor);
		else
			reflection_simple(v_reflect, v_nor_new, shi->view);
		
		sampcol[0]= sampcol[1]= sampcol[2]= sampcol[3]= 0.0f;

		traceray(shi, shr, shi->mat->ray_depth, shi->co, v_reflect, sampcol, shi->obi, shi->vlr, 0);

		
		col[0] += sampcol[0];
		col[1] += sampcol[1];
		col[2] += sampcol[2];
	
		/* for variance calc */
		colsq[0] += sampcol[0]*sampcol[0];
		colsq[1] += sampcol[1]*sampcol[1];
		colsq[2] += sampcol[2]*sampcol[2];
		
		samples++;

		/* adaptive sampling */
		if (adapt_thresh > 0.0f && samples > max_samples/3) {
			if (adaptive_sample_variance(samples, col, colsq, adapt_thresh))
				break;
			
			/* if the pixel so far is very dark, we can get away with less samples */
			if ( (col[0] + col[1] + col[2])/3.0f/(float)samples < 0.01f )
				max_samples--;
		
			/* reduce samples when reflection is dim due to low ray mirror blend value or fresnel factor
			 * and when reflection is blurry */
			if (fresnelfac < 0.1f * (blur+1)) {
				max_samples--;
				
				/* even more for very dim */
				if (fresnelfac < 0.05f * (blur+1))
					max_samples--;
			}
		}
	}
	
	col[0] /= (float)samples;
	col[1] /= (float)samples;
	col[2] /= (float)samples;
	
	if (qsa)
		release_thread_qmcsampler(&R, shi->thread, qsa);
}

/* extern call from render loop */
void ray_trace(ShadeInput *shi, ShadeResult *shr)
{
	float f1, fr, fg, fb;
	float mircol[4], tracol[4];
	float diff[3];
	int do_tra, do_mir;
	
	do_tra = ((shi->mode & MA_TRANSP) && (shi->mode & MA_RAYTRANSP) && shr->alpha != 1.0f && (shi->depth <= shi->mat->ray_depth_tra));
	do_mir = ((shi->mat->mode & MA_RAYMIRROR) && shi->ray_mirror != 0.0f && (shi->depth <= shi->mat->ray_depth));
	
	/* raytrace mirror and refract like to separate the spec color */
	if (shi->combinedflag & SCE_PASS_SPEC)
		sub_v3_v3v3(diff, shr->combined, shr->spec);
	else
		copy_v3_v3(diff, shr->combined);
	
	if (do_tra) {
		float olddiff[3], f;
		
		trace_refract(tracol, shi, shr);
		
		f= shr->alpha; f1= 1.0f-f;
		fr= 1.0f+ shi->mat->filter*(shi->r-1.0f);
		fg= 1.0f+ shi->mat->filter*(shi->g-1.0f);
		fb= 1.0f+ shi->mat->filter*(shi->b-1.0f);
		
		/* for refract pass */
		copy_v3_v3(olddiff, diff);
		
		diff[0]= f*diff[0] + f1*fr*tracol[0];
		diff[1]= f*diff[1] + f1*fg*tracol[1];
		diff[2]= f*diff[2] + f1*fb*tracol[2];
		
		if (shi->passflag & SCE_PASS_REFRACT)
			sub_v3_v3v3(shr->refr, diff, olddiff);
		
		if (!(shi->combinedflag & SCE_PASS_REFRACT))
			sub_v3_v3v3(diff, diff, shr->refr);
		
		shr->alpha = min_ff(1.0f, tracol[3]);
	}
	
	if (do_mir) {
		const float i= shi->ray_mirror*fresnel_fac(shi->view, shi->vn, shi->mat->fresnel_mir_i, shi->mat->fresnel_mir);
		if (i!=0.0f) {
		
			trace_reflect(mircol, shi, shr, i);
			
			fr= i*shi->mirr;
			fg= i*shi->mirg;
			fb= i*shi->mirb;

			if (shi->passflag & SCE_PASS_REFLECT) {
				/* mirror pass is not blocked out with spec */
				shr->refl[0]= fr*mircol[0] - fr*diff[0];
				shr->refl[1]= fg*mircol[1] - fg*diff[1];
				shr->refl[2]= fb*mircol[2] - fb*diff[2];
			}
			
			if (shi->combinedflag & SCE_PASS_REFLECT) {
				/* values in shr->spec can be greater than 1.0.
				 * In this case the mircol uses a zero blending factor, so ignoring it is ok.
				 * Fixes bug #18837 - when the spec is higher then 1.0,
				 * diff can become a negative color - Campbell  */
				
				f1= 1.0f-i;
				
				diff[0] *= f1;
				diff[1] *= f1;
				diff[2] *= f1;
				
				if (shr->spec[0]<1.0f)	diff[0] += mircol[0] * (fr*(1.0f-shr->spec[0]));
				if (shr->spec[1]<1.0f)	diff[1] += mircol[1] * (fg*(1.0f-shr->spec[1]));
				if (shr->spec[2]<1.0f)	diff[2] += mircol[2] * (fb*(1.0f-shr->spec[2]));
			}
		}
	}
	/* put back together */
	if (shi->combinedflag & SCE_PASS_SPEC)
		add_v3_v3v3(shr->combined, diff, shr->spec);
	else
		copy_v3_v3(shr->combined, diff);
}

/* color 'shadfac' passes through 'col' with alpha and filter */
/* filter is only applied on alpha defined transparent part */
static void addAlphaLight(float shadfac[4], const float col[3], float alpha, float filter)
{
	float fr, fg, fb;
	
	fr= 1.0f+ filter*(col[0]-1.0f);
	fg= 1.0f+ filter*(col[1]-1.0f);
	fb= 1.0f+ filter*(col[2]-1.0f);
	
	shadfac[0]= alpha*col[0] + fr*(1.0f-alpha)*shadfac[0];
	shadfac[1]= alpha*col[1] + fg*(1.0f-alpha)*shadfac[1];
	shadfac[2]= alpha*col[2] + fb*(1.0f-alpha)*shadfac[2];
	
	shadfac[3]= (1.0f-alpha)*shadfac[3];
}

static void ray_trace_shadow_tra(Isect *is, ShadeInput *origshi, int depth, int traflag, float col[4])
{
	/* ray to lamp, find first face that intersects, check alpha properties,
	 * if it has col[3]>0.0f  continue. so exit when alpha is full */
	const float initial_dist = is->dist;

	if (RE_rayobject_raycast(R.raytree, is)) {
		/* Warning regarding initializing to zero's, This is not that nice,
		 * and possibly a bit slow for every ray, however some variables were
		 * not initialized properly in, unless using
		 * shade_input_initialize(...), we need to zero them. */
		ShadeInput shi= {NULL};
		/* end warning! - Campbell */

		ShadeResult shr;

		/* we got a face */

		shi.depth= origshi->depth + 1;					/* only used to indicate tracing */
		shi.mask= origshi->mask;
		shi.thread= origshi->thread;
		shi.passflag= SCE_PASS_COMBINED;
		shi.combinedflag= 0xFFFFFF;		 /* ray trace does all options */
	
		shi.xs= origshi->xs;
		shi.ys= origshi->ys;
		shi.do_manage= origshi->do_manage;
		shi.lay= origshi->lay;
		shi.nodes= origshi->nodes;
		
		RE_instance_rotate_ray_restore(origshi->obi, is);

		shade_ray(is, &shi, &shr);
		if (shi.mat->material_type == MA_TYPE_SURFACE) {
			const float d = (shi.mat->mode & MA_RAYTRANSP) ?
			                ((traflag & RAY_TRA) ? shade_by_transmission(is, &shi, &shr) : 1.0f) :
			                0.0f;
			/* mix colors based on shadfac (rgb + amount of light factor) */
			addAlphaLight(col, shr.diff, shr.alpha, d*shi.mat->filter);
		}
		else if (shi.mat->material_type == MA_TYPE_VOLUME) {
			const float a = col[3];
			
			col[0] = a*col[0] + shr.alpha*shr.combined[0];
			col[1] = a*col[1] + shr.alpha*shr.combined[1];
			col[2] = a*col[2] + shr.alpha*shr.combined[2];
			
			col[3] = (1.0f - shr.alpha)*a;
		}
		
		if (depth>0 && col[3]>0.0f) {
			
			/* adapt isect struct */
			copy_v3_v3(is->start, shi.co);
			is->dist = initial_dist-is->dist;
			is->orig.ob   = shi.obi;
			is->orig.face = shi.vlr;

			ray_trace_shadow_tra(is, origshi, depth-1, traflag | RAY_TRA, col);
		}
		
		RE_RC_MERGE(&origshi->raycounter, &shi.raycounter);
	}
}


/* aolight: function to create random unit sphere vectors for total random sampling */

/* calc distributed spherical energy */
static void DS_energy(float *sphere, int tot, float vec[3])
{
	float *fp, fac, force[3], res[3];
	int a;
	
	res[0]= res[1]= res[2]= 0.0f;
	
	for (a=0, fp=sphere; a<tot; a++, fp+=3) {
		sub_v3_v3v3(force, vec, fp);
		fac = dot_v3v3(force, force);
		if (fac!=0.0f) {
			fac= 1.0f/fac;
			res[0]+= fac*force[0];
			res[1]+= fac*force[1];
			res[2]+= fac*force[2];
		}
	}

	mul_v3_fl(res, 0.5);
	add_v3_v3(vec, res);
	normalize_v3(vec);
	
}

/* called from convertBlenderScene.c */
/* creates an equally distributed spherical sample pattern */
/* and allocates threadsafe memory */
void init_ao_sphere(Render *re, World *wrld)
{
	/* fixed random */
	const int num_threads = re->r.threads;
	RNG *rng;
	float *fp;
	int a, tot, iter= 16;

	/* we make twice the amount of samples, because only a hemisphere is used */
	tot= 2*wrld->aosamp*wrld->aosamp;
	
	wrld->aosphere= MEM_mallocN(3*tot*sizeof(float), "AO sphere");
	rng = BLI_rng_new_srandom(tot);
	
	/* init */
	fp= wrld->aosphere;
	for (a=0; a<tot; a++, fp+= 3) {
		BLI_rng_get_float_unit_v3(rng, fp);
	}
	
	while (iter--) {
		for (a=0, fp= wrld->aosphere; a<tot; a++, fp+= 3) {
			DS_energy(wrld->aosphere, tot, fp);
		}
	}
	
	/* tables */
	wrld->aotables= MEM_mallocN(num_threads*3*tot*sizeof(float), "AO tables");

	BLI_rng_free(rng);
}

/* give per thread a table, we have to compare xs ys because of way OSA works... */
static float *threadsafe_table_sphere(int test, int thread, int xs, int ys, int tot)
{
	static int xso[BLENDER_MAX_THREADS], yso[BLENDER_MAX_THREADS];
	static int firsttime= 1;
	
	if (firsttime) {
		memset(xso, 255, sizeof(xso));
		memset(yso, 255, sizeof(yso));
		firsttime= 0;
	}
	
	if (xs==xso[thread] && ys==yso[thread]) return R.wrld.aotables+ thread*tot*3;
	if (test) return NULL;
	xso[thread]= xs; yso[thread]= ys;
	return R.wrld.aotables+ thread*tot*3;
}

static float *sphere_sampler(int type, int resol, int thread, int xs, int ys, int reset)
{
	int tot;
	float *vec;
	
	tot= 2*resol*resol;

	if (type & WO_AORNDSMP) {
		/* total random sampling. NOT THREADSAFE! (should be removed, is not useful) */
		RNG *rng = BLI_rng_new(BLI_thread_rand(thread));
		float *sphere;
		int a;
		
		/* always returns table */
		sphere= threadsafe_table_sphere(0, thread, xs, ys, tot);

		vec= sphere;
		for (a=0; a<tot; a++, vec+=3) {
			BLI_rng_get_float_unit_v3(rng, vec);
		}

		BLI_rng_free(rng);
		
		return sphere;
	}
	else {
		float *sphere;
		float *vec1;
		
		/* returns table if xs and ys were equal to last call, and not resetting */
		sphere= (reset)? NULL: threadsafe_table_sphere(1, thread, xs, ys, tot);
		if (sphere==NULL) {
			float cosfi, sinfi, cost, sint;
			float ang;
			int a;

			sphere= threadsafe_table_sphere(0, thread, xs, ys, tot);
			
			/* random rotation */
			ang = BLI_thread_frand(thread);
			sinfi = sinf(ang); cosfi = cosf(ang);
			ang = BLI_thread_frand(thread);
			sint = sinf(ang); cost = cosf(ang);
			
			vec= R.wrld.aosphere;
			vec1= sphere;
			for (a=0; a<tot; a++, vec+=3, vec1+=3) {
				vec1[0]= cost*cosfi*vec[0] - sinfi*vec[1] + sint*cosfi*vec[2];
				vec1[1]= cost*sinfi*vec[0] + cosfi*vec[1] + sint*sinfi*vec[2];
				vec1[2]= -sint*vec[0] + cost*vec[2];
			}
		}
		return sphere;
	}
}

static void ray_ao_qmc(ShadeInput *shi, float ao[3], float env[3])
{
	Isect isec;
	RayHint point_hint;
	QMCSampler *qsa=NULL;
	float samp3d[3];
	float up[3], side[3], dir[3], nrm[3];
	
	float maxdist = R.wrld.aodist;
	float fac=0.0f, prev=0.0f;
	float adapt_thresh = R.wrld.ao_adapt_thresh;
	float adapt_speed_fac = R.wrld.ao_adapt_speed_fac;
	
	int samples=0;
	int max_samples = R.wrld.aosamp*R.wrld.aosamp;
	
	float dxyview[3], skyadded=0;
	int envcolor;
	
	RE_RC_INIT(isec, *shi);
	isec.orig.ob   = shi->obi;
	isec.orig.face = shi->vlr;
	isec.check = RE_CHECK_VLR_NON_SOLID_MATERIAL;
	isec.skip = RE_SKIP_VLR_NEIGHBOUR;
	isec.hint = NULL;

	isec.hit.ob   = NULL;
	isec.hit.face = NULL;

	isec.last_hit = NULL;
	
	isec.mode= (R.wrld.aomode & WO_AODIST)?RE_RAY_SHADOW_TRA:RE_RAY_SHADOW;
	isec.lay= -1;
	
	copy_v3_v3(isec.start, shi->co);
	
	RE_instance_rotate_ray_start(shi->obi, &isec);
	
	RE_rayobject_hint_bb(R.raytree, &point_hint, isec.start, isec.start);
	isec.hint = &point_hint;

	zero_v3(ao);
	zero_v3(env);
	
	/* prevent sky colors to be added for only shadow (shadow becomes alpha) */
	envcolor= R.wrld.aocolor;
	if (shi->mat->mode & MA_ONLYSHADOW)
		envcolor= WO_AOPLAIN;
	
	if (envcolor == WO_AOSKYTEX) {
		dxyview[0]= 1.0f/(float)R.wrld.aosamp;
		dxyview[1]= 1.0f/(float)R.wrld.aosamp;
		dxyview[2]= 0.0f;
	}
	
	if (shi->vlr->flag & R_SMOOTH) {
		copy_v3_v3(nrm, shi->vn);
	}
	else {
		copy_v3_v3(nrm, shi->facenor);
	}

	ortho_basis_v3v3_v3(up, side, nrm);
	
	/* sampling init */
	if (R.wrld.ao_samp_method==WO_AOSAMP_HALTON) {
		float speedfac;
		
		speedfac = get_avg_speed(shi) * adapt_speed_fac;
		CLAMP(speedfac, 1.0f, 1000.0f);
		max_samples /= speedfac;
		if (max_samples < 5) max_samples = 5;
		
		qsa = get_thread_qmcsampler(&R, shi->thread, SAMP_TYPE_HALTON, max_samples);
	}
	else if (R.wrld.ao_samp_method==WO_AOSAMP_HAMMERSLEY)
		qsa = get_thread_qmcsampler(&R, shi->thread, SAMP_TYPE_HAMMERSLEY, max_samples);

	QMC_initPixel(qsa, shi->thread);
	
	while (samples < max_samples) {

		/* sampling, returns quasi-random vector in unit hemisphere */
		QMC_sampleHemi(samp3d, qsa, shi->thread, samples);

		dir[0] = (samp3d[0]*up[0] + samp3d[1]*side[0] + samp3d[2]*nrm[0]);
		dir[1] = (samp3d[0]*up[1] + samp3d[1]*side[1] + samp3d[2]*nrm[1]);
		dir[2] = (samp3d[0]*up[2] + samp3d[1]*side[2] + samp3d[2]*nrm[2]);
		
		normalize_v3(dir);
		
		isec.dir[0] = -dir[0];
		isec.dir[1] = -dir[1];
		isec.dir[2] = -dir[2];
		isec.dist = maxdist;
		
		RE_instance_rotate_ray_dir(shi->obi, &isec);
		
		prev = fac;
		
		if (RE_rayobject_raycast(R.raytree, &isec)) {
			if (R.wrld.aomode & WO_AODIST) fac+= expf(-isec.dist*R.wrld.aodistfac);
			else fac+= 1.0f;
		}
		else if (envcolor!=WO_AOPLAIN) {
			float skycol[4];
			float view[3];
			
			view[0]= -dir[0];
			view[1]= -dir[1];
			view[2]= -dir[2];
			normalize_v3(view);
			
			if (envcolor==WO_AOSKYCOL) {
				const float skyfac= 0.5f * (1.0f + dot_v3v3(view, R.grvec));
				env[0]+= (1.0f-skyfac)*R.wrld.horr + skyfac*R.wrld.zenr;
				env[1]+= (1.0f-skyfac)*R.wrld.horg + skyfac*R.wrld.zeng;
				env[2]+= (1.0f-skyfac)*R.wrld.horb + skyfac*R.wrld.zenb;
			}
			else {	/* WO_AOSKYTEX */
				shadeSkyView(skycol, isec.start, view, dxyview, shi->thread);
				shadeSunView(skycol, shi->view);
				env[0]+= skycol[0];
				env[1]+= skycol[1];
				env[2]+= skycol[2];
			}
			skyadded++;
		}
		
		samples++;
		
		if (qsa && qsa->type == SAMP_TYPE_HALTON) {
			/* adaptive sampling - consider samples below threshold as in shadow (or vice versa) and exit early */
			if (adapt_thresh > 0.0f && (samples > max_samples/2) ) {
				
				if (adaptive_sample_contrast_val(samples, prev, fac, adapt_thresh)) {
					break;
				}
			}
		}
	}
	
	/* average color times distances/hits formula */
	ao[0]= ao[1]= ao[2]= 1.0f - fac/(float)samples;

	if (envcolor!=WO_AOPLAIN && skyadded)
		mul_v3_fl(env, (1.0f - fac/(float)samples)/((float)skyadded));
	else
		copy_v3_v3(env, ao);
	
	if (qsa)
		release_thread_qmcsampler(&R, shi->thread, qsa);
}

/* extern call from shade_lamp_loop, ambient occlusion calculus */
static void ray_ao_spheresamp(ShadeInput *shi, float ao[3], float env[3])
{
	Isect isec;
	RayHint point_hint;
	float *vec, *nrm, bias, sh=0.0f;
	float maxdist = R.wrld.aodist;
	float dxyview[3];
	int j= -1, tot, actual=0, skyadded=0, envcolor, resol= R.wrld.aosamp;
	
	RE_RC_INIT(isec, *shi);
	isec.orig.ob   = shi->obi;
	isec.orig.face = shi->vlr;
	isec.check = RE_CHECK_VLR_RENDER;
	isec.skip = RE_SKIP_VLR_NEIGHBOUR;
	isec.hint = NULL;

	isec.hit.ob   = NULL;
	isec.hit.face = NULL;
	
	isec.last_hit = NULL;
	
	isec.mode= (R.wrld.aomode & WO_AODIST)?RE_RAY_SHADOW_TRA:RE_RAY_SHADOW;
	isec.lay= -1;

	copy_v3_v3(isec.start, shi->co);
	RE_instance_rotate_ray_start(shi->obi, &isec);

	RE_rayobject_hint_bb(R.raytree, &point_hint, isec.start, isec.start);
	isec.hint = &point_hint;

	zero_v3(ao);
	zero_v3(env);

	/* bias prevents smoothed faces to appear flat */
	if (shi->vlr->flag & R_SMOOTH) {
		bias= R.wrld.aobias;
		nrm= shi->vn;
	}
	else {
		bias= 0.0f;
		nrm= shi->facenor;
	}

	/* prevent sky colors to be added for only shadow (shadow becomes alpha) */
	envcolor= R.wrld.aocolor;
	if (shi->mat->mode & MA_ONLYSHADOW)
		envcolor= WO_AOPLAIN;
	
	if (resol>32) resol= 32;

	/* get sphere samples. for faces we get the same samples for sample x/y values,
	 * for strand render we always require a new sampler because x/y are not set */
	vec= sphere_sampler(R.wrld.aomode, resol, shi->thread, shi->xs, shi->ys, shi->strand != NULL);
	
	/* warning: since we use full sphere now, and dotproduct is below, we do twice as much */
	tot= 2*resol*resol;

	if (envcolor == WO_AOSKYTEX) {
		dxyview[0]= 1.0f/(float)resol;
		dxyview[1]= 1.0f/(float)resol;
		dxyview[2]= 0.0f;
	}
	
	while (tot--) {
		
		if (dot_v3v3(vec, nrm) > bias) {
			/* only ao samples for mask */
			if (R.r.mode & R_OSA) {
				j++;
				if (j==R.osa) j= 0;
				if (!(shi->mask & (1<<j))) {
					vec+=3;
					continue;
				}
			}
			
			actual++;
			
			/* always set start/vec/dist */
			isec.dir[0] = -vec[0];
			isec.dir[1] = -vec[1];
			isec.dir[2] = -vec[2];
			isec.dist = maxdist;
			
			RE_instance_rotate_ray_dir(shi->obi, &isec);

			/* do the trace */
			if (RE_rayobject_raycast(R.raytree, &isec)) {
				if (R.wrld.aomode & WO_AODIST) sh+= expf(-isec.dist*R.wrld.aodistfac);
				else sh+= 1.0f;
			}
			else if (envcolor!=WO_AOPLAIN) {
				float skycol[4];
				float view[3];
				
				view[0]= -vec[0];
				view[1]= -vec[1];
				view[2]= -vec[2];
				normalize_v3(view);
				
				if (envcolor==WO_AOSKYCOL) {
					const float fac = 0.5f * (1.0f + dot_v3v3(view, R.grvec));
					env[0]+= (1.0f-fac)*R.wrld.horr + fac*R.wrld.zenr;
					env[1]+= (1.0f-fac)*R.wrld.horg + fac*R.wrld.zeng;
					env[2]+= (1.0f-fac)*R.wrld.horb + fac*R.wrld.zenb;
				}
				else {	/* WO_AOSKYTEX */
					shadeSkyView(skycol, isec.start, view, dxyview, shi->thread);
					shadeSunView(skycol, shi->view);
					env[0]+= skycol[0];
					env[1]+= skycol[1];
					env[2]+= skycol[2];
				}
				skyadded++;
			}
		}
		/* samples */
		vec+= 3;
	}
	
	if (actual==0) sh= 1.0f;
	else sh = 1.0f - sh/((float)actual);
	
	/* average color times distances/hits formula */
	ao[0]= ao[1]= ao[2]= sh;

	if (envcolor!=WO_AOPLAIN && skyadded)
		mul_v3_fl(env, sh/((float)skyadded));
	else
		copy_v3_v3(env, ao);
}

void ray_ao(ShadeInput *shi, float ao[3], float env[3])
{
	/* Unfortunately, the unusual way that the sphere sampler calculates roughly twice as many
	 * samples as are actually traced, and skips them based on bias and OSA settings makes it very difficult
	 * to reuse code between these two functions. This is the easiest way I can think of to do it
	 * --broken */
	if (ELEM(R.wrld.ao_samp_method, WO_AOSAMP_HAMMERSLEY, WO_AOSAMP_HALTON))
		ray_ao_qmc(shi, ao, env);
	else if (R.wrld.ao_samp_method == WO_AOSAMP_CONSTANT)
		ray_ao_spheresamp(shi, ao, env);
}

static void ray_shadow_jittered_coords(ShadeInput *shi, int max, float jitco[RE_MAX_OSA][3], int *totjitco)
{
	/* magic numbers for reordering sample positions to give better
	 * results with adaptive sample, when it usually only takes 4 samples */
	int order8[8] = {0, 1, 5, 6, 2, 3, 4, 7};
	int order11[11] = {1, 3, 8, 10, 0, 2, 4, 5, 6, 7, 9};
	int order16[16] = {1, 3, 9, 12, 0, 6, 7, 8, 13, 2, 4, 5, 10, 11, 14, 15};
	int count = count_mask(shi->mask);

	/* for better antialising shadow samples are distributed over the subpixel
	 * sample coordinates, this only works for raytracing depth 0 though */
	if (!shi->strand && shi->depth == 0 && count > 1 && count <= max) {
		float xs, ys, zs, view[3];
		int samp, ordsamp, tot= 0;

		for (samp=0; samp<R.osa; samp++) {
			if (R.osa == 8) ordsamp = order8[samp];
			else if (R.osa == 11) ordsamp = order11[samp];
			else if (R.osa == 16) ordsamp = order16[samp];
			else ordsamp = samp;

			if (shi->mask & (1<<ordsamp)) {
				/* zbuffer has this inverse corrected, ensures xs,ys are inside pixel */
				xs= (float)shi->scanco[0] + R.jit[ordsamp][0] + 0.5f;
				ys= (float)shi->scanco[1] + R.jit[ordsamp][1] + 0.5f;
				zs= shi->scanco[2];

				shade_input_calc_viewco(shi, xs, ys, zs, view, NULL, jitco[tot], NULL, NULL);
				tot++;
			}
		}

		*totjitco= tot;
	}
	else {
		copy_v3_v3(jitco[0], shi->co);
		*totjitco= 1;
	}
}

static void ray_shadow_qmc(ShadeInput *shi, LampRen *lar, const float lampco[3], float shadfac[4], Isect *isec)
{
	QMCSampler *qsa=NULL;
	int samples=0;
	float samp3d[3];

	float fac=0.0f, vec[3], end[3];
	float colsq[4];
	float adapt_thresh = lar->adapt_thresh;
	int min_adapt_samples=4, max_samples = lar->ray_totsamp;
	float start[3];
	bool do_soft = true, full_osa = false;
	int i;

	float min[3], max[3];
	RayHint bb_hint;

	float jitco[RE_MAX_OSA][3];
	int totjitco;

	colsq[0] = colsq[1] = colsq[2] = 0.0;
	if (isec->mode==RE_RAY_SHADOW_TRA) {
		shadfac[0]= shadfac[1]= shadfac[2]= shadfac[3]= 0.0f;
	}
	else
		shadfac[3]= 1.0f;
	
	if (lar->ray_totsamp < 2) do_soft = false;
	if ((R.r.mode & R_OSA) && (R.osa > 0) && (shi->vlr->flag & R_FULL_OSA)) full_osa = true;
	
	if (full_osa) {
		if (do_soft) max_samples  = max_samples/R.osa + 1;
		else max_samples = 1;
	}
	else {
		if (do_soft) max_samples = lar->ray_totsamp;
		else if (shi->depth == 0) max_samples = (R.osa > 4)?R.osa:5;
		else max_samples = 1;
	}
	
	ray_shadow_jittered_coords(shi, max_samples, jitco, &totjitco);

	/* sampling init */
	if (lar->ray_samp_method==LA_SAMP_HALTON)
		qsa = get_thread_qmcsampler(&R, shi->thread, SAMP_TYPE_HALTON, max_samples);
	else if (lar->ray_samp_method==LA_SAMP_HAMMERSLEY)
		qsa = get_thread_qmcsampler(&R, shi->thread, SAMP_TYPE_HAMMERSLEY, max_samples);
	
	QMC_initPixel(qsa, shi->thread);

	INIT_MINMAX(min, max);
	for (i = 0; i < totjitco; i++) {
		minmax_v3v3_v3(min, max, jitco[i]);
	}
	if (shi->obi->flag & R_ENV_TRANSFORMED) {
		mul_m4_v3(shi->obi->imat, min);
		mul_m4_v3(shi->obi->imat, max);
	}
	RE_rayobject_hint_bb(R.raytree, &bb_hint, min, max);
	
	isec->hint = &bb_hint;
	isec->check = RE_CHECK_VLR_RENDER;
	isec->skip = RE_SKIP_VLR_NEIGHBOUR;
	copy_v3_v3(vec, lampco);
	
	while (samples < max_samples) {

		isec->orig.ob   = shi->obi;
		isec->orig.face = shi->vlr;

		/* manually jitter the start shading co-ord per sample
		 * based on the pre-generated OSA texture sampling offsets, 
		 * for anti-aliasing sharp shadow edges. */
		copy_v3_v3(start, jitco[samples % totjitco]);

		if (do_soft) {
			/* sphere shadow source */
			if (lar->type == LA_LOCAL) {
				float ru[3], rv[3], v[3], s[3];
				
				/* calc tangent plane vectors */
				sub_v3_v3v3(v, start, lampco);
				normalize_v3(v);
				ortho_basis_v3v3_v3(ru, rv, v);
				
				/* sampling, returns quasi-random vector in area_size disc */
				QMC_sampleDisc(samp3d, qsa, shi->thread, samples, lar->area_size);

				/* distribute disc samples across the tangent plane */
				s[0] = samp3d[0]*ru[0] + samp3d[1]*rv[0];
				s[1] = samp3d[0]*ru[1] + samp3d[1]*rv[1];
				s[2] = samp3d[0]*ru[2] + samp3d[1]*rv[2];
				
				copy_v3_v3(samp3d, s);
			}
			else {
				/* sampling, returns quasi-random vector in [sizex,sizey]^2 plane */
				QMC_sampleRect(samp3d, qsa, shi->thread, samples, lar->area_size, lar->area_sizey);
								
				/* align samples to lamp vector */
				mul_m3_v3(lar->mat, samp3d);
			}
			end[0] = vec[0]+samp3d[0];
			end[1] = vec[1]+samp3d[1];
			end[2] = vec[2]+samp3d[2];
		}
		else {
			copy_v3_v3(end, vec);
		}

		if (shi->strand) {
			/* bias away somewhat to avoid self intersection */
			float jitbias= 0.5f*(len_v3(shi->dxco) + len_v3(shi->dyco));
			float v[3];

			sub_v3_v3v3(v, start, end);
			normalize_v3(v);

			start[0] -= jitbias*v[0];
			start[1] -= jitbias*v[1];
			start[2] -= jitbias*v[2];
		}
		
		copy_v3_v3(isec->start, start);
		sub_v3_v3v3(isec->dir, end, start);
		isec->dist = normalize_v3(isec->dir);
		
		RE_instance_rotate_ray(shi->obi, isec);

		/* trace the ray */
		if (isec->mode==RE_RAY_SHADOW_TRA) {
			float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
			
			ray_trace_shadow_tra(isec, shi, DEPTH_SHADOW_TRA, 0, col);
			shadfac[0] += col[0];
			shadfac[1] += col[1];
			shadfac[2] += col[2];
			shadfac[3] += col[3];
			
			/* for variance calc */
			colsq[0] += col[0]*col[0];
			colsq[1] += col[1]*col[1];
			colsq[2] += col[2]*col[2];
		}
		else {
			if ( RE_rayobject_raycast(R.raytree, isec) ) fac+= 1.0f;
		}
		
		samples++;
		
		if (lar->ray_samp_method == LA_SAMP_HALTON) {
		
			/* adaptive sampling - consider samples below threshold as in shadow (or vice versa) and exit early */
			if ((max_samples > min_adapt_samples) && (adapt_thresh > 0.0f) && (samples > max_samples / 3)) {
				if (isec->mode==RE_RAY_SHADOW_TRA) {
					if ((shadfac[3] / samples > (1.0f-adapt_thresh)) || (shadfac[3] / samples < adapt_thresh))
						break;
					else if (adaptive_sample_variance(samples, shadfac, colsq, adapt_thresh))
						break;
				}
				else {
					if ((fac / samples > (1.0f-adapt_thresh)) || (fac / samples < adapt_thresh))
						break;
				}
			}
		}
	}
	
	if (isec->mode==RE_RAY_SHADOW_TRA) {
		shadfac[0] /= samples;
		shadfac[1] /= samples;
		shadfac[2] /= samples;
		shadfac[3] /= samples;
	}
	else
		shadfac[3]= 1.0f-fac/samples;

	if (qsa)
		release_thread_qmcsampler(&R, shi->thread, qsa);
}

static void ray_shadow_jitter(ShadeInput *shi, LampRen *lar, const float lampco[3], float shadfac[4], Isect *isec)
{
	/* area soft shadow */
	const float *jitlamp;
	float fac=0.0f, div=0.0f, vec[3];
	int a, j= -1, mask;
	RayHint point_hint;
	
	if (isec->mode==RE_RAY_SHADOW_TRA) {
		shadfac[0]= shadfac[1]= shadfac[2]= shadfac[3]= 0.0f;
	}
	else shadfac[3]= 1.0f;
	
	fac= 0.0f;
	jitlamp= give_jitter_plane(lar, shi->thread, shi->xs, shi->ys);

	a= lar->ray_totsamp;
	
	/* this correction to make sure we always take at least 1 sample */
	mask= shi->mask;
	if (a==4) mask |= (mask>>4)|(mask>>8);
	else if (a==9) mask |= (mask>>9);
	
	copy_v3_v3(isec->start, shi->co);
	RE_instance_rotate_ray_start(shi->obi, isec);
	
	isec->orig.ob   = shi->obi;
	isec->orig.face = shi->vlr;
	RE_rayobject_hint_bb(R.raytree, &point_hint, isec->start, isec->start);
	isec->hint = &point_hint;
	
	while (a--) {
		
		if (R.r.mode & R_OSA) {
			j++;
			if (j>=R.osa) j= 0;
			if (!(mask & (1<<j))) {
				jitlamp+= 2;
				continue;
			}
		}
		
		vec[0]= jitlamp[0];
		vec[1]= jitlamp[1];
		vec[2]= 0.0f;
		mul_m3_v3(lar->mat, vec);
		
		/* set start and vec */
		isec->dir[0] = vec[0]+lampco[0]-shi->co[0];
		isec->dir[1] = vec[1]+lampco[1]-shi->co[1];
		isec->dir[2] = vec[2]+lampco[2]-shi->co[2];
		
		RE_instance_rotate_ray_dir(shi->obi, isec);
		
		isec->dist = 1.0f;
		isec->check = RE_CHECK_VLR_RENDER;
		isec->skip = RE_SKIP_VLR_NEIGHBOUR;
		
		if (isec->mode==RE_RAY_SHADOW_TRA) {
			/* isec.col is like shadfac, so defines amount of light (0.0 is full shadow) */
			float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
			
			ray_trace_shadow_tra(isec, shi, DEPTH_SHADOW_TRA, 0, col);
			shadfac[0] += col[0];
			shadfac[1] += col[1];
			shadfac[2] += col[2];
			shadfac[3] += col[3];
		}
		else if ( RE_rayobject_raycast(R.raytree, isec) ) fac+= 1.0f;
		
		div+= 1.0f;
		jitlamp+= 2;
	}
	
	if (isec->mode==RE_RAY_SHADOW_TRA) {
		shadfac[0] /= div;
		shadfac[1] /= div;
		shadfac[2] /= div;
		shadfac[3] /= div;
	}
	else {
		/* sqrt makes nice umbra effect */
		if (lar->ray_samp_type & LA_SAMP_UMBRA)
			shadfac[3] = sqrtf(1.0f - fac / div);
		else
			shadfac[3] = 1.0f - fac / div;
	}
}
/* extern call from shade_lamp_loop */
void ray_shadow(ShadeInput *shi, LampRen *lar, float shadfac[4])
{
	Isect isec;
	float lampco[3];

	/* setup isec */
	RE_RC_INIT(isec, *shi);
	if (shi->mat->mode & MA_SHADOW_TRA) isec.mode= RE_RAY_SHADOW_TRA;
	else isec.mode= RE_RAY_SHADOW;
	isec.hint = NULL;
	
	if (lar->mode & (LA_LAYER|LA_LAYER_SHADOW))
		isec.lay= lar->lay;
	else
		isec.lay= -1;

	/* only when not mir tracing, first hit optimm */
	if (shi->depth==0) {
		isec.last_hit = lar->last_hit[shi->thread];
	}
	else {
		isec.last_hit = NULL;
	}
	
	if (lar->type==LA_SUN || lar->type==LA_HEMI) {
		/* jitter and QMC sampling add a displace vector to the lamp position
		 * that's incorrect because a SUN lamp does not has an exact position
		 * and the displace should be done at the ray vector instead of the
		 * lamp position.
		 * This is easily verified by noticing that shadows of SUN lights change
		 * with the scene BB.
		 * 
		 * This was detected during SoC 2009 - Raytrace Optimization, but to keep
		 * consistency with older render code it wasn't removed.
		 * 
		 * If the render code goes through some recode/serious bug-fix then this
		 * is something to consider!
		 */
		lampco[0]= shi->co[0] - R.maxdist*lar->vec[0];
		lampco[1]= shi->co[1] - R.maxdist*lar->vec[1];
		lampco[2]= shi->co[2] - R.maxdist*lar->vec[2];
	}
	else {
		copy_v3_v3(lampco, lar->co);
	}
	
	if (ELEM(lar->ray_samp_method, LA_SAMP_HALTON, LA_SAMP_HAMMERSLEY)) {
		
		ray_shadow_qmc(shi, lar, lampco, shadfac, &isec);
		
	}
	else {
		if (lar->ray_totsamp<2) {
			
			isec.orig.ob   = shi->obi;
			isec.orig.face = shi->vlr;
			
			shadfac[3]= 1.0f;  /* 1.0=full light */

			/* set up isec.dir */
			copy_v3_v3(isec.start, shi->co);
			sub_v3_v3v3(isec.dir, lampco, isec.start);
			isec.dist = normalize_v3(isec.dir);

			RE_instance_rotate_ray(shi->obi, &isec);

			if (isec.mode==RE_RAY_SHADOW_TRA) {
				/* isec.col is like shadfac, so defines amount of light (0.0 is full shadow) */
				float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};

				ray_trace_shadow_tra(&isec, shi, DEPTH_SHADOW_TRA, 0, col);
				copy_v4_v4(shadfac, col);
			}
			else if (RE_rayobject_raycast(R.raytree, &isec))
				shadfac[3]= 0.0f;
		}
		else {
			ray_shadow_jitter(shi, lar, lampco, shadfac, &isec);
		}
	}
		
	/* for first hit optim, set last interesected shadow face */
	if (shi->depth==0) {
		lar->last_hit[shi->thread] = isec.last_hit;
	}

}

