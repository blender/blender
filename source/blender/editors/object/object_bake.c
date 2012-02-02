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
 * The Original Code is Copyright (C) 2004 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Morten Mikkelsen,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_bake.c
 *  \ingroup edobj
 */


/*
	meshtools.c: no editmode (violated already :), tools operating on meshes
*/

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_math_geom.h"

#include "BKE_blender.h"
#include "BKE_screen.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_multires.h"
#include "BKE_report.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_DerivedMesh.h"
#include "BKE_subsurf.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"

#include "PIL_time.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "GPU_draw.h" /* GPU_free_image */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"

#include "object_intern.h"

/* ****************** multires BAKING ********************** */

/* holder of per-object data needed for bake job
   needed to make job totally thread-safe */
typedef struct MultiresBakerJobData {
	struct MultiresBakerJobData *next, *prev;
	DerivedMesh *lores_dm, *hires_dm;
	int simple, lvl, tot_lvl;
} MultiresBakerJobData;

/* data passing to multires-baker job */
typedef struct {
	ListBase data;
	int bake_clear, bake_filter;
	short mode, use_lores_mesh;
} MultiresBakeJob;

/* data passing to multires baker */
typedef struct {
	DerivedMesh *lores_dm, *hires_dm;
	int simple, lvl, tot_lvl, bake_filter;
	short mode, use_lores_mesh;

	int tot_obj, tot_image;
	ListBase image;

	int baked_objects, baked_faces;

	short *stop;
	short *do_update;
	float *progress;
} MultiresBakeRender;

typedef void (*MPassKnownData)(DerivedMesh *lores_dm, DerivedMesh *hires_dm, const void *bake_data,
                               const int face_index, const int lvl, const float st[2],
                               float tangmat[3][3], const int x, const int y);

typedef void* (*MInitBakeData)(MultiresBakeRender *bkr, Image* ima);
typedef void (*MApplyBakeData)(void *bake_data);
typedef void (*MFreeBakeData)(void *bake_data);

typedef struct {
	MVert *mvert;
	MFace *mface;
	MTFace *mtface;
	float *pvtangent;
	float *precomputed_normals;
	int w, h;
	int face_index;
	int i0, i1, i2;
	DerivedMesh *lores_dm, *hires_dm;
	int lvl;
	void *bake_data;
	MPassKnownData pass_data;
} MResolvePixelData;

typedef void (*MFlushPixel)(const MResolvePixelData *data, const int x, const int y);

typedef struct {
	int w, h;
	char *texels;
	const MResolvePixelData *data;
	MFlushPixel flush_pixel;
} MBakeRast;

typedef struct {
	float *heights;
	float height_min, height_max;
	Image *ima;
	DerivedMesh *ssdm;
	const int *origindex;
} MHeightBakeData;

typedef struct {
	const int *origindex;
} MNormalBakeData;

static void multiresbake_get_normal(const MResolvePixelData *data, float norm[], const int face_num, const int vert_index)
{
	unsigned int indices[]= {data->mface[face_num].v1, data->mface[face_num].v2,
	                         data->mface[face_num].v3, data->mface[face_num].v4};
	const int smoothnormal= (data->mface[face_num].flag & ME_SMOOTH);

	if(!smoothnormal)  { /* flat */
		if(data->precomputed_normals) {
			copy_v3_v3(norm, &data->precomputed_normals[3*face_num]);
		} else {
			float nor[3];
			float *p0, *p1, *p2;
			const int iGetNrVerts= data->mface[face_num].v4!=0 ? 4 : 3;

			p0= data->mvert[indices[0]].co;
			p1= data->mvert[indices[1]].co;
			p2= data->mvert[indices[2]].co;

			if(iGetNrVerts==4) {
				float *p3= data->mvert[indices[3]].co;
				normal_quad_v3(nor, p0, p1, p2, p3);
			} else {
				normal_tri_v3(nor, p0, p1, p2);
			}

			copy_v3_v3(norm, nor);
		}
	} else {
		short *no= data->mvert[indices[vert_index]].no;

		normal_short_to_float_v3(norm, no);
		normalize_v3(norm);
	}
}

static void init_bake_rast(MBakeRast *bake_rast, const ImBuf *ibuf, const MResolvePixelData *data, MFlushPixel flush_pixel)
{
	memset(bake_rast, 0, sizeof(MBakeRast));

	bake_rast->texels = ibuf->userdata;
	bake_rast->w= ibuf->x;
	bake_rast->h= ibuf->y;
	bake_rast->data= data;
	bake_rast->flush_pixel= flush_pixel;
}

static void flush_pixel(const MResolvePixelData *data, const int x, const int y)
{
	float st[2]= {(x+0.5f)/data->w, (y+0.5f)/data->h};
	float *st0, *st1, *st2;
	float *tang0, *tang1, *tang2;
	float no0[3], no1[3], no2[3];
	float fUV[2], from_tang[3][3], to_tang[3][3];
	float u, v, w, sign;
	int r;

	const int i0= data->i0;
	const int i1= data->i1;
	const int i2= data->i2;

	st0= data->mtface[data->face_index].uv[i0];
	st1= data->mtface[data->face_index].uv[i1];
	st2= data->mtface[data->face_index].uv[i2];

	tang0= data->pvtangent + data->face_index*16 + i0*4;
	tang1= data->pvtangent + data->face_index*16 + i1*4;
	tang2= data->pvtangent + data->face_index*16 + i2*4;

	multiresbake_get_normal(data, no0, data->face_index, i0);	/* can optimize these 3 into one call */
	multiresbake_get_normal(data, no1, data->face_index, i1);
	multiresbake_get_normal(data, no2, data->face_index, i2);

	resolve_tri_uv(fUV, st, st0, st1, st2);

	u= fUV[0];
	v= fUV[1];
	w= 1-u-v;

	/* the sign is the same at all face vertices for any non degenerate face.
	   Just in case we clamp the interpolated value though. */
	sign= (tang0[3]*u + tang1[3]*v + tang2[3]*w)<0 ? (-1.0f) : 1.0f;

	/* this sequence of math is designed specifically as is with great care
	   to be compatible with our shader. Please don't change without good reason. */
	for(r= 0; r<3; r++) {
		from_tang[0][r]= tang0[r]*u + tang1[r]*v + tang2[r]*w;
		from_tang[2][r]= no0[r]*u + no1[r]*v + no2[r]*w;
	}

	cross_v3_v3v3(from_tang[1], from_tang[2], from_tang[0]);  /* B = sign * cross(N, T)  */
	mul_v3_fl(from_tang[1], sign);
	invert_m3_m3(to_tang, from_tang);
	/* sequence end */

	data->pass_data(data->lores_dm, data->hires_dm, data->bake_data,
	                data->face_index, data->lvl, st, to_tang, x, y);
}

