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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/sss.c
 *  \ingroup render
 */


/* Possible Improvements:
 * - add fresnel terms
 * - adapt Rd table to scale, now with small scale there are a lot of misses?
 * - possible interesting method: perform sss on all samples in the tree,
 *   and then use those values interpolated somehow later. can also do this
 *   filtering on demand for speed. since we are doing things in screen
 *   space now there is an exact correspondence
 * - avoid duplicate shading (filtering points in advance, irradiance cache
 *   like lookup?)
 * - lower resolution samples
 */

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

/* external modules: */
#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"

#include "PIL_time.h"

#include "DNA_material_types.h"

#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_scene.h"


/* this module */
#include "render_types.h"
#include "rendercore.h"
#include "renderdatabase.h" 
#include "shading.h"
#include "sss.h"
#include "zbuf.h"

extern Render R; // meh

/* Generic Multiple Scattering API */

/* Relevant papers:
 * [1] A Practical Model for Subsurface Light Transport
 * [2] A Rapid Hierarchical Rendering Technique for Translucent Materials
 * [3] Efficient Rendering of Local Subsurface Scattering
 * [4] Implementing a skin BSSRDF (or several...)
 */

/* Defines */

#define RD_TABLE_RANGE 		100.0f
#define RD_TABLE_RANGE_2	10000.0f
#define RD_TABLE_SIZE		10000

#define MAX_OCTREE_NODE_POINTS	8
#define MAX_OCTREE_DEPTH		15

/* Struct Definitions */

struct ScatterSettings {
	float eta;		/* index of refraction */
	float sigma_a;	/* absorption coefficient */
	float sigma_s_; /* reduced scattering coefficient */
	float sigma_t_; /* reduced extinction coefficient */
	float sigma;	/* effective extinction coefficient */
	float Fdr;		/* diffuse fresnel reflectance */
	float D;		/* diffusion constant */
	float A;
	float alpha_;	/* reduced albedo */
	float zr;		/* distance of virtual lightsource above surface */
	float zv;		/* distance of virtual lightsource below surface */
	float ld;		/* mean free path */
	float ro;		/* diffuse reflectance */
	float color;
	float invsigma_t_;
	float frontweight;
	float backweight;

	float *tableRd;  /* lookup table to avoid computing Rd */
	float *tableRd2; /* lookup table to avoid computing Rd for bigger values */
};

typedef struct ScatterPoint {
	float co[3];
	float rad[3];
	float area;
	int back;
} ScatterPoint;

typedef struct ScatterNode {
	float co[3];
	float rad[3];
	float backrad[3];
	float area, backarea;

	int totpoint;
	ScatterPoint *points;

	float split[3];
	struct ScatterNode *child[8];
} ScatterNode;

struct ScatterTree {
	MemArena *arena;

	ScatterSettings *ss[3];
	float error, scale;

	ScatterNode *root;
	ScatterPoint *points;
	ScatterPoint **refpoints;
	ScatterPoint **tmppoints;
	int totpoint;
	float min[3], max[3];
};

typedef struct ScatterResult {
	float rad[3];
	float backrad[3];
	float rdsum[3];
	float backrdsum[3];
} ScatterResult;

/* Functions for BSSRDF reparametrization in to more intuitive parameters,
 * see [2] section 4 for more info. */

static float f_Rd(float alpha_, float A, float ro)
{
	float sq;

	sq= sqrt(3.0f*(1.0f - alpha_));
	return (alpha_/2.0f)*(1.0f + expf((-4.0f/3.0f)*A*sq))*expf(-sq) - ro;
}

static float compute_reduced_albedo(ScatterSettings *ss)
{
	const float tolerance= 1e-8;
	const int max_iteration_count= 20;
	float d, fsub, xn_1= 0.0f , xn= 1.0f, fxn, fxn_1;
	int i;

	/* use secant method to compute reduced albedo using Rd function inverse
	 * with a given reflectance */
	fxn= f_Rd(xn, ss->A, ss->ro);
	fxn_1= f_Rd(xn_1, ss->A, ss->ro);

	for(i= 0; i < max_iteration_count; i++) {
		fsub= (fxn - fxn_1);
		if(fabsf(fsub) < tolerance)
			break;
		d= ((xn - xn_1)/fsub)*fxn;
		if(fabsf(d) < tolerance)
			break;

		xn_1= xn;
		fxn_1= fxn;
		xn= xn - d;

		if(xn > 1.0f) xn= 1.0f;
		if(xn_1 > 1.0f) xn_1= 1.0f;
		
		fxn= f_Rd(xn, ss->A, ss->ro);
	}

	/* avoid division by zero later */
	if(xn <= 0.0f)
		xn= 0.00001f;

	return xn;
}

