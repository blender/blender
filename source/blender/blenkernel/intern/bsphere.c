/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2011 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_utildefines.h"
#include "BLI_heap.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_subsurf.h"

#include <assert.h>

typedef struct Tri {
	struct Tri *next, *prev;
	unsigned int v[3];
	float no[3];
	float area;
	int marked;
} Tri;

typedef struct Edge {
	struct Edge *next, *prev;
	unsigned int v[2];
	Tri *tri[2];
} Edge;

/* note: a `frame' is four vertices (contiguous within the MVert
   array), stored simply as the index of the first vertex */

static void create_frame(float frame[4][3], const float co[3], float radius,
			 const float mat[3][3], float x_offset)
{
	float rx[3], ry[3], rz[3];
	int i;

	mul_v3_v3fl(ry, mat[1], radius);
	mul_v3_v3fl(rz, mat[2], radius);
	
	add_v3_v3v3(frame[3], co, ry);
	add_v3_v3v3(frame[3], frame[3], rz);

	sub_v3_v3v3(frame[2], co, ry);
	add_v3_v3v3(frame[2], frame[2], rz);

	sub_v3_v3v3(frame[1], co, ry);
	sub_v3_v3v3(frame[1], frame[1], rz);

	add_v3_v3v3(frame[0], co, ry);
	sub_v3_v3v3(frame[0], frame[0], rz);

	mul_v3_v3fl(rx, mat[0], x_offset);
	for(i = 0; i < 4; i++)
		add_v3_v3v3(frame[i], frame[i], rx);
}

static void create_mid_frame(float frame[4][3],
			     const float co1[3], const float co2[3],
			     const SkinNode *n1, const SkinNode *n2,
			     const float mat[3][3])
{
	create_frame(frame, co1, (n1->radius + n2->radius) / 2,
		     mat, len_v3v3(co1, co2) / 2);
}

static void add_face(MFace *mface, int *totface,
		     int v1, int v2, int v3, int v4)
{
	MFace *f;

	f = &mface[*totface];
	f->v1 = v1;
	f->v2 = v2;
	f->v3 = v3;
	f->v4 = v4 == -1 ? 0 : v4;
	if(!v4) {
		f->v1 = v3;
		f->v2 = v4;
		f->v3 = v1;
		f->v4 = v2;
	}
	(*totface)++;
}

static void connect_frames(MFace *mface, int *totface, int frame1, int frame2)
{
	int i;

	for(i = 0; i < 4; i++) {
		add_face(mface, totface,
			 frame2 + i,
			 frame2 + (i+1) % 4,
			 frame1 + (i+1) % 4,
			 frame1 + i);
	}
}

static Tri *add_triangle(ListBase *tris, MVert *mvert, int v1, int v2, int v3)
{
	Tri *t;

	t = MEM_callocN(sizeof(Tri), "add_triangle.tri");
	t->v[0] = v1;
	t->v[1] = v2;
	t->v[2] = v3;
	BLI_addtail(tris, t);

	normal_tri_v3(t->no, mvert[t->v[0]].co, mvert[t->v[1]].co, mvert[t->v[2]].co);
	t->area = area_tri_v3(mvert[t->v[0]].co, mvert[t->v[1]].co, mvert[t->v[2]].co);

	return t;
}

static int point_tri_side(const Tri *t, MVert *mvert, int v)
{
	float p[3], d;
	sub_v3_v3v3(p, mvert[v].co, mvert[t->v[0]].co);
	d = dot_v3v3(t->no, p);
	if(d < 0) return -1;
	else if(d > 0) return 1;
	else return 0;
}

static int mark_v_outside_tris(ListBase *tris, MVert *mvert, int v)
{
	Tri *t;
	int outside = 0;

	/* probably there's a much better way to do this */
	for(t = tris->first; t; t = t->next) {
		if((t->marked = point_tri_side(t, mvert, v) > 0))
			outside = 1;
	}
	return outside;
}

static int edge_match(int e1_0, int e1_1, const unsigned int e2[2])
{
	/* XXX: maybe isn't necesseary to check both directions? */
	return (e1_0 == e2[0] && e1_1 == e2[1]) ||
	       (e1_0 == e2[1] && e1_1 == e2[0]);
}

/* returns true if the edge (e1, e2) is already in edges; that edge is
   deleted here as well. if not found just returns 0 */
