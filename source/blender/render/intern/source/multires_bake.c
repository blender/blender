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
 * The Original Code is Copyright (C) 2012 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Morten Mikkelsen,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/multires_bake.c
 *  \ingroup render
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_threads.h"

#include "BKE_ccg.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_multires.h"
#include "BKE_modifier.h"
#include "BKE_subsurf.h"

#include "RE_multires_bake.h"
#include "RE_pipeline.h"
#include "RE_shader_ext.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "rayintersection.h"
#include "rayobject.h"
#include "rendercore.h"

typedef void (*MPassKnownData)(DerivedMesh *lores_dm, DerivedMesh *hires_dm, void *thread_data,
                               void *bake_data, ImBuf *ibuf, const int face_index, const int lvl,
                               const float st[2], float tangmat[3][3], const int x, const int y);

typedef void * (*MInitBakeData)(MultiresBakeRender *bkr, Image *ima);
typedef void   (*MFreeBakeData)(void *bake_data);

typedef struct MultiresBakeResult {
	float height_min, height_max;
} MultiresBakeResult;

typedef struct {
	MVert *mvert;
	MFace *mface;
	MTFace *mtface;
	float *pvtangent;
	const float *precomputed_normals;
	int w, h;
	int face_index;
	int i0, i1, i2;
	DerivedMesh *lores_dm, *hires_dm;
	int lvl;
	void *thread_data;
	void *bake_data;
	ImBuf *ibuf;
	MPassKnownData pass_data;
} MResolvePixelData;

typedef void (*MFlushPixel)(const MResolvePixelData *data, const int x, const int y);

typedef struct {
	int w, h;
	char *texels;
	const MResolvePixelData *data;
	MFlushPixel flush_pixel;
	short *do_update;
} MBakeRast;

typedef struct {
	float *heights;
	Image *ima;
	DerivedMesh *ssdm;
	const int *orig_index_mf_to_mpoly;
	const int *orig_index_mp_to_orig;
} MHeightBakeData;

typedef struct {
	const int *orig_index_mf_to_mpoly;
	const int *orig_index_mp_to_orig;
} MNormalBakeData;

typedef struct {
	int number_of_rays;
	float bias;

	unsigned short *permutation_table_1;
	unsigned short *permutation_table_2;

	RayObject *raytree;
	RayFace *rayfaces;

	const int *orig_index_mf_to_mpoly;
	const int *orig_index_mp_to_orig;
} MAOBakeData;

static void multiresbake_get_normal(const MResolvePixelData *data, float norm[],const int face_num, const int vert_index)
{
	unsigned int indices[] = {data->mface[face_num].v1, data->mface[face_num].v2,
	                          data->mface[face_num].v3, data->mface[face_num].v4};
	const int smoothnormal = (data->mface[face_num].flag & ME_SMOOTH);

	if (!smoothnormal) { /* flat */
		if (data->precomputed_normals) {
			copy_v3_v3(norm, &data->precomputed_normals[3 * face_num]);
		}
		else {
			float nor[3];
			const float *p0, *p1, *p2;
			const int iGetNrVerts = data->mface[face_num].v4 != 0 ? 4 : 3;

			p0 = data->mvert[indices[0]].co;
			p1 = data->mvert[indices[1]].co;
			p2 = data->mvert[indices[2]].co;

			if (iGetNrVerts == 4) {
				const float *p3 = data->mvert[indices[3]].co;
				normal_quad_v3(nor, p0, p1, p2, p3);
			}
			else {
				normal_tri_v3(nor, p0, p1, p2);
			}

			copy_v3_v3(norm, nor);
		}
	}
	else {
		const short *no = data->mvert[indices[vert_index]].no;

		normal_short_to_float_v3(norm, no);
		normalize_v3(norm);
	}
}

static void init_bake_rast(MBakeRast *bake_rast, const ImBuf *ibuf, const MResolvePixelData *data,
                           MFlushPixel flush_pixel, short *do_update)
{
	BakeImBufuserData *userdata = (BakeImBufuserData *) ibuf->userdata;

	memset(bake_rast, 0, sizeof(MBakeRast));

	bake_rast->texels = userdata->mask_buffer;
	bake_rast->w = ibuf->x;
	bake_rast->h = ibuf->y;
	bake_rast->data = data;
	bake_rast->flush_pixel = flush_pixel;
	bake_rast->do_update = do_update;
}

static void flush_pixel(const MResolvePixelData *data, const int x, const int y)
{
	float st[2] = {(x + 0.5f) / data->w, (y + 0.5f) / data->h};
	const float *st0, *st1, *st2;
	const float *tang0, *tang1, *tang2;
	float no0[3], no1[3], no2[3];
	float fUV[2], from_tang[3][3], to_tang[3][3];
	float u, v, w, sign;
	int r;

	const int i0 = data->i0;
	const int i1 = data->i1;
	const int i2 = data->i2;

	st0 = data->mtface[data->face_index].uv[i0];
	st1 = data->mtface[data->face_index].uv[i1];
	st2 = data->mtface[data->face_index].uv[i2];

	multiresbake_get_normal(data, no0, data->face_index, i0);   /* can optimize these 3 into one call */
	multiresbake_get_normal(data, no1, data->face_index, i1);
	multiresbake_get_normal(data, no2, data->face_index, i2);

	resolve_tri_uv_v2(fUV, st, st0, st1, st2);

	u = fUV[0];
	v = fUV[1];
	w = 1 - u - v;

	if (data->pvtangent) {
		tang0 = data->pvtangent + data->face_index * 16 + i0 * 4;
		tang1 = data->pvtangent + data->face_index * 16 + i1 * 4;
		tang2 = data->pvtangent + data->face_index * 16 + i2 * 4;

		/* the sign is the same at all face vertices for any non degenerate face.
		 * Just in case we clamp the interpolated value though. */
		sign = (tang0[3] * u + tang1[3] * v + tang2[3] * w) < 0 ? (-1.0f) : 1.0f;

		/* this sequence of math is designed specifically as is with great care
		 * to be compatible with our shader. Please don't change without good reason. */
		for (r = 0; r < 3; r++) {
			from_tang[0][r] = tang0[r] * u + tang1[r] * v + tang2[r] * w;
			from_tang[2][r] = no0[r] * u + no1[r] * v + no2[r] * w;
		}

		cross_v3_v3v3(from_tang[1], from_tang[2], from_tang[0]);  /* B = sign * cross(N, T)  */
		mul_v3_fl(from_tang[1], sign);
		invert_m3_m3(to_tang, from_tang);
	}
	else {
		zero_m3(to_tang);
	}

	data->pass_data(data->lores_dm, data->hires_dm, data->thread_data, data->bake_data,
	                data->ibuf, data->face_index, data->lvl, st, to_tang, x, y);
}

