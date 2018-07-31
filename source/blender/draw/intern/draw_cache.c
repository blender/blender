/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file draw_cache.c
 *  \ingroup draw
 */


#include "DNA_scene_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_modifier_types.h"
#include "DNA_lattice_types.h"

#include "UI_resources.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_batch_utils.h"

#include "draw_cache.h"
#include "draw_cache_impl.h"

/* Batch's only (free'd as an array) */
static struct DRWShapeCache {
	GPUBatch *drw_single_vertice;
	GPUBatch *drw_cursor;
	GPUBatch *drw_cursor_only_circle;
	GPUBatch *drw_fullscreen_quad;
	GPUBatch *drw_fullscreen_quad_texcoord;
	GPUBatch *drw_quad;
	GPUBatch *drw_sphere;
	GPUBatch *drw_screenspace_circle;
	GPUBatch *drw_plain_axes;
	GPUBatch *drw_single_arrow;
	GPUBatch *drw_cube;
	GPUBatch *drw_circle;
	GPUBatch *drw_square;
	GPUBatch *drw_line;
	GPUBatch *drw_line_endpoints;
	GPUBatch *drw_empty_cube;
	GPUBatch *drw_empty_sphere;
	GPUBatch *drw_empty_cylinder;
	GPUBatch *drw_empty_capsule_body;
	GPUBatch *drw_empty_capsule_cap;
	GPUBatch *drw_empty_cone;
	GPUBatch *drw_arrows;
	GPUBatch *drw_axis_names;
	GPUBatch *drw_image_plane;
	GPUBatch *drw_image_plane_wire;
	GPUBatch *drw_field_wind;
	GPUBatch *drw_field_force;
	GPUBatch *drw_field_vortex;
	GPUBatch *drw_field_tube_limit;
	GPUBatch *drw_field_cone_limit;
	GPUBatch *drw_lamp;
	GPUBatch *drw_lamp_shadows;
	GPUBatch *drw_lamp_sunrays;
	GPUBatch *drw_lamp_area_square;
	GPUBatch *drw_lamp_area_disk;
	GPUBatch *drw_lamp_hemi;
	GPUBatch *drw_lamp_spot;
	GPUBatch *drw_lamp_spot_square;
	GPUBatch *drw_speaker;
	GPUBatch *drw_lightprobe_cube;
	GPUBatch *drw_lightprobe_planar;
	GPUBatch *drw_lightprobe_grid;
	GPUBatch *drw_bone_octahedral;
	GPUBatch *drw_bone_octahedral_wire;
	GPUBatch *drw_bone_box;
	GPUBatch *drw_bone_box_wire;
	GPUBatch *drw_bone_wire_wire;
	GPUBatch *drw_bone_envelope;
	GPUBatch *drw_bone_envelope_outline;
	GPUBatch *drw_bone_point;
	GPUBatch *drw_bone_point_wire;
	GPUBatch *drw_bone_stick;
	GPUBatch *drw_bone_arrows;
	GPUBatch *drw_camera;
	GPUBatch *drw_camera_frame;
	GPUBatch *drw_camera_tria;
	GPUBatch *drw_camera_focus;
	GPUBatch *drw_particle_cross;
	GPUBatch *drw_particle_circle;
	GPUBatch *drw_particle_axis;
	GPUBatch *drw_gpencil_axes;
} SHC = {NULL};

void DRW_shape_cache_free(void)
{
	uint i = sizeof(SHC) / sizeof(GPUBatch *);
	GPUBatch **batch = (GPUBatch **)&SHC;
	while (i--) {
		GPU_BATCH_DISCARD_SAFE(*batch);
		batch++;
	}
}

void DRW_shape_cache_reset(void)
{
	uint i = sizeof(SHC) / sizeof(GPUBatch *);
	GPUBatch **batch = (GPUBatch **)&SHC;
	while (i--) {
		if (*batch) {
			GPU_batch_vao_cache_clear(*batch);
		}
		batch++;
	}
}

/* -------------------------------------------------------------------- */

/** \name Helper functions
 * \{ */

static void UNUSED_FUNCTION(add_fancy_edge)(
        GPUVertBuf *vbo, uint pos_id, uint n1_id, uint n2_id,
        uint *v_idx, const float co1[3], const float co2[3],
        const float n1[3], const float n2[3])
{
	GPU_vertbuf_attr_set(vbo, n1_id, *v_idx, n1);
	GPU_vertbuf_attr_set(vbo, n2_id, *v_idx, n2);
	GPU_vertbuf_attr_set(vbo, pos_id, (*v_idx)++, co1);

	GPU_vertbuf_attr_set(vbo, n1_id, *v_idx, n1);
	GPU_vertbuf_attr_set(vbo, n2_id, *v_idx, n2);
	GPU_vertbuf_attr_set(vbo, pos_id, (*v_idx)++, co2);
}

#if 0 /* UNUSED */
static void add_lat_lon_vert(
        GPUVertBuf *vbo, uint pos_id, uint nor_id,
        uint *v_idx, const float rad, const float lat, const float lon)
{
	float pos[3], nor[3];
	nor[0] = sinf(lat) * cosf(lon);
	nor[1] = cosf(lat);
	nor[2] = sinf(lat) * sinf(lon);
	mul_v3_v3fl(pos, nor, rad);

	GPU_vertbuf_attr_set(vbo, nor_id, *v_idx, nor);
	GPU_vertbuf_attr_set(vbo, pos_id, (*v_idx)++, pos);
}
#endif

static GPUVertBuf *fill_arrows_vbo(const float scale)
{
	/* Position Only 3D format */
	static GPUVertFormat format = { 0 };
	static struct { uint pos; } attr_id;
	if (format.attr_len == 0) {
		attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	}

	/* Line */
	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, 6 * 3);

	float v1[3] = {0.0, 0.0, 0.0};
	float v2[3] = {0.0, 0.0, 0.0};
	float vtmp1[3], vtmp2[3];

	for (int axis = 0; axis < 3; axis++) {
		const int arrow_axis = (axis == 0) ? 1 : 0;

		v2[axis] = 1.0f;
		mul_v3_v3fl(vtmp1, v1, scale);
		mul_v3_v3fl(vtmp2, v2, scale);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 0, vtmp1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 1, vtmp2);

		v1[axis] = 0.85f;
		v1[arrow_axis] = -0.08f;
		mul_v3_v3fl(vtmp1, v1, scale);
		mul_v3_v3fl(vtmp2, v2, scale);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 2, vtmp1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 3, vtmp2);

		v1[arrow_axis] = 0.08f;
		mul_v3_v3fl(vtmp1, v1, scale);
		mul_v3_v3fl(vtmp2, v2, scale);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 4, vtmp1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 5, vtmp2);

		/* reset v1 & v2 to zero */
		v1[arrow_axis] = v1[axis] = v2[axis] = 0.0f;
	}

	return vbo;
}

static GPUVertBuf *sphere_wire_vbo(const float rad)
{
#define NSEGMENTS 32
	/* Position Only 3D format */
	static GPUVertFormat format = { 0 };
	static struct { uint pos; } attr_id;
	if (format.attr_len == 0) {
		attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, NSEGMENTS * 2 * 3);

	/* a single ring of vertices */
	float p[NSEGMENTS][2];
	for (int i = 0; i < NSEGMENTS; ++i) {
		float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
		p[i][0] = rad * cosf(angle);
		p[i][1] = rad * sinf(angle);
	}

	for (int axis = 0; axis < 3; ++axis) {
		for (int i = 0; i < NSEGMENTS; ++i) {
			for (int j = 0; j < 2; ++j) {
				float cv[2], v[3];

				cv[0] = p[(i + j) % NSEGMENTS][0];
				cv[1] = p[(i + j) % NSEGMENTS][1];

				if (axis == 0) {
					ARRAY_SET_ITEMS(v, cv[0], cv[1], 0.0f);
				}
				else if (axis == 1) {
					ARRAY_SET_ITEMS(v, cv[0], 0.0f, cv[1]);
				}
				else {
					ARRAY_SET_ITEMS(v, 0.0f, cv[0], cv[1]);
				}
				GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 2 + j + (NSEGMENTS * 2 * axis), v);
			}
		}
	}

	return vbo;
#undef NSEGMENTS
}

/* Quads */
/* Use this one for rendering fullscreen passes. For 3D objects use DRW_cache_quad_get(). */
GPUBatch *DRW_cache_fullscreen_quad_get(void)
{
	if (!SHC.drw_fullscreen_quad) {
		/* Use a triangle instead of a real quad */
		/* https://www.slideshare.net/DevCentralAMD/vertex-shader-tricks-bill-bilodeau - slide 14 */
		float pos[3][2] = {{-1.0f, -1.0f}, { 3.0f, -1.0f}, {-1.0f,  3.0f}};
		float uvs[3][2] = {{ 0.0f,  0.0f}, { 2.0f,  0.0f}, { 0.0f,  2.0f}};

		/* Position Only 2D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos, uvs; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			attr_id.uvs = GPU_vertformat_attr_add(&format, "uvs", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			GPU_vertformat_alias_add(&format, "texCoord");
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 3);

		for (int i = 0; i < 3; ++i) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i, pos[i]);
			GPU_vertbuf_attr_set(vbo, attr_id.uvs, i, uvs[i]);
		}

		SHC.drw_fullscreen_quad = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_fullscreen_quad;
}

/* Just a regular quad with 4 vertices. */
GPUBatch *DRW_cache_quad_get(void)
{
	if (!SHC.drw_quad) {
		float pos[4][2] = {{-1.0f, -1.0f}, { 1.0f, -1.0f}, {1.0f,  1.0f}, {-1.0f,  1.0f}};
		float uvs[4][2] = {{ 0.0f,  0.0f}, { 1.0f,  0.0f}, {1.0f,  1.0f}, { 0.0f,  1.0f}};

		/* Position Only 2D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos, uvs; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			attr_id.uvs = GPU_vertformat_attr_add(&format, "uvs", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 4);

		for (int i = 0; i < 4; ++i) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i, pos[i]);
			GPU_vertbuf_attr_set(vbo, attr_id.uvs, i, uvs[i]);
		}

		SHC.drw_quad = GPU_batch_create_ex(GPU_PRIM_TRI_FAN, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_quad;
}

/* Sphere */
GPUBatch *DRW_cache_sphere_get(void)
{
	if (!SHC.drw_sphere) {
		SHC.drw_sphere = gpu_batch_sphere(32, 24);
	}
	return SHC.drw_sphere;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Common
 * \{ */

GPUBatch *DRW_cache_cube_get(void)
{
	if (!SHC.drw_cube) {
		const GLfloat verts[8][3] = {
			{-1.0f, -1.0f, -1.0f},
			{-1.0f, -1.0f,  1.0f},
			{-1.0f,  1.0f, -1.0f},
			{-1.0f,  1.0f,  1.0f},
			{ 1.0f, -1.0f, -1.0f},
			{ 1.0f, -1.0f,  1.0f},
			{ 1.0f,  1.0f, -1.0f},
			{ 1.0f,  1.0f,  1.0f}
		};

		const uint indices[36] = {
			0, 1, 2,
			1, 3, 2,
			0, 4, 1,
			4, 5, 1,
			6, 5, 4,
			6, 7, 5,
			2, 7, 6,
			2, 3, 7,
			3, 1, 7,
			1, 5, 7,
			0, 2, 4,
			2, 6, 4,
		};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 36);

		for (int i = 0; i < 36; ++i) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i, verts[indices[i]]);
		}

		SHC.drw_cube = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_cube;
}

GPUBatch *DRW_cache_empty_cube_get(void)
{
	if (!SHC.drw_empty_cube) {
		const GLfloat verts[8][3] = {
			{-1.0f, -1.0f, -1.0f},
			{-1.0f, -1.0f,  1.0f},
			{-1.0f,  1.0f, -1.0f},
			{-1.0f,  1.0f,  1.0f},
			{ 1.0f, -1.0f, -1.0f},
			{ 1.0f, -1.0f,  1.0f},
			{ 1.0f,  1.0f, -1.0f},
			{ 1.0f,  1.0f,  1.0f}
		};

		const GLubyte indices[24] = {0, 1, 1, 3, 3, 2, 2, 0, 0, 4, 4, 5, 5, 7, 7, 6, 6, 4, 1, 5, 3, 7, 2, 6};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 24);

		for (int i = 0; i < 24; ++i) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i, verts[indices[i]]);
		}

		SHC.drw_empty_cube = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_empty_cube;
}

GPUBatch *DRW_cache_circle_get(void)
{
#define CIRCLE_RESOL 64
	if (!SHC.drw_circle) {
		float v[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL);

		for (int a = 0; a < CIRCLE_RESOL; a++) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[2] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[1] = 0.0f;
			GPU_vertbuf_attr_set(vbo, attr_id.pos, a, v);
		}

		SHC.drw_circle = GPU_batch_create_ex(GPU_PRIM_LINE_LOOP, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_circle;
#undef CIRCLE_RESOL
}

GPUBatch *DRW_cache_square_get(void)
{
	if (!SHC.drw_square) {
		float p[4][3] = {{ 1.0f, 0.0f,  1.0f},
		                 { 1.0f, 0.0f, -1.0f},
		                 {-1.0f, 0.0f, -1.0f},
		                 {-1.0f, 0.0f,  1.0f}};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 8);

		for (int i = 0; i < 4; i++) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 2,     p[i % 4]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 2 + 1, p[(i + 1) % 4]);
		}

		SHC.drw_square = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_square;
}

GPUBatch *DRW_cache_single_line_get(void)
{
	/* Z axis line */
	if (!SHC.drw_line) {
		float v1[3] = {0.0f, 0.0f, 0.0f};
		float v2[3] = {0.0f, 0.0f, 1.0f};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 2);

		GPU_vertbuf_attr_set(vbo, attr_id.pos, 0, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 1, v2);

		SHC.drw_line = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_line;
}

GPUBatch *DRW_cache_single_line_endpoints_get(void)
{
	/* Z axis line */
	if (!SHC.drw_line_endpoints) {
		float v1[3] = {0.0f, 0.0f, 0.0f};
		float v2[3] = {0.0f, 0.0f, 1.0f};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 2);

		GPU_vertbuf_attr_set(vbo, attr_id.pos, 0, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 1, v2);

		SHC.drw_line_endpoints = GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_line_endpoints;
}

GPUBatch *DRW_cache_screenspace_circle_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_screenspace_circle) {
		float v[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL + 1);

		for (int a = 0; a <= CIRCLE_RESOL; a++) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			GPU_vertbuf_attr_set(vbo, attr_id.pos, a, v);
		}

		SHC.drw_screenspace_circle = GPU_batch_create_ex(GPU_PRIM_LINE_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_screenspace_circle;
#undef CIRCLE_RESOL
}

