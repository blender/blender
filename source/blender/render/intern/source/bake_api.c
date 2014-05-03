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
 * Contributors:
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/bake_api.c
 *  \ingroup render
 *
 * \brief The API itself is simple. Blender sends a populated array of BakePixels to the renderer, and gets back an
 * array of floats with the result.
 *
 * \section bake_api Development Notes for External Engines
 *
 * The Bake API is fully implemented with Python rna functions. The operator expects/call a function:
 *
 * ``def bake(scene, object, pass_type, pixel_array, num_pixels, depth, result)``
 * - scene: current scene (Python object)
 * - object: object to render (Python object)
 * - pass_type: pass to render (string, e.g., "COMBINED", "AO", "NORMAL", ...)
 * - pixel_array: list of primitive ids and barycentric coordinates to bake(Python object, see bake_pixel)
 * - num_pixels: size of pixel_array, number of pixels to bake (int)
 * - depth: depth of pixels to return (int, assuming always 4 now)
 * - result: array to be populated by the engine (float array, PyLong_AsVoidPtr)
 *
 * \note Normals are expected to be in World Space and in the +X, +Y, +Z orientation.
 *
 * \subsection bake_pixel BakePixel data structure
 *
 * pixel_array is a Python object storing BakePixel elements:
 *
 * <pre>
 * struct BakePixel {
 *     int primitive_id;
 *     float uv[2];
 *     float du_dx, du_dy;
 *     float dv_dx, dv_dy;
 * };
 * </pre>
 *
 * In python you have access to:
 * - ``primitive_id``, ``uv``, ``du_dx``, ``du_dy``, ``next``
 * - ``next()`` is a function that returns the next #BakePixel in the array.
 *
 * \note Pixels that should not be baked have ``primitive_id == -1``
 *
 * For a complete implementation example look at the Cycles Bake commit.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "DNA_mesh_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_image.h"
#include "BKE_node.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RE_bake.h"

/* local include */
#include "render_types.h"
#include "zbuf.h"


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


typedef struct BakeDataZSpan {
	BakePixel *pixel_array;
	int primitive_id;
	BakeImage *bk_image;
	ZSpan *zspan;
} BakeDataZSpan;

/**
 * struct wrapping up tangent space data
 */
typedef struct TSpace {
	float tangent[3];
	float sign;
} TSpace;

typedef struct TriTessFace {
	const MVert *mverts[3];
	const TSpace *tspace[3];
	float normal[3]; /* for flat faces */
	bool is_smooth;
} TriTessFace;

static void store_bake_pixel(void *handle, int x, int y, float u, float v)
{
	BakeDataZSpan *bd = (BakeDataZSpan *)handle;
	BakePixel *pixel;

	const int width = bd->bk_image->width;
	const int offset = bd->bk_image->offset;
	const int i = offset + y * width + x;

	pixel = &bd->pixel_array[i];
	pixel->primitive_id = bd->primitive_id;

	copy_v2_fl2(pixel->uv, u, v);

	pixel->du_dx =
	pixel->du_dy =
	pixel->dv_dx =
	pixel->dv_dy =
	0.0f;
}

void RE_bake_mask_fill(const BakePixel pixel_array[], const int num_pixels, char *mask)
{
	int i;
	if (!mask)
		return;

	/* only extend to pixels outside the mask area */
	for (i = 0; i < num_pixels; i++) {
		if (pixel_array[i].primitive_id != -1) {
			mask[i] = FILTER_MASK_USED;
		}
	}
}

void RE_bake_margin(ImBuf *ibuf, char *mask, const int margin)
{
	/* margin */
	IMB_filter_extend(ibuf, mask, margin);

	if (ibuf->planes != R_IMF_PLANES_RGBA)
		/* clear alpha added by filtering */
		IMB_rectfill_alpha(ibuf, 1.0f);
}

/**
 * This function returns the coordinate and normal of a barycentric u,v for a face defined by the primitive_id index.
 */