static void set_rast_triangle(const MBakeRast *bake_rast, const int x, const int y)
{
	const int w= bake_rast->w;
	const int h= bake_rast->h;

	if(x>=0 && x<w && y>=0 && y<h) {
		if((bake_rast->texels[y*w+x])==0) {
			flush_pixel(bake_rast->data, x, y);
			bake_rast->texels[y*w+x]= FILTER_MASK_USED;
		}
	}
}

static void rasterize_half(const MBakeRast *bake_rast,
                           const float s0_s, const float t0_s, const float s1_s, const float t1_s,
                           const float s0_l, const float t0_l, const float s1_l, const float t1_l,
                           const int y0_in, const int y1_in, const int is_mid_right)
{
	const int s_stable= fabsf(t1_s-t0_s)>FLT_EPSILON ? 1 : 0;
	const int l_stable= fabsf(t1_l-t0_l)>FLT_EPSILON ? 1 : 0;
	const int w= bake_rast->w;
	const int h= bake_rast->h;
	int y, y0, y1;

	if(y1_in<=0 || y0_in>=h)
		return;

	y0= y0_in<0 ? 0 : y0_in;
	y1= y1_in>=h ? h : y1_in;

	for(y= y0; y<y1; y++) {
		/*-b(x-x0) + a(y-y0) = 0 */
		int iXl, iXr, x;
		float x_l= s_stable!=0 ? (s0_s + (((s1_s-s0_s)*(y-t0_s))/(t1_s-t0_s))) : s0_s;
		float x_r= l_stable!=0 ? (s0_l + (((s1_l-s0_l)*(y-t0_l))/(t1_l-t0_l))) : s0_l;

		if(is_mid_right!=0)
			SWAP(float, x_l, x_r);

		iXl= (int)ceilf(x_l);
		iXr= (int)ceilf(x_r);

		if(iXr>0 && iXl<w) {
			iXl= iXl<0?0:iXl;
			iXr= iXr>=w?w:iXr;

			for(x= iXl; x<iXr; x++)
				set_rast_triangle(bake_rast, x, y);
		}
	}
}

static void bake_rasterize(const MBakeRast *bake_rast, const float st0_in[2], const float st1_in[2], const float st2_in[2])
{
	const int w= bake_rast->w;
	const int h= bake_rast->h;
	float slo= st0_in[0]*w - 0.5f;
	float tlo= st0_in[1]*h - 0.5f;
	float smi= st1_in[0]*w - 0.5f;
	float tmi= st1_in[1]*h - 0.5f;
	float shi= st2_in[0]*w - 0.5f;
	float thi= st2_in[1]*h - 0.5f;
	int is_mid_right= 0, ylo, yhi, yhi_beg;

	/* skip degenerates */
	if((slo==smi && tlo==tmi) || (slo==shi && tlo==thi) || (smi==shi && tmi==thi))
		return;

	/* sort by T */
	if(tlo>tmi && tlo>thi) {
		SWAP(float, shi, slo);
		SWAP(float, thi, tlo);
	} else if(tmi>thi) {
		SWAP(float, shi, smi);
		SWAP(float, thi, tmi);
	}

	if(tlo>tmi) {
		SWAP(float, slo, smi);
		SWAP(float, tlo, tmi);
	}

	/* check if mid point is to the left or to the right of the lo-hi edge */
	is_mid_right= (-(shi-slo)*(tmi-thi) + (thi-tlo)*(smi-shi))>0 ? 1 : 0;
	ylo= (int) ceilf(tlo);
	yhi_beg= (int) ceilf(tmi);
	yhi= (int) ceilf(thi);

	/*if(fTmi>ceilf(fTlo))*/
	rasterize_half(bake_rast, slo, tlo, smi, tmi, slo, tlo, shi, thi, ylo, yhi_beg, is_mid_right);
	rasterize_half(bake_rast, smi, tmi, shi, thi, slo, tlo, shi, thi, yhi_beg, yhi, is_mid_right);
}

static int multiresbake_test_break(MultiresBakeRender *bkr)
{
	if(!bkr->stop) {
		/* this means baker is executed outside from job system */
		return 0;
	}

	return G.afbreek;
}

static void do_multires_bake(MultiresBakeRender *bkr, Image* ima, MPassKnownData passKnownData,
                             MInitBakeData initBakeData, MApplyBakeData applyBakeData, MFreeBakeData freeBakeData)
{
	DerivedMesh *dm= bkr->lores_dm;
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	const int lvl= bkr->lvl;
	const int tot_face= dm->getNumFaces(dm);
	MVert *mvert= dm->getVertArray(dm);
	MFace *mface= dm->getFaceArray(dm);
	MTFace *mtface= dm->getFaceDataArray(dm, CD_MTFACE);
	float *pvtangent= NULL;

	if(CustomData_get_layer_index(&dm->faceData, CD_TANGENT) == -1)
		DM_add_tangent_layer(dm);

	pvtangent= DM_get_face_data_layer(dm, CD_TANGENT);

	if(tot_face > 0) {  /* sanity check */
		int f= 0;
		MBakeRast bake_rast;
		MResolvePixelData data={NULL};

		data.mface= mface;
		data.mvert= mvert;
		data.mtface= mtface;
		data.pvtangent= pvtangent;
		data.precomputed_normals= dm->getFaceDataArray(dm, CD_NORMAL);	/* don't strictly need this */
		data.w= ibuf->x;
		data.h= ibuf->y;
		data.lores_dm= dm;
		data.hires_dm= bkr->hires_dm;
		data.lvl= lvl;
		data.pass_data= passKnownData;

		if(initBakeData)
			data.bake_data= initBakeData(bkr, ima);

		init_bake_rast(&bake_rast, ibuf, &data, flush_pixel);

		for(f= 0; f<tot_face; f++) {
			MTFace *mtfate= &mtface[f];
			int verts[3][2], nr_tris, t;

			if(multiresbake_test_break(bkr))
				break;

			if(mtfate->tpage!=ima)
				continue;

			data.face_index= f;

			/* might support other forms of diagonal splits later on such as
			   split by shortest diagonal.*/
			verts[0][0]=0;
			verts[1][0]=1;
			verts[2][0]=2;

			verts[0][1]=0;
			verts[1][1]=2;
			verts[2][1]=3;

			nr_tris= mface[f].v4!=0 ? 2 : 1;
			for(t= 0; t<nr_tris; t++) {
				data.i0= verts[0][t];
				data.i1= verts[1][t];
				data.i2 =verts[2][t];

				bake_rasterize(&bake_rast, mtfate->uv[data.i0], mtfate->uv[data.i1], mtfate->uv[data.i2]);
			}

			bkr->baked_faces++;

			if(bkr->do_update)
				*bkr->do_update= 1;

			if(bkr->progress)
				*bkr->progress= ((float)bkr->baked_objects + (float)bkr->baked_faces / tot_face) / bkr->tot_obj;
		}

		if(applyBakeData)
			applyBakeData(data.bake_data);

		if(freeBakeData)
			freeBakeData(data.bake_data);
	}
}

