/**
 * BME_interp.c    August 2008
 *
 *	BM interpolation functions.
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h" 
#include "BKE_utildefines.h"
#include "BKE_multires.h"

#include "BLI_array.h"
#include "BLI_math.h"
#include "BLI_cellalloc.h"

#include "bmesh.h"
#include "bmesh_private.h"

/*
 * BME_INTERP.C
 *
 * Functions for interpolating data across the surface of a mesh.
 *
*/

/**
 *			bmesh_data_interp_from_verts
 *
 *  Interpolates per-vertex data from two sources to a target.
 * 
 *  Returns -
 *	Nothing
 */
void BM_Data_Interp_From_Verts(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v, float fac)
{
	void *src[2];
	float w[2];
	if (v1->head.data && v2->head.data) {
		src[0]= v1->head.data;
		src[1]= v2->head.data;
		w[0] = 1.0f-fac;
		w[1] = fac;
		CustomData_bmesh_interp(&bm->vdata, src, w, NULL, 2, v->head.data);
	}
}

/*
    BM Data Vert Average

    Sets all the customdata (e.g. vert, loop) associated with a vert
    to the average of the face regions surrounding it.
*/


void BM_Data_Vert_Average(BMesh *UNUSED(bm), BMFace *UNUSED(f))
{
	// BMIter iter;
}

/**
 *			bmesh_data_facevert_edgeinterp
 *
 *  Walks around the faces of an edge and interpolates the per-face-edge
 *  data between two sources to a target.
 * 
 *  Returns -
 *	Nothing
*/
 
void BM_Data_Facevert_Edgeinterp(BMesh *bm, BMVert *v1, BMVert *UNUSED(v2), BMVert *v, BMEdge *e1, float fac){
	void *src[2];
	float w[2];
	BMLoop *l=NULL, *v1loop = NULL, *vloop = NULL, *v2loop = NULL;
	
	w[1] = 1.0f - fac;
	w[0] = fac;

	if(!e1->l) return;
	l = e1->l;
	do{
		if(l->v == v1){ 
			v1loop = l;
			vloop = (BMLoop*)(v1loop->next);
			v2loop = (BMLoop*)(vloop->next);
		}else if(l->v == v){
			v1loop = (BMLoop*)(l->next);
			vloop = l;
			v2loop = (BMLoop*)(l->prev);
			
		}
		
		if (!v1loop || !v2loop)
			return;
		
		src[0] = v1loop->head.data;
		src[1] = v2loop->head.data;					

		CustomData_bmesh_interp(&bm->ldata, src,w, NULL, 2, vloop->head.data); 				
		l = l->radial_next;
	}while(l!=e1->l);
}

void BM_loops_to_corners(BMesh *bm, Mesh *me, int findex,
                         BMFace *f, int numTex, int numCol) 
{
	BMLoop *l;
	BMIter iter;
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;
	int i, j;

	for(i=0; i < numTex; i++){
		texface = CustomData_get_n(&me->fdata, CD_MTFACE, findex, i);
		texpoly = CustomData_bmesh_get_n(&bm->pdata, f->head.data, CD_MTEXPOLY, i);
		
		texface->tpage = texpoly->tpage;
		texface->flag = texpoly->flag;
		texface->transp = texpoly->transp;
		texface->mode = texpoly->mode;
		texface->tile = texpoly->tile;
		texface->unwrap = texpoly->unwrap;

		j = 0;
		BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
			mloopuv = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPUV, i);
			texface->uv[j][0] = mloopuv->uv[0];
			texface->uv[j][1] = mloopuv->uv[1];

			j++;
		}

	}

	for(i=0; i < numCol; i++){
		mcol = CustomData_get_n(&me->fdata, CD_MCOL, findex, i);

		j = 0;
		BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
			mloopcol = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPCOL, i);
			mcol[j].r = mloopcol->r;
			mcol[j].g = mloopcol->g;
			mcol[j].b = mloopcol->b;
			mcol[j].a = mloopcol->a;

			j++;
		}
	}
}

