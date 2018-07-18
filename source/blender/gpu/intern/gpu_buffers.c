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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_buffers.c
 *  \ingroup gpu
 *
 * Mesh drawing using OpenGL VBO (Vertex Buffer Objects)
 */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "DNA_meshdata_types.h"

#include "BKE_ccg.h"
#include "BKE_DerivedMesh.h"
#include "BKE_paint.h"
#include "BKE_mesh.h"
#include "BKE_pbvh.h"

#include "GPU_buffers.h"
#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_batch.h"

#include "bmesh.h"

static ThreadMutex buffer_mutex = BLI_MUTEX_INITIALIZER;

/* multires global buffer, can be used for many grids having the same grid size */
typedef struct GridCommonGPUBuffer {
	GPUIndexBuf *mres_buffer;
	int mres_prev_gridsize;
	unsigned mres_prev_totquad;
} GridCommonGPUBuffer;

/* XXX: the rest of the code in this file is used for optimized PBVH
 * drawing and doesn't interact at all with the buffer code above */

struct GPU_PBVH_Buffers {
	GPUIndexBuf *index_buf, *index_buf_fast;
	GPUVertBuf *vert_buf;

	GPUBatch *triangles;
	GPUBatch *triangles_fast;

	/* mesh pointers in case buffer allocation fails */
	const MPoly *mpoly;
	const MLoop *mloop;
	const MLoopTri *looptri;
	const MVert *mvert;

	const int *face_indices;
	int        face_indices_len;
	const float *vmask;

	/* grid pointers */
	CCGKey gridkey;
	CCGElem **grids;
	const DMFlagMat *grid_flag_mats;
	BLI_bitmap * const *grid_hidden;
	const int *grid_indices;
	int totgrid;
	bool has_hidden;
	bool is_index_buf_global;  /* Means index_buf uses global bvh's grid_common_gpu_buffer, **DO NOT** free it! */

	bool use_bmesh;

	unsigned int tot_tri, tot_quad;

	/* The PBVH ensures that either all faces in the node are
	 * smooth-shaded or all faces are flat-shaded */
	bool smooth;

	bool show_diffuse_color;
	bool show_mask;

	float diffuse_color[4];
};

static struct {
	uint pos, nor, col;
} g_vbo_id = {0};

static void gpu_material_diffuse_get(int UNUSED(nr), float diff[4])
{
	/* TODO: sculpt diffuse color option not supported in 2.8 yet. */
	diff[0] = 0.8f;
	diff[1] = 0.8f;
	diff[2] = 0.8f;
	diff[3] = 1.0f;
}

/* Allocates a non-initialized buffer to be sent to GPU.
 * Return is false it indicates that the memory map failed. */
static bool gpu_pbvh_vert_buf_data_set(GPU_PBVH_Buffers *buffers, unsigned int vert_len)
{
	if (buffers->vert_buf == NULL) {
		/* Initialize vertex buffer */
		/* match 'VertexBufferFormat' */

		static GPUVertFormat format = {0};
		if (format.attr_len == 0) {
			g_vbo_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			g_vbo_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I16, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
			g_vbo_id.col = GPU_vertformat_attr_add(&format, "color", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
		}
#if 0
		buffers->vert_buf = GPU_vertbuf_create_with_format_ex(&format, GPU_USAGE_DYNAMIC);
		GPU_vertbuf_data_alloc(buffers->vert_buf, vert_len);
	}
	else if (vert_len != buffers->vert_buf->vertex_len) {
		GPU_vertbuf_data_resize(buffers->vert_buf, vert_len);
	}
#else
		buffers->vert_buf = GPU_vertbuf_create_with_format_ex(&format, GPU_USAGE_STATIC);
	}
	GPU_vertbuf_data_alloc(buffers->vert_buf, vert_len);
#endif
	return buffers->vert_buf->data != NULL;
}

static void gpu_pbvh_batch_init(GPU_PBVH_Buffers *buffers)
{
	/* force flushing to the GPU */
	if (buffers->vert_buf->data) {
		GPU_vertbuf_use(buffers->vert_buf);
	}

	if (buffers->triangles == NULL) {
		buffers->triangles = GPU_batch_create(
		        GPU_PRIM_TRIS, buffers->vert_buf,
		        /* can be NULL */
		        buffers->index_buf);
	}

	if ((buffers->triangles_fast == NULL) && buffers->index_buf_fast) {
		buffers->triangles_fast = GPU_batch_create(
		        GPU_PRIM_TRIS, buffers->vert_buf,
		        /* can be NULL */
		        buffers->index_buf_fast);
	}
}

static float gpu_color_from_mask(float mask)
{
	return 1.0f - mask * 0.75f;
}