/* Exponential falloff functions */

static float Rd_rsquare(ScatterSettings *ss, float rr)
{
	float sr, sv, Rdr, Rdv;

	sr= sqrt(rr + ss->zr*ss->zr);
	sv= sqrt(rr + ss->zv*ss->zv);

	Rdr= ss->zr*(1.0f + ss->sigma*sr)*expf(-ss->sigma*sr)/(sr*sr*sr);
	Rdv= ss->zv*(1.0f + ss->sigma*sv)*expf(-ss->sigma*sv)/(sv*sv*sv);

	return /*ss->alpha_*/(1.0f/(4.0f*(float)M_PI))*(Rdr + Rdv);
}

static float Rd(ScatterSettings *ss, float r)
{
	return Rd_rsquare(ss, r*r);
}

/* table lookups for Rd. this avoids expensive exp calls. we use two
 * separate tables as well for lower and higher numbers to improve
 * precision, since the number are poorly distributed because we do
 * a lookup with the squared distance for smaller distances, saving
 * another sqrt. */

static void approximate_Rd_rgb(ScatterSettings **ss, float rr, float *rd)
{
	float indexf, t, idxf;
	int index;

	if(rr > (RD_TABLE_RANGE_2*RD_TABLE_RANGE_2));
	else if(rr > RD_TABLE_RANGE) {
		rr= sqrt(rr);
		indexf= rr*(RD_TABLE_SIZE/RD_TABLE_RANGE_2);
		index= (int)indexf;
		idxf= (float)index;
		t= indexf - idxf;

		if(index >= 0 && index < RD_TABLE_SIZE) {
			rd[0]= (ss[0]->tableRd2[index]*(1-t) + ss[0]->tableRd2[index+1]*t);
			rd[1]= (ss[1]->tableRd2[index]*(1-t) + ss[1]->tableRd2[index+1]*t);
			rd[2]= (ss[2]->tableRd2[index]*(1-t) + ss[2]->tableRd2[index+1]*t);
			return;
		}
	}
	else {
		indexf= rr*(RD_TABLE_SIZE/RD_TABLE_RANGE);
		index= (int)indexf;
		idxf= (float)index;
		t= indexf - idxf;

		if(index >= 0 && index < RD_TABLE_SIZE) {
			rd[0]= (ss[0]->tableRd[index]*(1-t) + ss[0]->tableRd[index+1]*t);
			rd[1]= (ss[1]->tableRd[index]*(1-t) + ss[1]->tableRd[index+1]*t);
			rd[2]= (ss[2]->tableRd[index]*(1-t) + ss[2]->tableRd[index+1]*t);
			return;
		}
	}

	/* fallback to slow Rd computation */
	rd[0]= Rd_rsquare(ss[0], rr);
	rd[1]= Rd_rsquare(ss[1], rr);
	rd[2]= Rd_rsquare(ss[2], rr);
}

static void build_Rd_table(ScatterSettings *ss)
{
	float r;
	int i, size = RD_TABLE_SIZE+1;

	ss->tableRd= MEM_mallocN(sizeof(float)*size, "scatterTableRd");
	ss->tableRd2= MEM_mallocN(sizeof(float)*size, "scatterTableRd");

	for(i= 0; i < size; i++) {
		r= i*(RD_TABLE_RANGE/RD_TABLE_SIZE);
		/*if(r < ss->invsigma_t_*ss->invsigma_t_)
			r= ss->invsigma_t_*ss->invsigma_t_;*/
		ss->tableRd[i]= Rd(ss, sqrt(r));

		r= i*(RD_TABLE_RANGE_2/RD_TABLE_SIZE);
		/*if(r < ss->invsigma_t_)
			r= ss->invsigma_t_;*/
		ss->tableRd2[i]= Rd(ss, r);
	}
}