static void calc_point_from_barycentric(
        TriTessFace *triangles, int primitive_id, float u, float v, float cage_extrusion,
        float r_co[3], float r_dir[3])
{
	float data[3][3];
	float coord[3];
	float dir[3];
	float cage[3];

	TriTessFace *triangle = &triangles[primitive_id];

	copy_v3_v3(data[0], triangle->mverts[0]->co);
	copy_v3_v3(data[1], triangle->mverts[1]->co);
	copy_v3_v3(data[2], triangle->mverts[2]->co);

	interp_barycentric_tri_v3(data, u, v, coord);

	normal_short_to_float_v3(data[0], triangle->mverts[0]->no);
	normal_short_to_float_v3(data[1], triangle->mverts[1]->no);
	normal_short_to_float_v3(data[2], triangle->mverts[2]->no);

	interp_barycentric_tri_v3(data, u, v, dir);
	normalize_v3_v3(cage, dir);
	mul_v3_fl(cage, cage_extrusion);

	add_v3_v3(coord, cage);

	normalize_v3_v3(dir, dir);
	mul_v3_fl(dir, -1.0f);

	copy_v3_v3(r_co, coord);
	copy_v3_v3(r_dir, dir);
}

/**
 * This function returns the barycentric u,v of a face for a coordinate. The face is defined by its index.
 */
static void calc_barycentric_from_point(
        TriTessFace *triangles, const int index, const float co[3],
        int *r_primitive_id, float r_uv[2])
{
	TriTessFace *triangle = &triangles[index];
	resolve_tri_uv_v3(r_uv, co,
	                  triangle->mverts[0]->co,
	                  triangle->mverts[1]->co,
	                  triangle->mverts[2]->co);
	*r_primitive_id = index;
}

/**
 * This function populates pixel_array and returns TRUE if things are correct
 */
static bool cast_ray_highpoly(
        BVHTreeFromMesh *treeData, TriTessFace *triangles[], BakeHighPolyData *highpoly,
        float const co_low[3], const float dir[3], const int pixel_id, const int tot_highpoly)
{
	int i;
	int primitive_id = -1;
	float uv[2];
	int hit_mesh = -1;
	float hit_distance = FLT_MAX;

	BVHTreeRayHit *hits;
	hits = MEM_mallocN(sizeof(BVHTreeRayHit) * tot_highpoly, "Bake Highpoly to Lowpoly: BVH Rays");

	for (i = 0; i < tot_highpoly; i++) {
		float co_high[3];
		hits[i].index = -1;
		/* TODO: we should use FLT_MAX here, but sweepsphere code isn't prepared for that */
		hits[i].dist = 10000.0f;

		copy_v3_v3(co_high, co_low);

		/* transform the ray from the lowpoly to the highpoly space */
		mul_m4_v3(highpoly[i].mat_lowtohigh, co_high);

		/* cast ray */
		BLI_bvhtree_ray_cast(treeData[i].tree, co_high, dir, 0.0f, &hits[i], treeData[i].raycast_callback, &treeData[i]);

		if (hits[i].index != -1) {
			/* cull backface */
			const float dot = dot_v3v3(dir, hits[i].no);
			if (dot < 0.0f) {
				if (hits[i].dist < hit_distance) {
					hit_mesh = i;
					hit_distance = hits[i].dist;
				}
			}
		}
	}

	for (i = 0; i < tot_highpoly; i++) {
		if (hit_mesh == i) {
			calc_barycentric_from_point(triangles[i], hits[i].index, hits[i].co, &primitive_id, uv);
			highpoly[i].pixel_array[pixel_id].primitive_id = primitive_id;
			copy_v2_v2(highpoly[i].pixel_array[pixel_id].uv, uv);
		}
		else {
			highpoly[i].pixel_array[pixel_id].primitive_id = -1;
		}
	}

	MEM_freeN(hits);
	return hit_mesh != -1;
}

/**
 * This function populates an array of verts for the triangles of a mesh
 * Tangent and Normals are also stored
 */