static void gpu_color_from_mask_copy(float mask, const float diffuse_color[4], unsigned char out[3])
{
	float mask_color;

	mask_color = gpu_color_from_mask(mask) * 255.0f;

	out[0] = diffuse_color[0] * mask_color;
	out[1] = diffuse_color[1] * mask_color;
	out[2] = diffuse_color[2] * mask_color;
}

static void gpu_color_from_mask_quad_copy(const CCGKey *key,
                                          CCGElem *a, CCGElem *b,
                                          CCGElem *c, CCGElem *d,
                                          const float *diffuse_color,
                                          unsigned char out[3])
{
	float mask_color =
	    gpu_color_from_mask((*CCG_elem_mask(key, a) +
	                         *CCG_elem_mask(key, b) +
	                         *CCG_elem_mask(key, c) +
	                         *CCG_elem_mask(key, d)) * 0.25f) * 255.0f;

	out[0] = diffuse_color[0] * mask_color;
	out[1] = diffuse_color[1] * mask_color;
	out[2] = diffuse_color[2] * mask_color;
}

void GPU_pbvh_mesh_buffers_update(
        GPU_PBVH_Buffers *buffers, const MVert *mvert,
        const int *vert_indices, int totvert, const float *vmask,
        const int (*face_vert_indices)[3],
        const int update_flags)
{
	const bool show_diffuse_color = (update_flags & GPU_PBVH_BUFFERS_SHOW_DIFFUSE_COLOR) != 0;
	const bool show_mask = (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0;

	buffers->vmask = vmask;
	buffers->show_diffuse_color = show_diffuse_color;
	buffers->show_mask = show_mask;

	{
		int totelem = (buffers->smooth ? totvert : (buffers->tot_tri * 3));
		float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 0.8f};

		if (show_diffuse_color) {
			const MLoopTri *lt = &buffers->looptri[buffers->face_indices[0]];
			const MPoly *mp = &buffers->mpoly[lt->poly];

			gpu_material_diffuse_get(mp->mat_nr + 1, diffuse_color);
		}

		copy_v4_v4(buffers->diffuse_color, diffuse_color);

		uchar diffuse_color_ub[4];
		rgba_float_to_uchar(diffuse_color_ub, diffuse_color);

		/* Build VBO */
		if (gpu_pbvh_vert_buf_data_set(buffers, totelem)) {
			/* Vertex data is shared if smooth-shaded, but separate
			 * copies are made for flat shading because normals
			 * shouldn't be shared. */
			if (buffers->smooth) {
				for (uint i = 0; i < totvert; ++i) {
					const MVert *v = &mvert[vert_indices[i]];
					GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.pos, i, v->co);
					GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.nor, i, v->no);
				}

				for (uint i = 0; i < buffers->face_indices_len; i++) {
					const MLoopTri *lt = &buffers->looptri[buffers->face_indices[i]];
					for (uint j = 0; j < 3; j++) {
						int vidx = face_vert_indices[i][j];
						if (vmask && show_mask) {
							int v_index = buffers->mloop[lt->tri[j]].v;
							uchar color_ub[3];
							gpu_color_from_mask_copy(vmask[v_index], diffuse_color, color_ub);
							GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col, vidx, color_ub);
						}
						else {
							GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col, vidx, diffuse_color_ub);
						}
					}
				}
			}
			else {
				/* calculate normal for each polygon only once */
				unsigned int mpoly_prev = UINT_MAX;
				short no[3];
				int vbo_index = 0;

				for (uint i = 0; i < buffers->face_indices_len; i++) {
					const MLoopTri *lt = &buffers->looptri[buffers->face_indices[i]];
					const unsigned int vtri[3] = {
					    buffers->mloop[lt->tri[0]].v,
					    buffers->mloop[lt->tri[1]].v,
					    buffers->mloop[lt->tri[2]].v,
					};

					if (paint_is_face_hidden(lt, mvert, buffers->mloop))
						continue;

					/* Face normal and mask */
					if (lt->poly != mpoly_prev) {
						const MPoly *mp = &buffers->mpoly[lt->poly];
						float fno[3];
						BKE_mesh_calc_poly_normal(mp, &buffers->mloop[mp->loopstart], mvert, fno);
						normal_float_to_short_v3(no, fno);
						mpoly_prev = lt->poly;
					}

					uchar color_ub[3];
					if (vmask && show_mask) {
						float fmask = (vmask[vtri[0]] + vmask[vtri[1]] + vmask[vtri[2]]) / 3.0f;
						gpu_color_from_mask_copy(fmask, diffuse_color, color_ub);
					}
					else {
						copy_v3_v3_uchar(color_ub, diffuse_color_ub);
					}

					for (uint j = 0; j < 3; j++) {
						const MVert *v = &mvert[vtri[j]];

						GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.pos, vbo_index, v->co);
						GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.nor, vbo_index, no);
						GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col, vbo_index, color_ub);

						vbo_index++;
					}
				}
			}

			gpu_pbvh_batch_init(buffers);
		}
	}

	buffers->mvert = mvert;
}