/**
 *			BM_data_interp_from_face
 *
 *  projects target onto source, and pulls interpolated customdata from
 *  source.
 * 
 *  Returns -
 *	Nothing
*/
void BM_face_interp_from_face(BMesh *bm, BMFace *target, BMFace *source)
{
	BMLoop *l1, *l2;
	void **blocks=NULL;
	float (*cos)[3]=NULL, *w=NULL;
	BLI_array_staticdeclare(cos, 64);
	BLI_array_staticdeclare(w, 64);
	BLI_array_staticdeclare(blocks, 64);
	
	BM_Copy_Attributes(bm, bm, source, target);
	
	l2 = bm_firstfaceloop(source);
	do {
		BLI_array_growone(cos);
		copy_v3_v3(cos[BLI_array_count(cos)-1], l2->v->co);
		BLI_array_growone(w);
		BLI_array_append(blocks, l2->head.data);
		l2 = l2->next;
	} while (l2 != bm_firstfaceloop(source));

	l1 = bm_firstfaceloop(target);
	do {
		interp_weights_poly_v3(w, cos, source->len, l1->v->co);
		CustomData_bmesh_interp(&bm->ldata, blocks, w, NULL, BLI_array_count(blocks), l1->head.data);
		l1 = l1->next;
	} while (l1 != bm_firstfaceloop(target));

	BLI_array_free(cos);
	BLI_array_free(w);
	BLI_array_free(blocks);
}

/***** multires interpolation*****

mdisps is a grid of displacements, ordered thus:

v1/center -- v4/next -> x
|                 |
|				  |
v2/prev ---- v3/cur
|
V

y
*/
static int compute_mdisp_quad(BMLoop *l, float v1[3], float v2[3], float v3[3], float v4[3], float e1[3], float e2[3])
{
	float cent[3] = {0.0f, 0.0f, 0.0f}, n[3], p[3];
	BMLoop *l2;
	
	/*computer center*/
	l2 = bm_firstfaceloop(l->f);
	do {
		add_v3_v3(cent, l2->v->co);
		l2 = l2->next;
	} while (l2 != bm_firstfaceloop(l->f));
	
	mul_v3_fl(cent, 1.0/(float)l->f->len);
	
	add_v3_v3v3(p, l->prev->v->co, l->v->co);
	mul_v3_fl(p, 0.5);
	add_v3_v3v3(n, l->next->v->co, l->v->co);
	mul_v3_fl(n, 0.5);
	
	copy_v3_v3(v1, cent);
	copy_v3_v3(v2, p);
	copy_v3_v3(v3, l->v->co);
	copy_v3_v3(v4, n);
	
	sub_v3_v3v3(e1, v2, v1);
	sub_v3_v3v3(e2, v3, v4);
	
	return 1;
}


int isect_ray_tri_threshold_v3_uvw(float p1[3], float d[3], float _v0[3], float _v1[3], float _v2[3], float *lambda, float uv[3], float threshold)
{
	float p[3], s[3], e1[3], e2[3], q[3];
	float a, f, u, v;
	float du = 0, dv = 0;
	float v0[3], v1[3], v2[3], c[3];
	
	/*expand triangle a bit*/
#if 1
	cent_tri_v3(c, _v0, _v1, _v2);
	sub_v3_v3v3(v0, _v0, c);
	sub_v3_v3v3(v1, _v1, c);
	sub_v3_v3v3(v2, _v2, c);
	mul_v3_fl(v0, 1.0+threshold);
	mul_v3_fl(v1, 1.0+threshold);
	mul_v3_fl(v2, 1.0+threshold);
	add_v3_v3(v0, c);
	add_v3_v3(v1, c);
	add_v3_v3(v2, c);
#else
	copy_v3_v3(v0, _v0);
	copy_v3_v3(v1, _v1);
	copy_v3_v3(v2, _v2);
#endif
	
	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);
	
	cross_v3_v3v3(p, d, e2);
	a = dot_v3v3(e1, p);
	if ((a > -0.000001) && (a < 0.000001)) return 0;
	f = 1.0f/a;
	
	sub_v3_v3v3(s, p1, v0);
	
	cross_v3_v3v3(q, s, e1);
	*lambda = f * dot_v3v3(e2, q);
	if ((*lambda < 0.0+FLT_EPSILON)) return 0;
	
	u = f * dot_v3v3(s, p);
	v = f * dot_v3v3(d, q);
	
	if (u < 0) du = u;
	if (u > 1) du = u - 1;
	if (v < 0) dv = v;
	if (v > 1) dv = v - 1;
	if (u > 0 && v > 0 && u + v > 1)
	{
		float t = u + v - 1;
		du = u - t/2;
		dv = v - t/2;
	}

	mul_v3_fl(e1, du);
	mul_v3_fl(e2, dv);
	
	if (dot_v3v3(e1, e1) + dot_v3v3(e2, e2) > threshold * threshold)
	{
		return 0;
	}

	if(uv) {
		uv[0]= u;
		uv[1]= v;
		uv[2]= fabs(1.0-u-v);
	}
	
	return 1;
}