static void mesh_calc_tri_tessface(
        TriTessFace *triangles, Mesh *me, bool tangent, DerivedMesh *dm)
{
	int i;
	int p_id;
	MFace *mface;
	MVert *mvert;
	TSpace *tspace;
	float *precomputed_normals;
	bool calculate_normal;

	mface = CustomData_get_layer(&me->fdata, CD_MFACE);
	mvert = CustomData_get_layer(&me->vdata, CD_MVERT);

	if (tangent) {
		DM_ensure_normals(dm);
		DM_add_tangent_layer(dm);

		precomputed_normals = dm->getTessFaceDataArray(dm, CD_NORMAL);
		calculate_normal = precomputed_normals ? false : true;

		//mface = dm->getTessFaceArray(dm);
		//mvert = dm->getVertArray(dm);

		tspace = dm->getTessFaceDataArray(dm, CD_TANGENT);
		BLI_assert(tspace);
	}

	p_id = -1;
	for (i = 0; i < me->totface; i++) {
		MFace *mf = &mface[i];
		TSpace *ts = &tspace[i * 4];

		p_id++;

		triangles[p_id].mverts[0] = &mvert[mf->v1];
		triangles[p_id].mverts[1] = &mvert[mf->v2];
		triangles[p_id].mverts[2] = &mvert[mf->v3];
		triangles[p_id].is_smooth = (mf->flag & ME_SMOOTH) != 0;

		if (tangent) {
			triangles[p_id].tspace[0] = &ts[0];
			triangles[p_id].tspace[1] = &ts[1];
			triangles[p_id].tspace[2] = &ts[2];

			if (calculate_normal) {
				if (mf->v4 != 0) {
					normal_quad_v3(triangles[p_id].normal,
					               mvert[mf->v1].co,
					               mvert[mf->v2].co,
					               mvert[mf->v3].co,
					               mvert[mf->v4].co);
				}
				else {
					normal_tri_v3(triangles[p_id].normal,
					              triangles[p_id].mverts[0]->co,
					              triangles[p_id].mverts[1]->co,
					              triangles[p_id].mverts[2]->co);
				}
			}
			else {
				copy_v3_v3(triangles[p_id].normal, &precomputed_normals[3 * i]);
			}
		}

		/* 4 vertices in the face */
		if (mf->v4 != 0) {
			p_id++;

			triangles[p_id].mverts[0] = &mvert[mf->v1];
			triangles[p_id].mverts[1] = &mvert[mf->v3];
			triangles[p_id].mverts[2] = &mvert[mf->v4];
			triangles[p_id].is_smooth = (mf->flag & ME_SMOOTH) != 0;

			if (tangent) {
				triangles[p_id].tspace[0] = &ts[0];
				triangles[p_id].tspace[1] = &ts[2];
				triangles[p_id].tspace[2] = &ts[3];

				/* same normal as the other "triangle" */
				copy_v3_v3(triangles[p_id].normal, triangles[p_id - 1].normal);
			}
		}
	}

	BLI_assert(p_id < me->totface * 2);
}

void RE_bake_pixels_populate_from_objects(
        struct Mesh *me_low, BakePixel pixel_array_from[],
        BakeHighPolyData highpoly[], const int tot_highpoly, const int num_pixels,
        const float cage_extrusion)
{
	int i;
	int primitive_id;
	float u, v;

	DerivedMesh **dm_highpoly;
	BVHTreeFromMesh *treeData;

	/* Note: all coordinates are in local space */
	TriTessFace *tris_low;
	TriTessFace **tris_high;

	/* assume all lowpoly tessfaces can be quads */
	tris_low = MEM_callocN(sizeof(TriTessFace) * (me_low->totface * 2), "MVerts Lowpoly Mesh");
	tris_high = MEM_callocN(sizeof(TriTessFace *) * tot_highpoly, "MVerts Highpoly Mesh Array");

	/* assume all highpoly tessfaces are triangles */
	dm_highpoly = MEM_callocN(sizeof(DerivedMesh *) * tot_highpoly, "Highpoly Derived Meshes");
	treeData = MEM_callocN(sizeof(BVHTreeFromMesh) * tot_highpoly, "Highpoly BVH Trees");

	mesh_calc_tri_tessface(tris_low, me_low, false, NULL);

	for (i = 0; i < tot_highpoly; i++) {
		tris_high[i] = MEM_callocN(sizeof(TriTessFace) * highpoly[i].me->totface, "MVerts Highpoly Mesh");
		mesh_calc_tri_tessface(tris_high[i], highpoly[i].me, false, NULL);

		dm_highpoly[i] = CDDM_from_mesh(highpoly[i].me);

		/* Create a bvh-tree for each highpoly object */
		bvhtree_from_mesh_faces(&treeData[i], dm_highpoly[i], 0.0, 2, 6);

		if (&treeData[i].tree == NULL) {
			printf("Baking: Out of memory\n");
			goto cleanup;
		}
	}

	for (i = 0; i < num_pixels; i++) {
		float co[3];
		float dir[3];

		primitive_id = pixel_array_from[i].primitive_id;

		if (primitive_id == -1) {
			int j;
			for (j = 0; j < tot_highpoly; j++) {
				highpoly[j].pixel_array[i].primitive_id = -1;
			}
			continue;
		}

		u = pixel_array_from[i].uv[0];
		v = pixel_array_from[i].uv[1];

		/* calculate from low poly mesh cage */
		calc_point_from_barycentric(tris_low, primitive_id, u, v, cage_extrusion, co, dir);

		/* cast ray */
		if (!cast_ray_highpoly(treeData, tris_high, highpoly, co, dir, i, tot_highpoly)) {
			/* if it fails mask out the original pixel array */
			pixel_array_from[i].primitive_id = -1;
		}
	}


	/* garbage collection */
cleanup:
	for (i = 0; i < tot_highpoly; i++) {
		free_bvhtree_from_mesh(&treeData[i]);
		dm_highpoly[i]->release(dm_highpoly[i]);
		MEM_freeN(tris_high[i]);
	}

	MEM_freeN(tris_low);
	MEM_freeN(tris_high);
	MEM_freeN(treeData);
	MEM_freeN(dm_highpoly);
}

