/* 
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/occlusion.c
 *  \ingroup render
 */


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_memarena.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_scene.h"


#include "RE_shader_ext.h"

/* local includes */
#include "occlusion.h"
#include "render_types.h"
#include "rendercore.h"
#include "renderdatabase.h"
#include "pixelshading.h"
#include "shading.h"
#include "zbuf.h"

/* ------------------------- Declarations --------------------------- */

#define INVALID_INDEX ((int)(~0))
#define INVPI 0.31830988618379069f
#define TOTCHILD 8
#define CACHE_STEP 3

typedef struct OcclusionCacheSample {
	float co[3], n[3], ao[3], env[3], indirect[3], intensity, dist2;
	int x, y, filled;
} OcclusionCacheSample;

typedef struct OcclusionCache {
	OcclusionCacheSample *sample;
	int x, y, w, h, step;
} OcclusionCache;

typedef struct OccFace {
	int obi;
	int facenr;
} OccFace;

typedef struct OccNode {
	float co[3], area;
	float sh[9], dco;
	float occlusion, rad[3];
	int childflag;
	union {
		//OccFace face;
		int face;
		struct OccNode *node;
	} child[TOTCHILD];
} OccNode;

typedef struct OcclusionTree {
	MemArena *arena;

	float (*co)[3];		/* temporary during build */

	OccFace *face;		/* instance and face indices */
	float *occlusion;	/* occlusion for faces */
	float (*rad)[3];	/* radiance for faces */
	
	OccNode *root;

	OccNode **stack[BLENDER_MAX_THREADS];
	int maxdepth;

	int totface;

	float error;
	float distfac;

	int dothreadedbuild;
	int totbuildthread;
	int doindirect;

	OcclusionCache *cache;
} OcclusionTree;

typedef struct OcclusionThread {
	Render *re;
	StrandSurface *mesh;
	float (*faceao)[3];
	float (*faceenv)[3];
	float (*faceindirect)[3];
	int begin, end;
	int thread;
} OcclusionThread;

typedef struct OcclusionBuildThread {
	OcclusionTree *tree;
	int begin, end, depth;
	OccNode *node;
} OcclusionBuildThread;

/* ------------------------- Shading --------------------------- */

extern Render R; // meh

static void occ_shade(ShadeSample *ssamp, ObjectInstanceRen *obi, VlakRen *vlr, float *rad)
{
	ShadeInput *shi= ssamp->shi;
	ShadeResult *shr= ssamp->shr;
	float l, u, v, *v1, *v2, *v3;
	
	/* init */
	if (vlr->v4) {
		shi->u= u= 0.5f;
		shi->v= v= 0.5f;
	}
	else {
		shi->u= u= 1.0f/3.0f;
		shi->v= v= 1.0f/3.0f;
	}

	/* setup render coordinates */
	v1= vlr->v1->co;
	v2= vlr->v2->co;
	v3= vlr->v3->co;
	
	/* renderco */
	l= 1.0f-u-v;
	
	shi->co[0]= l*v3[0]+u*v1[0]+v*v2[0];
	shi->co[1]= l*v3[1]+u*v1[1]+v*v2[1];
	shi->co[2]= l*v3[2]+u*v1[2]+v*v2[2];

	shade_input_set_triangle_i(shi, obi, vlr, 0, 1, 2);

	/* set up view vector */
	copy_v3_v3(shi->view, shi->co);
	normalize_v3(shi->view);
	
	/* cache for shadow */
	shi->samplenr++;
	
	shi->xs= 0; // TODO
	shi->ys= 0;
	
	shade_input_set_normals(shi);

	/* no normal flip */
	if (shi->flippednor)
		shade_input_flip_normals(shi);

	madd_v3_v3fl(shi->co, shi->vn, 0.0001f); /* ugly.. */

	/* not a pretty solution, but fixes common cases */
	if (shi->obr->ob && shi->obr->ob->transflag & OB_NEG_SCALE) {
		negate_v3(shi->vn);
		negate_v3(shi->vno);
		negate_v3(shi->nmapnorm);
	}

	/* init material vars */
	// note, keep this synced with render_types.h
	memcpy(&shi->r, &shi->mat->r, 23*sizeof(float));
	shi->har= shi->mat->har;
	
	/* render */
	shade_input_set_shade_texco(shi);
	shade_material_loop(shi, shr); /* todo: nodes */
	
	copy_v3_v3(rad, shr->combined);
}

static void occ_build_shade(Render *re, OcclusionTree *tree)
{
	ShadeSample ssamp;
	ObjectInstanceRen *obi;
	VlakRen *vlr;
	int a;

	R= *re;

	/* setup shade sample with correct passes */
	memset(&ssamp, 0, sizeof(ShadeSample));
	ssamp.shi[0].lay= re->lay;
	ssamp.shi[0].passflag= SCE_PASS_DIFFUSE|SCE_PASS_RGBA;
	ssamp.shi[0].combinedflag= ~(SCE_PASS_SPEC);
	ssamp.tot= 1;

	for (a=0; a<tree->totface; a++) {
		obi= &R.objectinstance[tree->face[a].obi];
		vlr= RE_findOrAddVlak(obi->obr, tree->face[a].facenr);

		occ_shade(&ssamp, obi, vlr, tree->rad[a]);
	}
}

/* ------------------------- Spherical Harmonics --------------------------- */

/* Use 2nd order SH => 9 coefficients, stored in this order:
 * 0 = (0,0),
 * 1 = (1,-1), 2 = (1,0), 3 = (1,1),
 * 4 = (2,-2), 5 = (2,-1), 6 = (2,0), 7 = (2,1), 8 = (2,2) */

static void sh_copy(float *shresult, float *sh)
{
	memcpy(shresult, sh, sizeof(float)*9);
}

static void sh_mul(float *sh, float f)
{
	int i;

	for (i=0; i<9; i++)
		sh[i] *= f;
}

static void sh_add(float *shresult, float *sh1, float *sh2)
{
	int i;

	for (i=0; i<9; i++)
		shresult[i]= sh1[i] + sh2[i];
}

static void sh_from_disc(float *n, float area, float *shresult)
{
	/* See formula (3) in:
	 * "An Efficient Representation for Irradiance Environment Maps" */
	float sh[9], x, y, z;

	x= n[0];
	y= n[1];
	z= n[2];

	sh[0]= 0.282095f;

	sh[1]= 0.488603f*y;
	sh[2]= 0.488603f*z;
	sh[3]= 0.488603f*x;
	
	sh[4]= 1.092548f*x*y;
	sh[5]= 1.092548f*y*z;
	sh[6]= 0.315392f*(3.0f*z*z - 1.0f);
	sh[7]= 1.092548f*x*z;
	sh[8]= 0.546274f*(x*x - y*y);

	sh_mul(sh, area);
	sh_copy(shresult, sh);
}

static float sh_eval(float *sh, float *v)
{
	/* See formula (13) in:
	 * "An Efficient Representation for Irradiance Environment Maps" */
	static const float c1 = 0.429043f, c2 = 0.511664f, c3 = 0.743125f;
	static const float c4 = 0.886227f, c5 = 0.247708f;
	float x, y, z, sum;

	x= v[0];
	y= v[1];
	z= v[2];

	sum= c1*sh[8]*(x*x - y*y);
	sum += c3*sh[6]*z*z;
	sum += c4*sh[0];
	sum += -c5*sh[6];
	sum += 2.0f*c1*(sh[4]*x*y + sh[7]*x*z + sh[5]*y*z);
	sum += 2.0f*c2*(sh[3]*x + sh[1]*y + sh[2]*z);

	return sum;
}

/* ------------------------------ Building --------------------------------- */