/* find closest point to p on line through l1,l2 and return lambda,
 * where (0 <= lambda <= 1) when cp is in the line segement l1,l2
 */
static double closest_to_line_v3_d(double cp[3], const double p[3], const double l1[3], const double l2[3])
{
	double h[3],u[3],lambda;
	VECSUB(u, l2, l1);
	VECSUB(h, p, l1);
	lambda =INPR(u,h)/INPR(u,u);
	cp[0] = l1[0] + u[0] * lambda;
	cp[1] = l1[1] + u[1] * lambda;
	cp[2] = l1[2] + u[2] * lambda;
	return lambda;
}

/* point closest to v1 on line v2-v3 in 3D */
static void closest_to_line_segment_v3_d(double *closest, double v1[3], double v2[3], double v3[3])
{
	double lambda, cp[3];

	lambda= closest_to_line_v3_d(cp,v1, v2, v3);

	if(lambda <= 0.0) {
		VECCOPY(closest, v2);
	} else if(lambda >= 1.0) {
		VECCOPY(closest, v3);
	} else {
		VECCOPY(closest, cp);
	}
}

static double len_v3v3_d(const double a[3], const double b[3])
{
	double d[3];

	VECSUB(d, b, a);
	return sqrt(INPR(d, d));
}

/*funnily enough, I think this is identical to face_to_crn_interp, heh*/
double quad_coord(double aa[3], double bb[3], double cc[3], double dd[3], int a1, int a2)
{
	double x, y, z, f1, f2;
	
	x = aa[a1]*cc[a2]-cc[a1]*aa[a2];
	y = aa[a1]*dd[a2]+bb[a1]*cc[a2]-cc[a1]*bb[a2]-dd[a1]*aa[a2];
	z = bb[a1]*dd[a2]-dd[a1]*bb[a2];
	
	
	if (fabs(2*(x-y+z)) > DBL_EPSILON*1000.0) {
		f1 = (sqrt(y*y-4.0*x*z) - y + 2.0*z) / (2.0*(x-y+z));
		f2 = (-sqrt(y*y-4.0*x*z) - y + 2.0*z) / (2.0*(x-y+z));
	} else f1 = -1;
	
	if (isnan(f1) || f1 == -1.0)  {
		int i, tot=200;
		double d, lastd=-1.0;
		
		//return -1.0f;
#if 0
		double co[3], p[3] = {0.0, 0.0, 0.0};
		closest_to_line_segment_v3_d(co, p, aa, bb);

		return len_v3v3_d(bb, co) / len_v3v3_d(aa, bb);
#endif
#if 1
		f1 = 1.0;
		f2 = 0.0;
		for (i=0; i<tot; i++) {
			double f3, v1[3], v2[3], co[3], p[3] = {0.0, 0.0, 0.0};
			
			VECINTERP(v1, aa, bb, f1);
			VECINTERP(v2, cc, dd, f1);
			
			closest_to_line_segment_v3_d(co, p, v1, v2);
			d = len_v3v3_d(co, p);
			
			f3 = f1;
			if (d < lastd) {
				f1 += (f1-f2)*0.5;
			} else {
				f1 -= (f1-f2)*0.5;
			}
			
			f2 = f3;
			lastd = d;
		}
		
		if (d > 0.0000001 || f1 < -FLT_EPSILON*1000 || f1 >= 1.0+FLT_EPSILON*100)
			return -1.0;
		
		CLAMP(f1, 0.0, 1.0+DBL_EPSILON);
		return 1.0 - f1;
#endif
	}
	
	f1 = MIN2(fabs(f1), fabs(f2));
	CLAMP(f1, 0.0, 1.0+DBL_EPSILON);
	
	return f1;
}