static void interp_bilinear_quad_data(float data[4][3], float u, float v, float res[3])
{
	float vec[3];

	copy_v3_v3(res, data[0]);
	mul_v3_fl(res, (1-u)*(1-v));
	copy_v3_v3(vec, data[1]);
	mul_v3_fl(vec, u*(1-v)); add_v3_v3(res, vec);
	copy_v3_v3(vec, data[2]);
	mul_v3_fl(vec, u*v); add_v3_v3(res, vec);
	copy_v3_v3(vec, data[3]);
	mul_v3_fl(vec, (1-u)*v); add_v3_v3(res, vec);
}

static void interp_barycentric_tri_data(float data[3][3], float u, float v, float res[3])
{
	float vec[3];

	copy_v3_v3(res, data[0]);
	mul_v3_fl(res, u);
	copy_v3_v3(vec, data[1]);
	mul_v3_fl(vec, v); add_v3_v3(res, vec);
	copy_v3_v3(vec, data[2]);
	mul_v3_fl(vec, 1.0f-u-v); add_v3_v3(res, vec);
}

/* mode = 0: interpolate normals,
   mode = 1: interpolate coord */
static void interp_bilinear_grid(DMGridData *grid, int grid_size, float crn_x, float crn_y, int mode, float res[3])
{
	int x0, x1, y0, y1;
	float u, v;
	float data[4][3];

	x0= (int) crn_x;
	x1= x0>=(grid_size-1) ? (grid_size-1) : (x0+1);

	y0= (int) crn_y;
	y1= y0>=(grid_size-1) ? (grid_size-1) : (y0+1);

	u= crn_x-x0;
	v= crn_y-y0;

	if(mode == 0) {
		copy_v3_v3(data[0], grid[y0 * grid_size + x0].no);
		copy_v3_v3(data[1], grid[y0 * grid_size + x1].no);
		copy_v3_v3(data[2], grid[y1 * grid_size + x1].no);
		copy_v3_v3(data[3], grid[y1 * grid_size + x0].no);
	} else {
		copy_v3_v3(data[0], grid[y0 * grid_size + x0].co);
		copy_v3_v3(data[1], grid[y0 * grid_size + x1].co);
		copy_v3_v3(data[2], grid[y1 * grid_size + x1].co);
		copy_v3_v3(data[3], grid[y1 * grid_size + x0].co);
	}

	interp_bilinear_quad_data(data, u, v, res);
}

static void get_ccgdm_data(DerivedMesh *lodm, DerivedMesh *hidm, const int *origindex,  const int lvl, const int face_index, const float u, const float v, float co[3], float n[3])
{
	MFace mface;
	DMGridData **grid_data;
	float crn_x, crn_y;
	int grid_size, S, face_side;
	int *grid_offset, g_index;

	lodm->getFace(lodm, face_index, &mface);

	grid_size= hidm->getGridSize(hidm);
	grid_data= hidm->getGridData(hidm);
	grid_offset= hidm->getGridOffset(hidm);

	face_side= (grid_size<<1)-1;

	if(lvl==0) {
		g_index= grid_offset[face_index];
		S= mdisp_rot_face_to_crn(mface.v4 ? 4 : 3, face_side, u*(face_side-1), v*(face_side-1), &crn_x, &crn_y);
	} else {
		int side= (1 << (lvl-1)) + 1;
		int grid_index= origindex[face_index];
		int loc_offs= face_index % (1<<(2*lvl));
		int cell_index= loc_offs % ((side-1)*(side-1));
		int cell_side= grid_size / (side-1);
		int row= cell_index / (side-1);
		int col= cell_index % (side-1);

		S= face_index / (1<<(2*(lvl-1))) - grid_offset[grid_index];
		g_index= grid_offset[grid_index];

		crn_y= (row * cell_side) + u * cell_side;
		crn_x= (col * cell_side) + v * cell_side;
	}

	CLAMP(crn_x, 0.0f, grid_size);
	CLAMP(crn_y, 0.0f, grid_size);

	if(n != NULL)
		interp_bilinear_grid(grid_data[g_index + S], grid_size, crn_x, crn_y, 0, n);

	if(co != NULL)
		interp_bilinear_grid(grid_data[g_index + S], grid_size, crn_x, crn_y, 1, co);
}

/* mode = 0: interpolate normals,
   mode = 1: interpolate coord */
static void interp_bilinear_mface(DerivedMesh *dm, MFace *mface, const float u, const float v, const int mode, float res[3])
{
	float data[4][3];

	if(mode == 0) {
		dm->getVertNo(dm, mface->v1, data[0]);
		dm->getVertNo(dm, mface->v2, data[1]);
		dm->getVertNo(dm, mface->v3, data[2]);
		dm->getVertNo(dm, mface->v4, data[3]);
	} else {
		dm->getVertCo(dm, mface->v1, data[0]);
		dm->getVertCo(dm, mface->v2, data[1]);
		dm->getVertCo(dm, mface->v3, data[2]);
		dm->getVertCo(dm, mface->v4, data[3]);
	}

	interp_bilinear_quad_data(data, u, v, res);
}

/* mode = 0: interpolate normals,
   mode = 1: interpolate coord */