static void set_rast_triangle(const MBakeRast *bake_rast, const int x, const int y)
{
	const int w = bake_rast->w;
	const int h = bake_rast->h;

	if (x >= 0 && x < w && y >= 0 && y < h) {
		if ((bake_rast->texels[y * w + x]) == 0) {
			bake_rast->texels[y * w + x] = FILTER_MASK_USED;
			flush_pixel(bake_rast->data, x, y);
			if (bake_rast->do_update) {
				*bake_rast->do_update = true;
			}
		}
	}
}

static void rasterize_half(const MBakeRast *bake_rast,
                           const float s0_s, const float t0_s, const float s1_s, const float t1_s,
                           const float s0_l, const float t0_l, const float s1_l, const float t1_l,
                           const int y0_in, const int y1_in, const int is_mid_right)
{
	const int s_stable = fabsf(t1_s - t0_s) > FLT_EPSILON ? 1 : 0;
	const int l_stable = fabsf(t1_l - t0_l) > FLT_EPSILON ? 1 : 0;
	const int w = bake_rast->w;
	const int h = bake_rast->h;
	int y, y0, y1;

	if (y1_in <= 0 || y0_in >= h)
		return;

	y0 = y0_in < 0 ? 0 : y0_in;
	y1 = y1_in >= h ? h : y1_in;

	for (y = y0; y < y1; y++) {
		/*-b(x-x0) + a(y-y0) = 0 */
		int iXl, iXr, x;
		float x_l = s_stable != 0 ? (s0_s + (((s1_s - s0_s) * (y - t0_s)) / (t1_s - t0_s))) : s0_s;
		float x_r = l_stable != 0 ? (s0_l + (((s1_l - s0_l) * (y - t0_l)) / (t1_l - t0_l))) : s0_l;

		if (is_mid_right != 0)
			SWAP(float, x_l, x_r);

		iXl = (int)ceilf(x_l);
		iXr = (int)ceilf(x_r);

		if (iXr > 0 && iXl < w) {
			iXl = iXl < 0 ? 0 : iXl;
			iXr = iXr >= w ? w : iXr;

			for (x = iXl; x < iXr; x++)
				set_rast_triangle(bake_rast, x, y);
		}
	}
}

static void bake_rasterize(const MBakeRast *bake_rast, const float st0_in[2], const float st1_in[2], const float st2_in[2])
{
	const int w = bake_rast->w;
	const int h = bake_rast->h;
	float slo = st0_in[0] * w - 0.5f;
	float tlo = st0_in[1] * h - 0.5f;
	float smi = st1_in[0] * w - 0.5f;
	float tmi = st1_in[1] * h - 0.5f;
	float shi = st2_in[0] * w - 0.5f;
	float thi = st2_in[1] * h - 0.5f;
	int is_mid_right = 0, ylo, yhi, yhi_beg;

	/* skip degenerates */
	if ((slo == smi && tlo == tmi) || (slo == shi && tlo == thi) || (smi == shi && tmi == thi))
		return;

	/* sort by T */
	if (tlo > tmi && tlo > thi) {
		SWAP(float, shi, slo);
		SWAP(float, thi, tlo);
	}
	else if (tmi > thi) {
		SWAP(float, shi, smi);
		SWAP(float, thi, tmi);
	}

	if (tlo > tmi) {
		SWAP(float, slo, smi);
		SWAP(float, tlo, tmi);
	}

	/* check if mid point is to the left or to the right of the lo-hi edge */
	is_mid_right = (-(shi - slo) * (tmi - thi) + (thi - tlo) * (smi - shi)) > 0 ? 1 : 0;
	ylo = (int) ceilf(tlo);
	yhi_beg = (int) ceilf(tmi);
	yhi = (int) ceilf(thi);

	/*if (fTmi>ceilf(fTlo))*/
	rasterize_half(bake_rast, slo, tlo, smi, tmi, slo, tlo, shi, thi, ylo, yhi_beg, is_mid_right);
	rasterize_half(bake_rast, smi, tmi, shi, thi, slo, tlo, shi, thi, yhi_beg, yhi, is_mid_right);
}

static int multiresbake_test_break(MultiresBakeRender *bkr)
{
	if (!bkr->stop) {
		/* this means baker is executed outside from job system */
		return 0;
	}

	return *bkr->stop || G.is_break;
}

/* **** Threading routines **** */

typedef struct MultiresBakeQueue {
	int cur_face;
	int tot_face;
	SpinLock spin;
} MultiresBakeQueue;

typedef struct MultiresBakeThread {
	/* this data is actually shared between all the threads */
	MultiresBakeQueue *queue;
	MultiresBakeRender *bkr;
	Image *image;
	void *bake_data;

	/* thread-specific data */
	MBakeRast bake_rast;
	MResolvePixelData data;

	/* displacement-specific data */
	float height_min, height_max;
} MultiresBakeThread;

static int multires_bake_queue_next_face(MultiresBakeQueue *queue)
{
	int face = -1;

	/* TODO: it could worth making it so thread will handle neighbor faces
	 *       for better memory cache utilization
	 */

	BLI_spin_lock(&queue->spin);
	if (queue->cur_face < queue->tot_face) {
		face = queue->cur_face;
		queue->cur_face++;
	}
	BLI_spin_unlock(&queue->spin);

	return face;
}