GPU_PBVH_Buffers *GPU_pbvh_mesh_buffers_build(
        const int (*face_vert_indices)[3],
        const MPoly *mpoly, const MLoop *mloop, const MLoopTri *looptri,
        const MVert *mvert,
        const int *face_indices,
        const int  face_indices_len)
{
	GPU_PBVH_Buffers *buffers;
	int i, tottri;

	buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");

	/* smooth or flat for all */
#if 0
	buffers->smooth = mpoly[looptri[face_indices[0]].poly].flag & ME_SMOOTH;
#else
	/* for DrawManager we dont support mixed smooth/flat */
	buffers->smooth = (mpoly[0].flag & ME_SMOOTH) != 0;
#endif

	buffers->show_diffuse_color = false;
	buffers->show_mask = true;

	/* Count the number of visible triangles */
	for (i = 0, tottri = 0; i < face_indices_len; ++i) {
		const MLoopTri *lt = &looptri[face_indices[i]];
		if (!paint_is_face_hidden(lt, mvert, mloop))
			tottri++;
	}

	if (tottri == 0) {
		buffers->tot_tri = 0;

		buffers->mpoly = mpoly;
		buffers->mloop = mloop;
		buffers->looptri = looptri;
		buffers->face_indices = face_indices;
		buffers->face_indices_len = 0;

		return buffers;
	}

	/* An element index buffer is used for smooth shading, but flat
	 * shading requires separate vertex normals so an index buffer is
	 * can't be used there. */
	if (buffers->smooth) {
		/* Fill the triangle buffer */
		buffers->index_buf = NULL;
		GPUIndexBufBuilder elb;
		GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tottri, INT_MAX);

		for (i = 0; i < face_indices_len; ++i) {
			const MLoopTri *lt = &looptri[face_indices[i]];

			/* Skip hidden faces */
			if (paint_is_face_hidden(lt, mvert, mloop))
				continue;

			GPU_indexbuf_add_tri_verts(&elb, UNPACK3(face_vert_indices[i]));
		}
		buffers->index_buf = GPU_indexbuf_build(&elb);
	}
	else {
		if (!buffers->is_index_buf_global) {
			GPU_INDEXBUF_DISCARD_SAFE(buffers->index_buf);
		}
		buffers->index_buf = NULL;
		buffers->is_index_buf_global = false;
	}

	buffers->tot_tri = tottri;

	buffers->mpoly = mpoly;
	buffers->mloop = mloop;
	buffers->looptri = looptri;

	buffers->face_indices = face_indices;
	buffers->face_indices_len = face_indices_len;

	return buffers;
}