/* Grease Pencil object */
GPUBatch *DRW_cache_gpencil_axes_get(void)
{
	if (!SHC.drw_gpencil_axes) {
		int axis;
		float v1[3] = { 0.0f, 0.0f, 0.0f };
		float v2[3] = { 0.0f, 0.0f, 0.0f };

		/* cube data */
		const GLfloat verts[8][3] = {
			{ -0.25f, -0.25f, -0.25f },
			{ -0.25f, -0.25f,  0.25f },
			{ -0.25f,  0.25f, -0.25f },
			{ -0.25f,  0.25f,  0.25f },
			{ 0.25f, -0.25f, -0.25f },
			{ 0.25f, -0.25f,  0.25f },
			{ 0.25f,  0.25f, -0.25f },
			{ 0.25f,  0.25f,  0.25f }
		};

		const GLubyte indices[24] = { 0, 1, 1, 3, 3, 2, 2, 0, 0, 4, 4, 5, 5, 7, 7, 6, 6, 4, 1, 5, 3, 7, 2, 6 };

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static uint pos_id;
		if (format.attr_len == 0) {
			pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo =  GPU_vertbuf_create_with_format(&format);

		/* alloc 30 elements for cube and 3 axis */
		GPU_vertbuf_data_alloc(vbo, ARRAY_SIZE(indices) + 6);

		/* draw axis */
		for (axis = 0; axis < 3; axis++) {
			v1[axis] = 1.0f;
			v2[axis] = -1.0f;

			GPU_vertbuf_attr_set(vbo, pos_id, axis * 2, v1);
			GPU_vertbuf_attr_set(vbo, pos_id, axis * 2 + 1, v2);

			/* reset v1 & v2 to zero for next axis */
			v1[axis] = v2[axis] = 0.0f;
		}

		/* draw cube */
		for (int i = 0; i < 24; ++i) {
			GPU_vertbuf_attr_set(vbo, pos_id, i + 6, verts[indices[i]]);
		}

		SHC.drw_gpencil_axes = GPU_batch_create(GPU_PRIM_LINES, vbo, NULL);
	}
	return SHC.drw_gpencil_axes;
}


/* -------------------------------------------------------------------- */

/** \name Common Object API
* \{ */

GPUBatch *DRW_cache_object_wire_outline_get(Object *ob)
{
	switch (ob->type) {
		case OB_MESH:
			return DRW_cache_mesh_wire_outline_get(ob);

		/* TODO, should match 'DRW_cache_object_surface_get' */
		default:
			return NULL;
	}
}

GPUBatch *DRW_cache_object_edge_detection_get(Object *ob, bool *r_is_manifold)
{
	switch (ob->type) {
		case OB_MESH:
			return DRW_cache_mesh_edge_detection_get(ob, r_is_manifold);

		/* TODO, should match 'DRW_cache_object_surface_get' */
		default:
			return NULL;
	}
}

/* Returns a buffer texture. */
void DRW_cache_object_face_wireframe_get(
        Object *ob, struct GPUTexture **r_vert_tx, struct GPUTexture **r_faceid_tx, int *r_tri_count)
{
	switch (ob->type) {
		case OB_MESH:
			DRW_mesh_batch_cache_get_wireframes_face_texbuf((Mesh *)ob->data, r_vert_tx, r_faceid_tx, r_tri_count);

		/* TODO, should match 'DRW_cache_object_surface_get' */
	}
}

GPUBatch *DRW_cache_object_loose_edges_get(struct Object *ob)
{
	switch (ob->type) {
		case OB_MESH:
			return DRW_cache_mesh_loose_edges_get(ob);

		/* TODO, should match 'DRW_cache_object_surface_get' */
		default:
			return NULL;
	}
}

GPUBatch *DRW_cache_object_surface_get(Object *ob)
{
	switch (ob->type) {
		case OB_MESH:
			return DRW_cache_mesh_surface_get(ob);
		case OB_CURVE:
			return DRW_cache_curve_surface_get(ob);
		case OB_SURF:
			return DRW_cache_surf_surface_get(ob);
		case OB_FONT:
			return DRW_cache_text_surface_get(ob);
		case OB_MBALL:
			return DRW_cache_mball_surface_get(ob);
		default:
			return NULL;
	}
}

GPUBatch **DRW_cache_object_surface_material_get(
        struct Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len,
        char **auto_layer_names, int **auto_layer_is_srgb, int *auto_layer_count)
{
	if (auto_layer_names != NULL) {
		*auto_layer_names = NULL;
		*auto_layer_is_srgb = NULL;
		*auto_layer_count = 0;
	}

	switch (ob->type) {
		case OB_MESH:
			return DRW_cache_mesh_surface_shaded_get(ob, gpumat_array, gpumat_array_len,
			                                         auto_layer_names, auto_layer_is_srgb, auto_layer_count);
		case OB_CURVE:
			return DRW_cache_curve_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
		case OB_SURF:
			return DRW_cache_surf_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
		case OB_FONT:
			return DRW_cache_text_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
		case OB_MBALL:
			return DRW_cache_mball_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
		default:
			return NULL;
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Empties
 * \{ */

GPUBatch *DRW_cache_plain_axes_get(void)
{
	if (!SHC.drw_plain_axes) {
		int axis;
		float v1[3] = {0.0f, 0.0f, 0.0f};
		float v2[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 6);

		for (axis = 0; axis < 3; axis++) {
			v1[axis] = 1.0f;
			v2[axis] = -1.0f;

			GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 2, v1);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 2 + 1, v2);

			/* reset v1 & v2 to zero for next axis */
			v1[axis] = v2[axis] = 0.0f;
		}

		SHC.drw_plain_axes = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_plain_axes;
}

GPUBatch *DRW_cache_single_arrow_get(void)
{
	if (!SHC.drw_single_arrow) {
		float v1[3] = {0.0f, 0.0f, 1.0f}, v2[3], v3[3];

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		/* Square Pyramid */
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 12);

		v2[0] = 0.035f; v2[1] = 0.035f;
		v3[0] = -0.035f; v3[1] = 0.035f;
		v2[2] = v3[2] = 0.75f;

		for (int sides = 0; sides < 4; sides++) {
			if (sides % 2 == 1) {
				v2[0] = -v2[0];
				v3[1] = -v3[1];
			}
			else {
				v2[1] = -v2[1];
				v3[0] = -v3[0];
			}

			GPU_vertbuf_attr_set(vbo, attr_id.pos, sides * 3 + 0, v1);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, sides * 3 + 1, v2);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, sides * 3 + 2, v3);
		}

		SHC.drw_single_arrow = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_single_arrow;
}

GPUBatch *DRW_cache_empty_sphere_get(void)
{
	if (!SHC.drw_empty_sphere) {
		GPUVertBuf *vbo = sphere_wire_vbo(1.0f);
		SHC.drw_empty_sphere = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_empty_sphere;
}

GPUBatch *DRW_cache_empty_cone_get(void)
{
#define NSEGMENTS 8
	if (!SHC.drw_empty_cone) {
		/* a single ring of vertices */
		float p[NSEGMENTS][2];
		for (int i = 0; i < NSEGMENTS; ++i) {
			float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
			p[i][0] = cosf(angle);
			p[i][1] = sinf(angle);
		}

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, NSEGMENTS * 4);

		for (int i = 0; i < NSEGMENTS; ++i) {
			float cv[2], v[3];
			cv[0] = p[(i) % NSEGMENTS][0];
			cv[1] = p[(i) % NSEGMENTS][1];

			/* cone sides */
			ARRAY_SET_ITEMS(v, cv[0], 0.0f, cv[1]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 4, v);
			ARRAY_SET_ITEMS(v, 0.0f, 2.0f, 0.0f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 1, v);

			/* end ring */
			ARRAY_SET_ITEMS(v, cv[0], 0.0f, cv[1]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 2, v);
			cv[0] = p[(i + 1) % NSEGMENTS][0];
			cv[1] = p[(i + 1) % NSEGMENTS][1];
			ARRAY_SET_ITEMS(v, cv[0], 0.0f, cv[1]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 3, v);
		}

		SHC.drw_empty_cone = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_empty_cone;
#undef NSEGMENTS
}

GPUBatch *DRW_cache_empty_cylinder_get(void)
{
#define NSEGMENTS 12
	if (!SHC.drw_empty_cylinder) {
		/* a single ring of vertices */
		float p[NSEGMENTS][2];
		for (int i = 0; i < NSEGMENTS; ++i) {
			float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
			p[i][0] = cosf(angle);
			p[i][1] = sinf(angle);
		}

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, NSEGMENTS * 6);

		for (int i = 0; i < NSEGMENTS; ++i) {
			float cv[2], pv[2], v[3];
			cv[0] = p[(i) % NSEGMENTS][0];
			cv[1] = p[(i) % NSEGMENTS][1];
			pv[0] = p[(i + 1) % NSEGMENTS][0];
			pv[1] = p[(i + 1) % NSEGMENTS][1];

			/* cylinder sides */
			copy_v3_fl3(v, cv[0], cv[1], -1.0f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 6, v);
			copy_v3_fl3(v, cv[0], cv[1],  1.0f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 6 + 1, v);

			/* top ring */
			copy_v3_fl3(v, cv[0], cv[1],  1.0f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 6 + 2, v);
			copy_v3_fl3(v, pv[0], pv[1],  1.0f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 6 + 3, v);

			/* bottom ring */
			copy_v3_fl3(v, cv[0], cv[1], -1.0f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 6 + 4, v);
			copy_v3_fl3(v, pv[0], pv[1], -1.0f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 6 + 5, v);
		}

		SHC.drw_empty_cylinder = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_empty_cylinder;
#undef NSEGMENTS
}

GPUBatch *DRW_cache_empty_capsule_body_get(void)
{
	if (!SHC.drw_empty_capsule_body) {
		const float pos[8][3] = {
			{ 1.0f,  0.0f, 1.0f},
			{ 1.0f,  0.0f, 0.0f},
			{ 0.0f,  1.0f, 1.0f},
			{ 0.0f,  1.0f, 0.0f},
			{-1.0f,  0.0f, 1.0f},
			{-1.0f,  0.0f, 0.0f},
			{ 0.0f, -1.0f, 1.0f},
			{ 0.0f, -1.0f, 0.0f}
		};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 8);
		GPU_vertbuf_attr_fill(vbo, attr_id.pos, pos);

		SHC.drw_empty_capsule_body = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_empty_capsule_body;
}

GPUBatch *DRW_cache_empty_capsule_cap_get(void)
{
#define NSEGMENTS 24 /* Must be multiple of 2. */
	if (!SHC.drw_empty_capsule_cap) {
		/* a single ring of vertices */
		float p[NSEGMENTS][2];
		for (int i = 0; i < NSEGMENTS; ++i) {
			float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
			p[i][0] = cosf(angle);
			p[i][1] = sinf(angle);
		}

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, (NSEGMENTS * 2) * 2);

		/* Base circle */
		int vidx = 0;
		for (int i = 0; i < NSEGMENTS; ++i) {
			float v[3] = {0.0f, 0.0f, 0.0f};
			copy_v2_v2(v, p[(i) % NSEGMENTS]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
			copy_v2_v2(v, p[(i + 1) % NSEGMENTS]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
		}

		for (int i = 0; i < NSEGMENTS / 2; ++i) {
			float v[3] = {0.0f, 0.0f, 0.0f};
			int ci = i % NSEGMENTS;
			int pi = (i + 1) % NSEGMENTS;
			/* Y half circle */
			copy_v3_fl3(v, p[ci][0], 0.0f, p[ci][1]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
			copy_v3_fl3(v, p[pi][0], 0.0f, p[pi][1]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
			/* X half circle */
			copy_v3_fl3(v, 0.0f, p[ci][0], p[ci][1]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
			copy_v3_fl3(v, 0.0f, p[pi][0], p[pi][1]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
		}

		SHC.drw_empty_capsule_cap = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_empty_capsule_cap;
#undef NSEGMENTS
}

GPUBatch *DRW_cache_arrows_get(void)
{
	if (!SHC.drw_arrows) {
		GPUVertBuf *vbo = fill_arrows_vbo(1.0f);

		SHC.drw_arrows = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_arrows;
}

GPUBatch *DRW_cache_axis_names_get(void)
{
	if (!SHC.drw_axis_names) {
		const float size = 0.1f;
		float v1[3], v2[3];

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			/* Using 3rd component as axis indicator */
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		/* Line */
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 14);

		/* X */
		copy_v3_fl3(v1, -size,  size, 0.0f);
		copy_v3_fl3(v2,  size, -size, 0.0f);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 0, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 1, v2);

		copy_v3_fl3(v1,  size,  size, 0.0f);
		copy_v3_fl3(v2, -size, -size, 0.0f);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 2, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 3, v2);

		/* Y */
		copy_v3_fl3(v1, -size + 0.25f * size,  size, 1.0f);
		copy_v3_fl3(v2,  0.0f,  0.0f, 1.0f);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 4, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 5, v2);

		copy_v3_fl3(v1,  size - 0.25f * size,  size, 1.0f);
		copy_v3_fl3(v2, -size + 0.25f * size, -size, 1.0f);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 6, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 7, v2);

		/* Z */
		copy_v3_fl3(v1, -size,  size, 2.0f);
		copy_v3_fl3(v2,  size,  size, 2.0f);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 8, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 9, v2);

		copy_v3_fl3(v1,  size,  size, 2.0f);
		copy_v3_fl3(v2, -size, -size, 2.0f);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 10, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 11, v2);

		copy_v3_fl3(v1, -size, -size, 2.0f);
		copy_v3_fl3(v2,  size, -size, 2.0f);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 12, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 13, v2);

		SHC.drw_axis_names = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_axis_names;
}

GPUBatch *DRW_cache_image_plane_get(void)
{
	if (!SHC.drw_image_plane) {
		const float quad[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
		static GPUVertFormat format = { 0 };
		static struct { uint pos, texCoords; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			attr_id.texCoords = GPU_vertformat_attr_add(&format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 4);
		for (uint j = 0; j < 4; j++) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, j, quad[j]);
			GPU_vertbuf_attr_set(vbo, attr_id.texCoords, j, quad[j]);
		}
		SHC.drw_image_plane = GPU_batch_create_ex(GPU_PRIM_TRI_FAN, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_image_plane;
}

GPUBatch *DRW_cache_image_plane_wire_get(void)
{
	if (!SHC.drw_image_plane_wire) {
		const float quad[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 4);
		for (uint j = 0; j < 4; j++) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, j, quad[j]);
		}
		SHC.drw_image_plane_wire = GPU_batch_create_ex(GPU_PRIM_LINE_LOOP, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_image_plane_wire;
}

/* Force Field */
GPUBatch *DRW_cache_field_wind_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_field_wind) {
		float v[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2 * 4);

		for (int i = 0; i < 4; i++) {
			float z = 0.05f * (float)i;
			for (int a = 0; a < CIRCLE_RESOL; a++) {
				v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GPU_vertbuf_attr_set(vbo, attr_id.pos, i * CIRCLE_RESOL * 2 + a * 2, v);

				v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GPU_vertbuf_attr_set(vbo, attr_id.pos, i * CIRCLE_RESOL * 2 + a * 2 + 1, v);
			}
		}

		SHC.drw_field_wind = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_field_wind;
#undef CIRCLE_RESOL
}

GPUBatch *DRW_cache_field_force_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_field_force) {
		float v[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2 * 3);

		for (int i = 0; i < 3; i++) {
			float radius = 1.0f + 0.5f * (float)i;
			for (int a = 0; a < CIRCLE_RESOL; a++) {
				v[0] = radius * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[1] = radius * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[2] = 0.0f;
				GPU_vertbuf_attr_set(vbo, attr_id.pos, i * CIRCLE_RESOL * 2 + a * 2, v);

				v[0] = radius * sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[1] = radius * cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[2] = 0.0f;
				GPU_vertbuf_attr_set(vbo, attr_id.pos, i * CIRCLE_RESOL * 2 + a * 2 + 1, v);
			}
		}

		SHC.drw_field_force = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_field_force;
#undef CIRCLE_RESOL
}

GPUBatch *DRW_cache_field_vortex_get(void)
{
#define SPIRAL_RESOL 32
	if (!SHC.drw_field_vortex) {
		float v[3] = {0.0f, 0.0f, 0.0f};
		uint v_idx = 0;

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, SPIRAL_RESOL * 2 + 1);

		for (int a = SPIRAL_RESOL; a > -1; a--) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)SPIRAL_RESOL)) * (a / (float)SPIRAL_RESOL);
			v[1] = cosf((2.0f * M_PI * a) / ((float)SPIRAL_RESOL)) * (a / (float)SPIRAL_RESOL);

			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
		}

		for (int a = 1; a <= SPIRAL_RESOL; a++) {
			v[0] = -sinf((2.0f * M_PI * a) / ((float)SPIRAL_RESOL)) * (a / (float)SPIRAL_RESOL);
			v[1] = -cosf((2.0f * M_PI * a) / ((float)SPIRAL_RESOL)) * (a / (float)SPIRAL_RESOL);

			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
		}

		SHC.drw_field_vortex = GPU_batch_create_ex(GPU_PRIM_LINE_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_field_vortex;
#undef SPIRAL_RESOL
}