void RE_bake_pixels_populate(
        Mesh *me, BakePixel pixel_array[],
        const int num_pixels, const BakeImages *bake_images)
{
	BakeDataZSpan bd;
	int i, a;
	int p_id;

	MTFace *mtface;
	MFace *mface;

	/* we can't bake in edit mode */
	if (me->edit_btmesh)
		return;

	bd.pixel_array = pixel_array;
	bd.zspan = MEM_callocN(sizeof(ZSpan) * bake_images->size, "bake zspan");

	/* initialize all pixel arrays so we know which ones are 'blank' */
	for (i = 0; i < num_pixels; i++) {
		pixel_array[i].primitive_id = -1;
	}

	for (i = 0; i < bake_images->size; i++) {
		zbuf_alloc_span(&bd.zspan[i], bake_images->data[i].width, bake_images->data[i].height, R.clipcrop);
	}

	mtface = CustomData_get_layer(&me->fdata, CD_MTFACE);
	mface = CustomData_get_layer(&me->fdata, CD_MFACE);

	if (mtface == NULL)
		return;

	p_id = -1;
	for (i = 0; i < me->totface; i++) {
		float vec[4][2];
		MTFace *mtf = &mtface[i];
		MFace *mf = &mface[i];
		int mat_nr = mf->mat_nr;
		int image_id = bake_images->lookup[mat_nr];

		bd.bk_image = &bake_images->data[image_id];
		bd.primitive_id = ++p_id;

		for (a = 0; a < 4; a++) {
			/* Note, workaround for pixel aligned UVs which are common and can screw up our intersection tests
			 * where a pixel gets in between 2 faces or the middle of a quad,
			 * camera aligned quads also have this problem but they are less common.
			 * Add a small offset to the UVs, fixes bug #18685 - Campbell */
			vec[a][0] = mtf->uv[a][0] * (float)bd.bk_image->width - (0.5f + 0.001f);
			vec[a][1] = mtf->uv[a][1] * (float)bd.bk_image->height - (0.5f + 0.002f);
		}

		zspan_scanconvert(&bd.zspan[image_id], (void *)&bd, vec[0], vec[1], vec[2], store_bake_pixel);

		/* 4 vertices in the face */
		if (mf->v4 != 0) {
			bd.primitive_id = ++p_id;
			zspan_scanconvert(&bd.zspan[image_id], (void *)&bd, vec[0], vec[2], vec[3], store_bake_pixel);
		}
	}

	for (i = 0; i < bake_images->size; i++) {
		zbuf_free_span(&bd.zspan[i]);
	}
	MEM_freeN(bd.zspan);
}

/* ******************** NORMALS ************************ */

/**
 * convert a normalized normal to the -1.0 1.0 range
 * the input is expected to be POS_X, POS_Y, POS_Z
 */
static void normal_uncompress(float out[3], const float in[3])
{
	int i;
	for (i = 0; i < 3; i++)
		out[i] = 2.0f * in[i] - 1.0f;
}