void GPU_pbvh_grid_buffers_update(
        GPU_PBVH_Buffers *buffers, CCGElem **grids,
        const DMFlagMat *grid_flag_mats, int *grid_indices,
        int totgrid, const CCGKey *key,
        const int update_flags)
{
	const bool show_diffuse_color = (update_flags & GPU_PBVH_BUFFERS_SHOW_DIFFUSE_COLOR) != 0;
	const bool show_mask = (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0;
	int i, j, k, x, y;

	buffers->show_diffuse_color = show_diffuse_color;
	buffers->show_mask = show_mask;
	buffers->smooth = grid_flag_mats[grid_indices[0]].flag & ME_SMOOTH;

	/* Build VBO */
	if (buffers->index_buf) {
		const int has_mask = key->has_mask;
		float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 1.0f};

		if (show_diffuse_color) {
			const DMFlagMat *flags = &grid_flag_mats[grid_indices[0]];

			gpu_material_diffuse_get(flags->mat_nr + 1, diffuse_color);
		}

		copy_v4_v4(buffers->diffuse_color, diffuse_color);

		uint vbo_index_offset = 0;
		/* Build VBO */
		if (gpu_pbvh_vert_buf_data_set(buffers, totgrid * key->grid_area)) {
			for (i = 0; i < totgrid; ++i) {
				CCGElem *grid = grids[grid_indices[i]];
				int vbo_index = vbo_index_offset;

				for (y = 0; y < key->grid_size; y++) {
					for (x = 0; x < key->grid_size; x++) {
						CCGElem *elem = CCG_grid_elem(key, grid, x, y);
						GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.pos, vbo_index, CCG_elem_co(key, elem));

						if (buffers->smooth) {
							short no_short[3];
							normal_float_to_short_v3(no_short, CCG_elem_no(key, elem));
							GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.nor, vbo_index, no_short);

							if (has_mask) {
								uchar color_ub[3];
								if (show_mask) {
									gpu_color_from_mask_copy(*CCG_elem_mask(key, elem),
									                         diffuse_color, color_ub);
								}
								else {
									unit_float_to_uchar_clamp_v3(color_ub, diffuse_color);
								}
								GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col, vbo_index, color_ub);
							}
						}
						vbo_index += 1;
					}
				}

				if (!buffers->smooth) {
					for (j = 0; j < key->grid_size - 1; j++) {
						for (k = 0; k < key->grid_size - 1; k++) {
							CCGElem *elems[4] = {
								CCG_grid_elem(key, grid, k, j + 1),
								CCG_grid_elem(key, grid, k + 1, j + 1),
								CCG_grid_elem(key, grid, k + 1, j),
								CCG_grid_elem(key, grid, k, j)
							};
							float fno[3];

							normal_quad_v3(fno,
							               CCG_elem_co(key, elems[0]),
							               CCG_elem_co(key, elems[1]),
							               CCG_elem_co(key, elems[2]),
							               CCG_elem_co(key, elems[3]));

							vbo_index = vbo_index_offset + ((j + 1) * key->grid_size + k);
							short no_short[3];
							normal_float_to_short_v3(no_short, fno);
							GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.nor, vbo_index, no_short);

							if (has_mask) {
								uchar color_ub[3];
								if (show_mask) {
									gpu_color_from_mask_quad_copy(key,
									                              elems[0],
									                              elems[1],
									                              elems[2],
									                              elems[3],
									                              diffuse_color,
									                              color_ub);
								}
								else {
									unit_float_to_uchar_clamp_v3(color_ub, diffuse_color);
								}
								GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col, vbo_index, color_ub);
							}
						}
					}
				}

				vbo_index_offset += key->grid_area;
			}

			gpu_pbvh_batch_init(buffers);
		}
	}

	buffers->grids = grids;
	buffers->grid_indices = grid_indices;
	buffers->totgrid = totgrid;
	buffers->grid_flag_mats = grid_flag_mats;
	buffers->gridkey = *key;

	//printf("node updated %p\n", buffers);
}

/* Build the element array buffer of grid indices using either
 * unsigned shorts or unsigned ints. */
#define FILL_QUAD_BUFFER(max_vert_, tot_quad_, buffer_)                 \
	{                                                                   \
		int offset = 0;                                                 \
		int i, j, k;                                                    \
                                                                        \
		GPUIndexBufBuilder elb;                                         \
		GPU_indexbuf_init(                                              \
		       &elb, GPU_PRIM_TRIS, tot_quad_ * 2, max_vert_);          \
                                                                        \
		/* Fill the buffer */                                           \
		for (i = 0; i < totgrid; ++i) {                                 \
			BLI_bitmap *gh = NULL;                                      \
			if (grid_hidden)                                            \
				gh = grid_hidden[(grid_indices)[i]];                    \
                                                                        \
			for (j = 0; j < gridsize - 1; ++j) {                        \
				for (k = 0; k < gridsize - 1; ++k) {                    \
					/* Skip hidden grid face */                         \
					if (gh && paint_is_grid_face_hidden(                \
					        gh, gridsize, k, j))                        \
					{                                                   \
						continue;                                       \
					}                                                   \
					GPU_indexbuf_add_generic_vert(&elb, offset + j * gridsize + k + 1); \
					GPU_indexbuf_add_generic_vert(&elb, offset + j * gridsize + k);    \
					GPU_indexbuf_add_generic_vert(&elb, offset + (j + 1) * gridsize + k); \
                                                                        \
					GPU_indexbuf_add_generic_vert(&elb, offset + (j + 1) * gridsize + k + 1); \
					GPU_indexbuf_add_generic_vert(&elb, offset + j * gridsize + k + 1); \
					GPU_indexbuf_add_generic_vert(&elb, offset + (j + 1) * gridsize + k); \
				}                                                       \
			}                                                           \
                                                                        \
			offset += gridsize * gridsize;                              \
		}                                                               \
		buffer_ = GPU_indexbuf_build(&elb);                             \
	} (void)0
/* end FILL_QUAD_BUFFER */