GPUBatch *DRW_cache_field_tube_limit_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_field_tube_limit) {
		float v[3] = {0.0f, 0.0f, 0.0f};
		uint v_idx = 0;

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2 * 2 + 8);

		/* Caps */
		for (int i = 0; i < 2; i++) {
			float z = (float)i * 2.0f - 1.0f;
			for (int a = 0; a < CIRCLE_RESOL; a++) {
				v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);

				v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
			}
		}
		/* Side Edges */
		for (int a = 0; a < 4; a++) {
			for (int i = 0; i < 2; i++) {
				float z = (float)i * 2.0f - 1.0f;
				v[0] = sinf((2.0f * M_PI * a) / 4.0f);
				v[1] = cosf((2.0f * M_PI * a) / 4.0f);
				v[2] = z;
				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
			}
		}

		SHC.drw_field_tube_limit = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_field_tube_limit;
#undef CIRCLE_RESOL
}

GPUBatch *DRW_cache_field_cone_limit_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_field_cone_limit) {
		float v[3] = {0.0f, 0.0f, 0.0f};
		uint v_idx = 0;

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2 * 2 + 8);

		/* Caps */
		for (int i = 0; i < 2; i++) {
			float z = (float)i * 2.0f - 1.0f;
			for (int a = 0; a < CIRCLE_RESOL; a++) {
				v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);

				v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
			}
		}
		/* Side Edges */
		for (int a = 0; a < 4; a++) {
			for (int i = 0; i < 2; i++) {
				float z = (float)i * 2.0f - 1.0f;
				v[0] = z * sinf((2.0f * M_PI * a) / 4.0f);
				v[1] = z * cosf((2.0f * M_PI * a) / 4.0f);
				v[2] = z;
				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
			}
		}

		SHC.drw_field_cone_limit = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_field_cone_limit;
#undef CIRCLE_RESOL
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Lamps
 * \{ */

GPUBatch *DRW_cache_lamp_get(void)
{
#define NSEGMENTS 8
	if (!SHC.drw_lamp) {
		float v[2];

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, NSEGMENTS * 2);

		for (int a = 0; a < NSEGMENTS * 2; a += 2) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)NSEGMENTS * 2));
			v[1] = cosf((2.0f * M_PI * a) / ((float)NSEGMENTS * 2));
			GPU_vertbuf_attr_set(vbo, attr_id.pos, a, v);

			v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)NSEGMENTS * 2));
			v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)NSEGMENTS * 2));
			GPU_vertbuf_attr_set(vbo, attr_id.pos, a + 1, v);
		}

		SHC.drw_lamp = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp;
#undef NSEGMENTS
}

GPUBatch *DRW_cache_lamp_shadows_get(void)
{
#define NSEGMENTS 10
	if (!SHC.drw_lamp_shadows) {
		float v[2];

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, NSEGMENTS * 2);

		for (int a = 0; a < NSEGMENTS * 2; a += 2) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)NSEGMENTS * 2));
			v[1] = cosf((2.0f * M_PI * a) / ((float)NSEGMENTS * 2));
			GPU_vertbuf_attr_set(vbo, attr_id.pos, a, v);

			v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)NSEGMENTS * 2));
			v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)NSEGMENTS * 2));
			GPU_vertbuf_attr_set(vbo, attr_id.pos, a + 1, v);
		}

		SHC.drw_lamp_shadows = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_shadows;
#undef NSEGMENTS
}