ScatterSettings *scatter_settings_new(float refl, float radius, float ior, float reflfac, float frontweight, float backweight)
{
	ScatterSettings *ss;
	
	ss= MEM_callocN(sizeof(ScatterSettings), "ScatterSettings");

	/* see [1] and [3] for these formulas */
	ss->eta= ior;
	ss->Fdr= -1.440f/ior*ior + 0.710f/ior + 0.668f + 0.0636f*ior;
	ss->A= (1.0f + ss->Fdr)/(1.0f - ss->Fdr);
	ss->ld= radius;
	ss->ro= MIN2(refl, 0.999f);
	ss->color= ss->ro*reflfac + (1.0f-reflfac);

	ss->alpha_= compute_reduced_albedo(ss);

	ss->sigma= 1.0f/ss->ld;
	ss->sigma_t_= ss->sigma/sqrtf(3.0f*(1.0f - ss->alpha_));
	ss->sigma_s_= ss->alpha_*ss->sigma_t_;
	ss->sigma_a= ss->sigma_t_ - ss->sigma_s_;

	ss->D= 1.0f/(3.0f*ss->sigma_t_);

	ss->zr= 1.0f/ss->sigma_t_;
	ss->zv= ss->zr + 4.0f*ss->A*ss->D;

	ss->invsigma_t_= 1.0f/ss->sigma_t_;

	ss->frontweight= frontweight;
	ss->backweight= backweight;

	/* precompute a table of Rd values for quick lookup */
	build_Rd_table(ss);

	return ss;
}

void scatter_settings_free(ScatterSettings *ss)
{
	MEM_freeN(ss->tableRd);
	MEM_freeN(ss->tableRd2);
	MEM_freeN(ss);
}

/* Hierarchical method as in [2]. */

/* traversal */

#define SUBNODE_INDEX(co, split) \
	((co[0]>=split[0]) + (co[1]>=split[1])*2 + (co[2]>=split[2])*4)
	
static void add_radiance(ScatterTree *tree, float *frontrad, float *backrad, float area, float backarea, float rr, ScatterResult *result)
{
	float rd[3], frontrd[3], backrd[3];

	approximate_Rd_rgb(tree->ss, rr, rd);

	if(frontrad && area) {
		frontrd[0] = rd[0]*area;
		frontrd[1] = rd[1]*area;
		frontrd[2] = rd[2]*area;

		result->rad[0] += frontrad[0]*frontrd[0];
		result->rad[1] += frontrad[1]*frontrd[1];
		result->rad[2] += frontrad[2]*frontrd[2];

		result->rdsum[0] += frontrd[0];
		result->rdsum[1] += frontrd[1];
		result->rdsum[2] += frontrd[2];
	}
	if(backrad && backarea) {
		backrd[0] = rd[0]*backarea;
		backrd[1] = rd[1]*backarea;
		backrd[2] = rd[2]*backarea;

		result->backrad[0] += backrad[0]*backrd[0];
		result->backrad[1] += backrad[1]*backrd[1];
		result->backrad[2] += backrad[2]*backrd[2];

		result->backrdsum[0] += backrd[0];
		result->backrdsum[1] += backrd[1];
		result->backrdsum[2] += backrd[2];
	}
}

static void traverse_octree(ScatterTree *tree, ScatterNode *node, float *co, int self, ScatterResult *result)
{
	float sub[3], dist;
	int i, index = 0;

	if(node->totpoint > 0) {
		/* leaf - add radiance from all samples */
		for(i=0; i<node->totpoint; i++) {
			ScatterPoint *p= &node->points[i];

			sub_v3_v3v3(sub, co, p->co);
			dist= dot_v3v3(sub, sub);

			if(p->back)
				add_radiance(tree, NULL, p->rad, 0.0f, p->area, dist, result);
			else
				add_radiance(tree, p->rad, NULL, p->area, 0.0f, dist, result);
		}
	}
	else {
		/* branch */
		if (self)
			index = SUBNODE_INDEX(co, node->split);

		for(i=0; i<8; i++) {
			if(node->child[i]) {
				ScatterNode *subnode= node->child[i];

				if(self && index == i) {
					/* always traverse node containing the point */
					traverse_octree(tree, subnode, co, 1, result);
				}
				else {
					/* decide subnode traversal based on maximum solid angle */
					sub_v3_v3v3(sub, co, subnode->co);
					dist= dot_v3v3(sub, sub);

					/* actually area/dist > error, but this avoids division */
					if(subnode->area+subnode->backarea>tree->error*dist) {
						traverse_octree(tree, subnode, co, 0, result);
					}
					else {
						add_radiance(tree, subnode->rad, subnode->backrad,
							subnode->area, subnode->backarea, dist, result);
					}
				}
			}
		}
	}
}