static int check_for_dup(ListBase *edges, unsigned int e1, unsigned int e2)
{
	Edge *e, *next;

	for(e = edges->first; e; e = next) {
		next = e->next;

		if(edge_match(e1, e2, e->v)) {
			/* remove the interior edge */
			BLI_freelinkN(edges, e);
			return 1;
		}
	}

	return 0;
}

static void expand_boundary_edges(ListBase *edges, Tri *t)
{
	Edge *new;
	int i;

	/* insert each triangle edge into the boundary list; if any of
	   its edges are already in there, remove the edge entirely */
	for(i = 0; i < 3; i++) {
		if(!check_for_dup(edges, t->v[i], t->v[(i+1)%3])) {
			new = MEM_callocN(sizeof(Edge), "Edge");
			new->v[0] = t->v[i];
			new->v[1] = t->v[(i+1)%3];
			BLI_addtail(edges, new);
		}
	}
}

static int tri_matches_frame(const Tri *t, int frame)
{
	int i, j, match = 0;
	for(i = 0; i < 3; i++) {
		for(j = 0; j < 4; j++) {
			if(t->v[i] == frame + j) {
				match++;
				if(match >= 3)
					return 1;
			}
		}
	}
	return 0;
}

static void quad_from_tris(const Edge *e, int ndx[4])
{
	int i, j, opp = -1;

	/* find what the second tri has that the first doesn't */
	for(i = 0; i < 3; i++) {
		if(e->tri[1]->v[i] != e->tri[0]->v[0] &&
		   e->tri[1]->v[i] != e->tri[0]->v[1] &&
		   e->tri[1]->v[i] != e->tri[0]->v[2]) {
			opp = e->tri[1]->v[i];
			break;
		}
	}
	assert(opp != -1);

	for(i = 0, j = 0; i < 3; i++, j++) {
		ndx[j] = e->tri[0]->v[i];
		/* when the triangle edge cuts across our quad-to-be,
		   throw in the second triangle's vertex */
		if((e->tri[0]->v[i] == e->v[0] || e->tri[0]->v[i] == e->v[1]) &&
		   (e->tri[0]->v[(i+1)%3] == e->v[0] || e->tri[0]->v[(i+1)%3] == e->v[1])) {
			j++;
			ndx[j] = opp;
		}
	}
}

static void add_quad_from_tris(MFace *mface, int *totface, const Edge *e)
{
	int ndx[4];

	quad_from_tris(e, ndx);

	add_face(mface, totface, ndx[0], ndx[1], ndx[2], ndx[3]);
}

static GHash *calc_edges(ListBase *tris)
{
	GHash *adj;
	ListBase *edges;
	Edge *e;
	Tri *t;
	int i, e1, e2;

	/* XXX: vertex range might be a little funky? so using a
	   hash here */
	adj = BLI_ghash_new(BLI_ghashutil_inthash,
			    BLI_ghashutil_intcmp,
			    "calc_edges adj");

	for(t = tris->first; t; t = t->next) {
		for(i = 0; i < 3; i++) {
			e1 = t->v[i];
			e2 = t->v[(i+1)%3];
			assert(e1 != e2);
			if(e1 > e2)
				SWAP(int, e1, e2);

			edges = BLI_ghash_lookup(adj, SET_INT_IN_POINTER(e1));
			if(!edges) {
				edges = MEM_callocN(sizeof(ListBase),
						    "calc_edges ListBase");
				BLI_ghash_insert(adj, SET_INT_IN_POINTER(e1), edges);
			}

			/* find the edge in the adjacency list */
			for(e = edges->first; e; e = e->next) {
				assert(e->v[0] == e1);
				if(e->v[1] == e2)
					break;
			}

			/* if not found, create the edge */
			if(!e) {
				e = MEM_callocN(sizeof(Edge), "calc_edges Edge");
				e->v[0] = e1;
				e->v[1] = e2;
				BLI_addtail(edges, e);
			}

			/* should never be more than two faces
			   attached to an edge here */
			assert(!e->tri[0] || !e->tri[1]);

			if(!e->tri[0])
				e->tri[0] = t;
			else if(!e->tri[1])
				e->tri[1] = t;
		}
	}

	return adj;
}