GPUBatch *DRW_cache_lamp_sunrays_get(void)
{
	if (!SHC.drw_lamp_sunrays) {
		float v[2], v1[2], v2[2];

		/* Position Only 2D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 32);

		for (int a = 0; a < 8; a++) {
			v[0] = sinf((2.0f * M_PI * a) / 8.0f);
			v[1] = cosf((2.0f * M_PI * a) / 8.0f);

			mul_v2_v2fl(v1, v, 1.6f);
			mul_v2_v2fl(v2, v, 1.9f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, a * 4, v1);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, a * 4 + 1, v2);

			mul_v2_v2fl(v1, v, 2.2f);
			mul_v2_v2fl(v2, v, 2.5f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, a * 4 + 2, v1);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, a * 4 + 3, v2);
		}

		SHC.drw_lamp_sunrays = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_sunrays;
}

GPUBatch *DRW_cache_lamp_area_square_get(void)
{
	if (!SHC.drw_lamp_area_square) {
		float v1[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 8);

		v1[0] = v1[1] = 0.5f;
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 0, v1);
		v1[0] = -0.5f;
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 1, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 2, v1);
		v1[1] = -0.5f;
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 3, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 4, v1);
		v1[0] = 0.5f;
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 5, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 6, v1);
		v1[1] = 0.5f;
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 7, v1);

		SHC.drw_lamp_area_square = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_area_square;
}

GPUBatch *DRW_cache_lamp_area_disk_get(void)
{
#define NSEGMENTS 32
	if (!SHC.drw_lamp_area_disk) {
		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 2 * NSEGMENTS);

		float v[3] = {0.0f, 0.5f, 0.0f};
		GPU_vertbuf_attr_set(vbo, attr_id.pos, 0, v);
		for (int a = 1; a < NSEGMENTS; a++) {
			v[0] = 0.5f * sinf(2.0f * (float)M_PI * a / NSEGMENTS);
			v[1] = 0.5f * cosf(2.0f * (float)M_PI * a / NSEGMENTS);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, 2 * a - 1, v);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, 2 * a, v);
		}
		copy_v3_fl3(v, 0.0f, 0.5f, 0.0f);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, (2 * NSEGMENTS) - 1, v);

		SHC.drw_lamp_area_disk = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_area_disk;
#undef NSEGMENTS
}

GPUBatch *DRW_cache_lamp_hemi_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_lamp_hemi) {
		float v[3];
		int vidx = 0;

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2 * 2 - 6 * 2 * 2);

		/* XZ plane */
		for (int a = 3; a < CIRCLE_RESOL / 2 - 3; a++) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL) - M_PI / 2);
			v[2] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL) - M_PI / 2) - 1.0f;
			v[1] = 0.0f;
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);

			v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL) - M_PI / 2);
			v[2] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL) - M_PI / 2) - 1.0f;
			v[1] = 0.0f;
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
		}

		/* XY plane */
		for (int a = 3; a < CIRCLE_RESOL / 2 - 3; a++) {
			v[2] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL)) - 1.0f;
			v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[0] = 0.0f;
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);

			v[2] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL)) - 1.0f;
			v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[0] = 0.0f;
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
		}

		/* YZ plane full circle */
		/* lease v[2] as it is */
		const float rad = cosf((2.0f * M_PI * 3) / ((float)CIRCLE_RESOL));
		for (int a = 0; a < CIRCLE_RESOL; a++) {
			v[1] = rad * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[0] = rad * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);

			v[1] = rad * sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[0] = rad * cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
		}


		SHC.drw_lamp_hemi = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_hemi;
#undef CIRCLE_RESOL
}


GPUBatch *DRW_cache_lamp_spot_get(void)
{
#define NSEGMENTS 32
	if (!SHC.drw_lamp_spot) {
		/* a single ring of vertices */
		float p[NSEGMENTS][2];
		float n[NSEGMENTS][3];
		float neg[NSEGMENTS][3];
		float half_angle = 2 * M_PI / ((float)NSEGMENTS * 2);
		for (int i = 0; i < NSEGMENTS; ++i) {
			float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
			p[i][0] = cosf(angle);
			p[i][1] = sinf(angle);

			n[i][0] = cosf(angle - half_angle);
			n[i][1] = sinf(angle - half_angle);
			n[i][2] = cosf(M_PI / 16.0f); /* slope of the cone */
			normalize_v3(n[i]); /* necessary ? */
			negate_v3_v3(neg[i], n[i]);
		}

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos, n1, n2; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			attr_id.n1 = GPU_vertformat_attr_add(&format, "N1", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			attr_id.n2 = GPU_vertformat_attr_add(&format, "N2", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, NSEGMENTS * 4);

		for (int i = 0; i < NSEGMENTS; ++i) {
			float cv[2], v[3];
			cv[0] = p[i % NSEGMENTS][0];
			cv[1] = p[i % NSEGMENTS][1];

			/* cone sides */
			ARRAY_SET_ITEMS(v, cv[0], cv[1], -1.0f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 4, v);
			ARRAY_SET_ITEMS(v, 0.0f, 0.0f, 0.0f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 1, v);

			GPU_vertbuf_attr_set(vbo, attr_id.n1, i * 4,     n[(i) % NSEGMENTS]);
			GPU_vertbuf_attr_set(vbo, attr_id.n1, i * 4 + 1, n[(i) % NSEGMENTS]);
			GPU_vertbuf_attr_set(vbo, attr_id.n2, i * 4,     n[(i + 1) % NSEGMENTS]);
			GPU_vertbuf_attr_set(vbo, attr_id.n2, i * 4 + 1, n[(i + 1) % NSEGMENTS]);

			/* end ring */
			ARRAY_SET_ITEMS(v, cv[0], cv[1], -1.0f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 2, v);
			cv[0] = p[(i + 1) % NSEGMENTS][0];
			cv[1] = p[(i + 1) % NSEGMENTS][1];
			ARRAY_SET_ITEMS(v, cv[0], cv[1], -1.0f);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 3, v);

			GPU_vertbuf_attr_set(vbo, attr_id.n1, i * 4 + 2, n[(i) % NSEGMENTS]);
			GPU_vertbuf_attr_set(vbo, attr_id.n1, i * 4 + 3, n[(i) % NSEGMENTS]);
			GPU_vertbuf_attr_set(vbo, attr_id.n2, i * 4 + 2, neg[(i) % NSEGMENTS]);
			GPU_vertbuf_attr_set(vbo, attr_id.n2, i * 4 + 3, neg[(i) % NSEGMENTS]);
		}

		SHC.drw_lamp_spot = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_spot;
#undef NSEGMENTS
}

GPUBatch *DRW_cache_lamp_spot_square_get(void)
{
	if (!SHC.drw_lamp_spot_square) {
		float p[5][3] = {{ 0.0f,  0.0f,  0.0f},
		                 { 1.0f,  1.0f, -1.0f},
		                 { 1.0f, -1.0f, -1.0f},
		                 {-1.0f, -1.0f, -1.0f},
		                 {-1.0f,  1.0f, -1.0f}};

		uint v_idx = 0;

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 16);

		/* piramid sides */
		for (int i = 1; i <= 4; ++i) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, p[0]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, p[i]);

			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, p[(i % 4) + 1]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, p[((i + 1) % 4) + 1]);
		}

		SHC.drw_lamp_spot_square = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_spot_square;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Speaker
 * \{ */

GPUBatch *DRW_cache_speaker_get(void)
{
	if (!SHC.drw_speaker) {
		float v[3];
		const int segments = 16;
		int vidx = 0;

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 3 * segments * 2 + 4 * 4);

		for (int j = 0; j < 3; j++) {
			float z = 0.25f * j - 0.125f;
			float r = (j == 0 ? 0.5f : 0.25f);

			copy_v3_fl3(v, r, 0.0f, z);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
			for (int i = 1; i < segments; i++) {
				float x = cosf(2.f * (float)M_PI * i / segments) * r;
				float y = sinf(2.f * (float)M_PI * i / segments) * r;
				copy_v3_fl3(v, x, y, z);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
			}
			copy_v3_fl3(v, r, 0.0f, z);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
		}

		for (int j = 0; j < 4; j++) {
			float x = (((j + 1) % 2) * (j - 1)) * 0.5f;
			float y = ((j % 2) * (j - 2)) * 0.5f;
			for (int i = 0; i < 3; i++) {
				if (i == 1) {
					x *= 0.5f;
					y *= 0.5f;
				}

				float z = 0.25f * i - 0.125f;
				copy_v3_fl3(v, x, y, z);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
				if (i == 1) {
					GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
				}
			}
		}

		SHC.drw_speaker = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_speaker;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Probe
 * \{ */

GPUBatch *DRW_cache_lightprobe_cube_get(void)
{
	if (!SHC.drw_lightprobe_cube) {
		int v_idx = 0;
		const float sin_pi_3 = 0.86602540378f;
		const float cos_pi_3 = 0.5f;
		float v[7][3] = {
			{0.0f, 1.0f, 0.0f},
			{sin_pi_3, cos_pi_3, 0.0f},
			{sin_pi_3, -cos_pi_3, 0.0f},
			{0.0f, -1.0f, 0.0f},
			{-sin_pi_3, -cos_pi_3, 0.0f},
			{-sin_pi_3, cos_pi_3, 0.0f},
			{0.0f, 0.0f, 0.0f},
		};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, (6 + 3) * 2);

		for (int i = 0; i < 6; ++i) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[i]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[(i + 1) % 6]);
		}

		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[1]);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[5]);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[3]);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		SHC.drw_lightprobe_cube = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_lightprobe_cube;
}

GPUBatch *DRW_cache_lightprobe_grid_get(void)
{
	if (!SHC.drw_lightprobe_grid) {
		int v_idx = 0;
		const float sin_pi_3 = 0.86602540378f;
		const float cos_pi_3 = 0.5f;
		const float v[7][3] = {
			{0.0f, 1.0f, 0.0f},
			{sin_pi_3, cos_pi_3, 0.0f},
			{sin_pi_3, -cos_pi_3, 0.0f},
			{0.0f, -1.0f, 0.0f},
			{-sin_pi_3, -cos_pi_3, 0.0f},
			{-sin_pi_3, cos_pi_3, 0.0f},
			{0.0f, 0.0f, 0.0f},
		};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, (6 * 2 + 3) * 2);

		for (int i = 0; i < 6; ++i) {
			float tmp_v1[3], tmp_v2[3], tmp_tr[3];
			copy_v3_v3(tmp_v1, v[i]);
			copy_v3_v3(tmp_v2, v[(i + 1) % 6]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, tmp_v1);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, tmp_v2);

			/* Internal wires. */
			for (int j = 1; j < 2; ++j) {
				mul_v3_v3fl(tmp_tr, v[(i / 2) * 2 + 1], -0.5f * j);
				add_v3_v3v3(tmp_v1, v[i], tmp_tr);
				add_v3_v3v3(tmp_v2, v[(i + 1) % 6], tmp_tr);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, tmp_v1);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, tmp_v2);
			}
		}

		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[1]);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[5]);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[3]);
		GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		SHC.drw_lightprobe_grid = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_lightprobe_grid;
}