static void compute_radiance(ScatterTree *tree, float *co, float *rad)
{
	ScatterResult result;
	float rdsum[3], backrad[3], backrdsum[3];

	memset(&result, 0, sizeof(result));

	traverse_octree(tree, tree->root, co, 1, &result);

	/* the original paper doesn't do this, but we normalize over the
	 * sampled area and multiply with the reflectance. this is because
	 * our point samples are incomplete, there are no samples on parts
	 * of the mesh not visible from the camera. this can not only make
	 * it darker, but also lead to ugly color shifts */

	mul_v3_fl(result.rad, tree->ss[0]->frontweight);
	mul_v3_fl(result.backrad, tree->ss[0]->backweight);

	copy_v3_v3(rad, result.rad);
	add_v3_v3v3(backrad, result.rad, result.backrad);

	copy_v3_v3(rdsum, result.rdsum);
	add_v3_v3v3(backrdsum, result.rdsum, result.backrdsum);

	if(rdsum[0] > 1e-16f) rad[0]= tree->ss[0]->color*rad[0]/rdsum[0];
	if(rdsum[1] > 1e-16f) rad[1]= tree->ss[1]->color*rad[1]/rdsum[1];
	if(rdsum[2] > 1e-16f) rad[2]= tree->ss[2]->color*rad[2]/rdsum[2];

	if(backrdsum[0] > 1e-16f) backrad[0]= tree->ss[0]->color*backrad[0]/backrdsum[0];
	if(backrdsum[1] > 1e-16f) backrad[1]= tree->ss[1]->color*backrad[1]/backrdsum[1];
	if(backrdsum[2] > 1e-16f) backrad[2]= tree->ss[2]->color*backrad[2]/backrdsum[2];

	rad[0]= MAX2(rad[0], backrad[0]);
	rad[1]= MAX2(rad[1], backrad[1]);
	rad[2]= MAX2(rad[2], backrad[2]);
}

/* building */

static void sum_leaf_radiance(ScatterTree *UNUSED(tree), ScatterNode *node)
{
	ScatterPoint *p;
	float rad, totrad= 0.0f, inv;
	int i;

	node->co[0]= node->co[1]= node->co[2]= 0.0;
	node->rad[0]= node->rad[1]= node->rad[2]= 0.0;
	node->backrad[0]= node->backrad[1]= node->backrad[2]= 0.0;

	/* compute total rad, rad weighted average position,
	 * and total area */
	for(i=0; i<node->totpoint; i++) {
		p= &node->points[i];

		rad= p->area*fabsf(p->rad[0] + p->rad[1] + p->rad[2]);
		totrad += rad;

		node->co[0] += rad*p->co[0];
		node->co[1] += rad*p->co[1];
		node->co[2] += rad*p->co[2];

		if(p->back) {
			node->backrad[0] += p->rad[0]*p->area;
			node->backrad[1] += p->rad[1]*p->area;
			node->backrad[2] += p->rad[2]*p->area;

			node->backarea += p->area;
		}
		else {
			node->rad[0] += p->rad[0]*p->area;
			node->rad[1] += p->rad[1]*p->area;
			node->rad[2] += p->rad[2]*p->area;

			node->area += p->area;
		}
	}

	if(node->area > 1e-16f) {
		inv= 1.0f/node->area;
		node->rad[0] *= inv;
		node->rad[1] *= inv;
		node->rad[2] *= inv;
	}
	if(node->backarea > 1e-16f) {
		inv= 1.0f/node->backarea;
		node->backrad[0] *= inv;
		node->backrad[1] *= inv;
		node->backrad[2] *= inv;
	}

	if(totrad > 1e-16f) {
		inv= 1.0f/totrad;
		node->co[0] *= inv;
		node->co[1] *= inv;
		node->co[2] *= inv;
	}
	else {
		/* make sure that if radiance is 0.0f, we still have these points in
		 * the tree at a good position, they count for rdsum too */
		for(i=0; i<node->totpoint; i++) {
			p= &node->points[i];

			node->co[0] += p->co[0];
			node->co[1] += p->co[1];
			node->co[2] += p->co[2];
		}

		node->co[0] /= node->totpoint;
		node->co[1] /= node->totpoint;
		node->co[2] /= node->totpoint;
	}
}