static void interp_barycentric_mface(DerivedMesh *dm, MFace *mface, const float u, const float v, const int mode, float res[3])
{
	float data[3][3];

	if(mode == 0) {
		dm->getVertNo(dm, mface->v1, data[0]);
		dm->getVertNo(dm, mface->v2, data[1]);
		dm->getVertNo(dm, mface->v3, data[2]);
	} else {
		dm->getVertCo(dm, mface->v1, data[0]);
		dm->getVertCo(dm, mface->v2, data[1]);
		dm->getVertCo(dm, mface->v3, data[2]);
	}

	interp_barycentric_tri_data(data, u, v, res);
}

static void *init_heights_data(MultiresBakeRender *bkr, Image *ima)
{
	MHeightBakeData *height_data;
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	DerivedMesh *lodm= bkr->lores_dm;

	height_data= MEM_callocN(sizeof(MHeightBakeData), "MultiresBake heightData");

	height_data->ima= ima;
	height_data->heights= MEM_callocN(sizeof(float)*ibuf->x*ibuf->y, "MultiresBake heights");
	height_data->height_max= -FLT_MAX;
	height_data->height_min= FLT_MAX;

	if(!bkr->use_lores_mesh) {
		SubsurfModifierData smd= {{NULL}};
		int ss_lvl= bkr->tot_lvl - bkr->lvl;

		CLAMP(ss_lvl, 0, 6);

		smd.levels= smd.renderLevels= ss_lvl;
		smd.flags|= eSubsurfModifierFlag_SubsurfUv;

		if(bkr->simple)
			smd.subdivType= ME_SIMPLE_SUBSURF;

		height_data->ssdm= subsurf_make_derived_from_derived(bkr->lores_dm, &smd, 0, NULL, 0, 0, 0);
	}

	height_data->origindex= lodm->getFaceDataArray(lodm, CD_ORIGINDEX);

	return (void*)height_data;
}

static void *init_normal_data(MultiresBakeRender *bkr, Image *UNUSED(ima))
{
	MNormalBakeData *normal_data;
	DerivedMesh *lodm= bkr->lores_dm;

	normal_data= MEM_callocN(sizeof(MNormalBakeData), "MultiresBake normalData");

	normal_data->origindex= lodm->getFaceDataArray(lodm, CD_ORIGINDEX);

	return (void*)normal_data;
}

static void free_normal_data(void *bake_data)
{
	MNormalBakeData *normal_data= (MNormalBakeData*)bake_data;

	MEM_freeN(normal_data);
}

static void apply_heights_data(void *bake_data)
{
	MHeightBakeData *height_data= (MHeightBakeData*)bake_data;
	ImBuf *ibuf= BKE_image_get_ibuf(height_data->ima, NULL);
	int x, y, i;
	float height, *heights= height_data->heights;
	float min= height_data->height_min, max= height_data->height_max;

	for(x= 0; x<ibuf->x; x++) {
		for(y =0; y<ibuf->y; y++) {
			i= ibuf->x*y + x;

			if(((char*)ibuf->userdata)[i] != FILTER_MASK_USED)
				continue;

			if(ibuf->rect_float) {
				float *rrgbf= ibuf->rect_float + i*4;

				if(max-min > 1e-5f) height= (heights[i]-min)/(max-min);
				else height= 0;

				rrgbf[0]=rrgbf[1]=rrgbf[2]= height;
			} else {
				char *rrgb= (char*)ibuf->rect + i*4;

				if(max-min > 1e-5f) height= (heights[i]-min)/(max-min);
				else height= 0;

				rrgb[0]=rrgb[1]=rrgb[2]= FTOCHAR(height);
			}
		}
	}

	ibuf->userflags= IB_RECT_INVALID;
}

static void free_heights_data(void *bake_data)
{
	MHeightBakeData *height_data= (MHeightBakeData*)bake_data;

	if(height_data->ssdm)
		height_data->ssdm->release(height_data->ssdm);

	MEM_freeN(height_data->heights);
	MEM_freeN(height_data);
}

/* MultiresBake callback for heights baking
   general idea:
     - find coord of point with specified UV in hi-res mesh (let's call it p1)
     - find coord of point and normal with specified UV in lo-res mesh (or subdivided lo-res
       mesh to make texture smoother) let's call this point p0 and n.
     - height wound be dot(n, p1-p0) */
static void apply_heights_callback(DerivedMesh *lores_dm, DerivedMesh *hires_dm, const void *bake_data,
                                   const int face_index, const int lvl, const float st[2],
                                   float UNUSED(tangmat[3][3]), const int x, const int y)
{
	MTFace *mtface= CustomData_get_layer(&lores_dm->faceData, CD_MTFACE);
	MFace mface;
	Image *ima= mtface[face_index].tpage;
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	MHeightBakeData *height_data= (MHeightBakeData*)bake_data;
	float uv[2], *st0, *st1, *st2, *st3;
	int pixel= ibuf->x*y + x;
	float vec[3], p0[3], p1[3], n[3], len;

	lores_dm->getFace(lores_dm, face_index, &mface);

	st0= mtface[face_index].uv[0];
	st1= mtface[face_index].uv[1];
	st2= mtface[face_index].uv[2];

	if(mface.v4) {
		st3= mtface[face_index].uv[3];
		resolve_quad_uv(uv, st, st0, st1, st2, st3);
	} else
		resolve_tri_uv(uv, st, st0, st1, st2);

	CLAMP(uv[0], 0.0f, 1.0f);
	CLAMP(uv[1], 0.0f, 1.0f);

	get_ccgdm_data(lores_dm, hires_dm, height_data->origindex, lvl, face_index, uv[0], uv[1], p1, 0);

	if(height_data->ssdm) {
		get_ccgdm_data(lores_dm, height_data->ssdm, height_data->origindex, 0, face_index, uv[0], uv[1], p0, n);
	} else {
		lores_dm->getFace(lores_dm, face_index, &mface);

		if(mface.v4) {
			interp_bilinear_mface(lores_dm, &mface, uv[0], uv[1], 1, p0);
			interp_bilinear_mface(lores_dm, &mface, uv[0], uv[1], 0, n);
		} else {
			interp_barycentric_mface(lores_dm, &mface, uv[0], uv[1], 1, p0);
			interp_barycentric_mface(lores_dm, &mface, uv[0], uv[1], 0, n);
		}
	}

	sub_v3_v3v3(vec, p1, p0);
	len= dot_v3v3(n, vec);

	height_data->heights[pixel]= len;
	if(len<height_data->height_min) height_data->height_min= len;
	if(len>height_data->height_max) height_data->height_max= len;

	if(ibuf->rect_float) {
		float *rrgbf= ibuf->rect_float + pixel*4;
		rrgbf[3]= 1.0f;

		ibuf->userflags= IB_RECT_INVALID;
	} else {
		char *rrgb= (char*)ibuf->rect + pixel*4;
		rrgb[3]= 255;
	}
}