GPUBatch *DRW_cache_lightprobe_planar_get(void)
{
	if (!SHC.drw_lightprobe_planar) {
		int v_idx = 0;
		const float sin_pi_3 = 0.86602540378f;
		float v[4][3] = {
			{0.0f, 0.5f, 0.0f},
			{sin_pi_3, 0.0f, 0.0f},
			{0.0f, -0.5f, 0.0f},
			{-sin_pi_3, 0.0f, 0.0f},
		};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 4 * 2);

		for (int i = 0; i < 4; ++i) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[i]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[(i + 1) % 4]);
		}

		SHC.drw_lightprobe_planar = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_lightprobe_planar;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Armature Bones
 * \{ */

static const float bone_octahedral_verts[6][3] = {
	{ 0.0f, 0.0f,  0.0f},
	{ 0.1f, 0.1f,  0.1f},
	{ 0.1f, 0.1f, -0.1f},
	{-0.1f, 0.1f, -0.1f},
	{-0.1f, 0.1f,  0.1f},
	{ 0.0f, 1.0f,  0.0f}
};

static const float bone_octahedral_smooth_normals[6][3] = {
	{ 0.0f, -1.0f,  0.0f},
#if 0 /* creates problems for outlines when scaled */
	{ 0.943608f * M_SQRT1_2, -0.331048f,  0.943608f * M_SQRT1_2},
	{ 0.943608f * M_SQRT1_2, -0.331048f, -0.943608f * M_SQRT1_2},
	{-0.943608f * M_SQRT1_2, -0.331048f, -0.943608f * M_SQRT1_2},
	{-0.943608f * M_SQRT1_2, -0.331048f,  0.943608f * M_SQRT1_2},
#else
	{ M_SQRT1_2, 0.0f,  M_SQRT1_2},
	{ M_SQRT1_2, 0.0f, -M_SQRT1_2},
	{-M_SQRT1_2, 0.0f, -M_SQRT1_2},
	{-M_SQRT1_2, 0.0f,  M_SQRT1_2},
#endif
	{ 0.0f,  1.0f,  0.0f}
};

#if 0  /* UNUSED */

static const uint bone_octahedral_wire[24] = {
	0, 1,  1, 5,  5, 3,  3, 0,
	0, 4,  4, 5,  5, 2,  2, 0,
	1, 2,  2, 3,  3, 4,  4, 1,
};

/* aligned with bone_octahedral_wire
 * Contains adjacent normal index */
static const uint bone_octahedral_wire_adjacent_face[24] = {
	0, 3,  4, 7,  5, 6,  1, 2,
	2, 3,  6, 7,  4, 5,  0, 1,
	0, 4,  1, 5,  2, 6,  3, 7,
};
#endif

static const uint bone_octahedral_solid_tris[8][3] = {
	{2, 1, 0}, /* bottom */
	{3, 2, 0},
	{4, 3, 0},
	{1, 4, 0},

	{5, 1, 2}, /* top */
	{5, 2, 3},
	{5, 3, 4},
	{5, 4, 1}
};

/**
 * Store indices of generated verts from bone_octahedral_solid_tris to define adjacency infos.
 * Example: triangle {2, 1, 0} is adjacent to {3, 2, 0}, {1, 4, 0} and {5, 1, 2}.
 * {2, 1, 0} becomes {0, 1, 2}
 * {3, 2, 0} becomes {3, 4, 5}
 * {1, 4, 0} becomes {9, 10, 11}
 * {5, 1, 2} becomes {12, 13, 14}
 * According to opengl specification it becomes (starting from
 * the first vertex of the first face aka. vertex 2):
 * {0, 12, 1, 10, 2, 3}
 **/
static const uint bone_octahedral_wire_lines_adjacency[12][4] = {
	{ 0, 1, 2,  6}, { 0, 12, 1,  6}, { 0, 3, 12,  6}, { 0, 2, 3,  6},
	{ 1, 6, 2,  3}, { 1, 12, 6,  3}, { 1, 0, 12,  3}, { 1, 2, 0,  3},
	{ 2, 0, 1, 12}, { 2,  3, 0, 12}, { 2, 6,  3, 12}, { 2, 1, 6, 12},
};

#if 0 /* UNUSED */
static const uint bone_octahedral_solid_tris_adjacency[8][6] = {
	{ 0, 12,  1, 10,  2,  3},
	{ 3, 15,  4,  1,  5,  6},
	{ 6, 18,  7,  4,  8,  9},
	{ 9, 21, 10,  7, 11,  0},

	{12, 22, 13,  2, 14, 17},
	{15, 13, 16,  5, 17, 20},
	{18, 16, 19,  8, 20, 23},
	{21, 19, 22, 11, 23, 14},
};
#endif

/* aligned with bone_octahedral_solid_tris */
static const float bone_octahedral_solid_normals[8][3] = {
	{ M_SQRT1_2,   -M_SQRT1_2,    0.00000000f},
	{-0.00000000f, -M_SQRT1_2,   -M_SQRT1_2},
	{-M_SQRT1_2,   -M_SQRT1_2,    0.00000000f},
	{ 0.00000000f, -M_SQRT1_2,    M_SQRT1_2},
	{ 0.99388373f,  0.11043154f, -0.00000000f},
	{ 0.00000000f,  0.11043154f, -0.99388373f},
	{-0.99388373f,  0.11043154f,  0.00000000f},
	{ 0.00000000f,  0.11043154f,  0.99388373f}
};

GPUBatch *DRW_cache_bone_octahedral_get(void)
{
	if (!SHC.drw_bone_octahedral) {
		uint v_idx = 0;

		static GPUVertFormat format = { 0 };
		static struct { uint pos, nor, snor; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			attr_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			attr_id.snor = GPU_vertformat_attr_add(&format, "snor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		/* Vertices */
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 24);

		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 3; ++j) {
				GPU_vertbuf_attr_set(vbo, attr_id.nor, v_idx, bone_octahedral_solid_normals[i]);
				GPU_vertbuf_attr_set(vbo, attr_id.snor, v_idx, bone_octahedral_smooth_normals[bone_octahedral_solid_tris[i][j]]);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, bone_octahedral_verts[bone_octahedral_solid_tris[i][j]]);
			}
		}

		SHC.drw_bone_octahedral = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL,
		                                              GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_octahedral;
}

GPUBatch *DRW_cache_bone_octahedral_wire_get(void)
{
	if (!SHC.drw_bone_octahedral_wire) {
		GPUIndexBufBuilder elb;
		GPU_indexbuf_init(&elb, GPU_PRIM_LINES_ADJ, 12, 24);

		for (int i = 0; i < 12; i++) {
			GPU_indexbuf_add_line_adj_verts(&elb,
			                                bone_octahedral_wire_lines_adjacency[i][0],
			                                bone_octahedral_wire_lines_adjacency[i][1],
			                                bone_octahedral_wire_lines_adjacency[i][2],
			                                bone_octahedral_wire_lines_adjacency[i][3]);
		}

		/* HACK Reuse vertex buffer. */
		GPUBatch *pos_nor_batch = DRW_cache_bone_octahedral_get();

		SHC.drw_bone_octahedral_wire = GPU_batch_create_ex(GPU_PRIM_LINES_ADJ, pos_nor_batch->verts[0], GPU_indexbuf_build(&elb),
		                                                   GPU_BATCH_OWNS_INDEX);
	}
	return SHC.drw_bone_octahedral_wire;
}

/* XXX TODO move that 1 unit cube to more common/generic place? */
static const float bone_box_verts[8][3] = {
	{ 1.0f, 0.0f,  1.0f},
	{ 1.0f, 0.0f, -1.0f},
	{-1.0f, 0.0f, -1.0f},
	{-1.0f, 0.0f,  1.0f},
	{ 1.0f, 1.0f,  1.0f},
	{ 1.0f, 1.0f, -1.0f},
	{-1.0f, 1.0f, -1.0f},
	{-1.0f, 1.0f,  1.0f}
};

static const float bone_box_smooth_normals[8][3] = {
	{ M_SQRT3, -M_SQRT3,  M_SQRT3},
	{ M_SQRT3, -M_SQRT3, -M_SQRT3},
	{-M_SQRT3, -M_SQRT3, -M_SQRT3},
	{-M_SQRT3, -M_SQRT3,  M_SQRT3},
	{ M_SQRT3,  M_SQRT3,  M_SQRT3},
	{ M_SQRT3,  M_SQRT3, -M_SQRT3},
	{-M_SQRT3,  M_SQRT3, -M_SQRT3},
	{-M_SQRT3,  M_SQRT3,  M_SQRT3},
};

#if 0 /* UNUSED */
static const uint bone_box_wire[24] = {
	0, 1,  1, 2,  2, 3,  3, 0,
	4, 5,  5, 6,  6, 7,  7, 4,
	0, 4,  1, 5,  2, 6,  3, 7,
};

/* aligned with bone_octahedral_wire
 * Contains adjacent normal index */
static const uint bone_box_wire_adjacent_face[24] = {
	0,  2,   0,  4,   1,  6,   1,  8,
	3, 10,   5, 10,   7, 11,   9, 11,
	3,  8,   2,  5,   4,  7,   6,  9,
};
#endif

static const uint bone_box_solid_tris[12][3] = {
	{0, 2, 1}, /* bottom */
	{0, 3, 2},

	{0, 1, 5}, /* sides */
	{0, 5, 4},

	{1, 2, 6},
	{1, 6, 5},

	{2, 3, 7},
	{2, 7, 6},

	{3, 0, 4},
	{3, 4, 7},

	{4, 5, 6}, /* top */
	{4, 6, 7},
};

/**
 * Store indices of generated verts from bone_box_solid_tris to define adjacency infos.
 * See bone_octahedral_solid_tris for more infos.
 **/
static const uint bone_box_wire_lines_adjacency[12][4] = {
	{ 4,  2,  0, 11}, { 0,  1, 2,  8}, { 2, 4,  1,  14}, {  1,  0,  4, 20}, /* bottom */
	{ 0,  8, 11, 14}, { 2, 14, 8, 20}, { 1, 20, 14, 11}, {  4, 11, 20,  8}, /* top */
	{ 20, 0, 11,  2}, { 11, 2, 8,  1}, { 8, 1,  14,  4}, { 14,  4, 20,  0}, /* sides */
};

#if 0 /* UNUSED */
static const uint bone_box_solid_tris_adjacency[12][6] = {
	{ 0,  5,  1, 14,  2,  8},
	{ 3, 26,  4, 20,  5,  1},

	{ 6,  2,  7, 16,  8, 11},
	{ 9,  7, 10, 32, 11, 24},

	{12,  0, 13, 22, 14, 17},
	{15, 13, 16, 30, 17,  6},

	{18,  3, 19, 28, 20, 23},
	{21, 19, 22, 33, 23, 12},

	{24,  4, 25, 10, 26, 29},
	{27, 25, 28, 34, 29, 18},

	{30,  9, 31, 15, 32, 35},
	{33, 31, 34, 21, 35, 27},
};
#endif

/* aligned with bone_box_solid_tris */
static const float bone_box_solid_normals[12][3] = {
	{ 0.0f, -1.0f,  0.0f},
	{ 0.0f, -1.0f,  0.0f},

	{ 1.0f,  0.0f,  0.0f},
	{ 1.0f,  0.0f,  0.0f},

	{ 0.0f,  0.0f, -1.0f},
	{ 0.0f,  0.0f, -1.0f},

	{-1.0f,  0.0f,  0.0f},
	{-1.0f,  0.0f,  0.0f},

	{ 0.0f,  0.0f,  1.0f},
	{ 0.0f,  0.0f,  1.0f},

	{ 0.0f,  1.0f,  0.0f},
	{ 0.0f,  1.0f,  0.0f},
};

GPUBatch *DRW_cache_bone_box_get(void)
{
	if (!SHC.drw_bone_box) {
		uint v_idx = 0;

		static GPUVertFormat format = { 0 };
		static struct { uint pos, nor, snor; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			attr_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			attr_id.snor = GPU_vertformat_attr_add(&format, "snor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		/* Vertices */
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 36);

		for (int i = 0; i < 12; i++) {
			for (int j = 0; j < 3; j++) {
				GPU_vertbuf_attr_set(vbo, attr_id.nor, v_idx, bone_box_solid_normals[i]);
				GPU_vertbuf_attr_set(vbo, attr_id.snor, v_idx, bone_box_smooth_normals[bone_box_solid_tris[i][j]]);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, bone_box_verts[bone_box_solid_tris[i][j]]);
			}
		}

		SHC.drw_bone_box = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL,
		                                       GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_box;
}

GPUBatch *DRW_cache_bone_box_wire_get(void)
{
	if (!SHC.drw_bone_box_wire) {
		GPUIndexBufBuilder elb;
		GPU_indexbuf_init(&elb, GPU_PRIM_LINES_ADJ, 12, 36);

		for (int i = 0; i < 12; i++) {
			GPU_indexbuf_add_line_adj_verts(&elb,
			                                bone_box_wire_lines_adjacency[i][0],
			                                bone_box_wire_lines_adjacency[i][1],
			                                bone_box_wire_lines_adjacency[i][2],
			                                bone_box_wire_lines_adjacency[i][3]);
		}

		/* HACK Reuse vertex buffer. */
		GPUBatch *pos_nor_batch = DRW_cache_bone_box_get();

		SHC.drw_bone_box_wire = GPU_batch_create_ex(GPU_PRIM_LINES_ADJ, pos_nor_batch->verts[0], GPU_indexbuf_build(&elb),
		                                                   GPU_BATCH_OWNS_INDEX);
	}
	return SHC.drw_bone_box_wire;
}

/* Helpers for envelope bone's solid sphere-with-hidden-equatorial-cylinder.
 * Note that here we only encode head/tail in forth component of the vector. */
static void benv_lat_lon_to_co(const float lat, const float lon, float r_nor[3])
{
	r_nor[0] = sinf(lat) * cosf(lon);
	r_nor[1] = sinf(lat) * sinf(lon);
	r_nor[2] = cosf(lat);
}

GPUBatch *DRW_cache_bone_envelope_solid_get(void)
{
	if (!SHC.drw_bone_envelope) {
		const int lon_res = 24;
		const int lat_res = 24;
		const float lon_inc = 2.0f * M_PI / lon_res;
		const float lat_inc = M_PI / lat_res;
		uint v_idx = 0;

		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		/* Vertices */
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, ((lat_res + 1) * 2) * lon_res * 1);

		float lon = 0.0f;
		for (int i = 0; i < lon_res; i++, lon += lon_inc) {
			float lat = 0.0f;
			float co1[3], co2[3];

			/* Note: the poles are duplicated on purpose, to restart the strip. */

			/* 1st sphere */
			for (int j = 0; j < lat_res; j++, lat += lat_inc) {
				benv_lat_lon_to_co(lat, lon,           co1);
				benv_lat_lon_to_co(lat, lon + lon_inc, co2);

				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, co1);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, co2);
			}

			/* Closing the loop */
			benv_lat_lon_to_co(M_PI, lon,           co1);
			benv_lat_lon_to_co(M_PI, lon + lon_inc, co2);

			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, co1);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, co2);
		}

		SHC.drw_bone_envelope = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_envelope;
}