static void sum_branch_radiance(ScatterTree *UNUSED(tree), ScatterNode *node)
{
	ScatterNode *subnode;
	float rad, totrad= 0.0f, inv;
	int i, totnode;

	node->co[0]= node->co[1]= node->co[2]= 0.0;
	node->rad[0]= node->rad[1]= node->rad[2]= 0.0;
	node->backrad[0]= node->backrad[1]= node->backrad[2]= 0.0;

	/* compute total rad, rad weighted average position,
	 * and total area */
	for(i=0; i<8; i++) {
		if(node->child[i] == NULL)
			continue;

		subnode= node->child[i];

		rad= subnode->area*fabsf(subnode->rad[0] + subnode->rad[1] + subnode->rad[2]);
		rad += subnode->backarea*fabsf(subnode->backrad[0] + subnode->backrad[1] + subnode->backrad[2]);
		totrad += rad;

		node->co[0] += rad*subnode->co[0];
		node->co[1] += rad*subnode->co[1];
		node->co[2] += rad*subnode->co[2];

		node->rad[0] += subnode->rad[0]*subnode->area;
		node->rad[1] += subnode->rad[1]*subnode->area;
		node->rad[2] += subnode->rad[2]*subnode->area;

		node->backrad[0] += subnode->backrad[0]*subnode->backarea;
		node->backrad[1] += subnode->backrad[1]*subnode->backarea;
		node->backrad[2] += subnode->backrad[2]*subnode->backarea;

		node->area += subnode->area;
		node->backarea += subnode->backarea;
	}

	if(node->area > 1e-16f) {
		inv= 1.0f/node->area;
		node->rad[0] *= inv;
		node->rad[1] *= inv;
		node->rad[2] *= inv;
	}
	if(node->backarea > 1e-16f) {
		inv= 1.0f/node->backarea;
		node->backrad[0] *= inv;
		node->backrad[1] *= inv;
		node->backrad[2] *= inv;
	}

	if(totrad > 1e-16f) {
		inv= 1.0f/totrad;
		node->co[0] *= inv;
		node->co[1] *= inv;
		node->co[2] *= inv;
	}
	else {
		/* make sure that if radiance is 0.0f, we still have these points in
		 * the tree at a good position, they count for rdsum too */
		totnode= 0;

		for(i=0; i<8; i++) {
			if(node->child[i]) {
				subnode= node->child[i];

				node->co[0] += subnode->co[0];
				node->co[1] += subnode->co[1];
				node->co[2] += subnode->co[2];

				totnode++;
			}
		}

		node->co[0] /= totnode;
		node->co[1] /= totnode;
		node->co[2] /= totnode;
	}
}

static void sum_radiance(ScatterTree *tree, ScatterNode *node)
{
	if(node->totpoint > 0) {
		sum_leaf_radiance(tree, node);
	}
	else {
		int i;

		for(i=0; i<8; i++)
			if(node->child[i])
				sum_radiance(tree, node->child[i]);

		sum_branch_radiance(tree, node);
	}
}

static void subnode_middle(int i, float *mid, float *subsize, float *submid)
{
	int x= i & 1, y= i & 2, z= i & 4;

	submid[0]= mid[0] + ((x)? subsize[0]: -subsize[0]);
	submid[1]= mid[1] + ((y)? subsize[1]: -subsize[1]);
	submid[2]= mid[2] + ((z)? subsize[2]: -subsize[2]);
}