/* MultiresBake callback for normals' baking
   general idea:
     - find coord and normal of point with specified UV in hi-res mesh
     - multiply it by tangmat
     - vector in color space would be norm(vec) /2 + (0.5, 0.5, 0.5) */
static void apply_tangmat_callback(DerivedMesh *lores_dm, DerivedMesh *hires_dm, const void *bake_data,
                                   const int face_index, const int lvl, const float st[2],
                                   float tangmat[3][3], const int x, const int y)
{
	MTFace *mtface= CustomData_get_layer(&lores_dm->faceData, CD_MTFACE);
	MFace mface;
	Image *ima= mtface[face_index].tpage;
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	MNormalBakeData *normal_data= (MNormalBakeData*)bake_data;
	float uv[2], *st0, *st1, *st2, *st3;
	int pixel= ibuf->x*y + x;
	float n[3], vec[3], tmp[3]= {0.5, 0.5, 0.5};

	lores_dm->getFace(lores_dm, face_index, &mface);

	st0= mtface[face_index].uv[0];
	st1= mtface[face_index].uv[1];
	st2= mtface[face_index].uv[2];

	if(mface.v4) {
		st3= mtface[face_index].uv[3];
		resolve_quad_uv(uv, st, st0, st1, st2, st3);
	} else
		resolve_tri_uv(uv, st, st0, st1, st2);

	CLAMP(uv[0], 0.0f, 1.0f);
	CLAMP(uv[1], 0.0f, 1.0f);

	get_ccgdm_data(lores_dm, hires_dm, normal_data->origindex, lvl, face_index, uv[0], uv[1], NULL, n);

	mul_v3_m3v3(vec, tangmat, n);
	normalize_v3(vec);
	mul_v3_fl(vec, 0.5);
	add_v3_v3(vec, tmp);

	if(ibuf->rect_float) {
		float *rrgbf= ibuf->rect_float + pixel*4;
		rrgbf[0]= vec[0];
		rrgbf[1]= vec[1];
		rrgbf[2]= vec[2];
		rrgbf[3]= 1.0f;

		ibuf->userflags= IB_RECT_INVALID;
	} else {
		unsigned char *rrgb= (unsigned char *)ibuf->rect + pixel*4;
		rgb_float_to_uchar(rrgb, vec);
		rrgb[3]= 255;
	}
}

static void count_images(MultiresBakeRender *bkr)
{
	int a, totface;
	DerivedMesh *dm= bkr->lores_dm;
	MTFace *mtface= CustomData_get_layer(&dm->faceData, CD_MTFACE);

	bkr->image.first= bkr->image.last= NULL;
	bkr->tot_image= 0;

	totface= dm->getNumFaces(dm);

	for(a= 0; a<totface; a++)
		mtface[a].tpage->id.flag&= ~LIB_DOIT;

	for(a= 0; a<totface; a++) {
		Image *ima= mtface[a].tpage;
		if((ima->id.flag&LIB_DOIT)==0) {
			LinkData *data= BLI_genericNodeN(ima);
			BLI_addtail(&bkr->image, data);
			bkr->tot_image++;
			ima->id.flag|= LIB_DOIT;
		}
	}

	for(a= 0; a<totface; a++)
		mtface[a].tpage->id.flag&= ~LIB_DOIT;
}

static void bake_images(MultiresBakeRender *bkr)
{
	LinkData *link;

	for(link= bkr->image.first; link; link= link->next) {
		Image *ima= (Image*)link->data;
		ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);

		if(ibuf->x>0 && ibuf->y>0) {
			ibuf->userdata= MEM_callocN(ibuf->y*ibuf->x, "MultiresBake imbuf mask");

			switch(bkr->mode) {
				case RE_BAKE_NORMALS:
					do_multires_bake(bkr, ima, apply_tangmat_callback, init_normal_data, NULL, free_normal_data);
					break;
				case RE_BAKE_DISPLACEMENT:
					do_multires_bake(bkr, ima, apply_heights_callback, init_heights_data,
					                 apply_heights_data, free_heights_data);
					break;
			}
		}

		ima->id.flag|= LIB_DOIT;
	}
}

static void finish_images(MultiresBakeRender *bkr)
{
	LinkData *link;

	for(link= bkr->image.first; link; link= link->next) {
		Image *ima= (Image*)link->data;
		ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);

		if(ibuf->x<=0 || ibuf->y<=0)
			continue;

		RE_bake_ibuf_filter(ibuf, (char *)ibuf->userdata, bkr->bake_filter);

		ibuf->userflags|= IB_BITMAPDIRTY;

		if(ibuf->rect_float)
			ibuf->userflags|= IB_RECT_INVALID;

		if(ibuf->mipmap[0]) {
			ibuf->userflags|= IB_MIPMAP_INVALID;
			imb_freemipmapImBuf(ibuf);
		}

		if(ibuf->userdata) {
			MEM_freeN(ibuf->userdata);
			ibuf->userdata= NULL;
		}
	}
}

static void multiresbake_start(MultiresBakeRender *bkr)
{
	count_images(bkr);
	bake_images(bkr);
	finish_images(bkr);
}