static void normal_compress(float out[3], const float in[3], const BakeNormalSwizzle normal_swizzle[3])
{
	const int swizzle_index[6] = {
		0,  /* R_BAKE_POSX */
		1,  /* R_BAKE_POSY */
		2,  /* R_BAKE_POSZ */
		0,  /* R_BAKE_NEGX */
		1,  /* R_BAKE_NEGY */
		2,  /* R_BAKE_NEGZ */
	};
	const float swizzle_sign[6] = {
		+1.0f,  /* R_BAKE_POSX */
		+1.0f,  /* R_BAKE_POSY */
		+1.0f,  /* R_BAKE_POSZ */
		-1.0f,  /* R_BAKE_NEGX */
		-1.0f,  /* R_BAKE_NEGY */
		-1.0f,  /* R_BAKE_NEGZ */
	};

	int i;

	for (i = 0; i < 3; i++) {
		int index;
		float sign;

		sign  = swizzle_sign[normal_swizzle[i]];
		index = swizzle_index[normal_swizzle[i]];

		/*
		 * There is a small 1e-5f bias for precision issues. otherwise
		 * we randomly get 127 or 128 for neutral colors in tangent maps.
		 * we choose 128 because it is the convention flat color. *
		 */

		out[i] = sign * in[index] / 2.0f + 0.5f + 1e-5f;
	}
}

/**
 * This function converts an object space normal map to a tangent space normal map for a given low poly mesh
 */
void RE_bake_normal_world_to_tangent(
        const BakePixel pixel_array[], const int num_pixels, const int depth,
        float result[], Mesh *me, const BakeNormalSwizzle normal_swizzle[3])
{
	int i;

	TriTessFace *triangles;

	DerivedMesh *dm = CDDM_from_mesh(me);

	triangles = MEM_callocN(sizeof(TriTessFace) * (me->totface * 2), "MVerts Mesh");
	mesh_calc_tri_tessface(triangles, me, true, dm);

	BLI_assert(num_pixels >= 3);

	for (i = 0; i < num_pixels; i++) {
		TriTessFace *triangle;
		float tangents[3][3];
		float normals[3][3];
		float signs[3];
		int j;

		float tangent[3];
		float normal[3];
		float binormal[3];
		float sign;
		float u, v, w;

		float tsm[3][3]; /* tangent space matrix */
		float itsm[3][3];

		int offset;
		float nor[3]; /* texture normal */

		bool is_smooth;

		int primitive_id = pixel_array[i].primitive_id;

		offset = i * depth;

		if (primitive_id == -1) {
			copy_v3_fl3(&result[offset], 0.5f, 0.5f, 1.0f);
			continue;
		}

		triangle = &triangles[primitive_id];
		is_smooth = triangle->is_smooth;

		for (j = 0; j < 3; j++) {
			const TSpace *ts;

			if (is_smooth)
				normal_short_to_float_v3(normals[j], triangle->mverts[j]->no);
			else
				normal[j] = triangle->normal[j];

			ts = triangle->tspace[j];
			copy_v3_v3(tangents[j], ts->tangent);
			signs[j] = ts->sign;
		}

		u = pixel_array[i].uv[0];
		v = pixel_array[i].uv[1];
		w = 1.0f - u - v;

		/* normal */
		if (is_smooth)
			interp_barycentric_tri_v3(normals, u, v, normal);

		/* tangent */
		interp_barycentric_tri_v3(tangents, u, v, tangent);

		/* sign */
		/* The sign is the same at all face vertices for any non degenerate face.
		 * Just in case we clamp the interpolated value though. */
		sign = (signs[0]  * u + signs[1]  * v + signs[2] * w) < 0 ? (-1.0f) : 1.0f;

		/* binormal */
		/* B = sign * cross(N, T)  */
		cross_v3_v3v3(binormal, normal, tangent);
		mul_v3_fl(binormal, sign);

		/* populate tangent space matrix */
		copy_v3_v3(tsm[0], tangent);
		copy_v3_v3(tsm[1], binormal);
		copy_v3_v3(tsm[2], normal);

		/* texture values */
		normal_uncompress(nor, &result[offset]);

		invert_m3_m3(itsm, tsm);
		mul_m3_v3(itsm, nor);
		normalize_v3(nor);

		/* save back the values */
		normal_compress(&result[offset], nor, normal_swizzle);
	}

	/* garbage collection */
	MEM_freeN(triangles);

	if (dm)
		dm->release(dm);
}

void RE_bake_normal_world_to_object(
        const BakePixel pixel_array[], const int num_pixels, const int depth,
        float result[], struct Object *ob, const BakeNormalSwizzle normal_swizzle[3])
{
	int i;
	float iobmat[4][4];

	invert_m4_m4(iobmat, ob->obmat);

	for (i = 0; i < num_pixels; i++) {
		int offset;
		float nor[3];

		if (pixel_array[i].primitive_id == -1)
			continue;

		offset = i * depth;
		normal_uncompress(nor, &result[offset]);

		mul_m4_v3(iobmat, nor);
		normalize_v3(nor);

		/* save back the values */
		normal_compress(&result[offset], nor, normal_swizzle);
	}
}