static void create_octree_node(ScatterTree *tree, ScatterNode *node, float *mid, float *size, ScatterPoint **refpoints, int depth)
{
	ScatterNode *subnode;
	ScatterPoint **subrefpoints, **tmppoints= tree->tmppoints;
	int index, nsize[8], noffset[8], i, subco, usednodes, usedi;
	float submid[3], subsize[3];

	/* stopping condition */
	if(node->totpoint <= MAX_OCTREE_NODE_POINTS || depth == MAX_OCTREE_DEPTH) {
		for(i=0; i<node->totpoint; i++)
			node->points[i]= *(refpoints[i]);

		return;
	}

	subsize[0]= size[0]*0.5f;
	subsize[1]= size[1]*0.5f;
	subsize[2]= size[2]*0.5f;

	node->split[0]= mid[0];
	node->split[1]= mid[1];
	node->split[2]= mid[2];

	memset(nsize, 0, sizeof(nsize));
	memset(noffset, 0, sizeof(noffset));

	/* count points in subnodes */
	for(i=0; i<node->totpoint; i++) {
		index= SUBNODE_INDEX(refpoints[i]->co, node->split);
		tmppoints[i]= refpoints[i];
		nsize[index]++;
	}

	/* here we check if only one subnode is used. if this is the case, we don't
	 * create a new node, but rather call this function again, with different
	 * size and middle position for the same node. */
	for(usedi=0, usednodes=0, i=0; i<8; i++) {
		if(nsize[i]) {
			usednodes++;
			usedi = i;
		}
		if(i != 0)
			noffset[i]= noffset[i-1]+nsize[i-1];
	}
	
	if(usednodes<=1) {
		subnode_middle(usedi, mid, subsize, submid);
		create_octree_node(tree, node, submid, subsize, refpoints, depth+1);
		return;
	}

	/* reorder refpoints by subnode */
	for(i=0; i<node->totpoint; i++) {
		index= SUBNODE_INDEX(tmppoints[i]->co, node->split);
		refpoints[noffset[index]]= tmppoints[i];
		noffset[index]++;
	}

	/* create subnodes */
	for(subco=0, i=0; i<8; subco+=nsize[i], i++) {
		if(nsize[i] > 0) {
			subnode= BLI_memarena_alloc(tree->arena, sizeof(ScatterNode));
			node->child[i]= subnode;
			subnode->points= node->points + subco;
			subnode->totpoint= nsize[i];
			subrefpoints= refpoints + subco;

			subnode_middle(i, mid, subsize, submid);

			create_octree_node(tree, subnode, submid, subsize, subrefpoints,
				depth+1);
		}
		else
			node->child[i]= NULL;
	}

	node->points= NULL;
	node->totpoint= 0;
}

/* public functions */

ScatterTree *scatter_tree_new(ScatterSettings *ss[3], float scale, float error,
	float (*co)[3], float (*color)[3], float *area, int totpoint)
{
	ScatterTree *tree;
	ScatterPoint *points, **refpoints;
	int i;

	/* allocate tree */
	tree= MEM_callocN(sizeof(ScatterTree), "ScatterTree");
	tree->scale= scale;
	tree->error= error;
	tree->totpoint= totpoint;

	tree->ss[0]= ss[0];
	tree->ss[1]= ss[1];
	tree->ss[2]= ss[2];

	points= MEM_callocN(sizeof(ScatterPoint)*totpoint, "ScatterPoints");
	refpoints= MEM_callocN(sizeof(ScatterPoint*)*totpoint, "ScatterRefPoints");

	tree->points= points;
	tree->refpoints= refpoints;

	/* build points */
	INIT_MINMAX(tree->min, tree->max);

	for(i=0; i<totpoint; i++) {
		copy_v3_v3(points[i].co, co[i]);
		copy_v3_v3(points[i].rad, color[i]);
		points[i].area= fabsf(area[i])/(tree->scale*tree->scale);
		points[i].back= (area[i] < 0.0f);

		mul_v3_fl(points[i].co, 1.0f/tree->scale);
		DO_MINMAX(points[i].co, tree->min, tree->max);

		refpoints[i]= points + i;
	}

	return tree;
}