static void occ_face(const OccFace *face, float co[3], float normal[3], float *area)
{
	ObjectInstanceRen *obi;
	VlakRen *vlr;
	float v1[3], v2[3], v3[3], v4[3];

	obi= &R.objectinstance[face->obi];
	vlr= RE_findOrAddVlak(obi->obr, face->facenr);
	
	if (co) {
		if (vlr->v4)
			mid_v3_v3v3(co, vlr->v1->co, vlr->v3->co);
		else
			cent_tri_v3(co, vlr->v1->co, vlr->v2->co, vlr->v3->co);

		if (obi->flag & R_TRANSFORMED)
			mul_m4_v3(obi->mat, co);
	}
	
	if (normal) {
		normal[0]= -vlr->n[0];
		normal[1]= -vlr->n[1];
		normal[2]= -vlr->n[2];

		if (obi->flag & R_TRANSFORMED)
			mul_m3_v3(obi->nmat, normal);
	}

	if (area) {
		copy_v3_v3(v1, vlr->v1->co);
		copy_v3_v3(v2, vlr->v2->co);
		copy_v3_v3(v3, vlr->v3->co);
		if (vlr->v4) copy_v3_v3(v4, vlr->v4->co);

		if (obi->flag & R_TRANSFORMED) {
			mul_m4_v3(obi->mat, v1);
			mul_m4_v3(obi->mat, v2);
			mul_m4_v3(obi->mat, v3);
			if (vlr->v4) mul_m4_v3(obi->mat, v4);
		}

		/* todo: correct area for instances */
		if (vlr->v4)
			*area= area_quad_v3(v1, v2, v3, v4);
		else
			*area= area_tri_v3(v1, v2, v3);
	}
}

static void occ_sum_occlusion(OcclusionTree *tree, OccNode *node)
{
	OccNode *child;
	float occ, area, totarea, rad[3];
	int a, b, indirect= tree->doindirect;

	occ= 0.0f;
	totarea= 0.0f;
	if (indirect) zero_v3(rad);

	for (b=0; b<TOTCHILD; b++) {
		if (node->childflag & (1<<b)) {
			a= node->child[b].face;
			occ_face(&tree->face[a], 0, 0, &area);
			occ += area*tree->occlusion[a];
			if (indirect) madd_v3_v3fl(rad, tree->rad[a], area);
			totarea += area;
		}
		else if (node->child[b].node) {
			child= node->child[b].node;
			occ_sum_occlusion(tree, child);

			occ += child->area*child->occlusion;
			if (indirect) madd_v3_v3fl(rad, child->rad, child->area);
			totarea += child->area;
		}
	}

	if (totarea != 0.0f) {
		occ /= totarea;
		if (indirect) mul_v3_fl(rad, 1.0f/totarea);
	}
	
	node->occlusion= occ;
	if (indirect) copy_v3_v3(node->rad, rad);
}

static int occ_find_bbox_axis(OcclusionTree *tree, int begin, int end, float *min, float *max)
{
	float len, maxlen= -1.0f;
	int a, axis = 0;

	INIT_MINMAX(min, max);

	for (a = begin; a < end; a++) {
		DO_MINMAX(tree->co[a], min, max);
	}

	for (a=0; a<3; a++) {
		len= max[a] - min[a];

		if (len > maxlen) {
			maxlen= len;
			axis= a;
		}
	}

	return axis;
}

static void occ_node_from_face(OccFace *face, OccNode *node)
{
	float n[3];

	occ_face(face, node->co, n, &node->area);
	node->dco= 0.0f;
	sh_from_disc(n, node->area, node->sh);
}

static void occ_build_dco(OcclusionTree *tree, OccNode *node, const float co[3], float *dco)
{
	int b;
	for (b=0; b<TOTCHILD; b++) {
		float dist, d[3], nco[3];

		if (node->childflag & (1<<b)) {
			occ_face(tree->face+node->child[b].face, nco, NULL, NULL);
		}
		else if (node->child[b].node) {
			OccNode *child= node->child[b].node;
			occ_build_dco(tree, child, co, dco);
			copy_v3_v3(nco, child->co);
		}
		else {
			continue;
		}

		sub_v3_v3v3(d, nco, co);
		dist= dot_v3v3(d, d);
		if (dist > *dco)
			*dco= dist;
	}
}

static void occ_build_split(OcclusionTree *tree, int begin, int end, int *split)
{
	float min[3], max[3], mid;
	int axis, a, enda;

	/* split in middle of boundbox. this seems faster than median split
	 * on complex scenes, possibly since it avoids two distant faces to
	 * be in the same node better? */
	axis= occ_find_bbox_axis(tree, begin, end, min, max);
	mid= 0.5f*(min[axis]+max[axis]);

	a= begin;
	enda= end;
	while (a<enda) {
		if (tree->co[a][axis] > mid) {
			enda--;
			SWAP(OccFace, tree->face[a], tree->face[enda]);
			SWAP(float, tree->co[a][0], tree->co[enda][0]);
			SWAP(float, tree->co[a][1], tree->co[enda][1]);
			SWAP(float, tree->co[a][2], tree->co[enda][2]);
		}
		else
			a++;
	}

	*split= enda;
}

static void occ_build_8_split(OcclusionTree *tree, int begin, int end, int *offset, int *count)
{
	/* split faces into eight groups */
	int b, splitx, splity[2], splitz[4];

	occ_build_split(tree, begin, end, &splitx);

	/* force split if none found, to deal with degenerate geometry */
	if (splitx == begin || splitx == end)
		splitx= (begin+end)/2;

	occ_build_split(tree, begin, splitx, &splity[0]);
	occ_build_split(tree, splitx, end, &splity[1]);

	occ_build_split(tree, begin, splity[0], &splitz[0]);
	occ_build_split(tree, splity[0], splitx, &splitz[1]);
	occ_build_split(tree, splitx, splity[1], &splitz[2]);
	occ_build_split(tree, splity[1], end, &splitz[3]);

	offset[0]= begin;
	offset[1]= splitz[0];
	offset[2]= splity[0];
	offset[3]= splitz[1];
	offset[4]= splitx;
	offset[5]= splitz[2];
	offset[6]= splity[1];
	offset[7]= splitz[3];

	for (b=0; b<7; b++)
		count[b]= offset[b+1] - offset[b];
	count[7]= end - offset[7];
}

static void occ_build_recursive(OcclusionTree *tree, OccNode *node, int begin, int end, int depth);

static void *exec_occ_build(void *data)
{
	OcclusionBuildThread *othread= (OcclusionBuildThread*)data;

	occ_build_recursive(othread->tree, othread->node, othread->begin, othread->end, othread->depth);

	return 0;
}