static int multiresbake_check(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob;
	Mesh *me;
	MultiresModifierData *mmd;
	int ok= 1, a;

	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		ob= base->object;

		if(ob->type != OB_MESH) {
			BKE_report(op->reports, RPT_ERROR, "Basking of multires data only works with active object which is a mesh");

			ok= 0;
			break;
		}

		me= (Mesh*)ob->data;
		mmd= get_multires_modifier(scene, ob, 0);

		/* Multi-resolution should be and be last in the stack */
		if(ok && mmd) {
			ModifierData *md;

			ok= mmd->totlvl>0;

			for(md = (ModifierData*)mmd->modifier.next; md && ok; md = md->next) {
				if (modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
					ok= 0;
				}
			}
		} else ok= 0;

		if(!ok) {
			BKE_report(op->reports, RPT_ERROR, "Multires data baking requires multi-resolution object");

			break;
		}

		if(!me->mtface) {
			BKE_report(op->reports, RPT_ERROR, "Mesh should be unwrapped before multires data baking");

			ok= 0;
		} else {
			a= me->totface;
			while (ok && a--) {
				Image *ima= me->mtface[a].tpage;

				if(!ima) {
					BKE_report(op->reports, RPT_ERROR, "You should have active texture to use multires baker");

					ok= 0;
				} else {
					ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);

					if(!ibuf) {
						BKE_report(op->reports, RPT_ERROR, "Baking should happend to image with image buffer");

						ok= 0;
					} else {
						if(ibuf->rect==NULL && ibuf->rect_float==NULL)
							ok= 0;

						if(ibuf->rect_float && !(ibuf->channels==0 || ibuf->channels==4))
							ok= 0;

						if(!ok)
							BKE_report(op->reports, RPT_ERROR, "Baking to unsupported image type");
					}
				}
			}
		}

		if(!ok)
			break;
	}
	CTX_DATA_END;

	return ok;
}

static DerivedMesh *multiresbake_create_loresdm(Scene *scene, Object *ob, int *lvl)
{
	DerivedMesh *dm;
	MultiresModifierData *mmd= get_multires_modifier(scene, ob, 0);
	Mesh *me= (Mesh*)ob->data;

	*lvl= mmd->lvl;

	if(*lvl==0) {
		return NULL;
	} else {
		MultiresModifierData tmp_mmd= *mmd;
		DerivedMesh *cddm= CDDM_from_mesh(me, ob);

		tmp_mmd.lvl= *lvl;
		tmp_mmd.sculptlvl= *lvl;
		dm= multires_dm_create_from_derived(&tmp_mmd, 1, cddm, ob, 0, 0);
		cddm->release(cddm);
	}

	return dm;
}

static DerivedMesh *multiresbake_create_hiresdm(Scene *scene, Object *ob, int *lvl, int *simple)
{
	Mesh *me= (Mesh*)ob->data;
	MultiresModifierData *mmd= get_multires_modifier(scene, ob, 0);
	MultiresModifierData tmp_mmd= *mmd;
	DerivedMesh *cddm= CDDM_from_mesh(me, ob);
	DerivedMesh *dm;

	*lvl= mmd->totlvl;
	*simple= mmd->simple;

	tmp_mmd.lvl= mmd->totlvl;
	tmp_mmd.sculptlvl= mmd->totlvl;
	dm= multires_dm_create_from_derived(&tmp_mmd, 1, cddm, ob, 0, 0);
	cddm->release(cddm);

	return dm;
}

static void clear_images(MTFace *mtface, int totface)
{
	int a;
	const float vec_alpha[4]= {0.0f, 0.0f, 0.0f, 0.0f};
	const float vec_solid[4]= {0.0f, 0.0f, 0.0f, 1.0f};

	for(a= 0; a<totface; a++)
		mtface[a].tpage->id.flag&= ~LIB_DOIT;

	for(a= 0; a<totface; a++) {
		Image *ima= mtface[a].tpage;

		if((ima->id.flag&LIB_DOIT)==0) {
			ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);

			IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? vec_alpha : vec_solid);
			ima->id.flag|= LIB_DOIT;
		}
	}

	for(a= 0; a<totface; a++)
		mtface[a].tpage->id.flag&= ~LIB_DOIT;
}

static int multiresbake_image_exec_locked(bContext *C, wmOperator *op)
{
	Object *ob;
	Scene *scene= CTX_data_scene(C);
	int objects_baked= 0;

	if(!multiresbake_check(C, op))
		return OPERATOR_CANCELLED;

	if(scene->r.bake_flag&R_BAKE_CLEAR) {  /* clear images */
		CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
			Mesh *me;

			ob= base->object;
			me= (Mesh*)ob->data;

			clear_images(me->mtface, me->totface);
		}
		CTX_DATA_END;
	}

	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		MultiresBakeRender bkr= {0};

		ob= base->object;

		multires_force_update(ob);

		/* copy data stored in job descriptor */
		bkr.bake_filter= scene->r.bake_filter;
		bkr.mode= scene->r.bake_mode;
		bkr.use_lores_mesh= scene->r.bake_flag&R_BAKE_LORES_MESH;

		/* create low-resolution DM (to bake to) and hi-resolution DM (to bake from) */
		bkr.lores_dm= multiresbake_create_loresdm(scene, ob, &bkr.lvl);

		if(!bkr.lores_dm)
			continue;

		bkr.hires_dm= multiresbake_create_hiresdm(scene, ob, &bkr.tot_lvl, &bkr.simple);

		multiresbake_start(&bkr);

		BLI_freelistN(&bkr.image);

		bkr.lores_dm->release(bkr.lores_dm);
		bkr.hires_dm->release(bkr.hires_dm);

		objects_baked++;
	}
	CTX_DATA_END;

	if(!objects_baked)
		BKE_report(op->reports, RPT_ERROR, "No objects found to bake from");

	return OPERATOR_FINISHED;
}

/* Multiresbake adopted for job-system executing */
static void init_multiresbake_job(bContext *C, MultiresBakeJob *bkj)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob;

	/* backup scene settings, so their changing in UI would take no effect on baker */
	bkj->bake_filter= scene->r.bake_filter;
	bkj->mode= scene->r.bake_mode;
	bkj->use_lores_mesh= scene->r.bake_flag&R_BAKE_LORES_MESH;
	bkj->bake_clear= scene->r.bake_flag&R_BAKE_CLEAR;

	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		MultiresBakerJobData *data;
		DerivedMesh *lores_dm;
		int lvl;
		ob= base->object;

		multires_force_update(ob);

		lores_dm = multiresbake_create_loresdm(scene, ob, &lvl);
		if(!lores_dm)
			continue;

		data= MEM_callocN(sizeof(MultiresBakerJobData), "multiresBaker derivedMesh_data");
		data->lores_dm = lores_dm;
		data->lvl = lvl;
		data->hires_dm = multiresbake_create_hiresdm(scene, ob, &data->tot_lvl, &data->simple);

		BLI_addtail(&bkj->data, data);
	}
	CTX_DATA_END;
}