void quad_co(float *x, float *y, float v1[3], float v2[3], float v3[3], float v4[3], float p[3], float n[3])
{
	float projverts[4][3];
	double dverts[4][3];
	int i;
	
	sub_v3_v3v3(projverts[0], v1, p);
	sub_v3_v3v3(projverts[1], v2, p);
	sub_v3_v3v3(projverts[2], v3, p);
	sub_v3_v3v3(projverts[3], v4, p);
	
	/*rotate*/	
	poly_rotate_plane(n, projverts, 4);
	
	/*flatten*/
	for (i=0; i<4; i++) projverts[i][2] = 0.0f;
	
	VECCOPY(dverts[0], projverts[0]);
	VECCOPY(dverts[1], projverts[1]);
	VECCOPY(dverts[2], projverts[2]);
	VECCOPY(dverts[3], projverts[3]);
	
	*y = quad_coord(dverts[1], dverts[0], dverts[2], dverts[3], 0, 1);
	*x = quad_coord(dverts[2], dverts[1], dverts[3], dverts[0], 0, 1);
}


/*tl is loop to project onto, sl is loop whose internal displacement, co, is being
  projected.  x and y are location in loop's mdisps grid of co.*/
static int mdisp_in_mdispquad(BMesh *bm, BMLoop *l, BMLoop *tl, float p[3], float *x, float *y, int res)
{
	float v1[3], v2[3], c[3], co[3], v3[3], v4[3], e1[3], e2[3];
	float w[4], dir[3], uv[4] = {0.0f, 0.0f, 0.0f, 0.0f}, hit[3];
	float x2, y2, lm, eps = FLT_EPSILON*20;
	int ret=0;
	
	if (len_v3(l->f->no) < FLT_EPSILON*50)
		BM_Face_UpdateNormal(bm, l->f);

	if (len_v3(tl->f->no) < FLT_EPSILON*50)
		BM_Face_UpdateNormal(bm, tl->f);
		
	compute_mdisp_quad(tl, v1, v2, v3, v4, e1, e2);
	copy_v3_v3(dir, tl->f->no);
	copy_v3_v3(co, dir);
	mul_v3_fl(co, -0.001);
	add_v3_v3(co, p);
	
	/*four tests, two per triangle, once again normal, once along -normal*/
	ret = isect_ray_tri_threshold_v3_uvw(co, dir, v1, v2, v3, &lm, uv, eps);
	ret = ret || isect_ray_tri_threshold_v3_uvw(co, dir, v1, v3, v4, &lm, uv, eps);
		
	if (!ret) {
		/*now try other direction*/
		negate_v3(dir);
		ret = isect_ray_tri_threshold_v3_uvw(co, dir, v1, v2, v3, &lm, uv, eps);
		ret = ret || isect_ray_tri_threshold_v3_uvw(co, dir, v1, v3, v4, &lm, uv, eps);
	}
		
	if (!ret)
		return 0;
	
	if (isnan(lm))
		return 0;
	
	mul_v3_fl(dir, lm);
	add_v3_v3v3(hit, co, dir);

	/*expand quad a bit*/
#if 1
	cent_quad_v3(c, v1, v2, v3, v4);
	
	sub_v3_v3(v1, c); sub_v3_v3(v2, c);
	sub_v3_v3(v3, c); sub_v3_v3(v4, c);
	mul_v3_fl(v1, 1.0+eps); mul_v3_fl(v2, 1.0+eps);
	mul_v3_fl(v3, 1.0+eps);	mul_v3_fl(v4, 1.0+eps);
	add_v3_v3(v1, c); add_v3_v3(v2, c);
	add_v3_v3(v3, c); add_v3_v3(v4, c);
#endif
	
	quad_co(x, y, v1, v2, v3, v4, hit, tl->f->no);
	
	if (isnan(*x) || isnan(*y) || *x == -1.0f || *y == -1.0f) {		
		interp_weights_face_v3(uv, v1, v2, v3, v4, hit);
		
		x2 = ((1.0+FLT_EPSILON)*uv[2] + (1.0+FLT_EPSILON)*uv[3]);
		y2 = ((1.0+FLT_EPSILON)*uv[1] + (1.0+FLT_EPSILON)*uv[2]);

		if (*x == -1.0f || isnan(*x))
			*x = x2;
		if (*y == -1.0f || isnan(*y))
			*y = y2;
	}
	
	*x *= res-1-FLT_EPSILON*100;
	*y *= res-1-FLT_EPSILON*100;
	
	if (fabs(*x-x2) > 1.5 || fabs(*y-y2) > 1.5) {
		x2 = 1;
	}
	
	return 1;
}