static GPUIndexBuf *gpu_get_grid_buffer(
        int gridsize, unsigned *totquad, GridCommonGPUBuffer **grid_common_gpu_buffer,
        /* remove this arg  when GPU gets base-vertex support! */
        int totgrid)
{
	/* used in the FILL_QUAD_BUFFER macro */
	BLI_bitmap * const *grid_hidden = NULL;
	const int *grid_indices = NULL;
	// int totgrid = 1;

	GridCommonGPUBuffer *gridbuff = *grid_common_gpu_buffer;

	if (gridbuff == NULL) {
		*grid_common_gpu_buffer = gridbuff = MEM_mallocN(sizeof(GridCommonGPUBuffer), __func__);
		gridbuff->mres_buffer = NULL;
		gridbuff->mres_prev_gridsize = -1;
		gridbuff->mres_prev_totquad = 0;
	}

	/* VBO is already built */
	if (gridbuff->mres_buffer && gridbuff->mres_prev_gridsize == gridsize) {
		*totquad = gridbuff->mres_prev_totquad;
		return gridbuff->mres_buffer;
	}
	/* we can't reuse old, delete the existing buffer */
	else if (gridbuff->mres_buffer) {
		GPU_indexbuf_discard(gridbuff->mres_buffer);
		gridbuff->mres_buffer = NULL;
	}

	/* Build new VBO */
	*totquad = (gridsize - 1) * (gridsize - 1) * totgrid;
	int max_vert = gridsize * gridsize * totgrid;

	FILL_QUAD_BUFFER(max_vert, *totquad, gridbuff->mres_buffer);

	gridbuff->mres_prev_gridsize = gridsize;
	gridbuff->mres_prev_totquad = *totquad;
	return gridbuff->mres_buffer;
}

#define FILL_FAST_BUFFER() \
{ \
	GPUIndexBufBuilder elb; \
	GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, 6 * totgrid, INT_MAX); \
	for (int i = 0; i < totgrid; i++) { \
		GPU_indexbuf_add_generic_vert(&elb, i * gridsize * gridsize + gridsize - 1); \
		GPU_indexbuf_add_generic_vert(&elb, i * gridsize * gridsize); \
		GPU_indexbuf_add_generic_vert(&elb, (i + 1) * gridsize * gridsize - gridsize); \
		GPU_indexbuf_add_generic_vert(&elb, (i + 1) * gridsize * gridsize - 1); \
		GPU_indexbuf_add_generic_vert(&elb, i * gridsize * gridsize + gridsize - 1); \
		GPU_indexbuf_add_generic_vert(&elb, (i + 1) * gridsize * gridsize - gridsize); \
	} \
	buffers->index_buf_fast = GPU_indexbuf_build(&elb); \
} (void)0

GPU_PBVH_Buffers *GPU_pbvh_grid_buffers_build(
        int *grid_indices, int totgrid, BLI_bitmap **grid_hidden, int gridsize, const CCGKey *UNUSED(key),
        GridCommonGPUBuffer **grid_common_gpu_buffer)
{
	GPU_PBVH_Buffers *buffers;
	int totquad;
	int fully_visible_totquad = (gridsize - 1) * (gridsize - 1) * totgrid;

	buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
	buffers->grid_hidden = grid_hidden;
	buffers->totgrid = totgrid;

	buffers->show_diffuse_color = false;
	buffers->show_mask = true;

	/* Count the number of quads */
	totquad = BKE_pbvh_count_grid_quads(grid_hidden, grid_indices, totgrid, gridsize);

	/* totally hidden node, return here to avoid BufferData with zero below. */
	if (totquad == 0)
		return buffers;

	/* create and fill indices of the fast buffer too */
	FILL_FAST_BUFFER();

	if (totquad == fully_visible_totquad) {
		buffers->index_buf = gpu_get_grid_buffer(
		        gridsize, &buffers->tot_quad, grid_common_gpu_buffer, totgrid);
		buffers->has_hidden = false;
		buffers->is_index_buf_global = true;
	}
	else {
		uint max_vert = totgrid * gridsize * gridsize;
		buffers->tot_quad = totquad;

		FILL_QUAD_BUFFER(max_vert, totquad, buffers->index_buf);

		buffers->has_hidden = false;
		buffers->is_index_buf_global = false;
	}

#ifdef USE_BASE_ELEM
	/* Build coord/normal VBO */
	if (GLEW_ARB_draw_elements_base_vertex /* 3.2 */) {
		int i;
		buffers->baseelemarray = MEM_mallocN(sizeof(int) * totgrid * 2, "GPU_PBVH_Buffers.baseelemarray");
		buffers->baseindex = MEM_mallocN(sizeof(void *) * totgrid, "GPU_PBVH_Buffers.baseindex");
		for (i = 0; i < totgrid; i++) {
			buffers->baseelemarray[i] = buffers->tot_quad * 6;
			buffers->baseelemarray[i + totgrid] = i * key->grid_area;
			buffers->baseindex[i] = NULL;
		}
	}
#endif

	return buffers;
}