static void multiresbake_startjob(void *bkv, short *stop, short *do_update, float *progress)
{
	MultiresBakerJobData *data;
	MultiresBakeJob *bkj= bkv;
	int baked_objects= 0, tot_obj;

	tot_obj= BLI_countlist(&bkj->data);

	if(bkj->bake_clear) {  /* clear images */
		for(data= bkj->data.first; data; data= data->next) {
			DerivedMesh *dm= data->lores_dm;
			MTFace *mtface= CustomData_get_layer(&dm->faceData, CD_MTFACE);

			clear_images(mtface, dm->getNumFaces(dm));
		}
	}

	for(data= bkj->data.first; data; data= data->next) {
		MultiresBakeRender bkr= {0};

		/* copy data stored in job descriptor */
		bkr.bake_filter= bkj->bake_filter;
		bkr.mode= bkj->mode;
		bkr.use_lores_mesh= bkj->use_lores_mesh;

		/* create low-resolution DM (to bake to) and hi-resolution DM (to bake from) */
		bkr.lores_dm= data->lores_dm;
		bkr.hires_dm= data->hires_dm;
		bkr.tot_lvl= data->tot_lvl;
		bkr.lvl= data->lvl;
		bkr.simple= data->simple;

		/* needed for proper progress bar */
		bkr.tot_obj= tot_obj;
		bkr.baked_objects= baked_objects;

		bkr.stop= stop;
		bkr.do_update= do_update;
		bkr.progress= progress;

		multiresbake_start(&bkr);

		BLI_freelistN(&bkr.image);

		baked_objects++;
	}
}

static void multiresbake_freejob(void *bkv)
{
	MultiresBakeJob *bkj= bkv;
	MultiresBakerJobData *data, *next;

	data= bkj->data.first;
	while (data) {
		next= data->next;
		data->lores_dm->release(data->lores_dm);
		data->hires_dm->release(data->hires_dm);
		MEM_freeN(data);
		data= next;
	}

	MEM_freeN(bkj);
}

static int multiresbake_image_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	MultiresBakeJob *bkr;
	wmJob *steve;

	if(!multiresbake_check(C, op))
		return OPERATOR_CANCELLED;

	bkr= MEM_callocN(sizeof(MultiresBakeJob), "MultiresBakeJob data");
	init_multiresbake_job(C, bkr);

	if(!bkr->data.first) {
		BKE_report(op->reports, RPT_ERROR, "No objects found to bake from");
		return OPERATOR_CANCELLED;
	}

	/* setup job */
	steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, "Multires Bake", WM_JOB_EXCL_RENDER|WM_JOB_PRIORITY|WM_JOB_PROGRESS);
	WM_jobs_customdata(steve, bkr, multiresbake_freejob);
	WM_jobs_timer(steve, 0.2, NC_IMAGE, 0); /* TODO - only draw bake image, can we enforce this */
	WM_jobs_callbacks(steve, multiresbake_startjob, NULL, NULL, NULL);

	G.afbreek= 0;

	WM_jobs_start(CTX_wm_manager(C), steve);
	WM_cursor_wait(0);

	/* add modal handler for ESC */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* ****************** render BAKING ********************** */

/* threaded break test */
static int thread_break(void *UNUSED(arg))
{
	return G.afbreek;
}

typedef struct BakeRender {
	Render *re;
	Main *main;
	Scene *scene;
	struct Object *actob;
	int result, ready;

	ReportList *reports;

	short *stop;
	short *do_update;
	float *progress;
	
	ListBase threads;

	/* backup */
	short prev_wo_amb_occ;
	short prev_r_raytrace;

	/* for redrawing */
	ScrArea *sa;
} BakeRender;

/* use by exec and invoke */
static int test_bake_internal(bContext *C, ReportList *reports)
{
	Scene *scene= CTX_data_scene(C);

	if((scene->r.bake_flag & R_BAKE_TO_ACTIVE) && CTX_data_active_object(C)==NULL) {
		BKE_report(reports, RPT_ERROR, "No active object");
	}
	else if(scene->r.bake_mode==RE_BAKE_AO && scene->world==NULL) {
		BKE_report(reports, RPT_ERROR, "No world set up");
	}
	else {
		return 1;
	}

	return 0;
}

static void init_bake_internal(BakeRender *bkr, bContext *C)
{
	Scene *scene= CTX_data_scene(C);

	/* get editmode results */
	ED_object_exit_editmode(C, 0);  /* 0 = does not exit editmode */

	bkr->sa= BKE_screen_find_big_area(CTX_wm_screen(C), SPACE_IMAGE, 10); /* can be NULL */
	bkr->main= CTX_data_main(C);
	bkr->scene= scene;
	bkr->actob= (scene->r.bake_flag & R_BAKE_TO_ACTIVE) ? OBACT : NULL;
	bkr->re= RE_NewRender("_Bake View_");

	if(scene->r.bake_mode==RE_BAKE_AO) {
		/* If raytracing or AO is disabled, switch it on temporarily for baking. */
		bkr->prev_wo_amb_occ = (scene->world->mode & WO_AMB_OCC) != 0;
		scene->world->mode |= WO_AMB_OCC;
	}
	if(scene->r.bake_mode==RE_BAKE_AO || bkr->actob) {
		bkr->prev_r_raytrace = (scene->r.mode & R_RAYTRACE) != 0;
		scene->r.mode |= R_RAYTRACE;
	}
}

static void finish_bake_internal(BakeRender *bkr)
{
	RE_Database_Free(bkr->re);

	/* restore raytrace and AO */
	if(bkr->scene->r.bake_mode==RE_BAKE_AO)
		if(bkr->prev_wo_amb_occ == 0)
			bkr->scene->world->mode &= ~WO_AMB_OCC;

	if(bkr->scene->r.bake_mode==RE_BAKE_AO || bkr->actob)
		if(bkr->prev_r_raytrace == 0)
			bkr->scene->r.mode &= ~R_RAYTRACE;

	if(bkr->result==BAKE_RESULT_OK) {
		Image *ima;
		/* force OpenGL reload and mipmap recalc */
		for(ima= G.main->image.first; ima; ima= ima->id.next) {
			if(ima->ok==IMA_OK_LOADED) {
				ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
				if(ibuf) {
					if(ibuf->userflags & IB_BITMAPDIRTY) {
						GPU_free_image(ima);
						imb_freemipmapImBuf(ibuf);
					}

					/* freed when baking is done, but if its canceled we need to free here */
					if (ibuf->userdata) {
						MEM_freeN(ibuf->userdata);
						ibuf->userdata= NULL;
					}
				}
			}
		}
	}
}