static void *do_multires_bake_thread(void *data_v)
{
	MultiresBakeThread *handle = (MultiresBakeThread *) data_v;
	MResolvePixelData *data = &handle->data;
	MBakeRast *bake_rast = &handle->bake_rast;
	MultiresBakeRender *bkr = handle->bkr;
	int f;

	while ((f = multires_bake_queue_next_face(handle->queue)) >= 0) {
		MTFace *mtfate = &data->mtface[f];
		int verts[3][2], nr_tris, t;

		if (multiresbake_test_break(bkr))
			break;

		if (mtfate->tpage != handle->image)
			continue;

		data->face_index = f;

		/* might support other forms of diagonal splits later on such as
		 * split by shortest diagonal.*/
		verts[0][0] = 0;
		verts[1][0] = 1;
		verts[2][0] = 2;

		verts[0][1] = 0;
		verts[1][1] = 2;
		verts[2][1] = 3;

		nr_tris = data->mface[f].v4 != 0 ? 2 : 1;
		for (t = 0; t < nr_tris; t++) {
			data->i0 = verts[0][t];
			data->i1 = verts[1][t];
			data->i2 = verts[2][t];

			bake_rasterize(bake_rast, mtfate->uv[data->i0], mtfate->uv[data->i1], mtfate->uv[data->i2]);

			/* tag image buffer for refresh */
			if (data->ibuf->rect_float)
				data->ibuf->userflags |= IB_RECT_INVALID;

			data->ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
		}

		/* update progress */
		BLI_spin_lock(&handle->queue->spin);
		bkr->baked_faces++;

		if (bkr->do_update)
			*bkr->do_update = true;

		if (bkr->progress)
			*bkr->progress = ((float)bkr->baked_objects + (float)bkr->baked_faces / handle->queue->tot_face) / bkr->tot_obj;
		BLI_spin_unlock(&handle->queue->spin);
	}

	return NULL;
}

/* some of arrays inside ccgdm are lazy-initialized, which will generally
 * require lock around accessing such data
 * this function will ensure all arrays are allocated before threading started
 */
static void init_ccgdm_arrays(DerivedMesh *dm)
{
	CCGElem **grid_data;
	CCGKey key;
	int grid_size;
	const int *grid_offset;

	grid_size = dm->getGridSize(dm);
	grid_data = dm->getGridData(dm);
	grid_offset = dm->getGridOffset(dm);
	dm->getGridKey(dm, &key);

	(void) grid_size;
	(void) grid_data;
	(void) grid_offset;
}

static void do_multires_bake(MultiresBakeRender *bkr, Image *ima, bool require_tangent, MPassKnownData passKnownData,
                             MInitBakeData initBakeData, MFreeBakeData freeBakeData, MultiresBakeResult *result)
{
	DerivedMesh *dm = bkr->lores_dm;
	const int lvl = bkr->lvl;
	const int tot_face = dm->getNumTessFaces(dm);

	if (tot_face > 0) {
		MultiresBakeThread *handles;
		MultiresBakeQueue queue;

		ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
		MVert *mvert = dm->getVertArray(dm);
		MFace *mface = dm->getTessFaceArray(dm);
		MTFace *mtface = dm->getTessFaceDataArray(dm, CD_MTFACE);
		const float *precomputed_normals = dm->getTessFaceDataArray(dm, CD_NORMAL);
		float *pvtangent = NULL;

		ListBase threads;
		int i, tot_thread = bkr->threads > 0 ? bkr->threads : BLI_system_thread_count();

		void *bake_data = NULL;

		if (require_tangent) {
			if (CustomData_get_layer_index(&dm->faceData, CD_TANGENT) == -1)
				DM_add_tangent_layer(dm);

			pvtangent = DM_get_tessface_data_layer(dm, CD_TANGENT);
		}

		/* all threads shares the same custom bake data */
		if (initBakeData)
			bake_data = initBakeData(bkr, ima);

		if (tot_thread > 1)
			BLI_init_threads(&threads, do_multires_bake_thread, tot_thread);

		handles = MEM_callocN(tot_thread * sizeof(MultiresBakeThread), "do_multires_bake handles");

		init_ccgdm_arrays(bkr->hires_dm);

		/* faces queue */
		queue.cur_face = 0;
		queue.tot_face = tot_face;
		BLI_spin_init(&queue.spin);

		/* fill in threads handles */
		for (i = 0; i < tot_thread; i++) {
			MultiresBakeThread *handle = &handles[i];

			handle->bkr = bkr;
			handle->image = ima;
			handle->queue = &queue;

			handle->data.mface = mface;
			handle->data.mvert = mvert;
			handle->data.mtface = mtface;
			handle->data.pvtangent = pvtangent;
			handle->data.precomputed_normals = precomputed_normals;  /* don't strictly need this */
			handle->data.w = ibuf->x;
			handle->data.h = ibuf->y;
			handle->data.lores_dm = dm;
			handle->data.hires_dm = bkr->hires_dm;
			handle->data.lvl = lvl;
			handle->data.pass_data = passKnownData;
			handle->data.thread_data = handle;
			handle->data.bake_data = bake_data;
			handle->data.ibuf = ibuf;

			handle->height_min = FLT_MAX;
			handle->height_max = -FLT_MAX;

			init_bake_rast(&handle->bake_rast, ibuf, &handle->data, flush_pixel, bkr->do_update);

			if (tot_thread > 1)
				BLI_insert_thread(&threads, handle);
		}

		/* run threads */
		if (tot_thread > 1)
			BLI_end_threads(&threads);
		else
			do_multires_bake_thread(&handles[0]);

		/* construct bake result */
		result->height_min = handles[0].height_min;
		result->height_max = handles[0].height_max;

		for (i = 1; i < tot_thread; i++) {
			result->height_min = min_ff(result->height_min, handles[i].height_min);
			result->height_max = max_ff(result->height_max, handles[i].height_max);
		}

		BLI_spin_end(&queue.spin);

		/* finalize baking */
		if (freeBakeData)
			freeBakeData(bake_data);

		MEM_freeN(handles);

		BKE_image_release_ibuf(ima, ibuf, NULL);
	}
}