static int is_quad_symmetric(const MVert *mvert, const Edge *e)
{
	int ndx[4];
	float a[3];

	quad_from_tris(e, ndx);

	copy_v3_v3(a, mvert[ndx[0]].co);
	a[0] = -a[0];

	if(len_v3v3(a, mvert[ndx[1]].co) < FLT_EPSILON) {
		copy_v3_v3(a, mvert[ndx[2]].co);
		a[0] = -a[0];
		if(len_v3v3(a, mvert[ndx[3]].co) < FLT_EPSILON)
			return 1;
	}
	else if(len_v3v3(a, mvert[ndx[3]].co) < FLT_EPSILON) {
		copy_v3_v3(a, mvert[ndx[2]].co);
		a[0] = -a[0];
		if(len_v3v3(a, mvert[ndx[1]].co) < FLT_EPSILON)
			return 1;
	}

	return 0;
}

static void output_hull(const MVert *mvert, MFace *mface, int *totface, ListBase *tris)
{
	Heap *heap;
	GHash *adj;
	GHashIterator *iter;
	ListBase *edges;
	Edge *e;
	Tri *t;
	float score;

	heap = BLI_heap_new();
	adj = calc_edges(tris);

	/* unmark all triangles */
	for(t = tris->first; t; t = t->next)
		t->marked = 0;

	/* build heap */
	iter = BLI_ghashIterator_new(adj);
	while(!BLI_ghashIterator_isDone(iter)) {
		edges = BLI_ghashIterator_getValue(iter);
		for(e = edges->first; e; e = e->next) {
			/* only care if the edge is used by more than
			   one triangle */
			if(e->tri[0] && e->tri[1]) {
				score = (e->tri[0]->area + e->tri[1]->area) *
					dot_v3v3(e->tri[0]->no, e->tri[1]->no);

				/* increase score if the triangles
				   form a symmetric quad */
				if(is_quad_symmetric(mvert, e))
					score *= 10;

				BLI_heap_insert(heap, -score, e);
			}
		}

		BLI_ghashIterator_step(iter);
	}
	BLI_ghashIterator_free(iter);

	while(!BLI_heap_empty(heap)) {
		e = BLI_heap_popmin(heap);
		
		/* if both triangles still free, outupt as a quad */
		if(!e->tri[0]->marked && !e->tri[1]->marked) {
			add_quad_from_tris(mface, totface, e);
			e->tri[0]->marked = 1;
			e->tri[1]->marked = 1;
		}
	}

	/* free edge list */
	iter = BLI_ghashIterator_new(adj);
	while(!BLI_ghashIterator_isDone(iter)) {
		edges = BLI_ghashIterator_getValue(iter);
		BLI_freelistN(edges);
		MEM_freeN(edges);	
		BLI_ghashIterator_step(iter);
	}
	BLI_ghashIterator_free(iter);
	BLI_ghash_free(adj, NULL, NULL);
	BLI_heap_free(heap, NULL);

	/* write out any remaining triangles */
	for(t = tris->first; t; t = t->next) {
		if(!t->marked)
			add_face(mface, totface, t->v[0], t->v[1], t->v[2], -1);
	}
}	

/* for vertex `v', find which triangles must be deleted to extend the
   hull; find the boundary edges of that hole so that it can be filled
   with connections to the new vertex, and update the `tri' list to
   delete the marked triangles */
static void add_point(ListBase *tris, MVert *mvert, int v)
{
	ListBase edges = {NULL, NULL};
	Tri *t, *next;
	Edge *e;

	for(t = tris->first; t; t = next) {
		next = t->next;

		/* check if triangle is `visible' to v */
		if(t->marked) {
			expand_boundary_edges(&edges, t);
			/* remove the triangle */
			BLI_freelinkN(tris, t);
		}
	}

	/* fill hole boundary with triangles to new point */
	for(e = edges.first; e; e = e->next)
		add_triangle(tris, mvert, e->v[0], e->v[1], v);
	BLI_freelistN(&edges);
}