static void bmesh_loop_interp_mdisps(BMesh *bm, BMLoop *target, BMFace *source)
{
	MDisps *mdisps;
	BMLoop *l2;
	float x, y, d, v1[3], v2[3], v3[3], v4[3] = {0.0f, 0.0f, 0.0f}, e1[3], e2[3], e3[3], e4[3];
	int ix, iy, res;
	
	if (!CustomData_has_layer(&bm->ldata, CD_MDISPS))
		return;
	
	mdisps = CustomData_bmesh_get(&bm->ldata, target->head.data, CD_MDISPS);
	compute_mdisp_quad(target, v1, v2, v3, v4, e1, e2);
	
	/*if no disps data allocate a new grid, the size of the first grid in source.*/
	if (!mdisps->totdisp) {
		MDisps *md2 = CustomData_bmesh_get(&bm->ldata, bm_firstfaceloop(source)->head.data, CD_MDISPS);
		
		mdisps->totdisp = md2->totdisp;
		if (mdisps->totdisp)
			mdisps->disps = BLI_cellalloc_calloc(sizeof(float)*3*mdisps->totdisp, "mdisp->disps in bmesh_loop_intern_mdisps");
		else 
			return;
	}
	
	res = (int)sqrt(mdisps->totdisp);
	d = 1.0f/(float)(res-1);
	for (x=0.0f, ix=0; ix<res; x += d, ix++) {
		for (y=0.0f, iy=0; iy<res; y+= d, iy++) {
			float co1[3], co2[3], co[3];
			float xx, yy;
			
			copy_v3_v3(co1, e1);
			
			if (!iy) yy = y + FLT_EPSILON*2;
			else yy = y - FLT_EPSILON*2;
			
			mul_v3_fl(co1, yy);
			add_v3_v3(co1, v1);
			
			copy_v3_v3(co2, e2);
			mul_v3_fl(co2, yy);
			add_v3_v3(co2, v4);
			
			if (!ix) xx = x + FLT_EPSILON*2;
			else xx = x - FLT_EPSILON*2;
			
			sub_v3_v3v3(co, co2, co1);
			mul_v3_fl(co, xx);
			add_v3_v3(co, co1);
			
			l2 = bm_firstfaceloop(source);
			do {
				float x2, y2;
				int ix2, iy2;
				MDisps *md1, *md2;

				md1 = CustomData_bmesh_get(&bm->ldata, target->head.data, CD_MDISPS);
				md2 = CustomData_bmesh_get(&bm->ldata, l2->head.data, CD_MDISPS);
				
				if (mdisp_in_mdispquad(bm, target, l2, co, &x2, &y2, res)) {
					ix2 = (int)x2;
					iy2 = (int)y2;
					
					old_mdisps_bilinear(md1->disps[iy*res+ix], md2->disps, res, x2, y2);
				}
				l2 = l2->next;
			} while (l2 != bm_firstfaceloop(source));
		}
	}
}