static void occ_build_recursive(OcclusionTree *tree, OccNode *node, int begin, int end, int depth)
{
	ListBase threads;
	OcclusionBuildThread othreads[BLENDER_MAX_THREADS];
	OccNode *child, tmpnode;
	/* OccFace *face; */
	int a, b, totthread=0, offset[TOTCHILD], count[TOTCHILD];

	/* add a new node */
	node->occlusion= 1.0f;

	/* leaf node with only children */
	if (end - begin <= TOTCHILD) {
		for (a=begin, b=0; a<end; a++, b++) {
			/* face= &tree->face[a]; */
			node->child[b].face= a;
			node->childflag |= (1<<b);
		}
	}
	else {
		/* order faces */
		occ_build_8_split(tree, begin, end, offset, count);

		if (depth == 1 && tree->dothreadedbuild)
			BLI_init_threads(&threads, exec_occ_build, tree->totbuildthread);

		for (b=0; b<TOTCHILD; b++) {
			if (count[b] == 0) {
				node->child[b].node= NULL;
			}
			else if (count[b] == 1) {
				/* face= &tree->face[offset[b]]; */
				node->child[b].face= offset[b];
				node->childflag |= (1<<b);
			}
			else {
				if (tree->dothreadedbuild)
					BLI_lock_thread(LOCK_CUSTOM1);

				child= BLI_memarena_alloc(tree->arena, sizeof(OccNode));
				node->child[b].node= child;

				/* keep track of maximum depth for stack */
				if (depth+1 > tree->maxdepth)
					tree->maxdepth= depth+1;

				if (tree->dothreadedbuild)
					BLI_unlock_thread(LOCK_CUSTOM1);

				if (depth == 1 && tree->dothreadedbuild) {
					othreads[totthread].tree= tree;
					othreads[totthread].node= child;
					othreads[totthread].begin= offset[b];
					othreads[totthread].end= offset[b]+count[b];
					othreads[totthread].depth= depth+1;
					BLI_insert_thread(&threads, &othreads[totthread]);
					totthread++;
				}
				else
					occ_build_recursive(tree, child, offset[b], offset[b]+count[b], depth+1);
			}
		}

		if (depth == 1 && tree->dothreadedbuild)
			BLI_end_threads(&threads);
	}

	/* combine area, position and sh */
	for (b=0; b<TOTCHILD; b++) {
		if (node->childflag & (1<<b)) {
			child= &tmpnode;
			occ_node_from_face(tree->face+node->child[b].face, &tmpnode);
		}
		else {
			child= node->child[b].node;
		}

		if (child) {
			node->area += child->area;
			sh_add(node->sh, node->sh, child->sh);
			madd_v3_v3fl(node->co, child->co, child->area);
		}
	}

	if (node->area != 0.0f)
		mul_v3_fl(node->co, 1.0f/node->area);

	/* compute maximum distance from center */
	node->dco= 0.0f;
	if (node->area > 0.0f)
		occ_build_dco(tree, node, node->co, &node->dco);
}

static void occ_build_sh_normalize(OccNode *node)
{
	/* normalize spherical harmonics to not include area, so
	 * we can clamp the dot product and then mutliply by area */
	int b;

	if (node->area != 0.0f)
		sh_mul(node->sh, 1.0f/node->area);

	for (b=0; b<TOTCHILD; b++) {
		if (node->childflag & (1<<b));
		else if (node->child[b].node)
			occ_build_sh_normalize(node->child[b].node);
	}
}