static void build_hull(MFace *mface, int *totface,
		       MVert *mvert, int *frames, int totframe)
{
	ListBase tris = {NULL, NULL};
	Tri *t, *next;
	int i, j;

	/* use first frame to make initial degenerate pyramid */
	add_triangle(&tris, mvert, frames[0], frames[0] + 1, frames[0] + 2);
	add_triangle(&tris, mvert, frames[0] + 1, frames[0], frames[0] + 3);
	add_triangle(&tris, mvert, frames[0] + 2, frames[0] + 1, frames[0] + 3);
	add_triangle(&tris, mvert, frames[0], frames[0] + 2, frames[0] + 3);

	for(i = 1; i < totframe; i++) {
		for(j = 0; j < 4; j++) {
			int v = frames[i] + j;

			/* ignore point already inside hull */
			if(mark_v_outside_tris(&tris, mvert, v)) {
				/* expand hull and delete interior triangles */
				add_point(&tris, mvert, v);
			}
		}
	}

	/* remove triangles that would fill the original frames */
	for(t = tris.first; t; t = next) {
		next = t->next;

		for(i = 0; i < totframe; i++) {
			if(tri_matches_frame(t, frames[i])) {
				BLI_freelinkN(&tris, t);
				break;
			}
		}
	}

	output_hull(mvert, mface, totface, &tris);

	BLI_freelistN(&tris);
}

/* test: build hull on origdm */
#if 0
static DerivedMesh *test_hull(DerivedMesh *origdm)
{
	DerivedMesh *dm;
	MVert *mvert, *origmvert;
	MFace *mface;
	int *frames;
	int totvert = 0, totface = 0, totframe, i;

	/* TODO */
	totface = 2000;
	totvert = origdm->getNumVerts(origdm);
	dm = CDDM_new(totvert, 0, totface);

	mvert = CDDM_get_verts(dm);
	mface = CDDM_get_faces(dm);

	origmvert = CDDM_get_verts(origdm);
	for(i = 0; i < totvert; i++)
		mvert[i] = origmvert[i];

	assert(totvert % 4 == 0);
	totframe = totvert / 4;

	frames = MEM_callocN(sizeof(int) * totframe, "frames");
	for(i = 0; i < totframe; i++)
		frames[i] = i*4;

	totface = 0;
	build_hull(mface, &totface, mvert, frames, totframe);

	CDDM_calc_edges(dm);
	//CDDM_calc_normals(dm);

	MEM_freeN(frames);

	return dm;
}
#endif

/* TODO */
static void calc_edge_mat(float mat[3][3], const float a[3], const float b[3])
{
	float Z[3] = {0, 0, 1};
	float dot;

	/* x = edge direction */
	sub_v3_v3v3(mat[0], b, a);
	normalize_v3(mat[0]);

	dot = dot_v3v3(mat[0], Z);
	if(dot > -1 + FLT_EPSILON && dot < 1 - FLT_EPSILON) {
		/* y = Z cross x */
		cross_v3_v3v3(mat[1], Z, mat[0]);
		normalize_v3(mat[1]);

		/* z = x cross y */
		cross_v3_v3v3(mat[2], mat[0], mat[1]);
		normalize_v3(mat[2]);
	}
	else {
		mat[1][0] = 1;
		mat[1][1] = 0;
		mat[1][2] = 0;
		mat[2][0] = 0;
		mat[2][1] = 1;
		mat[2][2] = 0;
	}
}

/* BMesh paper */
#if 0
static float calc_R_sub_i_squared(const BSphereNode *n)
{
	const float alpha = 1.5;
	//const float alpha = 1;
	const float R_sub_i = n->size[0] * alpha;
	return R_sub_i * R_sub_i;
}

/* equation (2) */
static float calc_r_squared(const float v[3], const BSphereNode *n)
{
	return len_squared_v3v3(v, n->co);
}

/* equation (1) */
static float calc_f_sub_i(const float v[3], const BSphereNode *n)
{
	float r_squared;
	float R_sub_i_squared;

	r_squared = calc_r_squared(v, n);
	R_sub_i_squared = calc_R_sub_i_squared(n);

	if(r_squared <= R_sub_i_squared)
		return powf(1.0f - (r_squared / R_sub_i_squared), 2);
	else
		return 0;
}

/* equation (3) */
static float calc_I(const float x[3], float T, const ListBase *base)
{
	BSphereNode *n;
	float a = -T;

	for(n = base->first; n; n = n->next) {
		a += calc_I(x, 0, &n->children);
		a += calc_f_sub_i(x, n);
	}

	//printf("calc_I: %f, %f\n", -T, a);

	return a;
}