void scatter_tree_build(ScatterTree *tree)
{
	ScatterPoint *newpoints, **tmppoints;
	float mid[3], size[3];
	int totpoint= tree->totpoint;

	newpoints= MEM_callocN(sizeof(ScatterPoint)*totpoint, "ScatterPoints");
	tmppoints= MEM_callocN(sizeof(ScatterPoint*)*totpoint, "ScatterTmpPoints");
	tree->tmppoints= tmppoints;

	tree->arena= BLI_memarena_new(0x8000 * sizeof(ScatterNode), "sss tree arena");
	BLI_memarena_use_calloc(tree->arena);

	/* build tree */
	tree->root= BLI_memarena_alloc(tree->arena, sizeof(ScatterNode));
	tree->root->points= newpoints;
	tree->root->totpoint= totpoint;

	mid[0]= (tree->min[0]+tree->max[0])*0.5f;
	mid[1]= (tree->min[1]+tree->max[1])*0.5f;
	mid[2]= (tree->min[2]+tree->max[2])*0.5f;

	size[0]= (tree->max[0]-tree->min[0])*0.5f;
	size[1]= (tree->max[1]-tree->min[1])*0.5f;
	size[2]= (tree->max[2]-tree->min[2])*0.5f;

	create_octree_node(tree, tree->root, mid, size, tree->refpoints, 0);

	MEM_freeN(tree->points);
	MEM_freeN(tree->refpoints);
	MEM_freeN(tree->tmppoints);
	tree->refpoints= NULL;
	tree->tmppoints= NULL;
	tree->points= newpoints;
	
	/* sum radiance at nodes */
	sum_radiance(tree, tree->root);
}

void scatter_tree_sample(ScatterTree *tree, float *co, float *color)
{
	float sco[3];

	copy_v3_v3(sco, co);
	mul_v3_fl(sco, 1.0f/tree->scale);

	compute_radiance(tree, sco, color);
}

void scatter_tree_free(ScatterTree *tree)
{
	if (tree->arena) BLI_memarena_free(tree->arena);
	if (tree->points) MEM_freeN(tree->points);
	if (tree->refpoints) MEM_freeN(tree->refpoints);
		
	MEM_freeN(tree);
}

/* Internal Renderer API */

/* sss tree building */

typedef struct SSSData {
	ScatterTree *tree;
	ScatterSettings *ss[3];
} SSSData;

typedef struct SSSPoints {
	struct SSSPoints *next, *prev;

	float (*co)[3];
	float (*color)[3];
	float *area;
	int totpoint;
} SSSPoints;

static void sss_create_tree_mat(Render *re, Material *mat)
{
	SSSPoints *p;
	RenderResult *rr;
	ListBase points;
	float (*co)[3] = NULL, (*color)[3] = NULL, *area = NULL;
	int totpoint = 0, osa, osaflag, partsdone;

	if(re->test_break(re->tbh))
		return;
	
	points.first= points.last= NULL;

	/* TODO: this is getting a bit ugly, copying all those variables and
	 * setting them back, maybe we need to create our own Render? */

	/* do SSS preprocessing render */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	rr= re->result;
	osa= re->osa;
	osaflag= re->r.mode & R_OSA;
	partsdone= re->i.partsdone;

	re->osa= 0;
	re->r.mode &= ~R_OSA;
	re->sss_points= &points;
	re->sss_mat= mat;
	re->i.partsdone= 0;

	if(!(re->r.scemode & R_PREVIEWBUTS))
		re->result= NULL;
	BLI_rw_mutex_unlock(&re->resultmutex);

	RE_TileProcessor(re);
	
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	if(!(re->r.scemode & R_PREVIEWBUTS)) {
		RE_FreeRenderResult(re->result);
		re->result= rr;
	}
	BLI_rw_mutex_unlock(&re->resultmutex);

	re->i.partsdone= partsdone;
	re->sss_mat= NULL;
	re->sss_points= NULL;
	re->osa= osa;
	if (osaflag) re->r.mode |= R_OSA;

	/* no points? no tree */
	if(!points.first)
		return;

	/* merge points together into a single buffer */
	if(!re->test_break(re->tbh)) {
		for(totpoint=0, p=points.first; p; p=p->next)
			totpoint += p->totpoint;
		
		co= MEM_mallocN(sizeof(*co)*totpoint, "SSSCo");
		color= MEM_mallocN(sizeof(*color)*totpoint, "SSSColor");
		area= MEM_mallocN(sizeof(*area)*totpoint, "SSSArea");

		for(totpoint=0, p=points.first; p; p=p->next) {
			memcpy(co+totpoint, p->co, sizeof(*co)*p->totpoint);
			memcpy(color+totpoint, p->color, sizeof(*color)*p->totpoint);
			memcpy(area+totpoint, p->area, sizeof(*area)*p->totpoint);
			totpoint += p->totpoint;
		}
	}

	/* free points */
	for(p=points.first; p; p=p->next) {
		MEM_freeN(p->co);
		MEM_freeN(p->color);
		MEM_freeN(p->area);
	}
	BLI_freelistN(&points);

	/* build tree */
	if(!re->test_break(re->tbh)) {
		SSSData *sss= MEM_callocN(sizeof(*sss), "SSSData");
		float ior= mat->sss_ior, cfac= mat->sss_colfac;
		float *radius= mat->sss_radius;
		float fw= mat->sss_front, bw= mat->sss_back;
		float error = mat->sss_error;

		error= get_render_aosss_error(&re->r, error);
		if((re->r.scemode & R_PREVIEWBUTS) && error < 0.5f)
			error= 0.5f;
		
		sss->ss[0]= scatter_settings_new(mat->sss_col[0], radius[0], ior, cfac, fw, bw);
		sss->ss[1]= scatter_settings_new(mat->sss_col[1], radius[1], ior, cfac, fw, bw);
		sss->ss[2]= scatter_settings_new(mat->sss_col[2], radius[2], ior, cfac, fw, bw);
		sss->tree= scatter_tree_new(sss->ss, mat->sss_scale, error,
			co, color, area, totpoint);

		MEM_freeN(co);
		MEM_freeN(color);
		MEM_freeN(area);

		scatter_tree_build(sss->tree);

		BLI_ghash_insert(re->sss_hash, mat, sss);
	}
	else {
		if (co) MEM_freeN(co);
		if (color) MEM_freeN(color);
		if (area) MEM_freeN(area);
	}
}