/* mode = 0: interpolate normals,
 * mode = 1: interpolate coord */
static void interp_bilinear_grid(CCGKey *key, CCGElem *grid, float crn_x, float crn_y, int mode, float res[3])
{
	int x0, x1, y0, y1;
	float u, v;
	float data[4][3];

	x0 = (int) crn_x;
	x1 = x0 >= (key->grid_size - 1) ? (key->grid_size - 1) : (x0 + 1);

	y0 = (int) crn_y;
	y1 = y0 >= (key->grid_size - 1) ? (key->grid_size - 1) : (y0 + 1);

	u = crn_x - x0;
	v = crn_y - y0;

	if (mode == 0) {
		copy_v3_v3(data[0], CCG_grid_elem_no(key, grid, x0, y0));
		copy_v3_v3(data[1], CCG_grid_elem_no(key, grid, x1, y0));
		copy_v3_v3(data[2], CCG_grid_elem_no(key, grid, x1, y1));
		copy_v3_v3(data[3], CCG_grid_elem_no(key, grid, x0, y1));
	}
	else {
		copy_v3_v3(data[0], CCG_grid_elem_co(key, grid, x0, y0));
		copy_v3_v3(data[1], CCG_grid_elem_co(key, grid, x1, y0));
		copy_v3_v3(data[2], CCG_grid_elem_co(key, grid, x1, y1));
		copy_v3_v3(data[3], CCG_grid_elem_co(key, grid, x0, y1));
	}

	interp_bilinear_quad_v3(data, u, v, res);
}

static void get_ccgdm_data(DerivedMesh *lodm, DerivedMesh *hidm,
                           const int *index_mf_to_mpoly, const int *index_mp_to_orig,
                           const int lvl, const int face_index, const float u, const float v, float co[3], float n[3])
{
	MFace mface;
	CCGElem **grid_data;
	CCGKey key;
	float crn_x, crn_y;
	int grid_size, S, face_side;
	int *grid_offset, g_index;

	lodm->getTessFace(lodm, face_index, &mface);

	grid_size = hidm->getGridSize(hidm);
	grid_data = hidm->getGridData(hidm);
	grid_offset = hidm->getGridOffset(hidm);
	hidm->getGridKey(hidm, &key);

	face_side = (grid_size << 1) - 1;

	if (lvl == 0) {
		g_index = grid_offset[face_index];
		S = mdisp_rot_face_to_crn(mface.v4 ? 4 : 3, face_side, u * (face_side - 1), v * (face_side - 1), &crn_x, &crn_y);
	}
	else {
		int side = (1 << (lvl - 1)) + 1;
		int grid_index = DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, face_index);
		int loc_offs = face_index % (1 << (2 * lvl));
		int cell_index = loc_offs % ((side - 1) * (side - 1));
		int cell_side = (grid_size - 1) / (side - 1);
		int row = cell_index / (side - 1);
		int col = cell_index % (side - 1);

		S = face_index / (1 << (2 * (lvl - 1))) - grid_offset[grid_index];
		g_index = grid_offset[grid_index];

		crn_y = (row * cell_side) + u * cell_side;
		crn_x = (col * cell_side) + v * cell_side;
	}

	CLAMP(crn_x, 0.0f, grid_size);
	CLAMP(crn_y, 0.0f, grid_size);

	if (n != NULL)
		interp_bilinear_grid(&key, grid_data[g_index + S], crn_x, crn_y, 0, n);

	if (co != NULL)
		interp_bilinear_grid(&key, grid_data[g_index + S], crn_x, crn_y, 1, co);
}

/* mode = 0: interpolate normals,
 * mode = 1: interpolate coord */
static void interp_bilinear_mface(DerivedMesh *dm, MFace *mface, const float u, const float v, const int mode, float res[3])
{
	float data[4][3];

	if (mode == 0) {
		dm->getVertNo(dm, mface->v1, data[0]);
		dm->getVertNo(dm, mface->v2, data[1]);
		dm->getVertNo(dm, mface->v3, data[2]);
		dm->getVertNo(dm, mface->v4, data[3]);
	}
	else {
		dm->getVertCo(dm, mface->v1, data[0]);
		dm->getVertCo(dm, mface->v2, data[1]);
		dm->getVertCo(dm, mface->v3, data[2]);
		dm->getVertCo(dm, mface->v4, data[3]);
	}

	interp_bilinear_quad_v3(data, u, v, res);
}

/* mode = 0: interpolate normals,
 * mode = 1: interpolate coord */
static void interp_barycentric_mface(DerivedMesh *dm, MFace *mface, const float u, const float v, const int mode, float res[3])
{
	float data[3][3];

	if (mode == 0) {
		dm->getVertNo(dm, mface->v1, data[0]);
		dm->getVertNo(dm, mface->v2, data[1]);
		dm->getVertNo(dm, mface->v3, data[2]);
	}
	else {
		dm->getVertCo(dm, mface->v1, data[0]);
		dm->getVertCo(dm, mface->v2, data[1]);
		dm->getVertCo(dm, mface->v3, data[2]);
	}

	interp_barycentric_tri_v3(data, u, v, res);
}

/* **************** Displacement Baker **************** */