/* kinda my own guess here */
static void calc_gradient(float g[3], const float x[3], const ListBase *base)
{
	BSphereNode *n;

	for(n = base->first; n; n = n->next) {
		float R_sub_i_squared = calc_R_sub_i_squared(n);
		float dist = len_v3v3(x, n->co);
		float f = calc_f_sub_i(x, n);
		float d = 1.0f / R_sub_i_squared;
		float tmp[3];

		/* XXX: other possibilities... */
		if(f > 0) {
			sub_v3_v3v3(tmp, x, n->co);
			mul_v3_fl(tmp, -4*d * (1 - d * dist));

			/* not sure if I'm doing this right; intent is
			   to flip direction when x is `beneath' the
			   level set */
			if(len_v3v3(x, n->co) > n->size[0])
				negate_v3(tmp);

			add_v3_v3(g, tmp);
		}

		calc_gradient(g, x, &n->children);
	}
}

/* equation (5) */
static float calc_F(const float x[3], const float K[2], float T, const ListBase *base)
{
	float f_of_K;
	float I_target;

	/* TODO */
	f_of_K = 1.0f / (1 + fabs(K[0]) + fabs(K[1]));
	//f_of_K = 1;

	/* TODO */
	I_target = 0;

	return (calc_I(x, T, base) - I_target) /*TODO: * f_of_K*/;
}

static float smallest_radius(const ListBase *base)
{
	BSphereNode *n;
	float s = FLT_MAX, t;

	for(n = base->first; n; n = n->next) {
		if(n->size[0] < s)
			s = n->size[0];
		if((t = smallest_radius(&n->children)) < s)
			s = t;
	}

	return s;
}

/* equation (7) */
static float calc_delta_t(float F_max, float k, const ListBase *base, int print)
{
	float min_r_sub_i;
	float step;

	min_r_sub_i = smallest_radius(base);
	step = min_r_sub_i / powf(2, k);

	if(print) {
		printf("min_r: %f, 2^k=%f, step=%f\n", min_r_sub_i, powf(2, k), step);
	}

	return step / F_max;
}

/* equation (6) */
static float evolve_x(float x[3], const float K[2], float T,
		      int k, const ListBase *base, int i, float F_max)
{
	float tmp[3], no[3];
	float F, dt;

	F = calc_F(x, K, T, base);

	if(F < FLT_EPSILON) {
		//printf("stability reached for ndx=%d\n", i);
		return 0;
	}

	/* TODO ;-) */
	if(F > F_max)
		F_max = F;

	dt = calc_delta_t(F_max, k, base, 0);

	dt = F / 2;

	if(i == 0)
		;//printf("F=%.3f, dt=%.3f\n", F, calc_delta_t(F_max, k, base, 1));
	
	zero_v3(no);
	calc_gradient(no, x, base);
	normalize_v3(no);
	negate_v3(no);

	mul_v3_v3fl(tmp, no, F * dt);
	add_v3_v3(x, tmp);

	return F_max;
}

static void bsphere_evolve(MVert *mvert, int totvert, const ListBase *base,
			   float T, int steps, float subdiv_level) 
{
	int i, s;

	for(i = 0; i < totvert; i++) {
		float F_max = 0;
		/* TODO: for now just doing a constant number of steps */
		for(s = 0; s < steps; s++) {
			/* TODO */
			float K[2] = {0, 0};

			F_max = evolve_x(mvert[i].co, K, T, subdiv_level, base, i, F_max);
		}
	}
}
#endif

typedef enum {
	START_CAP = 1,
	END_CAP = 2,
	NODE_BISECT = 4,
	CHILD_EDGE_BISECT = 8,
	
	IN_FRAME = 16,
	OUT_FRAME = 32,
} FrameFlag;

typedef struct {
	int totframe;
	int outbase;
	FrameFlag flag;
	float co[0][4][3];
} Frames;

static Frames *frames_create(FrameFlag flag)
{
	Frames *frames;
	int totframe;
	totframe = (!!(flag & START_CAP) + !!(flag & END_CAP) +
		    !!(flag & NODE_BISECT) + !!(flag & CHILD_EDGE_BISECT));
	assert(totframe);
	frames = MEM_callocN(sizeof(Frames) + sizeof(float)*totframe*4*3,
			     "frames_create.frames");
	frames->totframe = totframe;
	frames->flag = flag;
	assert((flag & (END_CAP|CHILD_EDGE_BISECT)) != (END_CAP|CHILD_EDGE_BISECT));
	return frames;
}