void sss_add_points(Render *re, float (*co)[3], float (*color)[3], float *area, int totpoint)
{
	SSSPoints *p;
	
	if(totpoint > 0) {
		p= MEM_callocN(sizeof(SSSPoints), "SSSPoints");

		p->co= co;
		p->color= color;
		p->area= area;
		p->totpoint= totpoint;

		BLI_lock_thread(LOCK_CUSTOM1);
		BLI_addtail(re->sss_points, p);
		BLI_unlock_thread(LOCK_CUSTOM1);
	}
}

static void sss_free_tree(SSSData *sss)
{
	scatter_tree_free(sss->tree);
	scatter_settings_free(sss->ss[0]);
	scatter_settings_free(sss->ss[1]);
	scatter_settings_free(sss->ss[2]);
	MEM_freeN(sss);
}

/* public functions */

void make_sss_tree(Render *re)
{
	Material *mat;
	
	re->sss_hash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "make_sss_tree gh");

	re->i.infostr= "SSS preprocessing";
	re->stats_draw(re->sdh, &re->i);
	
	for(mat= re->main->mat.first; mat; mat= mat->id.next)
		if(mat->id.us && (mat->flag & MA_IS_USED) && (mat->sss_flag & MA_DIFF_SSS))
			sss_create_tree_mat(re, mat);
	
	/* XXX preview exception */
	/* localizing preview render data is not fun for node trees :( */
	if(re->main!=G.main) {
		for(mat= G.main->mat.first; mat; mat= mat->id.next)
			if(mat->id.us && (mat->flag & MA_IS_USED) && (mat->sss_flag & MA_DIFF_SSS))
				sss_create_tree_mat(re, mat);
	}
	
}

void free_sss(Render *re)
{
	if(re->sss_hash) {
		GHashIterator *it= BLI_ghashIterator_new(re->sss_hash);

		while(!BLI_ghashIterator_isDone(it)) {
			sss_free_tree(BLI_ghashIterator_getValue(it));
			BLI_ghashIterator_step(it);
		}

		BLI_ghashIterator_free(it);
		BLI_ghash_free(re->sss_hash, NULL, NULL);
		re->sss_hash= NULL;
	}
}

int sample_sss(Render *re, Material *mat, float *co, float *color)
{
	if(re->sss_hash) {
		SSSData *sss= BLI_ghash_lookup(re->sss_hash, mat);

		if(sss) {
			scatter_tree_sample(sss->tree, co, color);
			return 1;
		}
		else {
			color[0]= 0.0f;
			color[1]= 0.0f;
			color[2]= 0.0f;
		}
	}

	return 0;
}

int sss_pass_done(struct Render *re, struct Material *mat)
{
	return ((re->flag & R_BAKING) || !(re->r.mode & R_SSS) || (re->sss_hash && BLI_ghash_lookup(re->sss_hash, mat)));
}