GPUBatch *DRW_cache_bone_envelope_outline_get(void)
{
	if (!SHC.drw_bone_envelope_outline) {
#  define CIRCLE_RESOL 64
		float v0[2], v1[2], v2[2];
		const float radius = 1.0f;

		/* Position Only 2D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos0, pos1, pos2; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos0 = GPU_vertformat_attr_add(&format, "pos0", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			attr_id.pos1 = GPU_vertformat_attr_add(&format, "pos1", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			attr_id.pos2 = GPU_vertformat_attr_add(&format, "pos2", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, (CIRCLE_RESOL + 1) * 2);

		v0[0] = radius * sinf((2.0f * M_PI * -2) / ((float)CIRCLE_RESOL));
		v0[1] = radius * cosf((2.0f * M_PI * -2) / ((float)CIRCLE_RESOL));
		v1[0] = radius * sinf((2.0f * M_PI * -1) / ((float)CIRCLE_RESOL));
		v1[1] = radius * cosf((2.0f * M_PI * -1) / ((float)CIRCLE_RESOL));

		/* Output 4 verts for each position. See shader for explanation. */
		uint v = 0;
		for (int a = 0; a < CIRCLE_RESOL; a++) {
			v2[0] = radius * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v2[1] = radius * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			GPU_vertbuf_attr_set(vbo, attr_id.pos0, v,   v0);
			GPU_vertbuf_attr_set(vbo, attr_id.pos1, v,   v1);
			GPU_vertbuf_attr_set(vbo, attr_id.pos2, v++, v2);
			GPU_vertbuf_attr_set(vbo, attr_id.pos0, v,   v0);
			GPU_vertbuf_attr_set(vbo, attr_id.pos1, v,   v1);
			GPU_vertbuf_attr_set(vbo, attr_id.pos2, v++, v2);
			copy_v2_v2(v0, v1);
			copy_v2_v2(v1, v2);
		}
		v2[0] = 0.0f;
		v2[1] = radius;
		GPU_vertbuf_attr_set(vbo, attr_id.pos0, v,   v0);
		GPU_vertbuf_attr_set(vbo, attr_id.pos1, v,   v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos2, v++, v2);
		GPU_vertbuf_attr_set(vbo, attr_id.pos0, v,   v0);
		GPU_vertbuf_attr_set(vbo, attr_id.pos1, v,   v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos2, v++, v2);

		SHC.drw_bone_envelope_outline = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
#  undef CIRCLE_RESOL
	}
	return SHC.drw_bone_envelope_outline;
}