static int frame_offset(Frames *frames, FrameFlag flag)
{
	int n = 0;
	
	assert(flag == IN_FRAME || flag == OUT_FRAME ||
	       !(flag & (IN_FRAME|OUT_FRAME)));

	if(flag == IN_FRAME)
		return 0;
	else if(flag == OUT_FRAME)
		return frames->totframe-1;
	
	assert((flag & frames->flag) == flag);

	if(flag == START_CAP) return n;
	if(frames->flag & START_CAP) n++;

	if(flag == NODE_BISECT) return n;
	if(frames->flag & NODE_BISECT) n++;
	
	if(flag == END_CAP) return n;
	if(frames->flag & END_CAP) n++;

	if(flag == CHILD_EDGE_BISECT) return n;
	if(frames->flag & CHILD_EDGE_BISECT) n++;

	assert(!"bad frame offset flag");
}

static int frame_base(Frames *frames, FrameFlag flag)
{
	return frames->outbase + 4*frame_offset(frames, flag);
}

static void *frame_co(Frames *frames, FrameFlag flag)
{
	return frames->co[frame_offset(frames, flag)];
}

static int frames_totvert(Frames *frames)
{
	return frames->totframe*4;
}

#if 0
static int bsphere_build_frames(BSphereNode *n, GHash *ghash,
				float (*parent_mat)[3], FrameFlag flag);

/* builds the `cap' of polygons for the end of a BSphere chain */
static int bsphere_end_node_frames(BSphereNode *n,
				   GHash *ghash,
				   float parent_mat[3][3],
				   FrameFlag flag)
{
	Frames *frames;
	float rad;

	assert(flag == 0 || flag == START_CAP);
	frames = frames_create(NODE_BISECT|END_CAP|flag, n, ghash);

	/* TODO */
	rad = n->size[0];

	if(flag & START_CAP)
		create_frame(frame_co(frames, START_CAP), n, parent_mat, -rad, rad);

	/* frame to bisect the node */
	create_frame(frame_co(frames, NODE_BISECT), n, parent_mat, 0, rad);

	/* frame to cap end node */
	create_frame(frame_co(frames, END_CAP), n, parent_mat, rad, rad);

	return frames_totvert(frames);
}

static int bsphere_connection_node_frames(BSphereNode *n,
					  GHash *ghash,
					  float parent_mat[3][3],
					  FrameFlag flag)
{
	Frames *frames;
	BSphereNode *child;
	float childmat[3][3], nodemat[3][3], axis[3], angle, rad, child_rad;
	int totvert;

	assert(flag == 0 || flag == START_CAP);

	child = n->children.first;

	/* if child is not a branch node, bisect the child edge */
	if(BLI_countlist(&child->children) <= 1)
		flag |= CHILD_EDGE_BISECT;

	frames = frames_create(NODE_BISECT|flag, n, ghash);
	totvert = frames_totvert(frames);

	/* TODO */
	rad = n->size[0];
	child_rad = child->size[0];

	/* build matrix to give to child */
	sub_v3_v3v3(childmat[0], child->co, n->co);
	normalize_v3(childmat[0]);
	angle = angle_normalized_v3v3(parent_mat[0], childmat[0]);
	cross_v3_v3v3(axis, parent_mat[0], childmat[0]);
	normalize_v3(axis);
	rotate_normalized_v3_v3v3fl(childmat[1], parent_mat[1], axis, angle);
	rotate_normalized_v3_v3v3fl(childmat[2], parent_mat[2], axis, angle);

	/* build child */
	totvert += bsphere_build_frames(child, ghash, childmat, 0);

	/* frame that bisects the node */
	angle /= 2;
	copy_v3_v3(nodemat[0], parent_mat[0]);
	rotate_normalized_v3_v3v3fl(nodemat[1], parent_mat[1], axis, angle);
	rotate_normalized_v3_v3v3fl(nodemat[2], parent_mat[2], axis, angle);
	create_frame(frame_co(frames, NODE_BISECT), n, nodemat, 0, rad);

	if(flag & START_CAP)
		create_frame(frame_co(frames, START_CAP), n, nodemat, -rad, rad);

	if(flag & CHILD_EDGE_BISECT)
		create_mid_frame(frame_co(frames, CHILD_EDGE_BISECT), n, child, childmat);
	
	return totvert;
}