void RE_bake_normal_world_to_world(
        const BakePixel pixel_array[], const int num_pixels, const int depth,
        float result[], const BakeNormalSwizzle normal_swizzle[3])
{
	int i;

	for (i = 0; i < num_pixels; i++) {
		int offset;
		float nor[3];

		if (pixel_array[i].primitive_id == -1)
			continue;

		offset = i * depth;
		normal_uncompress(nor, &result[offset]);

		/* save back the values */
		normal_compress(&result[offset], nor, normal_swizzle);
	}
}

void RE_bake_ibuf_clear(BakeImages *bake_images, const bool is_tangent)
{
	ImBuf *ibuf;
	void *lock;
	Image *image;
	int i;

	const float vec_alpha[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	const float vec_solid[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	const float nor_alpha[4] = {0.5f, 0.5f, 1.0f, 0.0f};
	const float nor_solid[4] = {0.5f, 0.5f, 1.0f, 1.0f};

	for (i = 0; i < bake_images->size; i ++) {
		image = bake_images->data[i].image;

		ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);
		BLI_assert(ibuf);

		if (is_tangent)
			IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? nor_alpha : nor_solid);
		else
			IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? vec_alpha : vec_solid);

		BKE_image_release_ibuf(image, ibuf, lock);
	}
}

/* ************************************************************* */

/**
 * not the real UV, but the internal per-face UV instead
 * I'm using it to test if everything is correct */
static bool bake_uv(const BakePixel pixel_array[], const int num_pixels, const int depth, float result[])
{
	int i;

	for (i=0; i < num_pixels; i++) {
		int offset = i * depth;
		copy_v2_v2(&result[offset], pixel_array[i].uv);
	}

	return true;
}

bool RE_bake_internal(
        Render *UNUSED(re), Object *UNUSED(object), const BakePixel pixel_array[],
        const int num_pixels, const int depth, const ScenePassType pass_type, float result[])
{
	switch (pass_type) {
		case SCE_PASS_UV:
		{
			return bake_uv(pixel_array, num_pixels, depth, result);
			break;
		}
		default:
			break;
	}
	return false;
}

int RE_pass_depth(const ScenePassType pass_type)
{
	/* IMB_buffer_byte_from_float assumes 4 channels
	 * making it work for now - XXX */
	return 4;

	switch (pass_type) {
		case SCE_PASS_Z:
		case SCE_PASS_AO:
		case SCE_PASS_MIST:
		{
			return 1;
		}
		case SCE_PASS_UV:
		{
			return 2;
		}
		case SCE_PASS_RGBA:
		{
			return 4;
		}
		case SCE_PASS_COMBINED:
		case SCE_PASS_DIFFUSE:
		case SCE_PASS_SPEC:
		case SCE_PASS_SHADOW:
		case SCE_PASS_REFLECT:
		case SCE_PASS_NORMAL:
		case SCE_PASS_VECTOR:
		case SCE_PASS_REFRACT:
		case SCE_PASS_INDEXOB:  /* XXX double check */
		case SCE_PASS_INDIRECT:
		case SCE_PASS_RAYHITS:  /* XXX double check */
		case SCE_PASS_EMIT:
		case SCE_PASS_ENVIRONMENT:
		case SCE_PASS_INDEXMA:
		case SCE_PASS_DIFFUSE_DIRECT:
		case SCE_PASS_DIFFUSE_INDIRECT:
		case SCE_PASS_DIFFUSE_COLOR:
		case SCE_PASS_GLOSSY_DIRECT:
		case SCE_PASS_GLOSSY_INDIRECT:
		case SCE_PASS_GLOSSY_COLOR:
		case SCE_PASS_TRANSM_DIRECT:
		case SCE_PASS_TRANSM_INDIRECT:
		case SCE_PASS_TRANSM_COLOR:
		case SCE_PASS_SUBSURFACE_DIRECT:
		case SCE_PASS_SUBSURFACE_INDIRECT:
		case SCE_PASS_SUBSURFACE_COLOR:
		default:
		{
			return 3;
		}
	}
}