static void *do_bake_render(void *bake_v)
{
	BakeRender *bkr= bake_v;

	bkr->result= RE_bake_shade_all_selected(bkr->re, bkr->scene->r.bake_mode, bkr->actob, NULL, bkr->progress);
	bkr->ready= 1;

	return NULL;
}

static void bake_startjob(void *bkv, short *stop, short *do_update, float *progress)
{
	BakeRender *bkr= bkv;
	Scene *scene= bkr->scene;
	Main *bmain= bkr->main;

	bkr->stop= stop;
	bkr->do_update= do_update;
	bkr->progress= progress;

	RE_test_break_cb(bkr->re, NULL, thread_break);
	G.afbreek= 0;	/* blender_test_break uses this global */

	RE_Database_Baking(bkr->re, bmain, scene, scene->lay, scene->r.bake_mode, bkr->actob);

	/* baking itself is threaded, cannot use test_break in threads. we also update optional imagewindow */
	bkr->result= RE_bake_shade_all_selected(bkr->re, scene->r.bake_mode, bkr->actob, bkr->do_update, bkr->progress);
}

static void bake_update(void *bkv)
{
	BakeRender *bkr= bkv;

	if(bkr->sa && bkr->sa->spacetype==SPACE_IMAGE) { /* incase the user changed while baking */
		SpaceImage *sima= bkr->sa->spacedata.first;
		if(sima)
			sima->image= RE_bake_shade_get_image();
	}
}

static void bake_freejob(void *bkv)
{
	BakeRender *bkr= bkv;
	finish_bake_internal(bkr);

	if(bkr->result==BAKE_RESULT_NO_OBJECTS)
		BKE_report(bkr->reports, RPT_ERROR, "No objects or images found to bake to");
	else if(bkr->result==BAKE_RESULT_FEEDBACK_LOOP)
		BKE_report(bkr->reports, RPT_WARNING, "Feedback loop detected");

	MEM_freeN(bkr);
	G.rendering = 0;
}

/* catch esc */
static int objects_bake_render_modal(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	/* no running blender, remove handler and pass through */
	if(0==WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C)))
		return OPERATOR_FINISHED|OPERATOR_PASS_THROUGH;

	/* running render */
	switch (event->type) {
		case ESCKEY:
			return OPERATOR_RUNNING_MODAL;
			break;
	}
	return OPERATOR_PASS_THROUGH;
}

static int is_multires_bake(Scene *scene)
{
	if ( ELEM(scene->r.bake_mode, RE_BAKE_NORMALS, RE_BAKE_DISPLACEMENT))
		return scene->r.bake_flag & R_BAKE_MULTIRES;

	return 0;
}

static int objects_bake_render_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(_event))
{
	Scene *scene= CTX_data_scene(C);
	int result= OPERATOR_CANCELLED;

	if(is_multires_bake(scene)) {
		result= multiresbake_image_exec(C, op);
	} else {
		/* only one render job at a time */
		if(WM_jobs_test(CTX_wm_manager(C), scene))
			return OPERATOR_CANCELLED;

		if(test_bake_internal(C, op->reports)==0) {
			return OPERATOR_CANCELLED;
		}
		else {
			BakeRender *bkr= MEM_callocN(sizeof(BakeRender), "render bake");
			wmJob *steve;

			init_bake_internal(bkr, C);
			bkr->reports= op->reports;

			/* setup job */
			steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, "Texture Bake", WM_JOB_EXCL_RENDER|WM_JOB_PRIORITY|WM_JOB_PROGRESS);
			WM_jobs_customdata(steve, bkr, bake_freejob);
			WM_jobs_timer(steve, 0.2, NC_IMAGE, 0); /* TODO - only draw bake image, can we enforce this */
			WM_jobs_callbacks(steve, bake_startjob, NULL, bake_update, NULL);

			G.afbreek= 0;
			G.rendering = 1;

			WM_jobs_start(CTX_wm_manager(C), steve);

			WM_cursor_wait(0);

			/* add modal handler for ESC */
			WM_event_add_modal_handler(C, op);
		}

		result= OPERATOR_RUNNING_MODAL;
	}

	WM_event_add_notifier(C, NC_SCENE|ND_RENDER_RESULT, scene);

	return result;
}


static int bake_image_exec(bContext *C, wmOperator *op)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	int result= OPERATOR_CANCELLED;

	if(is_multires_bake(scene)) {
		result= multiresbake_image_exec_locked(C, op);
	} else  {
		if(test_bake_internal(C, op->reports)==0) {
			return OPERATOR_CANCELLED;
		}
		else {
			ListBase threads;
			BakeRender bkr= {NULL};

			init_bake_internal(&bkr, C);
			bkr.reports= op->reports;

			RE_test_break_cb(bkr.re, NULL, thread_break);
			G.afbreek= 0;	/* blender_test_break uses this global */

			RE_Database_Baking(bkr.re, bmain, scene, scene->lay, scene->r.bake_mode, (scene->r.bake_flag & R_BAKE_TO_ACTIVE)? OBACT: NULL);

			/* baking itself is threaded, cannot use test_break in threads  */
			BLI_init_threads(&threads, do_bake_render, 1);
			bkr.ready= 0;
			BLI_insert_thread(&threads, &bkr);

			while(bkr.ready==0) {
				PIL_sleep_ms(50);
				if(bkr.ready)
					break;

				/* used to redraw in 2.4x but this is just for exec in 2.5 */
				if (!G.background)
					blender_test_break();
			}
			BLI_end_threads(&threads);

			if(bkr.result==BAKE_RESULT_NO_OBJECTS)
				BKE_report(op->reports, RPT_ERROR, "No valid images found to bake to");
			else if(bkr.result==BAKE_RESULT_FEEDBACK_LOOP)
				BKE_report(op->reports, RPT_ERROR, "Feedback loop detected");

			finish_bake_internal(&bkr);

			result= OPERATOR_FINISHED;
		}
	}

	WM_event_add_notifier(C, NC_SCENE|ND_RENDER_RESULT, scene);

	return result;
}

void OBJECT_OT_bake_image(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Bake";
	ot->description= "Bake image textures of selected objects";
	ot->idname= "OBJECT_OT_bake_image";

	/* api callbacks */
	ot->exec= bake_image_exec;
	ot->invoke= objects_bake_render_invoke;
	ot->modal= objects_bake_render_modal;
}