void BM_multires_smooth_bounds(BMesh *bm, BMFace *f)
{
	BMLoop *l;
	BMIter liter;
	
	//return;//XXX
	
	if (!CustomData_has_layer(&bm->ldata, CD_MDISPS))
		return;
	
	BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
		MDisps *mdp = CustomData_bmesh_get(&bm->ldata, l->prev->head.data, CD_MDISPS);
		MDisps *mdl = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MDISPS);
		MDisps *mdn = CustomData_bmesh_get(&bm->ldata, l->next->head.data, CD_MDISPS);
		float co[3];
		int sides;
		int x, y;
		
		/*****
		mdisps is a grid of displacements, ordered thus:
		
		              v4/next
		                |		                
		 |       v1/cent-mid2 ---> x
		 |       |       | 
		 |       |       |
		v2/prev--mid1--v3/cur
		         |
		         V
		         y
		*****/
		  
		sides = sqrt(mdp->totdisp);
		for (y=0; y<sides; y++) {
			//add_v3_v3v3(co, mdp->disps[y*sides + sides-1], mdl->disps[y*sides]);
			//mul_v3_fl(co, 0.5);
			copy_v3_v3(co, mdn->disps[y*sides]);
			copy_v3_v3(mdn->disps[y*sides], mdl->disps[y]);
			copy_v3_v3(mdl->disps[y], co);

			//copy_v3_v3(mdp->disps[y*sides + sides-1], co);
			//copy_v3_v3(mdl->disps[y*sides], co);
		}
	}
}

void BM_loop_interp_multires(BMesh *bm, BMLoop *target, BMFace *source)
{
	bmesh_loop_interp_mdisps(bm, target, source);
}

void BM_loop_interp_from_face(BMesh *bm, BMLoop *target, BMFace *source, int do_vertex)
{
	BMLoop *l;
	void **blocks=NULL;
	void **vblocks=NULL;
	float (*cos)[3]=NULL, *w=NULL, cent[3] = {0.0f, 0.0f, 0.0f};
	BLI_array_staticdeclare(cos, 64);
	BLI_array_staticdeclare(w, 64);
	BLI_array_staticdeclare(blocks, 64);
	BLI_array_staticdeclare(vblocks, 64);
	int i;
	
	BM_Copy_Attributes(bm, bm, source, target->f);
	
	l = bm_firstfaceloop(source);
	do {
		BLI_array_growone(cos);
		copy_v3_v3(cos[BLI_array_count(cos)-1], l->v->co);
		add_v3_v3(cent, cos[BLI_array_count(cos)-1]);
		
		BLI_array_append(w, 0.0f);
		BLI_array_append(blocks, l->head.data);
	
		if (do_vertex)
			BLI_array_append(vblocks, l->v->head.data);
	
		l = l->next;
	} while (l != bm_firstfaceloop(source));

	/*scale source face coordinates a bit, so points sitting directonly on an
      edge will work.*/
	mul_v3_fl(cent, 1.0f/(float)source->len);
	for (i=0; i<source->len; i++) {
		float vec[3];
		sub_v3_v3v3(vec, cent, cos[i]);
		mul_v3_fl(vec, 0.01);
		add_v3_v3(cos[i], vec);
	}
	
	/*interpolate*/
	interp_weights_poly_v3(w, cos, source->len, target->v->co);
	CustomData_bmesh_interp(&bm->ldata, blocks, w, NULL, source->len, target->head.data);
	if (do_vertex) 
		CustomData_bmesh_interp(&bm->vdata, vblocks, w, NULL, source->len, target->v->head.data);

	BLI_array_free(cos);
	BLI_array_free(w);
	BLI_array_free(blocks);
	
	if (CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		bmesh_loop_interp_mdisps(bm, target, source);
	}
}


void BM_vert_interp_from_face(BMesh *bm, BMVert *v, BMFace *source)
{
	BMLoop *l;
	void **blocks=NULL;
	float (*cos)[3]=NULL, *w=NULL, cent[3] = {0.0f, 0.0f, 0.0f};
	BLI_array_staticdeclare(cos, 64);
	BLI_array_staticdeclare(w, 64);
	BLI_array_staticdeclare(blocks, 64);
	int i;
	
	l = bm_firstfaceloop(source);
	do {
		BLI_array_growone(cos);
		copy_v3_v3(cos[BLI_array_count(cos)-1], l->v->co);
		add_v3_v3(cent, cos[BLI_array_count(cos)-1]);
		
		BLI_array_append(w, 0.0f);
		BLI_array_append(blocks, l->v->head.data);
		l = l->next;
	} while (l != bm_firstfaceloop(source));

	/*scale source face coordinates a bit, so points sitting directonly on an
      edge will work.*/
	mul_v3_fl(cent, 1.0f/(float)source->len);
	for (i=0; i<source->len; i++) {
		float vec[3];
		sub_v3_v3v3(vec, cent, cos[i]);
		mul_v3_fl(vec, 0.01);
		add_v3_v3(cos[i], vec);
	}
	
	/*interpolate*/
	interp_weights_poly_v3(w, cos, source->len, v->co);
	CustomData_bmesh_interp(&bm->vdata, blocks, w, NULL, source->len, v->head.data);
	
	BLI_array_free(cos);
	BLI_array_free(w);
	BLI_array_free(blocks);
}