static int bsphere_branch_node_frames(BSphereNode *n, GHash *ghash)
{
	BSphereNode *child;
	int totvert = 0;
	float rad;

	rad = n->size[0];

	/* build children */
	for(child = n->children.first; child; child = child->next) {
		float childmat[3][3];

		calc_edge_mat(childmat, n->co, child->co);

		totvert += bsphere_build_frames(child, ghash, childmat, 0);
	}

	/* TODO: for now, builds no frames of its own */

	return totvert;
}

static int bsphere_build_frames(BSphereNode *n, GHash *ghash,
				float parent_mat[3][3], FrameFlag flag)
{
	BSphereNode *child;

	child = n->children.first;

	if(!child)
		return bsphere_end_node_frames(n, ghash, parent_mat, flag);
	else if(!child->next)
		return bsphere_connection_node_frames(n, ghash, parent_mat, flag);
	else
		return bsphere_branch_node_frames(n, ghash);
}

static GHash *bsphere_frames(BSphere *bs, int *totvert)
{
	GHash *ghash;
	BSphereNode *n, *child;

	ghash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp,
			      "bsphere_frames.ghash");
	(*totvert) = 0;

	for(n = bs->rootbase.first; n; n = n->next) {
		float mat[3][3];

		child = n->children.first;
		if(!child || (child && child->next)) {
			set_v3(mat[0], 0, 0, 1);
			set_v3(mat[1], 1, 0, 0);
			set_v3(mat[2], 0, 1, 0);
		}
		else if(!child->next) {
			calc_edge_mat(mat, n->co, child->co);
		}

		(*totvert) += bsphere_build_frames(n, ghash, mat, START_CAP);
	}

	return ghash;
}

static void output_frames(Frames *frames, MVert *mvert, MFace *mface,
			  int *totvert, int *totface)
{
	int b, i, j;

	frames->outbase = (*totvert);
	for(i = 0; i < frames->totframe; i++) {
		for(j = 0; j < 4; j++) {
			copy_v3_v3(mvert[(*totvert)].co, frames->co[i][j]);
			(*totvert)++;
		}

		if(i > 0) {
			connect_frames(mface, totface,
				       frames->outbase + 4*(i-1),
				       frames->outbase + 4*i);
		}
	}

	if(frames->flag & START_CAP) {
		b = frame_base(frames, START_CAP);
		add_face(mface, totface, b, b+1, b+2, b+3);
	}
	if(frames->flag & END_CAP) {
		b = frame_base(frames, END_CAP);
		add_face(mface, totface, b+3, b+2, b+1, b+0);
	}
}

static void bsphere_build_skin(BSphereNode *parent, ListBase *base,
			       GHash *frames, MVert *mvert,
			       MFace *mface, int *totvert, int *totface)
{
	BSphereNode *n, *child;
	Frames *node_frames, *child_frames, *parent_frames;
	int *hull_frames, totchild, i;

	for(n = base->first; n; n = n->next) {
		if((node_frames = BLI_ghash_lookup(frames, n)))
			output_frames(node_frames, mvert, mface, totvert, totface);

		bsphere_build_skin(n, &n->children, frames, mvert, mface, totvert, totface);

		child = n->children.first;
		if(child && !child->next) {
			child_frames = BLI_ghash_lookup(frames, child);
			if(child_frames) {
				connect_frames(mface, totface,
					       frame_base(node_frames, OUT_FRAME),
					       frame_base(child_frames, IN_FRAME));
			}
		}
		else if(child && child->next) {
			totchild = BLI_countlist(&n->children);
			hull_frames = MEM_callocN(sizeof(int) * (1+totchild),
						  "bsphere_build_skin.hull_frames");
			i = 0;
			for(child = n->children.first; child; child = child->next) {
				child_frames = BLI_ghash_lookup(frames, child);
				assert(child_frames);
				hull_frames[i++] = frame_base(child_frames, IN_FRAME);
			}
			parent_frames = BLI_ghash_lookup(frames, parent);
			if(parent_frames)
				hull_frames[i] = frame_base(parent_frames, OUT_FRAME);
			else
				totchild--;
			build_hull(mface, totface, mvert, hull_frames, totchild+1);
			MEM_freeN(hull_frames);
		}
	}
}