static void *init_heights_data(MultiresBakeRender *bkr, Image *ima)
{
	MHeightBakeData *height_data;
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
	DerivedMesh *lodm = bkr->lores_dm;
	BakeImBufuserData *userdata = ibuf->userdata;

	if (userdata->displacement_buffer == NULL)
		userdata->displacement_buffer = MEM_callocN(sizeof(float) * ibuf->x * ibuf->y, "MultiresBake heights");

	height_data = MEM_callocN(sizeof(MHeightBakeData), "MultiresBake heightData");

	height_data->ima = ima;
	height_data->heights = userdata->displacement_buffer;

	if (!bkr->use_lores_mesh) {
		SubsurfModifierData smd = {{NULL}};
		int ss_lvl = bkr->tot_lvl - bkr->lvl;

		CLAMP(ss_lvl, 0, 6);

		if (ss_lvl > 0) {
			smd.levels = smd.renderLevels = ss_lvl;
			smd.flags |= eSubsurfModifierFlag_SubsurfUv;

			if (bkr->simple)
				smd.subdivType = ME_SIMPLE_SUBSURF;

			height_data->ssdm = subsurf_make_derived_from_derived(bkr->lores_dm, &smd, NULL, 0);
			init_ccgdm_arrays(height_data->ssdm);
		}
	}

	height_data->orig_index_mf_to_mpoly = lodm->getTessFaceDataArray(lodm, CD_ORIGINDEX);
	height_data->orig_index_mp_to_orig = lodm->getPolyDataArray(lodm, CD_ORIGINDEX);

	BKE_image_release_ibuf(ima, ibuf, NULL);

	return (void *)height_data;
}

static void free_heights_data(void *bake_data)
{
	MHeightBakeData *height_data = (MHeightBakeData *)bake_data;

	if (height_data->ssdm)
		height_data->ssdm->release(height_data->ssdm);

	MEM_freeN(height_data);
}

/* MultiresBake callback for heights baking
 * general idea:
 *   - find coord of point with specified UV in hi-res mesh (let's call it p1)
 *   - find coord of point and normal with specified UV in lo-res mesh (or subdivided lo-res
 *     mesh to make texture smoother) let's call this point p0 and n.
 *   - height wound be dot(n, p1-p0) */
static void apply_heights_callback(DerivedMesh *lores_dm, DerivedMesh *hires_dm, void *thread_data_v, void *bake_data,
                                   ImBuf *ibuf, const int face_index, const int lvl, const float st[2],
                                   float UNUSED(tangmat[3][3]), const int x, const int y)
{
	MTFace *mtface = CustomData_get_layer(&lores_dm->faceData, CD_MTFACE);
	MFace mface;
	MHeightBakeData *height_data = (MHeightBakeData *)bake_data;
	MultiresBakeThread *thread_data = (MultiresBakeThread *) thread_data_v;
	float uv[2], *st0, *st1, *st2, *st3;
	int pixel = ibuf->x * y + x;
	float vec[3], p0[3], p1[3], n[3], len;

	lores_dm->getTessFace(lores_dm, face_index, &mface);

	st0 = mtface[face_index].uv[0];
	st1 = mtface[face_index].uv[1];
	st2 = mtface[face_index].uv[2];

	if (mface.v4) {
		st3 = mtface[face_index].uv[3];
		resolve_quad_uv_v2(uv, st, st0, st1, st2, st3);
	}
	else
		resolve_tri_uv_v2(uv, st, st0, st1, st2);

	CLAMP(uv[0], 0.0f, 1.0f);
	CLAMP(uv[1], 0.0f, 1.0f);

	get_ccgdm_data(lores_dm, hires_dm,
	               height_data->orig_index_mf_to_mpoly, height_data->orig_index_mp_to_orig,
	               lvl, face_index, uv[0], uv[1], p1, NULL);

	if (height_data->ssdm) {
		get_ccgdm_data(lores_dm, height_data->ssdm,
		               height_data->orig_index_mf_to_mpoly, height_data->orig_index_mp_to_orig,
		               0, face_index, uv[0], uv[1], p0, n);
	}
	else {
		lores_dm->getTessFace(lores_dm, face_index, &mface);

		if (mface.v4) {
			interp_bilinear_mface(lores_dm, &mface, uv[0], uv[1], 1, p0);
			interp_bilinear_mface(lores_dm, &mface, uv[0], uv[1], 0, n);
		}
		else {
			interp_barycentric_mface(lores_dm, &mface, uv[0], uv[1], 1, p0);
			interp_barycentric_mface(lores_dm, &mface, uv[0], uv[1], 0, n);
		}
	}

	sub_v3_v3v3(vec, p1, p0);
	len = dot_v3v3(n, vec);

	height_data->heights[pixel] = len;

	thread_data->height_min = min_ff(thread_data->height_min, len);
	thread_data->height_max = max_ff(thread_data->height_max, len);

	if (ibuf->rect_float) {
		float *rrgbf = ibuf->rect_float + pixel * 4;
		rrgbf[0] = rrgbf[1] = rrgbf[2] = len;
		rrgbf[3] = 1.0f;
	}
	else {
		char *rrgb = (char *)ibuf->rect + pixel * 4;
		rrgb[0] = rrgb[1] = rrgb[2] = FTOCHAR(len);
		rrgb[3] = 255;
	}
}

/* **************** Normal Maps Baker **************** */

static void *init_normal_data(MultiresBakeRender *bkr, Image *UNUSED(ima))
{
	MNormalBakeData *normal_data;
	DerivedMesh *lodm = bkr->lores_dm;

	normal_data = MEM_callocN(sizeof(MNormalBakeData), "MultiresBake normalData");

	normal_data->orig_index_mf_to_mpoly = lodm->getTessFaceDataArray(lodm, CD_ORIGINDEX);
	normal_data->orig_index_mp_to_orig = lodm->getPolyDataArray(lodm, CD_ORIGINDEX);

	return (void *)normal_data;
}

static void free_normal_data(void *bake_data)
{
	MNormalBakeData *normal_data = (MNormalBakeData *)bake_data;

	MEM_freeN(normal_data);
}

/* MultiresBake callback for normals' baking
 * general idea:
 *   - find coord and normal of point with specified UV in hi-res mesh
 *   - multiply it by tangmat
 *   - vector in color space would be norm(vec) /2 + (0.5, 0.5, 0.5) */