#undef FILL_QUAD_BUFFER

/* Output a BMVert into a VertexBufferFormat array
 *
 * The vertex is skipped if hidden, otherwise the output goes into
 * index '*v_index' in the 'vert_data' array and '*v_index' is
 * incremented.
 */
static void gpu_bmesh_vert_to_buffer_copy__gwn(
        BMVert *v,
        GPUVertBuf *vert_buf,
        int *v_index,
        const float fno[3],
        const float *fmask,
        const int cd_vert_mask_offset,
        const float diffuse_color[4],
        const bool show_mask)
{
	if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {

		/* Set coord, normal, and mask */
		GPU_vertbuf_attr_set(vert_buf, g_vbo_id.pos, *v_index, v->co);

		{
			short no_short[3];
			normal_float_to_short_v3(no_short, fno ? fno : v->no);
			GPU_vertbuf_attr_set(vert_buf, g_vbo_id.nor, *v_index, no_short);
		}

		{
			uchar color_ub[3];
			float effective_mask;
			if (show_mask) {
				effective_mask = fmask ? *fmask
				                       : BM_ELEM_CD_GET_FLOAT(v, cd_vert_mask_offset);
			}
			else {
				effective_mask = 0.0f;
			}

			gpu_color_from_mask_copy(
			        effective_mask,
			        diffuse_color,
			        color_ub);
			GPU_vertbuf_attr_set(vert_buf, g_vbo_id.col, *v_index, color_ub);
		}

		/* Assign index for use in the triangle index buffer */
		/* note: caller must set:  bm->elem_index_dirty |= BM_VERT; */
		BM_elem_index_set(v, (*v_index)); /* set_dirty! */

		(*v_index)++;
	}
}

/* Return the total number of vertices that don't have BM_ELEM_HIDDEN set */
static int gpu_bmesh_vert_visible_count(GSet *bm_unique_verts,
                                        GSet *bm_other_verts)
{
	GSetIterator gs_iter;
	int totvert = 0;

	GSET_ITER (gs_iter, bm_unique_verts) {
		BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
		if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN))
			totvert++;
	}
	GSET_ITER (gs_iter, bm_other_verts) {
		BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
		if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN))
			totvert++;
	}

	return totvert;
}

/* Return the total number of visible faces */
static int gpu_bmesh_face_visible_count(GSet *bm_faces)
{
	GSetIterator gh_iter;
	int totface = 0;

	GSET_ITER (gh_iter, bm_faces) {
		BMFace *f = BLI_gsetIterator_getKey(&gh_iter);

		if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN))
			totface++;
	}

	return totface;
}

/* Creates a vertex buffer (coordinate, normal, color) and, if smooth
 * shading, an element index buffer. */