static DerivedMesh *bsphere_base_skin(BSphere *bs)
{
	DerivedMesh *dm;
	GHash *ghash;
	MVert *mvert;
	MFace *mface;
	int totvert, totface = 0;

	/* no nodes, nothing to do */
	if(!bs->rootbase.first)
		return NULL;

	/* could probably do all this in one fell [recursive] swoop
	   using some tricksies and clevers, but multiple passes is
	   easier :) */
	ghash = bsphere_frames(bs, &totvert);

	totface = totvert * 1.5;
	/* XXX: sizes OK? */
	dm = CDDM_new(totvert, 0, totface);

	mvert = CDDM_get_verts(dm);
	mface = CDDM_get_faces(dm);

	totvert = totface = 0;
	bsphere_build_skin(NULL, &bs->rootbase, ghash, mvert, mface, &totvert, &totface);
	BLI_ghash_free(ghash, NULL, (void*)MEM_freeN);

	CDDM_lower_num_verts(dm, totvert);
	CDDM_lower_num_faces(dm, totface);

	CDDM_calc_edges(dm);
	CDDM_calc_normals(dm);

	return dm;
}

/* generate random points (just for testing) */
DerivedMesh *bsphere_random_points(BSphere *UNUSED(bs))
{
	DerivedMesh *dm;
	MVert *mvert;
	const int totvert = 1;//50000;
	int i;

	dm = CDDM_new(totvert, 0, 0);

	mvert = CDDM_get_verts(dm);
	
	srand(1);
	for(i = 0; i < totvert; i++) {
		mvert[i].co[0] = rand() / (float)RAND_MAX - 0.5;
		mvert[i].co[1] = rand() / (float)RAND_MAX - 0.5;
		mvert[i].co[2] = rand() / (float)RAND_MAX - 0.5;
	}
	
	return dm;
}

DerivedMesh *bsphere_test_surface(BSphere *bs, DerivedMesh *input)
{
	DerivedMesh *dm;
	MVert *mvert;

	if(!input)
		return NULL;

	dm = CDDM_copy(input);
	mvert = CDDM_get_verts(dm);
	bsphere_evolve(mvert, input->getNumVerts(input), &bs->rootbase,
		       bs->skin_threshold, bs->evolve, 2);

	return dm;
}

DerivedMesh *bsphere_test_gradient(BSphere *bs, DerivedMesh *input)
{
	DerivedMesh *dm;
	MVert *mvert;
	MEdge *medge;
	int totvert;
	int i;

	if(!input)
		return NULL;

	totvert = input->getNumVerts(input);
	dm = CDDM_new(totvert*2, totvert, 0);

	mvert = CDDM_get_verts(dm);
	medge = CDDM_get_edges(dm);

	memcpy(mvert, CDDM_get_verts(input), sizeof(MVert) * totvert);
	
	for(i = 0; i < totvert; i++) {
		const float *b = mvert[i].co;
		float *g = mvert[totvert+i].co;
		float K[2] = {0, 0};

		zero_v3(g);
		calc_gradient(g, b, &bs->rootbase);
		normalize_v3(g);
		
		mul_v3_fl(g, -calc_F(b, K, bs->skin_threshold, &bs->rootbase));
		add_v3_v3(g, b);

		medge[i].v1 = i;
		medge[i].v2 = totvert+i;
	}
	
	return dm;
}

DerivedMesh *bsphere_get_skin_dm(BSphere *bs)
{
	DerivedMesh *dm, *subdm;

	dm = bsphere_base_skin(bs);

	/* subdivide once */
	if(dm && bs->subdiv_level > 0) {
		SubsurfModifierData smd;

		memset(&smd, 0, sizeof(smd));
		smd.subdivType = ME_CC_SUBSURF;
		smd.levels = bs->subdiv_level;
		subdm = subsurf_make_derived_from_derived(dm, &smd, 0, 0, 0, 0);
		dm->release(dm);
		/* for convenience, convert back to cddm (could use ccgdm
		   later for better performance though? (TODO)) */
		dm = CDDM_copy(subdm);
		subdm->release(subdm);

		bsphere_evolve(CDDM_get_verts(dm), dm->getNumVerts(dm), &bs->rootbase,
			       bs->skin_threshold, bs->evolve, bs->subdiv_level);
	}

	return dm;
}
#endif