static void apply_tangmat_callback(DerivedMesh *lores_dm, DerivedMesh *hires_dm, void *UNUSED(thread_data),
                                   void *bake_data, ImBuf *ibuf, const int face_index, const int lvl,
                                   const float st[2], float tangmat[3][3], const int x, const int y)
{
	MTFace *mtface = CustomData_get_layer(&lores_dm->faceData, CD_MTFACE);
	MFace mface;
	MNormalBakeData *normal_data = (MNormalBakeData *)bake_data;
	float uv[2], *st0, *st1, *st2, *st3;
	int pixel = ibuf->x * y + x;
	float n[3], vec[3], tmp[3] = {0.5, 0.5, 0.5};

	lores_dm->getTessFace(lores_dm, face_index, &mface);

	st0 = mtface[face_index].uv[0];
	st1 = mtface[face_index].uv[1];
	st2 = mtface[face_index].uv[2];

	if (mface.v4) {
		st3 = mtface[face_index].uv[3];
		resolve_quad_uv_v2(uv, st, st0, st1, st2, st3);
	}
	else
		resolve_tri_uv_v2(uv, st, st0, st1, st2);

	CLAMP(uv[0], 0.0f, 1.0f);
	CLAMP(uv[1], 0.0f, 1.0f);

	get_ccgdm_data(lores_dm, hires_dm,
	               normal_data->orig_index_mf_to_mpoly, normal_data->orig_index_mp_to_orig,
	               lvl, face_index, uv[0], uv[1], NULL, n);

	mul_v3_m3v3(vec, tangmat, n);
	normalize_v3(vec);
	mul_v3_fl(vec, 0.5);
	add_v3_v3(vec, tmp);

	if (ibuf->rect_float) {
		float *rrgbf = ibuf->rect_float + pixel * 4;
		rrgbf[0] = vec[0];
		rrgbf[1] = vec[1];
		rrgbf[2] = vec[2];
		rrgbf[3] = 1.0f;
	}
	else {
		unsigned char *rrgb = (unsigned char *)ibuf->rect + pixel * 4;
		rgb_float_to_uchar(rrgb, vec);
		rrgb[3] = 255;
	}
}

/* **************** Ambient Occlusion Baker **************** */

// must be a power of two
#define MAX_NUMBER_OF_AO_RAYS 1024

static unsigned short ao_random_table_1[MAX_NUMBER_OF_AO_RAYS];
static unsigned short ao_random_table_2[MAX_NUMBER_OF_AO_RAYS];

static void init_ao_random(void)
{
	int i;

	for (i = 0; i < MAX_NUMBER_OF_AO_RAYS; i++) {
		ao_random_table_1[i] = rand() & 0xffff;
		ao_random_table_2[i] = rand() & 0xffff;
	}
}

static unsigned short get_ao_random1(const int i)
{
	return ao_random_table_1[i & (MAX_NUMBER_OF_AO_RAYS - 1)];
}

static unsigned short get_ao_random2(const int i)
{
	return ao_random_table_2[i & (MAX_NUMBER_OF_AO_RAYS - 1)];
}

static void build_permutation_table(unsigned short permutation[], unsigned short temp_permutation[],
                                    const int number_of_rays, const int is_first_perm_table)
{
	int i, k;

	for (i = 0; i < number_of_rays; i++)
		temp_permutation[i] = i;

	for (i = 0; i < number_of_rays; i++) {
		const unsigned int nr_entries_left = number_of_rays - i;
		unsigned short rnd = is_first_perm_table != false ? get_ao_random1(i) : get_ao_random2(i);
		const unsigned short entry = rnd % nr_entries_left;

		/* pull entry */
		permutation[i] = temp_permutation[entry];

		/* delete entry */
		for (k = entry; k < nr_entries_left - 1; k++) {
			temp_permutation[k] = temp_permutation[k + 1];
		}
	}

	/* verify permutation table
	 * every entry must appear exactly once
	 */
#if 0
	for (i = 0; i < number_of_rays; i++) temp_permutation[i] = 0;
	for (i = 0; i < number_of_rays; i++) ++temp_permutation[permutation[i]];
	for (i = 0; i < number_of_rays; i++) BLI_assert(temp_permutation[i] == 1);
#endif
}

static void create_ao_raytree(MultiresBakeRender *bkr, MAOBakeData *ao_data)
{
	DerivedMesh *hidm = bkr->hires_dm;
	RayObject *raytree;
	RayFace *face;
	CCGElem **grid_data;
	CCGKey key;
	int num_grids, grid_size /*, face_side */, num_faces;
	int i;

	num_grids = hidm->getNumGrids(hidm);
	grid_size = hidm->getGridSize(hidm);
	grid_data = hidm->getGridData(hidm);
	hidm->getGridKey(hidm, &key);

	/* face_side = (grid_size << 1) - 1; */  /* UNUSED */
	num_faces = num_grids * (grid_size - 1) * (grid_size - 1);

	raytree = ao_data->raytree = RE_rayobject_create(bkr->raytrace_structure, num_faces, bkr->octree_resolution);
	face = ao_data->rayfaces = (RayFace *) MEM_callocN(num_faces * sizeof(RayFace), "ObjectRen faces");

	for (i = 0; i < num_grids; i++) {
		int x, y;
		for (x = 0; x < grid_size - 1; x++) {
			for (y = 0; y < grid_size - 1; y++) {
				float co[4][3];

				copy_v3_v3(co[0], CCG_grid_elem_co(&key, grid_data[i], x, y));
				copy_v3_v3(co[1], CCG_grid_elem_co(&key, grid_data[i], x, y + 1));
				copy_v3_v3(co[2], CCG_grid_elem_co(&key, grid_data[i], x + 1, y + 1));
				copy_v3_v3(co[3], CCG_grid_elem_co(&key, grid_data[i], x + 1, y));

				RE_rayface_from_coords(face, ao_data, face, co[0], co[1], co[2], co[3]);
				RE_rayobject_add(raytree, RE_rayobject_unalignRayFace(face));

				face++;
			}
		}
	}

	RE_rayobject_done(raytree);
}