void GPU_pbvh_bmesh_buffers_update(
        GPU_PBVH_Buffers *buffers,
        BMesh *bm,
        GSet *bm_faces,
        GSet *bm_unique_verts,
        GSet *bm_other_verts,
        const int update_flags)
{
	const bool show_diffuse_color = (update_flags & GPU_PBVH_BUFFERS_SHOW_DIFFUSE_COLOR) != 0;
	const bool show_mask = (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0;
	int tottri, totvert, maxvert = 0;
	float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 1.0f};

	/* TODO, make mask layer optional for bmesh buffer */
	const int cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);

	buffers->show_diffuse_color = show_diffuse_color;
	buffers->show_mask = show_mask;

	/* Count visible triangles */
	tottri = gpu_bmesh_face_visible_count(bm_faces);

	if (buffers->smooth) {
		/* Smooth needs to recreate index buffer, so we have to invalidate the batch. */
		GPU_BATCH_DISCARD_SAFE(buffers->triangles);
		/* Count visible vertices */
		totvert = gpu_bmesh_vert_visible_count(bm_unique_verts, bm_other_verts);
	}
	else {
		totvert = tottri * 3;
	}

	if (!tottri) {
		buffers->tot_tri = 0;
		return;
	}

	if (show_diffuse_color) {
		/* due to dynamic nature of dyntopo, only get first material */
		GSetIterator gs_iter;
		BMFace *f;
		BLI_gsetIterator_init(&gs_iter, bm_faces);
		f = BLI_gsetIterator_getKey(&gs_iter);
		gpu_material_diffuse_get(f->mat_nr + 1, diffuse_color);
	}

	copy_v4_v4(buffers->diffuse_color, diffuse_color);

	/* Fill vertex buffer */
	if (gpu_pbvh_vert_buf_data_set(buffers, totvert)) {
		int v_index = 0;

		if (buffers->smooth) {
			GSetIterator gs_iter;

			/* Vertices get an index assigned for use in the triangle
			 * index buffer */
			bm->elem_index_dirty |= BM_VERT;

			GSET_ITER (gs_iter, bm_unique_verts) {
				gpu_bmesh_vert_to_buffer_copy__gwn(
				        BLI_gsetIterator_getKey(&gs_iter),
				        buffers->vert_buf, &v_index, NULL, NULL,
				        cd_vert_mask_offset, diffuse_color,
				        show_mask);
			}

			GSET_ITER (gs_iter, bm_other_verts) {
				gpu_bmesh_vert_to_buffer_copy__gwn(
				        BLI_gsetIterator_getKey(&gs_iter),
				        buffers->vert_buf, &v_index, NULL, NULL,
				        cd_vert_mask_offset, diffuse_color,
				        show_mask);
			}

			maxvert = v_index;
		}
		else {
			GSetIterator gs_iter;

			GSET_ITER (gs_iter, bm_faces) {
				BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

				BLI_assert(f->len == 3);

				if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
					BMVert *v[3];
					float fmask = 0;
					int i;

#if 0
					BM_iter_as_array(bm, BM_VERTS_OF_FACE, f, (void **)v, 3);
#endif
					BM_face_as_array_vert_tri(f, v);

					/* Average mask value */
					for (i = 0; i < 3; i++) {
						fmask += BM_ELEM_CD_GET_FLOAT(v[i], cd_vert_mask_offset);
					}
					fmask /= 3.0f;

					for (i = 0; i < 3; i++) {
						gpu_bmesh_vert_to_buffer_copy__gwn(
						        v[i], buffers->vert_buf,
						        &v_index, f->no, &fmask,
						        cd_vert_mask_offset, diffuse_color,
						        show_mask);
					}
				}
			}

			buffers->tot_tri = tottri;
		}

		/* gpu_bmesh_vert_to_buffer_copy sets dirty index values */
		bm->elem_index_dirty |= BM_VERT;
	}
	else {
		/* Memory map failed */
		return;
	}

	if (buffers->smooth) {
		/* Fill the triangle buffer */
		buffers->index_buf = NULL;
		GPUIndexBufBuilder elb;
		GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tottri, maxvert);

		/* Initialize triangle index buffer */
		buffers->is_index_buf_global = false;

		/* Fill triangle index buffer */

		{
			GSetIterator gs_iter;

			GSET_ITER (gs_iter, bm_faces) {
				BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

				if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
					BMVert *v[3];

					BM_face_as_array_vert_tri(f, v);
					GPU_indexbuf_add_tri_verts(
					        &elb, BM_elem_index_get(v[0]), BM_elem_index_get(v[1]), BM_elem_index_get(v[2]));
				}
			}

			buffers->tot_tri = tottri;

			if (buffers->index_buf == NULL) {
				buffers->index_buf = GPU_indexbuf_build(&elb);
			}
			else {
				GPU_indexbuf_build_in_place(&elb, buffers->index_buf);
			}
		}
	}
	else if (buffers->index_buf) {
		if (!buffers->is_index_buf_global) {
			GPU_INDEXBUF_DISCARD_SAFE(buffers->index_buf);
		}
		buffers->index_buf = NULL;
		buffers->is_index_buf_global = false;
	}

	gpu_pbvh_batch_init(buffers);
}

GPU_PBVH_Buffers *GPU_pbvh_bmesh_buffers_build(bool smooth_shading)
{
	GPU_PBVH_Buffers *buffers;

	buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
	buffers->use_bmesh = true;
	buffers->smooth = smooth_shading;
	buffers->show_diffuse_color = false;
	buffers->show_mask = true;

	return buffers;
}

GPUBatch *GPU_pbvh_buffers_batch_get(GPU_PBVH_Buffers *buffers, bool fast)
{
	return (fast && buffers->triangles_fast) ?
	        buffers->triangles_fast : buffers->triangles;
}