static void update_data_blocks(BMesh *bm, CustomData *olddata, CustomData *data)
{
	BMIter iter;
	// BLI_mempool *oldpool = olddata->pool;
	void *block;

	CustomData_bmesh_init_pool(data, data==&bm->ldata ? 2048 : 512);

	if (data == &bm->vdata) {
		BMVert *eve;
		
		BM_ITER(eve, &iter, bm, BM_VERTS_OF_MESH, NULL) {
			block = NULL;
			CustomData_bmesh_set_default(data, &block);
			CustomData_bmesh_copy_data(olddata, data, eve->head.data, &block);
			CustomData_bmesh_free_block(olddata, &eve->head.data);
			eve->head.data= block;
		}
	}
	else if (data == &bm->edata) {
		BMEdge *eed;

		BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			block = NULL;
			CustomData_bmesh_set_default(data, &block);
			CustomData_bmesh_copy_data(olddata, data, eed->head.data, &block);
			CustomData_bmesh_free_block(olddata, &eed->head.data);
			eed->head.data= block;
		}
	}
	else if (data == &bm->pdata || data == &bm->ldata) {
		BMIter liter;
		BMFace *efa;
		BMLoop *l;

		BM_ITER(efa, &iter, bm, BM_FACES_OF_MESH, NULL) {
			if (data == &bm->pdata) {
				block = NULL;
				CustomData_bmesh_set_default(data, &block);
				CustomData_bmesh_copy_data(olddata, data, efa->head.data, &block);
				CustomData_bmesh_free_block(olddata, &efa->head.data);
				efa->head.data= block;
			}

			if (data == &bm->ldata) {
				BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, efa) {
					block = NULL;
					CustomData_bmesh_set_default(data, &block);
					CustomData_bmesh_copy_data(olddata, data, l->head.data, &block);
					CustomData_bmesh_free_block(olddata, &l->head.data);
					l->head.data= block;
				}
			}
		}
	}
}


void BM_add_data_layer(BMesh *bm, CustomData *data, int type)
{
	CustomData olddata;

	olddata= *data;
	olddata.layers= (olddata.layers)? MEM_dupallocN(olddata.layers): NULL;
	CustomData_add_layer(data, type, CD_DEFAULT, NULL, 0);

	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

void BM_add_data_layer_named(BMesh *bm, CustomData *data, int type, char *name)
{
	CustomData olddata;

	olddata= *data;
	olddata.layers= (olddata.layers)? MEM_dupallocN(olddata.layers): NULL;
	CustomData_add_layer_named(data, type, CD_DEFAULT, NULL, 0, name);

	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

void BM_free_data_layer(BMesh *bm, CustomData *data, int type)
{
	CustomData olddata;

	olddata= *data;
	olddata.layers= (olddata.layers)? MEM_dupallocN(olddata.layers): NULL;
	CustomData_free_layer_active(data, type, 0);

	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

void BM_free_data_layer_n(BMesh *bm, CustomData *data, int type, int n)
{
	CustomData olddata;

	olddata= *data;
	olddata.layers= (olddata.layers)? MEM_dupallocN(olddata.layers): NULL;
	CustomData_free_layer(data, type, 0, CustomData_get_layer_index_n(data, type, n));
	
	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

float BM_GetCDf(CustomData *cd, void *element, int type)
{
	if (CustomData_has_layer(cd, type)) {
		float *f = CustomData_bmesh_get(cd, ((BMHeader*)element)->data, type);
		return *f;
	}

	return 0.0;
}

void BM_SetCDf(CustomData *cd, void *element, int type, float val)
{
	if (CustomData_has_layer(cd, type)) {
		float *f = CustomData_bmesh_get(cd, ((BMHeader*)element)->data, type);
		*f = val;
	}

	return;
}