static void *init_ao_data(MultiresBakeRender *bkr, Image *UNUSED(ima))
{
	MAOBakeData *ao_data;
	DerivedMesh *lodm = bkr->lores_dm;
	unsigned short *temp_permutation_table;
	size_t permutation_size;

	init_ao_random();

	ao_data = MEM_callocN(sizeof(MAOBakeData), "MultiresBake aoData");

	ao_data->number_of_rays = bkr->number_of_rays;
	ao_data->bias = bkr->bias;

	ao_data->orig_index_mf_to_mpoly = lodm->getTessFaceDataArray(lodm, CD_ORIGINDEX);
	ao_data->orig_index_mp_to_orig = lodm->getPolyDataArray(lodm, CD_ORIGINDEX);

	create_ao_raytree(bkr, ao_data);

	/* initialize permutation tables */
	permutation_size = sizeof(unsigned short) * bkr->number_of_rays;
	ao_data->permutation_table_1 = MEM_callocN(permutation_size, "multires AO baker perm1");
	ao_data->permutation_table_2 = MEM_callocN(permutation_size, "multires AO baker perm2");
	temp_permutation_table = MEM_callocN(permutation_size, "multires AO baker temp perm");

	build_permutation_table(ao_data->permutation_table_1, temp_permutation_table, bkr->number_of_rays, 1);
	build_permutation_table(ao_data->permutation_table_2, temp_permutation_table, bkr->number_of_rays, 0);

	MEM_freeN(temp_permutation_table);

	return (void *)ao_data;
}

static void free_ao_data(void *bake_data)
{
	MAOBakeData *ao_data = (MAOBakeData *) bake_data;

	RE_rayobject_free(ao_data->raytree);
	MEM_freeN(ao_data->rayfaces);

	MEM_freeN(ao_data->permutation_table_1);
	MEM_freeN(ao_data->permutation_table_2);

	MEM_freeN(ao_data);
}

/* builds an X and a Y axis from the given Z axis */
static void build_coordinate_frame(float axisX[3], float axisY[3], const float axisZ[3])
{
	const float faX = fabsf(axisZ[0]);
	const float faY = fabsf(axisZ[1]);
	const float faZ = fabsf(axisZ[2]);

	if (faX <= faY && faX <= faZ) {
		const float len = sqrtf(axisZ[1] * axisZ[1] + axisZ[2] * axisZ[2]);
		axisY[0] = 0; axisY[1] = axisZ[2] / len; axisY[2] = -axisZ[1] / len;
		cross_v3_v3v3(axisX, axisY, axisZ);
	}
	else if (faY <= faZ) {
		const float len = sqrtf(axisZ[0] * axisZ[0] + axisZ[2] * axisZ[2]);
		axisX[0] = axisZ[2] / len; axisX[1] = 0; axisX[2] = -axisZ[0] / len;
		cross_v3_v3v3(axisY, axisZ, axisX);
	}
	else {
		const float len = sqrtf(axisZ[0] * axisZ[0] + axisZ[1] * axisZ[1]);
		axisX[0] = axisZ[1] / len; axisX[1] = -axisZ[0] / len; axisX[2] = 0;
		cross_v3_v3v3(axisY, axisZ, axisX);
	}
}

/* return false if nothing was hit and true otherwise */
static int trace_ao_ray(MAOBakeData *ao_data, float ray_start[3], float ray_direction[3])
{
	Isect isect = {{0}};

	isect.dist = RE_RAYTRACE_MAXDIST;
	copy_v3_v3(isect.start, ray_start);
	copy_v3_v3(isect.dir, ray_direction);
	isect.lay = -1;

	normalize_v3(isect.dir);

	return RE_rayobject_raycast(ao_data->raytree, &isect);
}

static void apply_ao_callback(DerivedMesh *lores_dm, DerivedMesh *hires_dm, void *UNUSED(thread_data),
                              void *bake_data, ImBuf *ibuf, const int face_index, const int lvl,
                              const float st[2], float UNUSED(tangmat[3][3]), const int x, const int y)
{
	MAOBakeData *ao_data = (MAOBakeData *) bake_data;
	MTFace *mtface = CustomData_get_layer(&lores_dm->faceData, CD_MTFACE);
	MFace mface;

	int i, k, perm_offs;
	float pos[3], nrm[3];
	float cen[3];
	float axisX[3], axisY[3], axisZ[3];
	float shadow = 0;
	float value;
	int pixel = ibuf->x * y + x;
	float uv[2], *st0, *st1, *st2, *st3;

	lores_dm->getTessFace(lores_dm, face_index, &mface);

	st0 = mtface[face_index].uv[0];
	st1 = mtface[face_index].uv[1];
	st2 = mtface[face_index].uv[2];

	if (mface.v4) {
		st3 = mtface[face_index].uv[3];
		resolve_quad_uv_v2(uv, st, st0, st1, st2, st3);
	}
	else
		resolve_tri_uv_v2(uv, st, st0, st1, st2);

	CLAMP(uv[0], 0.0f, 1.0f);
	CLAMP(uv[1], 0.0f, 1.0f);

	get_ccgdm_data(lores_dm, hires_dm,
	               ao_data->orig_index_mf_to_mpoly, ao_data->orig_index_mp_to_orig,
	               lvl, face_index, uv[0], uv[1], pos, nrm);

	/* offset ray origin by user bias along normal */
	for (i = 0; i < 3; i++)
		cen[i] = pos[i] + ao_data->bias * nrm[i];

	/* build tangent frame */
	for (i = 0; i < 3; i++)
		axisZ[i] = nrm[i];

	build_coordinate_frame(axisX, axisY, axisZ);

	/* static noise */
	perm_offs = (get_ao_random2(get_ao_random1(x) + y)) & (MAX_NUMBER_OF_AO_RAYS - 1);

	/* importance sample shadow rays (cosine weighted) */
	for (i = 0; i < ao_data->number_of_rays; i++) {
		int hit_something;

		/* use N-Rooks to distribute our N ray samples across
		 * a multi-dimensional domain (2D)
		 */
		const unsigned short I = ao_data->permutation_table_1[(i + perm_offs) % ao_data->number_of_rays];
		const unsigned short J = ao_data->permutation_table_2[i];

		const float JitPh = (get_ao_random2(I + perm_offs) & (MAX_NUMBER_OF_AO_RAYS-1))/((float) MAX_NUMBER_OF_AO_RAYS);
		const float JitTh = (get_ao_random1(J + perm_offs) & (MAX_NUMBER_OF_AO_RAYS-1))/((float) MAX_NUMBER_OF_AO_RAYS);
		const float SiSqPhi = (I + JitPh) / ao_data->number_of_rays;
		const float Theta = (float)(2 * M_PI) * ((J + JitTh) / ao_data->number_of_rays);

		/* this gives results identical to the so-called cosine
		 * weighted distribution relative to the north pole.
		 */
		float SiPhi = sqrt(SiSqPhi);
		float CoPhi = SiSqPhi < 1.0f ? sqrtf(1.0f - SiSqPhi) : 0;
		float CoThe = cos(Theta);
		float SiThe = sin(Theta);

		const float dx = CoThe * CoPhi;
		const float dy = SiThe * CoPhi;
		const float dz = SiPhi;

		/* transform ray direction out of tangent frame */
		float dv[3];
		for (k = 0; k < 3; k++)
			dv[k] = axisX[k] * dx + axisY[k] * dy + axisZ[k] * dz;

		hit_something = trace_ao_ray(ao_data, cen, dv);

		if (hit_something != 0)
			shadow += 1;
	}

	value = 1.0f - (shadow / ao_data->number_of_rays);

	if (ibuf->rect_float) {
		float *rrgbf = ibuf->rect_float + pixel * 4;
		rrgbf[0] = rrgbf[1] = rrgbf[2] = value;
		rrgbf[3] = 1.0f;
	}
	else {
		unsigned char *rrgb = (unsigned char *) ibuf->rect + pixel * 4;
		rrgb[0] = rrgb[1] = rrgb[2] = FTOCHAR(value);
		rrgb[3] = 255;
	}
}