bool GPU_pbvh_buffers_diffuse_changed(GPU_PBVH_Buffers *buffers, GSet *bm_faces, bool show_diffuse_color)
{
	float diffuse_color[4];

	if (buffers->show_diffuse_color != show_diffuse_color)
		return true;

	if (buffers->show_diffuse_color == false)
		return false;

	if (buffers->looptri) {
		const MLoopTri *lt = &buffers->looptri[buffers->face_indices[0]];
		const MPoly *mp = &buffers->mpoly[lt->poly];

		gpu_material_diffuse_get(mp->mat_nr + 1, diffuse_color);
	}
	else if (buffers->use_bmesh) {
		/* due to dynamic nature of dyntopo, only get first material */
		if (BLI_gset_len(bm_faces) > 0) {
			GSetIterator gs_iter;
			BMFace *f;

			BLI_gsetIterator_init(&gs_iter, bm_faces);
			f = BLI_gsetIterator_getKey(&gs_iter);
			gpu_material_diffuse_get(f->mat_nr + 1, diffuse_color);
		}
		else {
			return false;
		}
	}
	else {
		const DMFlagMat *flags = &buffers->grid_flag_mats[buffers->grid_indices[0]];

		gpu_material_diffuse_get(flags->mat_nr + 1, diffuse_color);
	}

	return !equals_v3v3(diffuse_color, buffers->diffuse_color);
}

bool GPU_pbvh_buffers_mask_changed(GPU_PBVH_Buffers *buffers, bool show_mask)
{
	return (buffers->show_mask != show_mask);
}

void GPU_pbvh_buffers_free(GPU_PBVH_Buffers *buffers)
{
	if (buffers) {
		GPU_BATCH_DISCARD_SAFE(buffers->triangles);
		GPU_BATCH_DISCARD_SAFE(buffers->triangles_fast);
		if (!buffers->is_index_buf_global) {
			GPU_INDEXBUF_DISCARD_SAFE(buffers->index_buf);
		}
		GPU_INDEXBUF_DISCARD_SAFE(buffers->index_buf_fast);
		GPU_VERTBUF_DISCARD_SAFE(buffers->vert_buf);

#ifdef USE_BASE_ELEM
		if (buffers->baseelemarray)
			MEM_freeN(buffers->baseelemarray);
		if (buffers->baseindex)
			MEM_freeN(buffers->baseindex);
#endif

		MEM_freeN(buffers);
	}
}

void GPU_pbvh_multires_buffers_free(GridCommonGPUBuffer **grid_common_gpu_buffer)
{
	GridCommonGPUBuffer *gridbuff = *grid_common_gpu_buffer;

	if (gridbuff) {
		if (gridbuff->mres_buffer) {
			BLI_mutex_lock(&buffer_mutex);
			GPU_INDEXBUF_DISCARD_SAFE(gridbuff->mres_buffer);
			BLI_mutex_unlock(&buffer_mutex);
		}
		MEM_freeN(gridbuff);
		*grid_common_gpu_buffer = NULL;
	}
}

/* debug function, draws the pbvh BB */
void GPU_pbvh_BB_draw(float min[3], float max[3], bool leaf, unsigned int pos)
{
	if (leaf)
		immUniformColor4f(0.0, 1.0, 0.0, 0.5);
	else
		immUniformColor4f(1.0, 0.0, 0.0, 0.5);

	/* TODO(merwin): revisit this after we have mutable VertexBuffers
	 * could keep a static batch & index buffer, change the VBO contents per draw
	 */

	immBegin(GPU_PRIM_LINES, 24);

	/* top */
	immVertex3f(pos, min[0], min[1], max[2]);
	immVertex3f(pos, min[0], max[1], max[2]);

	immVertex3f(pos, min[0], max[1], max[2]);
	immVertex3f(pos, max[0], max[1], max[2]);

	immVertex3f(pos, max[0], max[1], max[2]);
	immVertex3f(pos, max[0], min[1], max[2]);

	immVertex3f(pos, max[0], min[1], max[2]);
	immVertex3f(pos, min[0], min[1], max[2]);

	/* bottom */
	immVertex3f(pos, min[0], min[1], min[2]);
	immVertex3f(pos, min[0], max[1], min[2]);

	immVertex3f(pos, min[0], max[1], min[2]);
	immVertex3f(pos, max[0], max[1], min[2]);

	immVertex3f(pos, max[0], max[1], min[2]);
	immVertex3f(pos, max[0], min[1], min[2]);

	immVertex3f(pos, max[0], min[1], min[2]);
	immVertex3f(pos, min[0], min[1], min[2]);

	/* sides */
	immVertex3f(pos, min[0], min[1], min[2]);
	immVertex3f(pos, min[0], min[1], max[2]);

	immVertex3f(pos, min[0], max[1], min[2]);
	immVertex3f(pos, min[0], max[1], max[2]);

	immVertex3f(pos, max[0], max[1], min[2]);
	immVertex3f(pos, max[0], max[1], max[2]);

	immVertex3f(pos, max[0], min[1], min[2]);
	immVertex3f(pos, max[0], min[1], max[2]);

	immEnd();
}

void GPU_pbvh_fix_linking()
{
}