GPUBatch *DRW_cache_bone_point_get(void)
{
	if (!SHC.drw_bone_point) {
#if 0 /* old style geometry sphere */
		const int lon_res = 16;
		const int lat_res = 8;
		const float rad = 0.05f;
		const float lon_inc = 2 * M_PI / lon_res;
		const float lat_inc = M_PI / lat_res;
		uint v_idx = 0;

		static GPUVertFormat format = { 0 };
		static struct { uint pos, nor; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			attr_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		/* Vertices */
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, (lat_res - 1) * lon_res * 6);

		float lon = 0.0f;
		for (int i = 0; i < lon_res; i++, lon += lon_inc) {
			float lat = 0.0f;
			for (int j = 0; j < lat_res; j++, lat += lat_inc) {
				if (j != lat_res - 1) { /* Pole */
					add_lat_lon_vert(vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat + lat_inc, lon + lon_inc);
					add_lat_lon_vert(vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat + lat_inc, lon);
					add_lat_lon_vert(vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat,           lon);
				}

				if (j != 0) { /* Pole */
					add_lat_lon_vert(vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat,           lon + lon_inc);
					add_lat_lon_vert(vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat + lat_inc, lon + lon_inc);
					add_lat_lon_vert(vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat,           lon);
				}
			}
		}

		SHC.drw_bone_point = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
#else
#  define CIRCLE_RESOL 64
		float v[2];
		const float radius = 0.05f;

		/* Position Only 2D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL);

		for (int a = 0; a < CIRCLE_RESOL; a++) {
			v[0] = radius * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[1] = radius * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			GPU_vertbuf_attr_set(vbo, attr_id.pos, a, v);
		}

		SHC.drw_bone_point = GPU_batch_create_ex(GPU_PRIM_TRI_FAN, vbo, NULL, GPU_BATCH_OWNS_VBO);
#  undef CIRCLE_RESOL
#endif
	}
	return SHC.drw_bone_point;
}

GPUBatch *DRW_cache_bone_point_wire_outline_get(void)
{
	if (!SHC.drw_bone_point_wire) {
#if 0 /* old style geometry sphere */
		GPUVertBuf *vbo = sphere_wire_vbo(0.05f);
		SHC.drw_bone_point_wire = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
#else
#  define CIRCLE_RESOL 64
		float v0[2], v1[2];
		const float radius = 0.05f;

		/* Position Only 2D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos0, pos1; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos0 = GPU_vertformat_attr_add(&format, "pos0", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			attr_id.pos1 = GPU_vertformat_attr_add(&format, "pos1", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, (CIRCLE_RESOL + 1) * 2);

		v0[0] = radius * sinf((2.0f * M_PI * -1) / ((float)CIRCLE_RESOL));
		v0[1] = radius * cosf((2.0f * M_PI * -1) / ((float)CIRCLE_RESOL));

		uint v = 0;
		for (int a = 0; a < CIRCLE_RESOL; a++) {
			v1[0] = radius * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v1[1] = radius * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			GPU_vertbuf_attr_set(vbo, attr_id.pos0, v,   v0);
			GPU_vertbuf_attr_set(vbo, attr_id.pos1, v++, v1);
			GPU_vertbuf_attr_set(vbo, attr_id.pos0, v,   v0);
			GPU_vertbuf_attr_set(vbo, attr_id.pos1, v++, v1);
			copy_v2_v2(v0, v1);
		}
		v1[0] = 0.0f;
		v1[1] = radius;
		GPU_vertbuf_attr_set(vbo, attr_id.pos0, v,   v0);
		GPU_vertbuf_attr_set(vbo, attr_id.pos1, v++, v1);
		GPU_vertbuf_attr_set(vbo, attr_id.pos0, v,   v0);
		GPU_vertbuf_attr_set(vbo, attr_id.pos1, v++, v1);

		SHC.drw_bone_point_wire = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
#  undef CIRCLE_RESOL
#endif
	}
	return SHC.drw_bone_point_wire;
}

/* keep in sync with armature_stick_vert.glsl */
#define COL_WIRE (1 << 0)
#define COL_HEAD (1 << 1)
#define COL_TAIL (1 << 2)
#define COL_BONE (1 << 3)

#define POS_HEAD (1 << 4)
#define POS_TAIL (1 << 5)
#define POS_BONE (1 << 6)

GPUBatch *DRW_cache_bone_stick_get(void)
{
	if (!SHC.drw_bone_stick) {
#define CIRCLE_RESOL 12
		uint v = 0;
		uint flag;
		const float radius = 2.0f; /* head/tail radius */
		float pos[2];

		/* Position Only 2D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos, flag; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos  = GPU_vertformat_attr_add(&format, "pos",  GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			attr_id.flag = GPU_vertformat_attr_add(&format, "flag", GPU_COMP_U32, 1, GPU_FETCH_INT);
		}

		const uint vcount = (CIRCLE_RESOL + 1) * 2 + 6;

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, vcount);

		GPUIndexBufBuilder elb;
		GPU_indexbuf_init_ex(&elb, GPU_PRIM_TRI_FAN, (CIRCLE_RESOL + 2) * 2 + 6 + 2, vcount, true);

		/* head/tail points */
		for (int i = 0; i < 2; ++i) {
			/* center vertex */
			copy_v2_fl(pos, 0.0f);
			flag  = (i == 0) ? POS_HEAD : POS_TAIL;
			flag |= (i == 0) ? COL_HEAD : COL_TAIL;
			GPU_vertbuf_attr_set(vbo, attr_id.pos,  v, pos);
			GPU_vertbuf_attr_set(vbo, attr_id.flag, v, &flag);
			GPU_indexbuf_add_generic_vert(&elb, v++);
			/* circle vertices */
			flag |= COL_WIRE;
			for (int a = 0; a < CIRCLE_RESOL; a++) {
				pos[0] = radius * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				pos[1] = radius * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				GPU_vertbuf_attr_set(vbo, attr_id.pos,  v, pos);
				GPU_vertbuf_attr_set(vbo, attr_id.flag, v, &flag);
				GPU_indexbuf_add_generic_vert(&elb, v++);
			}
			/* Close the circle */
			GPU_indexbuf_add_generic_vert(&elb, v - CIRCLE_RESOL);

			GPU_indexbuf_add_primitive_restart(&elb);
		}

		/* Bone rectangle */
		pos[0] = 0.0f;
		for (int i = 0; i < 6; ++i) {
			pos[1] = (i == 0 || i == 3) ? 0.0f : ((i < 3) ? 1.0f : -1.0f);
			flag   = ((i <  2 || i >  4) ? POS_HEAD : POS_TAIL) |
			         ((i == 0 || i == 3) ? 0 : COL_WIRE) | COL_BONE | POS_BONE;
			GPU_vertbuf_attr_set(vbo, attr_id.pos,  v, pos);
			GPU_vertbuf_attr_set(vbo, attr_id.flag, v, &flag);
			GPU_indexbuf_add_generic_vert(&elb, v++);
		}

		SHC.drw_bone_stick = GPU_batch_create_ex(GPU_PRIM_TRI_FAN, vbo, GPU_indexbuf_build(&elb),
		                                         GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
#undef CIRCLE_RESOL
	}
	return SHC.drw_bone_stick;
}

static void set_bone_axis_vert(
        GPUVertBuf *vbo, uint axis, uint pos, uint col,
        uint *v, const float *a, const float *p, const float *c)
{
	GPU_vertbuf_attr_set(vbo, axis, *v, a);
	GPU_vertbuf_attr_set(vbo, pos,  *v, p);
	GPU_vertbuf_attr_set(vbo, col,  *v, c);
	*v += 1;
}

#define S_X 0.0215f
#define S_Y 0.025f
static float x_axis_name[4][2] = {
	{ 0.9f * S_X,  1.0f * S_Y}, {-1.0f * S_X, -1.0f * S_Y},
	{-0.9f * S_X,  1.0f * S_Y}, { 1.0f * S_X, -1.0f * S_Y}
};
#define X_LEN (sizeof(x_axis_name) / (sizeof(float) * 2))
#undef S_X
#undef S_Y

#define S_X 0.0175f
#define S_Y 0.025f
static float y_axis_name[6][2] = {
	{-1.0f * S_X,  1.0f * S_Y}, { 0.0f * S_X, -0.1f * S_Y},
	{ 1.0f * S_X,  1.0f * S_Y}, { 0.0f * S_X, -0.1f * S_Y},
	{ 0.0f * S_X, -0.1f * S_Y}, { 0.0f * S_X, -1.0f * S_Y}
};
#define Y_LEN (sizeof(y_axis_name) / (sizeof(float) * 2))
#undef S_X
#undef S_Y

#define S_X 0.02f
#define S_Y 0.025f
static float z_axis_name[10][2] = {
	{-0.95f * S_X,  1.00f * S_Y}, { 0.95f * S_X,  1.00f * S_Y},
	{ 0.95f * S_X,  1.00f * S_Y}, { 0.95f * S_X,  0.90f * S_Y},
	{ 0.95f * S_X,  0.90f * S_Y}, {-1.00f * S_X, -0.90f * S_Y},
	{-1.00f * S_X, -0.90f * S_Y}, {-1.00f * S_X, -1.00f * S_Y},
	{-1.00f * S_X, -1.00f * S_Y}, { 1.00f * S_X, -1.00f * S_Y}
};
#define Z_LEN (sizeof(z_axis_name) / (sizeof(float) * 2))
#undef S_X
#undef S_Y

#define S_X 0.007f
#define S_Y 0.007f
static float axis_marker[8][2] = {
#if 0 /* square */
	{-1.0f * S_X,  1.0f * S_Y}, { 1.0f * S_X,  1.0f * S_Y},
	{ 1.0f * S_X,  1.0f * S_Y}, { 1.0f * S_X, -1.0f * S_Y},
	{ 1.0f * S_X, -1.0f * S_Y}, {-1.0f * S_X, -1.0f * S_Y},
	{-1.0f * S_X, -1.0f * S_Y}, {-1.0f * S_X,  1.0f * S_Y}
#else /* diamond */
	{-S_X,  0.f}, { 0.f,  S_Y},
	{ 0.f,  S_Y}, { S_X,  0.f},
	{ S_X,  0.f}, { 0.f, -S_Y},
	{ 0.f, -S_Y}, {-S_X,  0.f}
#endif
};
#define MARKER_LEN (sizeof(axis_marker) / (sizeof(float) * 2))
#define MARKER_FILL_LAYER 6
#undef S_X
#undef S_Y

#define S_X 0.0007f
#define S_Y 0.0007f
#define O_X  0.001f
#define O_Y -0.001f
static float axis_name_shadow[8][2] = {
	{-S_X + O_X,  S_Y + O_Y}, { S_X + O_X,  S_Y + O_Y},
	{ S_X + O_X,  S_Y + O_Y}, { S_X + O_X, -S_Y + O_Y},
	{ S_X + O_X, -S_Y + O_Y}, {-S_X + O_X, -S_Y + O_Y},
	{-S_X + O_X, -S_Y + O_Y}, {-S_X + O_X,  S_Y + O_Y}
};
// #define SHADOW_RES (sizeof(axis_name_shadow) / (sizeof(float) * 2))
#define SHADOW_RES 0
#undef O_X
#undef O_Y
#undef S_X
#undef S_Y

GPUBatch *DRW_cache_bone_arrows_get(void)
{
	if (!SHC.drw_bone_arrows) {
		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint axis, pos, col; } attr_id;
		if (format.attr_len == 0) {
			attr_id.axis = GPU_vertformat_attr_add(&format, "axis", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
			attr_id.pos = GPU_vertformat_attr_add(&format, "screenPos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			attr_id.col = GPU_vertformat_attr_add(&format, "colorAxis", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		/* Line */
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, (2 + MARKER_LEN * MARKER_FILL_LAYER) * 3 +
		                            (X_LEN + Y_LEN + Z_LEN) * (1 + SHADOW_RES));

		uint v = 0;

		for (int axis = 0; axis < 3; axis++) {
			float pos[2] = {0.0f, 0.0f};
			float c[3] = {0.0f, 0.0f, 0.0f};
			float a = 0.0f;
			/* center to axis line */
			set_bone_axis_vert(vbo, attr_id.axis, attr_id.pos, attr_id.col, &v, &a, pos, c);
			c[axis] = 0.5f;
			a = axis + 0.25f;
			set_bone_axis_vert(vbo, attr_id.axis, attr_id.pos, attr_id.col, &v, &a, pos, c);

			/* Axis end marker */
			for (int j = 1; j < MARKER_FILL_LAYER + 1; ++j) {
				for (int i = 0; i < MARKER_LEN; ++i) {
					float tmp[2];
					mul_v2_v2fl(tmp, axis_marker[i], j / (float)MARKER_FILL_LAYER);
					set_bone_axis_vert(vbo, attr_id.axis, attr_id.pos, attr_id.col,
					                   &v, &a, tmp, c);
				}
			}

			a = axis + 0.31f;
			/* Axis name */
			int axis_v_len;
			float (*axis_verts)[2];
			if (axis == 0) {
				axis_verts = x_axis_name;
				axis_v_len = X_LEN;
			}
			else if (axis == 1) {
				axis_verts = y_axis_name;
				axis_v_len = Y_LEN;
			}
			else {
				axis_verts = z_axis_name;
				axis_v_len = Z_LEN;
			}

			/* Axis name shadows */
			copy_v3_fl(c, 0.0f);
			c[axis] = 0.3f;
			for (int j = 0; j < SHADOW_RES; ++j) {
				for (int i = 0; i < axis_v_len; ++i) {
					float tmp[2];
					add_v2_v2v2(tmp, axis_verts[i], axis_name_shadow[j]);
					set_bone_axis_vert(vbo, attr_id.axis, attr_id.pos, attr_id.col,
					                   &v, &a, tmp, c);
				}
			}

			/* Axis name */
			copy_v3_fl(c, 0.1f);
			c[axis] = 1.0f;
			for (int i = 0; i < axis_v_len; ++i) {
				set_bone_axis_vert(vbo, attr_id.axis, attr_id.pos, attr_id.col,
				                   &v, &a, axis_verts[i], c);
			}
		}

		SHC.drw_bone_arrows = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_arrows;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Camera
 * \{ */

/**
 * We could make these more generic functions.
 * although filling 1d lines is not common.
 *
 * \note Use x coordinate to identify the vertex the vertex shader take care to place it appropriately.
 */

static const float camera_coords_frame_bounds[5] = {
	0.0f, /* center point */
	1.0f, /* + X + Y */
	2.0f, /* + X - Y */
	3.0f, /* - X - Y */
	4.0f, /* - X + Y */
};

static const float camera_coords_frame_tri[3] = {
	5.0f, /* tria + X */
	6.0f, /* tria - X */
	7.0f, /* tria + Y */
};

/** Draw a loop of lines. */
static void camera_fill_lines_loop_fl_v1(
        GPUVertBufRaw *pos_step,
        const float *coords, const uint coords_len)
{
	for (uint i = 0, i_prev = coords_len - 1; i < coords_len; i_prev = i++) {
		*((float *)GPU_vertbuf_raw_step(pos_step)) = coords[i_prev];
		*((float *)GPU_vertbuf_raw_step(pos_step)) = coords[i];
	}
}

/** Fan lines out from the first vertex. */
static void camera_fill_lines_fan_fl_v1(
        GPUVertBufRaw *pos_step,
        const float *coords, const uint coords_len)
{
	for (uint i = 1; i < coords_len; i++) {
		*((float *)GPU_vertbuf_raw_step(pos_step)) = coords[0];
		*((float *)GPU_vertbuf_raw_step(pos_step)) = coords[i];
	}
}

/** Simply fill the array. */
static void camera_fill_array_fl_v1(
        GPUVertBufRaw *pos_step,
        const float *coords, const uint coords_len)
{
	for (uint i = 0; i < coords_len; i++) {
		*((float *)GPU_vertbuf_raw_step(pos_step)) = coords[i];
	}
}


GPUBatch *DRW_cache_camera_get(void)
{
	if (!SHC.drw_camera) {
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
		}

		/* Vertices */
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		const int vbo_len_capacity = 22;
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);
		GPUVertBufRaw pos_step;
		GPU_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);

		/* camera cone (from center to frame) */
		camera_fill_lines_fan_fl_v1(&pos_step, camera_coords_frame_bounds, ARRAY_SIZE(camera_coords_frame_bounds));

		/* camera frame (skip center) */
		camera_fill_lines_loop_fl_v1(&pos_step, &camera_coords_frame_bounds[1], ARRAY_SIZE(camera_coords_frame_bounds) - 1);

		/* camera triangle (above the frame) */
		camera_fill_lines_loop_fl_v1(&pos_step, camera_coords_frame_tri, ARRAY_SIZE(camera_coords_frame_tri));

		BLI_assert(vbo_len_capacity == GPU_vertbuf_raw_used(&pos_step));

		SHC.drw_camera = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_camera;
}

GPUBatch *DRW_cache_camera_frame_get(void)
{
	if (!SHC.drw_camera_frame) {

		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
		}

		/* Vertices */
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		const int vbo_len_capacity = 8;
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);
		GPUVertBufRaw pos_step;
		GPU_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);

		/* camera frame (skip center) */
		camera_fill_lines_loop_fl_v1(&pos_step, &camera_coords_frame_bounds[1], ARRAY_SIZE(camera_coords_frame_bounds) - 1);

		BLI_assert(vbo_len_capacity == GPU_vertbuf_raw_used(&pos_step));

		SHC.drw_camera_frame = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_camera_frame;
}

GPUBatch *DRW_cache_camera_tria_get(void)
{
	if (!SHC.drw_camera_tria) {
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
		}

		/* Vertices */
		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		const int vbo_len_capacity = 3;
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);
		GPUVertBufRaw pos_step;
		GPU_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);

		/* camera triangle (above the frame) */
		camera_fill_array_fl_v1(&pos_step, camera_coords_frame_tri, ARRAY_SIZE(camera_coords_frame_tri));

		BLI_assert(vbo_len_capacity == GPU_vertbuf_raw_used(&pos_step));

		SHC.drw_camera_tria = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_camera_tria;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Object Mode Helpers
 * \{ */

/* Object Center */
GPUBatch *DRW_cache_single_vert_get(void)
{
	if (!SHC.drw_single_vertice) {
		float v1[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 1);

		GPU_vertbuf_attr_set(vbo, attr_id.pos, 0, v1);

		SHC.drw_single_vertice = GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_single_vertice;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Meshes
 * \{ */

GPUBatch *DRW_cache_mesh_surface_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);
	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_all_triangles(me);
}

void DRW_cache_mesh_wire_overlay_get(
        Object *ob,
        GPUBatch **r_tris, GPUBatch **r_ledges, GPUBatch **r_lverts)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;

	*r_tris = DRW_mesh_batch_cache_get_overlay_triangles(me);
	*r_ledges = DRW_mesh_batch_cache_get_overlay_loose_edges(me);
	*r_lverts = DRW_mesh_batch_cache_get_overlay_loose_verts(me);
}

void DRW_cache_mesh_normals_overlay_get(
        Object *ob,
        GPUBatch **r_tris, GPUBatch **r_ledges, GPUBatch **r_lverts)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;

	*r_tris = DRW_mesh_batch_cache_get_overlay_triangles_nor(me);
	*r_ledges = DRW_mesh_batch_cache_get_overlay_loose_edges_nor(me);
	*r_lverts = DRW_mesh_batch_cache_get_overlay_loose_verts(me);
}

GPUBatch *DRW_cache_face_centers_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;

	return DRW_mesh_batch_cache_get_overlay_facedots(me);
}

GPUBatch *DRW_cache_mesh_wire_outline_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_fancy_edges(me);
}

GPUBatch *DRW_cache_mesh_edge_detection_get(Object *ob, bool *r_is_manifold)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_edge_detection(me, r_is_manifold);
}

GPUBatch *DRW_cache_mesh_surface_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_triangles_with_normals(me);
}

GPUBatch *DRW_cache_mesh_loose_edges_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_loose_edges_with_normals(me);
}

GPUBatch *DRW_cache_mesh_surface_weights_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_triangles_with_normals_and_weights(me, ob->actdef - 1);
}

GPUBatch *DRW_cache_mesh_surface_vert_colors_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_triangles_with_normals_and_vert_colors(me);
}

/* Return list of batches */
GPUBatch **DRW_cache_mesh_surface_shaded_get(
        Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len,
        char **auto_layer_names, int **auto_layer_is_srgb, int *auto_layer_count)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_surface_shaded(me, gpumat_array, gpumat_array_len,
	                                               auto_layer_names, auto_layer_is_srgb, auto_layer_count);
}

/* Return list of batches */
GPUBatch **DRW_cache_mesh_surface_texpaint_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_surface_texpaint(me);
}

GPUBatch *DRW_cache_mesh_surface_texpaint_single_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_surface_texpaint_single(me);
}

GPUBatch *DRW_cache_mesh_surface_verts_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_points_with_normals(me);
}

GPUBatch *DRW_cache_mesh_edges_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_all_edges(me);
}

GPUBatch *DRW_cache_mesh_verts_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_all_verts(me);
}

GPUBatch *DRW_cache_mesh_edges_paint_overlay_get(Object *ob, bool use_wire, bool use_sel)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_weight_overlay_edges(me, use_wire, use_sel);
}

GPUBatch *DRW_cache_mesh_faces_weight_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_weight_overlay_faces(me);
}

GPUBatch *DRW_cache_mesh_verts_weight_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_weight_overlay_verts(me);
}