static OcclusionTree *occ_tree_build(Render *re)
{
	OcclusionTree *tree;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	Material *ma;
	VlakRen *vlr= NULL;
	int a, b, c, totface;

	/* count */
	totface= 0;
	for (obi=re->instancetable.first; obi; obi=obi->next) {
		obr= obi->obr;
		for (a=0; a<obr->totvlak; a++) {
			if ((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak;
			else vlr++;

			ma= vlr->mat;

			if ((ma->shade_flag & MA_APPROX_OCCLUSION) && (ma->material_type == MA_TYPE_SURFACE))
				totface++;
		}
	}

	if (totface == 0)
		return NULL;
	
	tree= MEM_callocN(sizeof(OcclusionTree), "OcclusionTree");
	tree->totface= totface;

	/* parameters */
	tree->error= get_render_aosss_error(&re->r, re->wrld.ao_approx_error);
	tree->distfac= (re->wrld.aomode & WO_AODIST)? re->wrld.aodistfac: 0.0f;
	tree->doindirect= (re->wrld.ao_indirect_energy > 0.0f && re->wrld.ao_indirect_bounces > 0);

	/* allocation */
	tree->arena= BLI_memarena_new(0x8000 * sizeof(OccNode), "occ tree arena");
	BLI_memarena_use_calloc(tree->arena);

	if (re->wrld.aomode & WO_AOCACHE)
		tree->cache= MEM_callocN(sizeof(OcclusionCache)*BLENDER_MAX_THREADS, "OcclusionCache");

	tree->face= MEM_callocN(sizeof(OccFace)*totface, "OcclusionFace");
	tree->co= MEM_callocN(sizeof(float)*3*totface, "OcclusionCo");
	tree->occlusion= MEM_callocN(sizeof(float)*totface, "OcclusionOcclusion");

	if (tree->doindirect)
		tree->rad= MEM_callocN(sizeof(float)*3*totface, "OcclusionRad");

	/* make array of face pointers */
	for (b=0, c=0, obi=re->instancetable.first; obi; obi=obi->next, c++) {
		obr= obi->obr;
		for (a=0; a<obr->totvlak; a++) {
			if ((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak;
			else vlr++;

			ma= vlr->mat;

			if ((ma->shade_flag & MA_APPROX_OCCLUSION) && (ma->material_type == MA_TYPE_SURFACE)) {
				tree->face[b].obi= c;
				tree->face[b].facenr= a;
				tree->occlusion[b]= 1.0f;
				occ_face(&tree->face[b], tree->co[b], NULL, NULL); 
				b++;
			}
		}
	}

	/* threads */
	tree->totbuildthread= (re->r.threads > 1 && totface > 10000)? 8: 1;
	tree->dothreadedbuild= (tree->totbuildthread > 1);

	/* recurse */
	tree->root= BLI_memarena_alloc(tree->arena, sizeof(OccNode));
	tree->maxdepth= 1;
	occ_build_recursive(tree, tree->root, 0, totface, 1);

	if (tree->doindirect) {
		occ_build_shade(re, tree);
		occ_sum_occlusion(tree, tree->root);
	}
	
	MEM_freeN(tree->co);
	tree->co= NULL;

	occ_build_sh_normalize(tree->root);

	for (a=0; a<BLENDER_MAX_THREADS; a++)
		tree->stack[a]= MEM_callocN(sizeof(OccNode)*TOTCHILD*(tree->maxdepth+1), "OccStack");

	return tree;
}

static void occ_free_tree(OcclusionTree *tree)
{
	int a;

	if (tree) {
		if (tree->arena) BLI_memarena_free(tree->arena);
		for (a=0; a<BLENDER_MAX_THREADS; a++)
			if (tree->stack[a])
				MEM_freeN(tree->stack[a]);
		if (tree->occlusion) MEM_freeN(tree->occlusion);
		if (tree->cache) MEM_freeN(tree->cache);
		if (tree->face) MEM_freeN(tree->face);
		if (tree->rad) MEM_freeN(tree->rad);
		MEM_freeN(tree);
	}
}

/* ------------------------- Traversal --------------------------- */

static float occ_solid_angle(OccNode *node, const float v[3], float d2, float invd2, const float receivenormal[3])
{
	float dotreceive, dotemit;
	float ev[3];

	ev[0]= -v[0]*invd2;
	ev[1]= -v[1]*invd2;
	ev[2]= -v[2]*invd2;
	dotemit= sh_eval(node->sh, ev);
	dotreceive= dot_v3v3(receivenormal, v)*invd2;

	CLAMP(dotemit, 0.0f, 1.0f);
	CLAMP(dotreceive, 0.0f, 1.0f);
	
	return ((node->area*dotemit*dotreceive)/(d2 + node->area*INVPI))*INVPI;
}

static void VecAddDir(float result[3], const float v1[3], const float v2[3], const float fac)
{
	result[0]= v1[0] + fac*(v2[0] - v1[0]);
	result[1]= v1[1] + fac*(v2[1] - v1[1]);
	result[2]= v1[2] + fac*(v2[2] - v1[2]);
}

static int occ_visible_quad(float *p, const float n[3], const float v0[3], const float *v1, const float *v2, float q0[3], float q1[3], float q2[3], float q3[3])
{
	static const float epsilon = 1e-6f;
	float c, sd[3];
	
	c= dot_v3v3(n, p);

	/* signed distances from the vertices to the plane. */
	sd[0]= dot_v3v3(n, v0) - c;
	sd[1]= dot_v3v3(n, v1) - c;
	sd[2]= dot_v3v3(n, v2) - c;

	if (fabsf(sd[0]) < epsilon) sd[0] = 0.0f;
	if (fabsf(sd[1]) < epsilon) sd[1] = 0.0f;
	if (fabsf(sd[2]) < epsilon) sd[2] = 0.0f;

	if (sd[0] > 0) {
		if (sd[1] > 0) {
			if (sd[2] > 0) {
				// +++
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
				// ++-
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				VecAddDir(q2, v1, v2, (sd[1]/(sd[1]-sd[2])));
				VecAddDir(q3, v0, v2, (sd[0]/(sd[0]-sd[2])));
			}
			else {
				// ++0
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
		}
		else if (sd[1] < 0) {
			if (sd[2] > 0) {
				// +-+
				copy_v3_v3(q0, v0);
				VecAddDir(q1, v0, v1, (sd[0]/(sd[0]-sd[1])));
				VecAddDir(q2, v1, v2, (sd[1]/(sd[1]-sd[2])));
				copy_v3_v3(q3, v2);
			}
			else if (sd[2] < 0) {
				// +--
				copy_v3_v3(q0, v0);
				VecAddDir(q1, v0, v1, (sd[0]/(sd[0]-sd[1])));
				VecAddDir(q2, v0, v2, (sd[0]/(sd[0]-sd[2])));
				copy_v3_v3(q3, q2);
			}
			else {
				// +-0
				copy_v3_v3(q0, v0);
				VecAddDir(q1, v0, v1, (sd[0]/(sd[0]-sd[1])));
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
		}
		else {
			if (sd[2] > 0) {
				// +0+
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
				// +0-
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				VecAddDir(q2, v0, v2, (sd[0]/(sd[0]-sd[2])));
				copy_v3_v3(q3, q2);
			}
			else {
				// +00
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
		}
	}
	else if (sd[0] < 0) {
		if (sd[1] > 0) {
			if (sd[2] > 0) {
				// -++
				VecAddDir(q0, v0, v1, (sd[0]/(sd[0]-sd[1])));
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				VecAddDir(q3, v0, v2, (sd[0]/(sd[0]-sd[2])));
			}
			else if (sd[2] < 0) {
				// -+-
				VecAddDir(q0, v0, v1, (sd[0]/(sd[0]-sd[1])));
				copy_v3_v3(q1, v1);
				VecAddDir(q2, v1, v2, (sd[1]/(sd[1]-sd[2])));
				copy_v3_v3(q3, q2);
			}
			else {
				// -+0
				VecAddDir(q0, v0, v1, (sd[0]/(sd[0]-sd[1])));
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
		}
		else if (sd[1] < 0) {
			if (sd[2] > 0) {
				// --+
				VecAddDir(q0, v0, v2, (sd[0]/(sd[0]-sd[2])));
				VecAddDir(q1, v1, v2, (sd[1]/(sd[1]-sd[2])));
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
				// ---
				return 0;
			}
			else {
				// --0
				return 0;
			}
		}
		else {
			if (sd[2] > 0) {
				// -0+
				VecAddDir(q0, v0, v2, (sd[0]/(sd[0]-sd[2])));
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
				// -0-
				return 0;
			}
			else {
				// -00
				return 0;
			}
		}
	}
	else {
		if (sd[1] > 0) {
			if (sd[2] > 0) {
				// 0++
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
				// 0+-
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				VecAddDir(q2, v1, v2, (sd[1]/(sd[1]-sd[2])));
				copy_v3_v3(q3, q2);
			}
			else {
				// 0+0
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
		}
		else if (sd[1] < 0) {
			if (sd[2] > 0) {
				// 0-+
				copy_v3_v3(q0, v0);
				VecAddDir(q1, v1, v2, (sd[1]/(sd[1]-sd[2])));
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
				// 0--
				return 0;
			}
			else {
				// 0-0
				return 0;
			}
		}
		else {
			if (sd[2] > 0) {
				// 00+
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
				// 00-
				return 0;
			}
			else {
				// 000
				return 0;
			}
		}
	}

	return 1;
}

/* altivec optimization, this works, but is unused */

#if 0
#include <Accelerate/Accelerate.h>

typedef union {
	vFloat v;
	float f[4];
} vFloatResult;

static vFloat vec_splat_float(float val)
{
	return (vFloat) {val, val, val, val};
}

static float occ_quad_form_factor(float *p, float *n, float *q0, float *q1, float *q2, float *q3)
{
	vFloat vcos, rlen, vrx, vry, vrz, vsrx, vsry, vsrz, gx, gy, gz, vangle;
	vUInt8 rotate = (vUInt8) {4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3};
	vFloatResult vresult;
	float result;

	/* compute r* */
	vrx = (vFloat) {q0[0], q1[0], q2[0], q3[0]} - vec_splat_float(p[0]);
	vry = (vFloat) {q0[1], q1[1], q2[1], q3[1]} - vec_splat_float(p[1]);
	vrz = (vFloat) {q0[2], q1[2], q2[2], q3[2]} - vec_splat_float(p[2]);

	/* normalize r* */
	rlen = vec_rsqrte(vrx*vrx + vry*vry + vrz*vrz + vec_splat_float(1e-16f));
	vrx = vrx*rlen;
	vry = vry*rlen;
	vrz = vrz*rlen;

	/* rotate r* for cross and dot */
	vsrx= vec_perm(vrx, vrx, rotate);
	vsry= vec_perm(vry, vry, rotate);
	vsrz= vec_perm(vrz, vrz, rotate);

	/* cross product */
	gx = vsry*vrz - vsrz*vry;
	gy = vsrz*vrx - vsrx*vrz;
	gz = vsrx*vry - vsry*vrx;

	/* normalize */
	rlen = vec_rsqrte(gx*gx + gy*gy + gz*gz + vec_splat_float(1e-16f));
	gx = gx*rlen;
	gy = gy*rlen;
	gz = gz*rlen;

	/* angle */
	vcos = vrx*vsrx + vry*vsry + vrz*vsrz;
	vcos= vec_max(vec_min(vcos, vec_splat_float(1.0f)), vec_splat_float(-1.0f));
	vangle= vacosf(vcos);

	/* dot */
	vresult.v = (vec_splat_float(n[0])*gx +
	             vec_splat_float(n[1])*gy +
	             vec_splat_float(n[2])*gz)*vangle;

	result= (vresult.f[0] + vresult.f[1] + vresult.f[2] + vresult.f[3])*(0.5f/(float)M_PI);
	result= MAX2(result, 0.0f);

	return result;
}

#endif

/* SSE optimization, acos code doesn't work */

#if 0

#include <xmmintrin.h>

static __m128 sse_approx_acos(__m128 x)
{
	/* needs a better approximation than taylor expansion of acos, since that
	 * gives big erros for near 1.0 values, sqrt(2*x)*acos(1-x) should work
	 * better, see http://www.tom.womack.net/projects/sse-fast-arctrig.html */

	return _mm_set_ps1(1.0f);
}

static float occ_quad_form_factor(float *p, float *n, float *q0, float *q1, float *q2, float *q3)
{
	float r0[3], r1[3], r2[3], r3[3], g0[3], g1[3], g2[3], g3[3];
	float a1, a2, a3, a4, dot1, dot2, dot3, dot4, result;
	float fresult[4] __attribute__((aligned(16)));
	__m128 qx, qy, qz, rx, ry, rz, rlen, srx, sry, srz, gx, gy, gz, glen, rcos, angle, aresult;

	/* compute r */
	qx = _mm_set_ps(q3[0], q2[0], q1[0], q0[0]);
	qy = _mm_set_ps(q3[1], q2[1], q1[1], q0[1]);
	qz = _mm_set_ps(q3[2], q2[2], q1[2], q0[2]);

	rx = qx - _mm_set_ps1(p[0]);
	ry = qy - _mm_set_ps1(p[1]);
	rz = qz - _mm_set_ps1(p[2]);

	/* normalize r */
	rlen = _mm_rsqrt_ps(rx*rx + ry*ry + rz*rz + _mm_set_ps1(1e-16f));
	rx = rx*rlen;
	ry = ry*rlen;
	rz = rz*rlen;

	/* cross product */
	srx = _mm_shuffle_ps(rx, rx, _MM_SHUFFLE(0,3,2,1));
	sry = _mm_shuffle_ps(ry, ry, _MM_SHUFFLE(0,3,2,1));
	srz = _mm_shuffle_ps(rz, rz, _MM_SHUFFLE(0,3,2,1));

	gx = sry*rz - srz*ry;
	gy = srz*rx - srx*rz;
	gz = srx*ry - sry*rx;

	/* normalize g */
	glen = _mm_rsqrt_ps(gx*gx + gy*gy + gz*gz + _mm_set_ps1(1e-16f));
	gx = gx*glen;
	gy = gy*glen;
	gz = gz*glen;

	/* compute angle */
	rcos = rx*srx + ry*sry + rz*srz;
	rcos= _mm_max_ps(_mm_min_ps(rcos, _mm_set_ps1(1.0f)), _mm_set_ps1(-1.0f));

	angle = sse_approx_cos(rcos);
	aresult = (_mm_set_ps1(n[0])*gx + _mm_set_ps1(n[1])*gy + _mm_set_ps1(n[2])*gz)*angle;

	/* sum together */
	result= (fresult[0] + fresult[1] + fresult[2] + fresult[3])*(0.5f/(float)M_PI);
	result= MAX2(result, 0.0f);

	return result;
}

#endif

static void normalizef(float *n)
{
	float d;
	
	d= dot_v3v3(n, n);

	if (d > 1.0e-35F) {
		d= 1.0f/sqrtf(d);

		n[0] *= d; 
		n[1] *= d; 
		n[2] *= d;
	} 
}

static float occ_quad_form_factor(const float p[3], const float n[3], const float q0[3], const float q1[3], const float q2[3], const float q3[3])
{
	float r0[3], r1[3], r2[3], r3[3], g0[3], g1[3], g2[3], g3[3];
	float a1, a2, a3, a4, dot1, dot2, dot3, dot4, result;

	sub_v3_v3v3(r0, q0, p);
	sub_v3_v3v3(r1, q1, p);
	sub_v3_v3v3(r2, q2, p);
	sub_v3_v3v3(r3, q3, p);

	normalizef(r0);
	normalizef(r1);
	normalizef(r2);
	normalizef(r3);

	cross_v3_v3v3(g0, r1, r0); normalizef(g0);
	cross_v3_v3v3(g1, r2, r1); normalizef(g1);
	cross_v3_v3v3(g2, r3, r2); normalizef(g2);
	cross_v3_v3v3(g3, r0, r3); normalizef(g3);

	a1= saacosf(dot_v3v3(r0, r1));
	a2= saacosf(dot_v3v3(r1, r2));
	a3= saacosf(dot_v3v3(r2, r3));
	a4= saacosf(dot_v3v3(r3, r0));

	dot1= dot_v3v3(n, g0);
	dot2= dot_v3v3(n, g1);
	dot3= dot_v3v3(n, g2);
	dot4= dot_v3v3(n, g3);

	result= (a1*dot1 + a2*dot2 + a3*dot3 + a4*dot4)*0.5f/(float)M_PI;
	result= MAX2(result, 0.0f);

	return result;
}

static float occ_form_factor(OccFace *face, float *p, float *n)
{
	ObjectInstanceRen *obi;
	VlakRen *vlr;
	float v1[3], v2[3], v3[3], v4[3], q0[3], q1[3], q2[3], q3[3], contrib= 0.0f;

	obi= &R.objectinstance[face->obi];
	vlr= RE_findOrAddVlak(obi->obr, face->facenr);

	copy_v3_v3(v1, vlr->v1->co);
	copy_v3_v3(v2, vlr->v2->co);
	copy_v3_v3(v3, vlr->v3->co);

	if (obi->flag & R_TRANSFORMED) {
		mul_m4_v3(obi->mat, v1);
		mul_m4_v3(obi->mat, v2);
		mul_m4_v3(obi->mat, v3);
	}

	if (occ_visible_quad(p, n, v1, v2, v3, q0, q1, q2, q3))
		contrib += occ_quad_form_factor(p, n, q0, q1, q2, q3);

	if (vlr->v4) {
		copy_v3_v3(v4, vlr->v4->co);
		if (obi->flag & R_TRANSFORMED)
			mul_m4_v3(obi->mat, v4);

		if (occ_visible_quad(p, n, v1, v3, v4, q0, q1, q2, q3))
			contrib += occ_quad_form_factor(p, n, q0, q1, q2, q3);
	}

	return contrib;
}

static void occ_lookup(OcclusionTree *tree, int thread, OccFace *exclude, float *pp, float *pn, float *occ, float rad[3], float bentn[3])
{
	OccNode *node, **stack;
	OccFace *face;
	float resultocc, resultrad[3], v[3], p[3], n[3], co[3], invd2;
	float distfac, fac, error, d2, weight, emitarea;
	int b, f, totstack;

	/* init variables */
	copy_v3_v3(p, pp);
	copy_v3_v3(n, pn);
	madd_v3_v3fl(p, n, 1e-4f);

	if (bentn)
		copy_v3_v3(bentn, n);
	
	error= tree->error;
	distfac= tree->distfac;

	resultocc= 0.0f;
	zero_v3(resultrad);

	/* init stack */
	stack= tree->stack[thread];
	stack[0]= tree->root;
	totstack= 1;

	while (totstack) {
		/* pop point off the stack */
		node= stack[--totstack];

		sub_v3_v3v3(v, node->co, p);
		d2= dot_v3v3(v, v) + 1e-16f;
		emitarea= MAX2(node->area, node->dco);

		if (d2*error > emitarea) {
			if (distfac != 0.0f) {
				fac= 1.0f/(1.0f + distfac*d2);
				if (fac < 0.01f)
					continue;
			}
			else
				fac= 1.0f;

			/* accumulate occlusion from spherical harmonics */
			invd2 = 1.0f/sqrtf(d2);
			weight= occ_solid_angle(node, v, d2, invd2, n);

			if (rad)
				madd_v3_v3fl(resultrad, node->rad, weight*fac);

			weight *= node->occlusion;

			if (bentn) {
				bentn[0] -= weight*invd2*v[0];
				bentn[1] -= weight*invd2*v[1];
				bentn[2] -= weight*invd2*v[2];
			}

			resultocc += weight*fac;
		}
		else {
			/* traverse into children */
			for (b=0; b<TOTCHILD; b++) {
				if (node->childflag & (1<<b)) {
					f= node->child[b].face;
					face= &tree->face[f];

					/* accumulate occlusion with face form factor */
					if (!exclude || !(face->obi == exclude->obi && face->facenr == exclude->facenr)) {
						if (bentn || distfac != 0.0f) {
							occ_face(face, co, NULL, NULL); 
							sub_v3_v3v3(v, co, p);
							d2= dot_v3v3(v, v) + 1e-16f;

							fac= (distfac == 0.0f)? 1.0f: 1.0f/(1.0f + distfac*d2);
							if (fac < 0.01f)
								continue;
						}
						else
							fac= 1.0f;

						weight= occ_form_factor(face, p, n);

						if (rad)
							madd_v3_v3fl(resultrad, tree->rad[f], weight*fac);

						weight *= tree->occlusion[f];

						if (bentn) {
							invd2= 1.0f/sqrtf(d2);
							bentn[0] -= weight*invd2*v[0];
							bentn[1] -= weight*invd2*v[1];
							bentn[2] -= weight*invd2*v[2];
						}

						resultocc += weight*fac;
					}
				}
				else if (node->child[b].node) {
					/* push child on the stack */
					stack[totstack++]= node->child[b].node;
				}
			}
		}
	}

	if (occ) *occ= resultocc;
	if (rad) copy_v3_v3(rad, resultrad);
#if 0
	if (rad && exclude) {
		int a;
		for (a=0; a<tree->totface; a++)
			if ((tree->face[a].obi == exclude->obi && tree->face[a].facenr == exclude->facenr))
				copy_v3_v3(rad, tree->rad[a]);
	}
#endif
	if (bentn) normalize_v3(bentn);
}

static void occ_compute_bounces(Render *re, OcclusionTree *tree, int totbounce)
{
	float (*rad)[3], (*sum)[3], (*tmp)[3], co[3], n[3], occ;
	int bounce, i;

	rad= MEM_callocN(sizeof(float)*3*tree->totface, "OcclusionBounceRad");
	sum= MEM_dupallocN(tree->rad);

	for (bounce=1; bounce<totbounce; bounce++) {
		for (i=0; i<tree->totface; i++) {
			occ_face(&tree->face[i], co, n, NULL);
			madd_v3_v3fl(co, n, 1e-8f);

			occ_lookup(tree, 0, &tree->face[i], co, n, &occ, rad[i], NULL);
			rad[i][0]= MAX2(rad[i][0], 0.0f);
			rad[i][1]= MAX2(rad[i][1], 0.0f);
			rad[i][2]= MAX2(rad[i][2], 0.0f);
			add_v3_v3(sum[i], rad[i]);

			if (re->test_break(re->tbh))
				break;
		}

		if (re->test_break(re->tbh))
			break;

		tmp= tree->rad;
		tree->rad= rad;
		rad= tmp;

		occ_sum_occlusion(tree, tree->root);
	}

	MEM_freeN(rad);
	MEM_freeN(tree->rad);
	tree->rad= sum;

	if (!re->test_break(re->tbh))
		occ_sum_occlusion(tree, tree->root);
}

static void occ_compute_passes(Render *re, OcclusionTree *tree, int totpass)
{
	float *occ, co[3], n[3];
	int pass, i;
	
	occ= MEM_callocN(sizeof(float)*tree->totface, "OcclusionPassOcc");

	for (pass=0; pass<totpass; pass++) {
		for (i=0; i<tree->totface; i++) {
			occ_face(&tree->face[i], co, n, NULL);
			negate_v3(n);
			madd_v3_v3fl(co, n, 1e-8f);

			occ_lookup(tree, 0, &tree->face[i], co, n, &occ[i], NULL, NULL);
			if (re->test_break(re->tbh))
				break;
		}

		if (re->test_break(re->tbh))
			break;

		for (i=0; i<tree->totface; i++) {
			tree->occlusion[i] -= occ[i]; //MAX2(1.0f-occ[i], 0.0f);
			if (tree->occlusion[i] < 0.0f)
				tree->occlusion[i]= 0.0f;
		}

		occ_sum_occlusion(tree, tree->root);
	}

	MEM_freeN(occ);
}

static void sample_occ_tree(Render *re, OcclusionTree *tree, OccFace *exclude, float *co, float *n, int thread, int onlyshadow, float *ao, float *env, float *indirect)
{
	float nn[3], bn[3], fac, occ, occlusion, correction, rad[3];
	int envcolor;

	envcolor= re->wrld.aocolor;
	if (onlyshadow)
		envcolor= WO_AOPLAIN;

	negate_v3_v3(nn, n);

	occ_lookup(tree, thread, exclude, co, nn, &occ, (tree->doindirect)? rad: NULL, (env && envcolor)? bn: NULL);

	correction= re->wrld.ao_approx_correction;

	occlusion= (1.0f-correction)*(1.0f-occ);
	CLAMP(occlusion, 0.0f, 1.0f);
	if (correction != 0.0f)
		occlusion += correction*expf(-occ);

	if (env) {
		/* sky shading using bent normal */
		if (ELEM(envcolor, WO_AOSKYCOL, WO_AOSKYTEX)) {
			fac= 0.5f * (1.0f + dot_v3v3(bn, re->grvec));
			env[0]= (1.0f-fac)*re->wrld.horr + fac*re->wrld.zenr;
			env[1]= (1.0f-fac)*re->wrld.horg + fac*re->wrld.zeng;
			env[2]= (1.0f-fac)*re->wrld.horb + fac*re->wrld.zenb;

			mul_v3_fl(env, occlusion);
		}
		else {
			env[0]= occlusion;
			env[1]= occlusion;
			env[2]= occlusion;
		}
#if 0
		else {	/* WO_AOSKYTEX */
			float dxyview[3];
			bn[0]= -bn[0];
			bn[1]= -bn[1];
			bn[2]= -bn[2];
			dxyview[0]= 1.0f;
			dxyview[1]= 1.0f;
			dxyview[2]= 0.0f;
			shadeSkyView(ao, co, bn, dxyview);
		}
#endif
	}

	if (ao) {
		ao[0]= occlusion;
		ao[1]= occlusion;
		ao[2]= occlusion;
	}

	if (tree->doindirect) copy_v3_v3(indirect, rad);
	else zero_v3(indirect);
}

/* ---------------------------- Caching ------------------------------- */

static OcclusionCacheSample *find_occ_sample(OcclusionCache *cache, int x, int y)
{
	x -= cache->x;
	y -= cache->y;

	x /= cache->step;
	y /= cache->step;
	x *= cache->step;
	y *= cache->step;

	if (x < 0 || x >= cache->w || y < 0 || y >= cache->h)
		return NULL;
	else
		return &cache->sample[y*cache->w + x];
}

static int sample_occ_cache(OcclusionTree *tree, float *co, float *n, int x, int y, int thread, float *ao, float *env, float *indirect)
{
	OcclusionCache *cache;
	OcclusionCacheSample *samples[4], *sample;
	float wn[4], wz[4], wb[4], tx, ty, w, totw, mino, maxo;
	float d[3], dist2;
	int i, x1, y1, x2, y2;

	if (!tree->cache)
		return 0;
	
	/* first try to find a sample in the same pixel */
	cache= &tree->cache[thread];

	if (cache->sample && cache->step) {
		sample= &cache->sample[(y-cache->y)*cache->w + (x-cache->x)];
		if (sample->filled) {
			sub_v3_v3v3(d, sample->co, co);
			dist2= dot_v3v3(d, d);
			if (dist2 < 0.5f*sample->dist2 && dot_v3v3(sample->n, n) > 0.98f) {
				copy_v3_v3(ao, sample->ao);
				copy_v3_v3(env, sample->env);
				copy_v3_v3(indirect, sample->indirect);
				return 1;
			}
		}
	}
	else
		return 0;

	/* try to interpolate between 4 neighboring pixels */
	samples[0]= find_occ_sample(cache, x, y);
	samples[1]= find_occ_sample(cache, x+cache->step, y);
	samples[2]= find_occ_sample(cache, x, y+cache->step);
	samples[3]= find_occ_sample(cache, x+cache->step, y+cache->step);

	for (i=0; i<4; i++)
		if (!samples[i] || !samples[i]->filled)
			return 0;

	/* require intensities not being too different */
	mino= MIN4(samples[0]->intensity, samples[1]->intensity, samples[2]->intensity, samples[3]->intensity);
	maxo= MAX4(samples[0]->intensity, samples[1]->intensity, samples[2]->intensity, samples[3]->intensity);

	if (maxo - mino > 0.05f)
		return 0;

	/* compute weighted interpolation between samples */
	zero_v3(ao);
	zero_v3(env);
	zero_v3(indirect);
	totw= 0.0f;

	x1= samples[0]->x;
	y1= samples[0]->y;
	x2= samples[3]->x;
	y2= samples[3]->y;

	tx= (float)(x2 - x)/(float)(x2 - x1);
	ty= (float)(y2 - y)/(float)(y2 - y1);

	wb[3]= (1.0f-tx)*(1.0f-ty);
	wb[2]= (tx)*(1.0f-ty);
	wb[1]= (1.0f-tx)*(ty);
	wb[0]= tx*ty;

	for (i=0; i<4; i++) {
		sub_v3_v3v3(d, samples[i]->co, co);
		//dist2= dot_v3v3(d, d);

		wz[i]= 1.0f; //(samples[i]->dist2/(1e-4f + dist2));
		wn[i]= pow(dot_v3v3(samples[i]->n, n), 32.0f);

		w= wb[i]*wn[i]*wz[i];

		totw += w;
		madd_v3_v3fl(ao, samples[i]->ao, w);
		madd_v3_v3fl(env, samples[i]->env, w);
		madd_v3_v3fl(indirect, samples[i]->indirect, w);
	}

	if (totw >= 0.9f) {
		totw= 1.0f/totw;
		mul_v3_fl(ao, totw);
		mul_v3_fl(env, totw);
		mul_v3_fl(indirect, totw);
		return 1;
	}

	return 0;
}

static void sample_occ_surface(ShadeInput *shi)
{
	StrandRen *strand= shi->strand;
	StrandSurface *mesh= strand->buffer->surface;
	int *face, *index = RE_strandren_get_face(shi->obr, strand, 0);
	float w[4], *co1, *co2, *co3, *co4;

	if (mesh && mesh->face && mesh->co && mesh->ao && index) {
		face= mesh->face[*index];

		co1= mesh->co[face[0]];
		co2= mesh->co[face[1]];
		co3= mesh->co[face[2]];
		co4= (face[3])? mesh->co[face[3]]: NULL;

		interp_weights_face_v3(w, co1, co2, co3, co4, strand->vert->co);

		zero_v3(shi->ao);
		zero_v3(shi->env);
		zero_v3(shi->indirect);

		madd_v3_v3fl(shi->ao, mesh->ao[face[0]], w[0]);
		madd_v3_v3fl(shi->env, mesh->env[face[0]], w[0]);
		madd_v3_v3fl(shi->indirect, mesh->indirect[face[0]], w[0]);
		madd_v3_v3fl(shi->ao, mesh->ao[face[1]], w[1]);
		madd_v3_v3fl(shi->env, mesh->env[face[1]], w[1]);
		madd_v3_v3fl(shi->indirect, mesh->indirect[face[1]], w[1]);
		madd_v3_v3fl(shi->ao, mesh->ao[face[2]], w[2]);
		madd_v3_v3fl(shi->env, mesh->env[face[2]], w[2]);
		madd_v3_v3fl(shi->indirect, mesh->indirect[face[2]], w[2]);
		if (face[3]) {
			madd_v3_v3fl(shi->ao, mesh->ao[face[3]], w[3]);
			madd_v3_v3fl(shi->env, mesh->env[face[3]], w[3]);
			madd_v3_v3fl(shi->indirect, mesh->indirect[face[3]], w[3]);
		}
	}
	else {
		shi->ao[0]= 1.0f;
		shi->ao[1]= 1.0f;
		shi->ao[2]= 1.0f;
		zero_v3(shi->env);
		zero_v3(shi->indirect);
	}
}

/* ------------------------- External Functions --------------------------- */

static void *exec_strandsurface_sample(void *data)
{
	OcclusionThread *othread= (OcclusionThread*)data;
	Render *re= othread->re;
	StrandSurface *mesh= othread->mesh;
	float ao[3], env[3], indirect[3], co[3], n[3], *co1, *co2, *co3, *co4;
	int a, *face;

	for (a=othread->begin; a<othread->end; a++) {
		face= mesh->face[a];
		co1= mesh->co[face[0]];
		co2= mesh->co[face[1]];
		co3= mesh->co[face[2]];

		if (face[3]) {
			co4= mesh->co[face[3]];

			mid_v3_v3v3(co, co1, co3);
			normal_quad_v3(n, co1, co2, co3, co4);
		}
		else {
			cent_tri_v3(co, co1, co2, co3);
			normal_tri_v3(n, co1, co2, co3);
		}
		negate_v3(n);

		sample_occ_tree(re, re->occlusiontree, NULL, co, n, othread->thread, 0, ao, env, indirect);
		copy_v3_v3(othread->faceao[a], ao);
		copy_v3_v3(othread->faceenv[a], env);
		copy_v3_v3(othread->faceindirect[a], indirect);
	}

	return 0;
}

void make_occ_tree(Render *re)
{
	OcclusionThread othreads[BLENDER_MAX_THREADS];
	OcclusionTree *tree;
	StrandSurface *mesh;
	ListBase threads;
	float ao[3], env[3], indirect[3], (*faceao)[3], (*faceenv)[3], (*faceindirect)[3];
	int a, totface, totthread, *face, *count;

	/* ugly, needed for occ_face */
	R= *re;

	re->i.infostr= "Occlusion preprocessing";
	re->stats_draw(re->sdh, &re->i);
	
	re->occlusiontree= tree= occ_tree_build(re);
	
	if (tree) {
		if (re->wrld.ao_approx_passes > 0)
			occ_compute_passes(re, tree, re->wrld.ao_approx_passes);
		if (tree->doindirect && (re->wrld.mode & WO_INDIRECT_LIGHT))
			occ_compute_bounces(re, tree, re->wrld.ao_indirect_bounces);

		for (mesh=re->strandsurface.first; mesh; mesh=mesh->next) {
			if (!mesh->face || !mesh->co || !mesh->ao)
				continue;

			count= MEM_callocN(sizeof(int)*mesh->totvert, "OcclusionCount");
			faceao= MEM_callocN(sizeof(float)*3*mesh->totface, "StrandSurfFaceAO");
			faceenv= MEM_callocN(sizeof(float)*3*mesh->totface, "StrandSurfFaceEnv");
			faceindirect= MEM_callocN(sizeof(float)*3*mesh->totface, "StrandSurfFaceIndirect");

			totthread= (mesh->totface > 10000)? re->r.threads: 1;
			totface= mesh->totface/totthread;
			for (a=0; a<totthread; a++) {
				othreads[a].re= re;
				othreads[a].faceao= faceao;
				othreads[a].faceenv= faceenv;
				othreads[a].faceindirect= faceindirect;
				othreads[a].thread= a;
				othreads[a].mesh= mesh;
				othreads[a].begin= a*totface;
				othreads[a].end= (a == totthread-1)? mesh->totface: (a+1)*totface;
			}

			if (totthread == 1) {
				exec_strandsurface_sample(&othreads[0]);
			}
			else {
				BLI_init_threads(&threads, exec_strandsurface_sample, totthread);

				for (a=0; a<totthread; a++)
					BLI_insert_thread(&threads, &othreads[a]);

				BLI_end_threads(&threads);
			}

			for (a=0; a<mesh->totface; a++) {
				face= mesh->face[a];

				copy_v3_v3(ao, faceao[a]);
				copy_v3_v3(env, faceenv[a]);
				copy_v3_v3(indirect, faceindirect[a]);

				add_v3_v3(mesh->ao[face[0]], ao);
				add_v3_v3(mesh->env[face[0]], env);
				add_v3_v3(mesh->indirect[face[0]], indirect);
				count[face[0]]++;
				add_v3_v3(mesh->ao[face[1]], ao);
				add_v3_v3(mesh->env[face[1]], env);
				add_v3_v3(mesh->indirect[face[1]], indirect);
				count[face[1]]++;
				add_v3_v3(mesh->ao[face[2]], ao);
				add_v3_v3(mesh->env[face[2]], env);
				add_v3_v3(mesh->indirect[face[2]], indirect);
				count[face[2]]++;

				if (face[3]) {
					add_v3_v3(mesh->ao[face[3]], ao);
					add_v3_v3(mesh->env[face[3]], env);
					add_v3_v3(mesh->indirect[face[3]], indirect);
					count[face[3]]++;
				}
			}

			for (a=0; a<mesh->totvert; a++) {
				if (count[a]) {
					mul_v3_fl(mesh->ao[a], 1.0f/count[a]);
					mul_v3_fl(mesh->env[a], 1.0f/count[a]);
					mul_v3_fl(mesh->indirect[a], 1.0f/count[a]);
				}
			}

			MEM_freeN(count);
			MEM_freeN(faceao);
			MEM_freeN(faceenv);
			MEM_freeN(faceindirect);
		}
	}
}

void free_occ(Render *re)
{
	if (re->occlusiontree) {
		occ_free_tree(re->occlusiontree);
		re->occlusiontree = NULL;
	}
}

void sample_occ(Render *re, ShadeInput *shi)
{
	OcclusionTree *tree= re->occlusiontree;
	OcclusionCache *cache;
	OcclusionCacheSample *sample;
	OccFace exclude;
	int onlyshadow;

	if (tree) {
		if (shi->strand) {
			sample_occ_surface(shi);
		}
		/* try to get result from the cache if possible */
		else if (shi->depth!=0 || !sample_occ_cache(tree, shi->co, shi->vno, shi->xs, shi->ys, shi->thread, shi->ao, shi->env, shi->indirect)) {
			/* no luck, let's sample the occlusion */
			exclude.obi= shi->obi - re->objectinstance;
			exclude.facenr= shi->vlr->index;
			onlyshadow= (shi->mat->mode & MA_ONLYSHADOW);
			sample_occ_tree(re, tree, &exclude, shi->co, shi->vno, shi->thread, onlyshadow, shi->ao, shi->env, shi->indirect);

			/* fill result into sample, each time */
			if (tree->cache) {
				cache= &tree->cache[shi->thread];

				if (cache->sample && cache->step) {
					sample= &cache->sample[(shi->ys-cache->y)*cache->w + (shi->xs-cache->x)];
					copy_v3_v3(sample->co, shi->co);
					copy_v3_v3(sample->n, shi->vno);
					copy_v3_v3(sample->ao, shi->ao);
					copy_v3_v3(sample->env, shi->env);
					copy_v3_v3(sample->indirect, shi->indirect);
					sample->intensity= MAX3(sample->ao[0], sample->ao[1], sample->ao[2]);
					sample->intensity= MAX2(sample->intensity, MAX3(sample->env[0], sample->env[1], sample->env[2]));
					sample->intensity= MAX2(sample->intensity, MAX3(sample->indirect[0], sample->indirect[1], sample->indirect[2]));
					sample->dist2= dot_v3v3(shi->dxco, shi->dxco) + dot_v3v3(shi->dyco, shi->dyco);
					sample->filled= 1;
				}
			}
		}
	}
	else {
		shi->ao[0]= 1.0f;
		shi->ao[1]= 1.0f;
		shi->ao[2]= 1.0f;

		shi->env[0]= 0.0f;
		shi->env[1]= 0.0f;
		shi->env[2]= 0.0f;

		shi->indirect[0]= 0.0f;
		shi->indirect[1]= 0.0f;
		shi->indirect[2]= 0.0f;
	}
}

void cache_occ_samples(Render *re, RenderPart *pa, ShadeSample *ssamp)
{
	OcclusionTree *tree= re->occlusiontree;
	PixStr ps;
	OcclusionCache *cache;
	OcclusionCacheSample *sample;
	OccFace exclude;
	ShadeInput *shi;
	intptr_t *rd=NULL;
	int *ro=NULL, *rp=NULL, *rz=NULL, onlyshadow;
	int x, y, step = CACHE_STEP;

	if (!tree->cache)
		return;

	cache= &tree->cache[pa->thread];
	cache->w= pa->rectx;
	cache->h= pa->recty;
	cache->x= pa->disprect.xmin;
	cache->y= pa->disprect.ymin;
	cache->step= step;
	cache->sample= MEM_callocN(sizeof(OcclusionCacheSample)*cache->w*cache->h, "OcclusionCacheSample");
	sample= cache->sample;

	if (re->osa) {
		rd= pa->rectdaps;
	}
	else {
		/* fake pixel struct for non-osa */
		ps.next= NULL;
		ps.mask= 0xFFFF;

		ro= pa->recto;
		rp= pa->rectp;
		rz= pa->rectz;
	}

	/* compute a sample at every step pixels */
	for (y=pa->disprect.ymin; y<pa->disprect.ymax; y++) {
		for (x=pa->disprect.xmin; x<pa->disprect.xmax; x++, sample++, rd++, ro++, rp++, rz++) {
			if (!(((x - pa->disprect.xmin + step) % step) == 0 || x == pa->disprect.xmax-1))
				continue;
			if (!(((y - pa->disprect.ymin + step) % step) == 0 || y == pa->disprect.ymax-1))
				continue;

			if (re->osa) {
				if (!*rd) continue;

				shade_samples_fill_with_ps(ssamp, (PixStr *)(*rd), x, y);
			}
			else {
				if (!*rp) continue;

				ps.obi= *ro;
				ps.facenr= *rp;
				ps.z= *rz;
				shade_samples_fill_with_ps(ssamp, &ps, x, y);
			}

			shi= ssamp->shi;
			if (shi->vlr) {
				onlyshadow= (shi->mat->mode & MA_ONLYSHADOW);
				exclude.obi= shi->obi - re->objectinstance;
				exclude.facenr= shi->vlr->index;
				sample_occ_tree(re, tree, &exclude, shi->co, shi->vno, shi->thread, onlyshadow, shi->ao, shi->env, shi->indirect);

				copy_v3_v3(sample->co, shi->co);
				copy_v3_v3(sample->n, shi->vno);
				copy_v3_v3(sample->ao, shi->ao);
				copy_v3_v3(sample->env, shi->env);
				copy_v3_v3(sample->indirect, shi->indirect);
				sample->intensity= MAX3(sample->ao[0], sample->ao[1], sample->ao[2]);
				sample->intensity= MAX2(sample->intensity, MAX3(sample->env[0], sample->env[1], sample->env[2]));
				sample->intensity= MAX2(sample->intensity, MAX3(sample->indirect[0], sample->indirect[1], sample->indirect[2]));
				sample->dist2= dot_v3v3(shi->dxco, shi->dxco) + dot_v3v3(shi->dyco, shi->dyco);
				sample->x= shi->xs;
				sample->y= shi->ys;
				sample->filled= 1;
			}

			if (re->test_break(re->tbh))
				break;
		}
	}
}

void free_occ_samples(Render *re, RenderPart *pa)
{
	OcclusionTree *tree= re->occlusiontree;
	OcclusionCache *cache;

	if (tree->cache) {
		cache= &tree->cache[pa->thread];

		if (cache->sample)
			MEM_freeN(cache->sample);

		cache->w= 0;
		cache->h= 0;
		cache->step= 0;
	}
}