/* **************** Common functions public API relates on **************** */

static void count_images(MultiresBakeRender *bkr)
{
	int a, totface;
	DerivedMesh *dm = bkr->lores_dm;
	MTFace *mtface = CustomData_get_layer(&dm->faceData, CD_MTFACE);

	BLI_listbase_clear(&bkr->image);
	bkr->tot_image = 0;

	totface = dm->getNumTessFaces(dm);

	for (a = 0; a < totface; a++)
		mtface[a].tpage->id.flag &= ~LIB_DOIT;

	for (a = 0; a < totface; a++) {
		Image *ima = mtface[a].tpage;
		if ((ima->id.flag & LIB_DOIT) == 0) {
			LinkData *data = BLI_genericNodeN(ima);
			BLI_addtail(&bkr->image, data);
			bkr->tot_image++;
			ima->id.flag |= LIB_DOIT;
		}
	}

	for (a = 0; a < totface; a++)
		mtface[a].tpage->id.flag &= ~LIB_DOIT;
}

static void bake_images(MultiresBakeRender *bkr, MultiresBakeResult *result)
{
	LinkData *link;

	for (link = bkr->image.first; link; link = link->next) {
		Image *ima = (Image *)link->data;
		ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);

		if (ibuf->x > 0 && ibuf->y > 0) {
			BakeImBufuserData *userdata = MEM_callocN(sizeof(BakeImBufuserData), "MultiresBake userdata");
			userdata->mask_buffer = MEM_callocN(ibuf->y * ibuf->x, "MultiresBake imbuf mask");
			ibuf->userdata = userdata;

			switch (bkr->mode) {
				case RE_BAKE_NORMALS:
					do_multires_bake(bkr, ima, true, apply_tangmat_callback, init_normal_data, free_normal_data, result);
					break;
				case RE_BAKE_DISPLACEMENT:
				case RE_BAKE_DERIVATIVE:
					do_multires_bake(bkr, ima, false, apply_heights_callback, init_heights_data, free_heights_data, result);
					break;
				case RE_BAKE_AO:
					do_multires_bake(bkr, ima, false, apply_ao_callback, init_ao_data, free_ao_data, result);
					break;
			}
		}

		BKE_image_release_ibuf(ima, ibuf, NULL);

		ima->id.flag |= LIB_DOIT;
	}
}

static void finish_images(MultiresBakeRender *bkr, MultiresBakeResult *result)
{
	LinkData *link;
	bool use_displacement_buffer = ELEM(bkr->mode, RE_BAKE_DISPLACEMENT, RE_BAKE_DERIVATIVE);

	for (link = bkr->image.first; link; link = link->next) {
		Image *ima = (Image *)link->data;
		ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
		BakeImBufuserData *userdata = (BakeImBufuserData *) ibuf->userdata;

		if (ibuf->x <= 0 || ibuf->y <= 0)
			continue;

		if (use_displacement_buffer) {
			if (bkr->mode == RE_BAKE_DERIVATIVE) {
				RE_bake_make_derivative(ibuf, userdata->displacement_buffer, userdata->mask_buffer,
				                        result->height_min, result->height_max, bkr->user_scale);
			}
			else {
				RE_bake_ibuf_normalize_displacement(ibuf, userdata->displacement_buffer, userdata->mask_buffer,
				                                    result->height_min, result->height_max);
			}
		}

		RE_bake_ibuf_filter(ibuf, userdata->mask_buffer, bkr->bake_filter);

		ibuf->userflags |= IB_BITMAPDIRTY | IB_DISPLAY_BUFFER_INVALID;

		if (ibuf->rect_float)
			ibuf->userflags |= IB_RECT_INVALID;

		if (ibuf->mipmap[0]) {
			ibuf->userflags |= IB_MIPMAP_INVALID;
			imb_freemipmapImBuf(ibuf);
		}

		if (ibuf->userdata) {
			if (userdata->displacement_buffer)
				MEM_freeN(userdata->displacement_buffer);

			MEM_freeN(userdata->mask_buffer);
			MEM_freeN(userdata);
			ibuf->userdata = NULL;
		}

		BKE_image_release_ibuf(ima, ibuf, NULL);
	}
}

void RE_multires_bake_images(MultiresBakeRender *bkr)
{
	MultiresBakeResult result;

	count_images(bkr);
	bake_images(bkr, &result);
	finish_images(bkr, &result);
}