void DRW_cache_mesh_sculpt_coords_ensure(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	DRW_mesh_cache_sculpt_coords_ensure(me);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Curve
 * \{ */

GPUBatch *DRW_cache_curve_edge_wire_get(Object *ob)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_wire_edge(cu, ob->runtime.curve_cache);
}

GPUBatch *DRW_cache_curve_edge_normal_get(Object *ob, float normal_size)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_normal_edge(cu, ob->runtime.curve_cache, normal_size);
}

GPUBatch *DRW_cache_curve_edge_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_overlay_edges(cu);
}

GPUBatch *DRW_cache_curve_vert_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_overlay_verts(cu);
}

GPUBatch *DRW_cache_curve_surface_get(Object *ob)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_triangles_with_normals(cu, ob->runtime.curve_cache);
}

/* Return list of batches */
GPUBatch **DRW_cache_curve_surface_shaded_get(
        Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_surface_shaded(cu, ob->runtime.curve_cache, gpumat_array, gpumat_array_len);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name MetaBall
 * \{ */

GPUBatch *DRW_cache_mball_surface_get(Object *ob)
{
	BLI_assert(ob->type == OB_MBALL);
	return DRW_metaball_batch_cache_get_triangles_with_normals(ob);
}

GPUBatch **DRW_cache_mball_surface_shaded_get(
        Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len)
{
	BLI_assert(ob->type == OB_MBALL);
	MetaBall *mb = ob->data;
	return DRW_metaball_batch_cache_get_surface_shaded(ob, mb, gpumat_array, gpumat_array_len);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Font
 * \{ */

GPUBatch *DRW_cache_text_edge_wire_get(Object *ob)
{
	BLI_assert(ob->type == OB_FONT);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_wire_edge(cu, ob->runtime.curve_cache);
}

GPUBatch *DRW_cache_text_surface_get(Object *ob)
{
	BLI_assert(ob->type == OB_FONT);
	struct Curve *cu = ob->data;
	if (cu->editfont && (cu->flag & CU_FAST)) {
		return NULL;
	}
	return DRW_curve_batch_cache_get_triangles_with_normals(cu, ob->runtime.curve_cache);
}

GPUBatch **DRW_cache_text_surface_shaded_get(
        Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len)
{
	BLI_assert(ob->type == OB_FONT);
	struct Curve *cu = ob->data;
	if (cu->editfont && (cu->flag & CU_FAST)) {
		return NULL;
	}
	return DRW_curve_batch_cache_get_surface_shaded(cu, ob->runtime.curve_cache, gpumat_array, gpumat_array_len);
}

GPUBatch *DRW_cache_text_cursor_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_FONT);
	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_overlay_cursor(cu);
}

GPUBatch *DRW_cache_text_select_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_FONT);
	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_overlay_select(cu);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Surface
 * \{ */

GPUBatch *DRW_cache_surf_surface_get(Object *ob)
{
	BLI_assert(ob->type == OB_SURF);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_triangles_with_normals(cu, ob->runtime.curve_cache);
}

/* Return list of batches */
GPUBatch **DRW_cache_surf_surface_shaded_get(
        Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len)
{
	BLI_assert(ob->type == OB_SURF);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_surface_shaded(cu, ob->runtime.curve_cache, gpumat_array, gpumat_array_len);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Lattice
 * \{ */

GPUBatch *DRW_cache_lattice_verts_get(Object *ob)
{
	BLI_assert(ob->type == OB_LATTICE);

	struct Lattice *lt = ob->data;
	return DRW_lattice_batch_cache_get_all_verts(lt);
}

GPUBatch *DRW_cache_lattice_wire_get(Object *ob, bool use_weight)
{
	BLI_assert(ob->type == OB_LATTICE);

	Lattice *lt = ob->data;
	int actdef = -1;

	if (use_weight && ob->defbase.first && lt->editlatt->latt->dvert) {
		actdef = ob->actdef - 1;
	}

	return DRW_lattice_batch_cache_get_all_edges(lt, use_weight, actdef);
}

GPUBatch *DRW_cache_lattice_vert_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_LATTICE);

	struct Lattice *lt = ob->data;
	return DRW_lattice_batch_cache_get_overlay_verts(lt);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Particles
 * \{ */

GPUBatch *DRW_cache_particles_get_hair(Object *object, ParticleSystem *psys, ModifierData *md)
{
	return DRW_particles_batch_cache_get_hair(object, psys, md);
}

GPUBatch *DRW_cache_particles_get_dots(Object *object, ParticleSystem *psys)
{
	return DRW_particles_batch_cache_get_dots(object, psys);
}

GPUBatch *DRW_cache_particles_get_edit_strands(
        Object *object,
        ParticleSystem *psys,
        struct PTCacheEdit *edit)
{
	return DRW_particles_batch_cache_get_edit_strands(object, psys, edit);
}

GPUBatch *DRW_cache_particles_get_edit_inner_points(
        Object *object,
        ParticleSystem *psys,
        struct PTCacheEdit *edit)
{
	return DRW_particles_batch_cache_get_edit_inner_points(object, psys, edit);
}

GPUBatch *DRW_cache_particles_get_edit_tip_points(
        Object *object,
        ParticleSystem *psys,
        struct PTCacheEdit *edit)
{
	return DRW_particles_batch_cache_get_edit_tip_points(object, psys, edit);
}

GPUBatch *DRW_cache_particles_get_prim(int type)
{
	switch (type) {
		case PART_DRAW_CROSS:
			if (!SHC.drw_particle_cross) {
				static GPUVertFormat format = { 0 };
				static uint pos_id, axis_id;

				if (format.attr_len == 0) {
					pos_id = GPU_vertformat_attr_add(&format, "inst_pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
					axis_id = GPU_vertformat_attr_add(&format, "axis", GPU_COMP_I32, 1, GPU_FETCH_INT);
				}

				GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
				GPU_vertbuf_data_alloc(vbo, 6);

				/* X axis */
				float co[3] = {-1.0f, 0.0f, 0.0f};
				int axis = -1;
				GPU_vertbuf_attr_set(vbo, pos_id, 0, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 0, &axis);

				co[0] = 1.0f;
				GPU_vertbuf_attr_set(vbo, pos_id, 1, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 1, &axis);

				/* Y axis */
				co[0] = 0.0f;
				co[1] = -1.0f;
				GPU_vertbuf_attr_set(vbo, pos_id, 2, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 2, &axis);

				co[1] = 1.0f;
				GPU_vertbuf_attr_set(vbo, pos_id, 3, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 3, &axis);

				/* Z axis */
				co[1] = 0.0f;
				co[2] = -1.0f;
				GPU_vertbuf_attr_set(vbo, pos_id, 4, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 4, &axis);

				co[2] = 1.0f;
				GPU_vertbuf_attr_set(vbo, pos_id, 5, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 5, &axis);

				SHC.drw_particle_cross = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
			}

			return SHC.drw_particle_cross;
		case PART_DRAW_AXIS:
			if (!SHC.drw_particle_axis) {
				static GPUVertFormat format = { 0 };
				static uint pos_id, axis_id;

				if (format.attr_len == 0) {
					pos_id = GPU_vertformat_attr_add(&format, "inst_pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
					axis_id = GPU_vertformat_attr_add(&format, "axis", GPU_COMP_I32, 1, GPU_FETCH_INT);
				}

				GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
				GPU_vertbuf_data_alloc(vbo, 6);

				/* X axis */
				float co[3] = {0.0f, 0.0f, 0.0f};
				int axis = 0;
				GPU_vertbuf_attr_set(vbo, pos_id, 0, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 0, &axis);

				co[0] = 1.0f;
				GPU_vertbuf_attr_set(vbo, pos_id, 1, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 1, &axis);

				/* Y axis */
				co[0] = 0.0f;
				axis = 1;
				GPU_vertbuf_attr_set(vbo, pos_id, 2, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 2, &axis);

				co[1] = 1.0f;
				GPU_vertbuf_attr_set(vbo, pos_id, 3, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 3, &axis);

				/* Z axis */
				co[1] = 0.0f;
				axis = 2;
				GPU_vertbuf_attr_set(vbo, pos_id, 4, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 4, &axis);

				co[2] = 1.0f;
				GPU_vertbuf_attr_set(vbo, pos_id, 5, co);
				GPU_vertbuf_attr_set(vbo, axis_id, 5, &axis);

				SHC.drw_particle_axis = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
			}

			return SHC.drw_particle_axis;
		case PART_DRAW_CIRC:
#define CIRCLE_RESOL 32
			if (!SHC.drw_particle_circle) {
				float v[3] = {0.0f, 0.0f, 0.0f};
				int axis = -1;

				static GPUVertFormat format = { 0 };
				static uint pos_id, axis_id;

				if (format.attr_len == 0) {
					pos_id = GPU_vertformat_attr_add(&format, "inst_pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
					axis_id = GPU_vertformat_attr_add(&format, "axis", GPU_COMP_I32, 1, GPU_FETCH_INT);
				}

				GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
				GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL);

				for (int a = 0; a < CIRCLE_RESOL; a++) {
					v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
					v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
					v[2] = 0.0f;
					GPU_vertbuf_attr_set(vbo, pos_id, a, v);
					GPU_vertbuf_attr_set(vbo, axis_id, a, &axis);
				}

				SHC.drw_particle_circle = GPU_batch_create_ex(GPU_PRIM_LINE_LOOP, vbo, NULL, GPU_BATCH_OWNS_VBO);
			}

			return SHC.drw_particle_circle;
#undef CIRCLE_RESOL
		default:
			BLI_assert(false);
			break;
	}

	return NULL;
}

/* 3D cursor */
GPUBatch *DRW_cache_cursor_get(bool crosshair_lines)
{
	GPUBatch **drw_cursor = crosshair_lines ? &SHC.drw_cursor : &SHC.drw_cursor_only_circle;

	if (*drw_cursor == NULL) {
		const float f5 = 0.25f;
		const float f10 = 0.5f;
		const float f20 = 1.0f;

		const int segments = 16;
		const int vert_len = segments + 8;
		const int index_len = vert_len + 5;

		uchar red[3] = {255, 0, 0};
		uchar white[3] = {255, 255, 255};

		static GPUVertFormat format = { 0 };
		static struct { uint pos, color; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			attr_id.color = GPU_vertformat_attr_add(&format, "color", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
		}

		GPUIndexBufBuilder elb;
		GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, index_len, vert_len, true);

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, vert_len);

		int v = 0;
		for (int i = 0; i < segments; ++i) {
			float angle = (float)(2 * M_PI) * ((float)i / (float)segments);
			float x = f10 * cosf(angle);
			float y = f10 * sinf(angle);

			if (i % 2 == 0)
				GPU_vertbuf_attr_set(vbo, attr_id.color, v, red);
			else
				GPU_vertbuf_attr_set(vbo, attr_id.color, v, white);

			GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){x, y});
			GPU_indexbuf_add_generic_vert(&elb, v++);
		}
		GPU_indexbuf_add_generic_vert(&elb, 0);

		if (crosshair_lines) {
			uchar crosshair_color[3];
			UI_GetThemeColor3ubv(TH_VIEW_OVERLAY, crosshair_color);

			GPU_indexbuf_add_primitive_restart(&elb);

			GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){-f20, 0});
			GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
			GPU_indexbuf_add_generic_vert(&elb, v++);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){-f5, 0});
			GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
			GPU_indexbuf_add_generic_vert(&elb, v++);

			GPU_indexbuf_add_primitive_restart(&elb);

			GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){+f5, 0});
			GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
			GPU_indexbuf_add_generic_vert(&elb, v++);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){+f20, 0});
			GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
			GPU_indexbuf_add_generic_vert(&elb, v++);

			GPU_indexbuf_add_primitive_restart(&elb);

			GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){0, -f20});
			GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
			GPU_indexbuf_add_generic_vert(&elb, v++);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){0, -f5});
			GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
			GPU_indexbuf_add_generic_vert(&elb, v++);

			GPU_indexbuf_add_primitive_restart(&elb);

			GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){0, +f5});
			GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
			GPU_indexbuf_add_generic_vert(&elb, v++);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){0, +f20});
			GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
			GPU_indexbuf_add_generic_vert(&elb, v++);
		}

		GPUIndexBuf *ibo = GPU_indexbuf_build(&elb);

		*drw_cursor = GPU_batch_create_ex(GPU_PRIM_LINE_STRIP, vbo, ibo, GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
	}
	return *drw_cursor;
}
