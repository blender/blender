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
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_modifier_types.h"
#include "DNA_lattice_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "GPU_batch.h"

#include "draw_cache.h"
#include "draw_cache_impl.h"

/* Batch's only (free'd as an array) */
static struct DRWShapeCache {
	Gwn_Batch *drw_single_vertice;
	Gwn_Batch *drw_fullscreen_quad;
	Gwn_Batch *drw_quad;
	Gwn_Batch *drw_sphere;
	Gwn_Batch *drw_screenspace_circle;
	Gwn_Batch *drw_plain_axes;
	Gwn_Batch *drw_single_arrow;
	Gwn_Batch *drw_cube;
	Gwn_Batch *drw_circle;
	Gwn_Batch *drw_square;
	Gwn_Batch *drw_line;
	Gwn_Batch *drw_line_endpoints;
	Gwn_Batch *drw_empty_sphere;
	Gwn_Batch *drw_empty_cone;
	Gwn_Batch *drw_arrows;
	Gwn_Batch *drw_axis_names;
	Gwn_Batch *drw_image_plane;
	Gwn_Batch *drw_image_plane_wire;
	Gwn_Batch *drw_field_wind;
	Gwn_Batch *drw_field_force;
	Gwn_Batch *drw_field_vortex;
	Gwn_Batch *drw_field_tube_limit;
	Gwn_Batch *drw_field_cone_limit;
	Gwn_Batch *drw_lamp;
	Gwn_Batch *drw_lamp_shadows;
	Gwn_Batch *drw_lamp_sunrays;
	Gwn_Batch *drw_lamp_area;
	Gwn_Batch *drw_lamp_hemi;
	Gwn_Batch *drw_lamp_spot;
	Gwn_Batch *drw_lamp_spot_square;
	Gwn_Batch *drw_speaker;
	Gwn_Batch *drw_lightprobe_cube;
	Gwn_Batch *drw_lightprobe_planar;
	Gwn_Batch *drw_lightprobe_grid;
	Gwn_Batch *drw_bone_octahedral;
	Gwn_Batch *drw_bone_octahedral_wire;
	Gwn_Batch *drw_bone_box;
	Gwn_Batch *drw_bone_box_wire;
	Gwn_Batch *drw_bone_wire_wire;
	Gwn_Batch *drw_bone_envelope;
	Gwn_Batch *drw_bone_envelope_distance;
	Gwn_Batch *drw_bone_envelope_wire;
	Gwn_Batch *drw_bone_envelope_head_wire;
	Gwn_Batch *drw_bone_point;
	Gwn_Batch *drw_bone_point_wire;
	Gwn_Batch *drw_bone_arrows;
	Gwn_Batch *drw_camera;
	Gwn_Batch *drw_camera_frame;
	Gwn_Batch *drw_camera_tria;
	Gwn_Batch *drw_camera_focus;
	Gwn_Batch *drw_particle_cross;
	Gwn_Batch *drw_particle_circle;
	Gwn_Batch *drw_particle_axis;
} SHC = {NULL};

void DRW_shape_cache_free(void)
{
	uint i = sizeof(SHC) / sizeof(Gwn_Batch *);
	Gwn_Batch **batch = (Gwn_Batch **)&SHC;
	while (i--) {
		GWN_BATCH_DISCARD_SAFE(*batch);
		batch++;
	}
}


/* -------------------------------------------------------------------- */

/** \name Helper functions
 * \{ */

static void add_fancy_edge(
        Gwn_VertBuf *vbo, unsigned int pos_id, unsigned int n1_id, unsigned int n2_id,
        unsigned int *v_idx, const float co1[3], const float co2[3],
const float n1[3], const float n2[3])
{
	GWN_vertbuf_attr_set(vbo, n1_id, *v_idx, n1);
	GWN_vertbuf_attr_set(vbo, n2_id, *v_idx, n2);
	GWN_vertbuf_attr_set(vbo, pos_id, (*v_idx)++, co1);

	GWN_vertbuf_attr_set(vbo, n1_id, *v_idx, n1);
	GWN_vertbuf_attr_set(vbo, n2_id, *v_idx, n2);
	GWN_vertbuf_attr_set(vbo, pos_id, (*v_idx)++, co2);
}

static void add_lat_lon_vert(
        Gwn_VertBuf *vbo, unsigned int pos_id, unsigned int nor_id,
        unsigned int *v_idx, const float rad, const float lat, const float lon)
{
	float pos[3], nor[3];
	nor[0] = sinf(lat) * cosf(lon);
	nor[1] = cosf(lat);
	nor[2] = sinf(lat) * sinf(lon);
	mul_v3_v3fl(pos, nor, rad);

	GWN_vertbuf_attr_set(vbo, nor_id, *v_idx, nor);
	GWN_vertbuf_attr_set(vbo, pos_id, (*v_idx)++, pos);
}

static Gwn_VertBuf *fill_arrows_vbo(const float scale)
{
	/* Position Only 3D format */
	static Gwn_VertFormat format = { 0 };
	static struct { uint pos; } attr_id;
	if (format.attrib_ct == 0) {
		attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	}

	/* Line */
	Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
	GWN_vertbuf_data_alloc(vbo, 6 * 3);

	float v1[3] = {0.0, 0.0, 0.0};
	float v2[3] = {0.0, 0.0, 0.0};
	float vtmp1[3], vtmp2[3];

	for (int axis = 0; axis < 3; axis++) {
		const int arrow_axis = (axis == 0) ? 1 : 0;

		v2[axis] = 1.0f;
		mul_v3_v3fl(vtmp1, v1, scale);
		mul_v3_v3fl(vtmp2, v2, scale);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 0, vtmp1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 1, vtmp2);

		v1[axis] = 0.85f;
		v1[arrow_axis] = -0.08f;
		mul_v3_v3fl(vtmp1, v1, scale);
		mul_v3_v3fl(vtmp2, v2, scale);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 2, vtmp1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 3, vtmp2);

		v1[arrow_axis] = 0.08f;
		mul_v3_v3fl(vtmp1, v1, scale);
		mul_v3_v3fl(vtmp2, v2, scale);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 4, vtmp1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 5, vtmp2);

		/* reset v1 & v2 to zero */
		v1[arrow_axis] = v1[axis] = v2[axis] = 0.0f;
	}

	return vbo;
}

static Gwn_VertBuf *sphere_wire_vbo(const float rad)
{
#define NSEGMENTS 32
	/* Position Only 3D format */
	static Gwn_VertFormat format = { 0 };
	static struct { uint pos; } attr_id;
	if (format.attrib_ct == 0) {
		attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	}

	Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
	GWN_vertbuf_data_alloc(vbo, NSEGMENTS * 2 * 3);

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

				if (axis == 0)
					v[0] = cv[0], v[1] = cv[1], v[2] = 0.0f;
				else if (axis == 1)
					v[0] = cv[0], v[1] = 0.0f,  v[2] = cv[1];
				else
					v[0] = 0.0f,  v[1] = cv[0], v[2] = cv[1];

				GWN_vertbuf_attr_set(vbo, attr_id.pos, i * 2 + j + (NSEGMENTS * 2 * axis), v);
			}
		}
	}

	return vbo;
#undef NSEGMENTS
}

/* Quads */
/* Use this one for rendering fullscreen passes. For 3D objects use DRW_cache_quad_get(). */
Gwn_Batch *DRW_cache_fullscreen_quad_get(void)
{
	if (!SHC.drw_fullscreen_quad) {
		/* Use a triangle instead of a real quad */
		/* https://www.slideshare.net/DevCentralAMD/vertex-shader-tricks-bill-bilodeau - slide 14 */
		float pos[3][2] = {{-1.0f, -1.0f}, { 3.0f, -1.0f}, {-1.0f,  3.0f}};
		float uvs[3][2] = {{ 0.0f,  0.0f}, { 2.0f,  0.0f}, { 0.0f,  2.0f}};

		/* Position Only 2D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, uvs; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
			attr_id.uvs = GWN_vertformat_attr_add(&format, "uvs", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 3);

		for (int i = 0; i < 3; ++i) {
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i, pos[i]);
			GWN_vertbuf_attr_set(vbo, attr_id.uvs, i, uvs[i]);
		}

		SHC.drw_fullscreen_quad = GWN_batch_create_ex(GWN_PRIM_TRIS, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_fullscreen_quad;
}

/* Just a regular quad with 4 vertices. */
Gwn_Batch *DRW_cache_quad_get(void)
{
	if (!SHC.drw_quad) {
		float pos[4][2] = {{-1.0f, -1.0f}, { 1.0f, -1.0f}, {1.0f,  1.0f}, {-1.0f,  1.0f}};
		float uvs[4][2] = {{ 0.0f,  0.0f}, { 1.0f,  0.0f}, {1.0f,  1.0f}, { 0.0f,  1.0f}};

		/* Position Only 2D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, uvs; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
			attr_id.uvs = GWN_vertformat_attr_add(&format, "uvs", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 4);

		for (int i = 0; i < 4; ++i) {
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i, pos[i]);
			GWN_vertbuf_attr_set(vbo, attr_id.uvs, i, uvs[i]);
		}

		SHC.drw_quad = GWN_batch_create_ex(GWN_PRIM_TRI_FAN, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_quad;
}

/* Sphere */
Gwn_Batch *DRW_cache_sphere_get(void)
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

Gwn_Batch *DRW_cache_cube_get(void)
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

		const GLubyte indices[24] = {0, 1, 1, 3, 3, 2, 2, 0, 0, 4, 4, 5, 5, 7, 7, 6, 6, 4, 1, 5, 3, 7, 2, 6};

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 24);

		for (int i = 0; i < 24; ++i) {
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i, verts[indices[i]]);
		}

		SHC.drw_cube = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_cube;
}

Gwn_Batch *DRW_cache_circle_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_circle) {
		float v[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2);

		for (int a = 0; a < CIRCLE_RESOL; a++) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[2] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[1] = 0.0f;
			GWN_vertbuf_attr_set(vbo, attr_id.pos, a * 2, v);

			v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[2] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[1] = 0.0f;
			GWN_vertbuf_attr_set(vbo, attr_id.pos, a * 2 + 1, v);
		}

		SHC.drw_circle = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_circle;
#undef CIRCLE_RESOL
}

Gwn_Batch *DRW_cache_square_get(void)
{
	if (!SHC.drw_square) {
		float p[4][3] = {{ 1.0f, 0.0f,  1.0f},
		                 { 1.0f, 0.0f, -1.0f},
		                 {-1.0f, 0.0f, -1.0f},
		                 {-1.0f, 0.0f,  1.0f}};

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 8);

		for (int i = 0; i < 4; i++) {
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i * 2,     p[i % 4]);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i * 2 + 1, p[(i + 1) % 4]);
		}

		SHC.drw_square = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_square;
}

Gwn_Batch *DRW_cache_single_line_get(void)
{
	/* Z axis line */
	if (!SHC.drw_line) {
		float v1[3] = {0.0f, 0.0f, 0.0f};
		float v2[3] = {0.0f, 0.0f, 1.0f};

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 2);

		GWN_vertbuf_attr_set(vbo, attr_id.pos, 0, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 1, v2);

		SHC.drw_line = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_line;
}

Gwn_Batch *DRW_cache_single_line_endpoints_get(void)
{
	/* Z axis line */
	if (!SHC.drw_line_endpoints) {
		float v1[3] = {0.0f, 0.0f, 0.0f};
		float v2[3] = {0.0f, 0.0f, 1.0f};

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 2);

		GWN_vertbuf_attr_set(vbo, attr_id.pos, 0, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 1, v2);

		SHC.drw_line_endpoints = GWN_batch_create_ex(GWN_PRIM_POINTS, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_line_endpoints;
}

Gwn_Batch *DRW_cache_screenspace_circle_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_screenspace_circle) {
		float v[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, CIRCLE_RESOL + 1);

		for (int a = 0; a <= CIRCLE_RESOL; a++) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			GWN_vertbuf_attr_set(vbo, attr_id.pos, a, v);
		}

		SHC.drw_screenspace_circle = GWN_batch_create_ex(GWN_PRIM_LINE_STRIP, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_screenspace_circle;
#undef CIRCLE_RESOL
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Common Object API
 * \{ */

Gwn_Batch *DRW_cache_object_wire_outline_get(Object *ob)
{
	switch (ob->type) {
		case OB_MESH:
			return DRW_cache_mesh_wire_outline_get(ob);

		/* TODO, should match 'DRW_cache_object_surface_get' */
		default:
			return NULL;
	}
}

Gwn_Batch *DRW_cache_object_surface_get(Object *ob)
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

Gwn_Batch **DRW_cache_object_surface_material_get(
        struct Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len)
{
	switch (ob->type) {
		case OB_MESH:
			return DRW_cache_mesh_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
		case OB_CURVE:
			return DRW_cache_curve_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
		case OB_SURF:
			return DRW_cache_surf_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
		case OB_FONT:
			return DRW_cache_text_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
		default:
			return NULL;
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Empties
 * \{ */

Gwn_Batch *DRW_cache_plain_axes_get(void)
{
	if (!SHC.drw_plain_axes) {
		int axis;
		float v1[3] = {0.0f, 0.0f, 0.0f};
		float v2[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 6);

		for (axis = 0; axis < 3; axis++) {
			v1[axis] = 1.0f;
			v2[axis] = -1.0f;

			GWN_vertbuf_attr_set(vbo, attr_id.pos, axis * 2, v1);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, axis * 2 + 1, v2);

			/* reset v1 & v2 to zero for next axis */
			v1[axis] = v2[axis] = 0.0f;
		}

		SHC.drw_plain_axes = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_plain_axes;
}

Gwn_Batch *DRW_cache_single_arrow_get(void)
{
	if (!SHC.drw_single_arrow) {
		float v1[3] = {0.0f, 0.0f, 1.0f}, v2[3], v3[3];

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		/* Square Pyramid */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 12);

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

			GWN_vertbuf_attr_set(vbo, attr_id.pos, sides * 3 + 0, v1);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, sides * 3 + 1, v2);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, sides * 3 + 2, v3);
		}

		SHC.drw_single_arrow = GWN_batch_create_ex(GWN_PRIM_TRIS, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_single_arrow;
}

Gwn_Batch *DRW_cache_empty_sphere_get(void)
{
	if (!SHC.drw_empty_sphere) {
		Gwn_VertBuf *vbo = sphere_wire_vbo(1.0f);
		SHC.drw_empty_sphere = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_empty_sphere;
}

Gwn_Batch *DRW_cache_empty_cone_get(void)
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
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, NSEGMENTS * 4);

		for (int i = 0; i < NSEGMENTS; ++i) {
			float cv[2], v[3];
			cv[0] = p[(i) % NSEGMENTS][0];
			cv[1] = p[(i) % NSEGMENTS][1];

			/* cone sides */
			v[0] = cv[0], v[1] = 0.0f, v[2] = cv[1];
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i * 4, v);
			v[0] = 0.0f, v[1] = 2.0f, v[2] = 0.0f;
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 1, v);

			/* end ring */
			v[0] = cv[0], v[1] = 0.0f, v[2] = cv[1];
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 2, v);
			cv[0] = p[(i + 1) % NSEGMENTS][0];
			cv[1] = p[(i + 1) % NSEGMENTS][1];
			v[0] = cv[0], v[1] = 0.0f, v[2] = cv[1];
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 3, v);
		}

		SHC.drw_empty_cone = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_empty_cone;
#undef NSEGMENTS
}

Gwn_Batch *DRW_cache_arrows_get(void)
{
	if (!SHC.drw_arrows) {
		Gwn_VertBuf *vbo = fill_arrows_vbo(1.0f);

		SHC.drw_arrows = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_arrows;
}

Gwn_Batch *DRW_cache_axis_names_get(void)
{
	if (!SHC.drw_axis_names) {
		const float size = 0.1f;
		float v1[3], v2[3];

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			/* Using 3rd component as axis indicator */
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		/* Line */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 14);

		/* X */
		copy_v3_fl3(v1, -size,  size, 0.0f);
		copy_v3_fl3(v2,  size, -size, 0.0f);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 0, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 1, v2);

		copy_v3_fl3(v1,  size,  size, 0.0f);
		copy_v3_fl3(v2, -size, -size, 0.0f);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 2, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 3, v2);

		/* Y */
		copy_v3_fl3(v1, -size + 0.25f * size,  size, 1.0f);
		copy_v3_fl3(v2,  0.0f,  0.0f, 1.0f);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 4, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 5, v2);

		copy_v3_fl3(v1,  size - 0.25f * size,  size, 1.0f);
		copy_v3_fl3(v2, -size + 0.25f * size, -size, 1.0f);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 6, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 7, v2);

		/* Z */
		copy_v3_fl3(v1, -size,  size, 2.0f);
		copy_v3_fl3(v2,  size,  size, 2.0f);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 8, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 9, v2);

		copy_v3_fl3(v1,  size,  size, 2.0f);
		copy_v3_fl3(v2, -size, -size, 2.0f);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 10, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 11, v2);

		copy_v3_fl3(v1, -size, -size, 2.0f);
		copy_v3_fl3(v2,  size, -size, 2.0f);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 12, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 13, v2);

		SHC.drw_axis_names = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_axis_names;
}

Gwn_Batch *DRW_cache_image_plane_get(void)
{
	if (!SHC.drw_image_plane) {
		const float quad[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, texCoords; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
			attr_id.texCoords = GWN_vertformat_attr_add(&format, "texCoord", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		}
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 4);
		for (uint j = 0; j < 4; j++) {
			GWN_vertbuf_attr_set(vbo, attr_id.pos, j, quad[j]);
			GWN_vertbuf_attr_set(vbo, attr_id.texCoords, j, quad[j]);
		}
		SHC.drw_image_plane = GWN_batch_create_ex(GWN_PRIM_TRI_FAN, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_image_plane;
}

Gwn_Batch *DRW_cache_image_plane_wire_get(void)
{
	if (!SHC.drw_image_plane_wire) {
		const float quad[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		}
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 4);
		for (uint j = 0; j < 4; j++) {
			GWN_vertbuf_attr_set(vbo, attr_id.pos, j, quad[j]);
		}
		SHC.drw_image_plane_wire = GWN_batch_create_ex(GWN_PRIM_LINE_LOOP, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_image_plane_wire;
}

/* Force Field */
Gwn_Batch *DRW_cache_field_wind_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_field_wind) {
		float v[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2 * 4);

		for (int i = 0; i < 4; i++) {
			float z = 0.05f * (float)i;
			for (int a = 0; a < CIRCLE_RESOL; a++) {
				v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GWN_vertbuf_attr_set(vbo, attr_id.pos, i * CIRCLE_RESOL * 2 + a * 2, v);

				v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GWN_vertbuf_attr_set(vbo, attr_id.pos, i * CIRCLE_RESOL * 2 + a * 2 + 1, v);
			}
		}

		SHC.drw_field_wind = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_field_wind;
#undef CIRCLE_RESOL
}

Gwn_Batch *DRW_cache_field_force_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_field_force) {
		float v[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2 * 3);

		for (int i = 0; i < 3; i++) {
			float radius = 1.0f + 0.5f * (float)i;
			for (int a = 0; a < CIRCLE_RESOL; a++) {
				v[0] = radius * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[1] = radius * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[2] = 0.0f;
				GWN_vertbuf_attr_set(vbo, attr_id.pos, i * CIRCLE_RESOL * 2 + a * 2, v);

				v[0] = radius * sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[1] = radius * cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[2] = 0.0f;
				GWN_vertbuf_attr_set(vbo, attr_id.pos, i * CIRCLE_RESOL * 2 + a * 2 + 1, v);
			}
		}

		SHC.drw_field_force = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_field_force;
#undef CIRCLE_RESOL
}

Gwn_Batch *DRW_cache_field_vortex_get(void)
{
#define SPIRAL_RESOL 32
	if (!SHC.drw_field_vortex) {
		float v[3] = {0.0f, 0.0f, 0.0f};
		unsigned int v_idx = 0;

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, SPIRAL_RESOL * 2 + 1);

		for (int a = SPIRAL_RESOL; a > -1; a--) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)SPIRAL_RESOL)) * (a / (float)SPIRAL_RESOL);
			v[1] = cosf((2.0f * M_PI * a) / ((float)SPIRAL_RESOL)) * (a / (float)SPIRAL_RESOL);

			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
		}

		for (int a = 1; a <= SPIRAL_RESOL; a++) {
			v[0] = -sinf((2.0f * M_PI * a) / ((float)SPIRAL_RESOL)) * (a / (float)SPIRAL_RESOL);
			v[1] = -cosf((2.0f * M_PI * a) / ((float)SPIRAL_RESOL)) * (a / (float)SPIRAL_RESOL);

			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
		}

		SHC.drw_field_vortex = GWN_batch_create_ex(GWN_PRIM_LINE_STRIP, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_field_vortex;
#undef SPIRAL_RESOL
}

Gwn_Batch *DRW_cache_field_tube_limit_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_field_tube_limit) {
		float v[3] = {0.0f, 0.0f, 0.0f};
		unsigned int v_idx = 0;

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2 * 2 + 8);

		/* Caps */
		for (int i = 0; i < 2; i++) {
			float z = (float)i * 2.0f - 1.0f;
			for (int a = 0; a < CIRCLE_RESOL; a++) {
				v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);

				v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
			}
		}
		/* Side Edges */
		for (int a = 0; a < 4; a++) {
			for (int i = 0; i < 2; i++) {
				float z = (float)i * 2.0f - 1.0f;
				v[0] = sinf((2.0f * M_PI * a) / 4.0f);
				v[1] = cosf((2.0f * M_PI * a) / 4.0f);
				v[2] = z;
				GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
			}
		}

		SHC.drw_field_tube_limit = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_field_tube_limit;
#undef CIRCLE_RESOL
}

Gwn_Batch *DRW_cache_field_cone_limit_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_field_cone_limit) {
		float v[3] = {0.0f, 0.0f, 0.0f};
		unsigned int v_idx = 0;

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2 * 2 + 8);

		/* Caps */
		for (int i = 0; i < 2; i++) {
			float z = (float)i * 2.0f - 1.0f;
			for (int a = 0; a < CIRCLE_RESOL; a++) {
				v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);

				v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
				v[2] = z;
				GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
			}
		}
		/* Side Edges */
		for (int a = 0; a < 4; a++) {
			for (int i = 0; i < 2; i++) {
				float z = (float)i * 2.0f - 1.0f;
				v[0] = z * sinf((2.0f * M_PI * a) / 4.0f);
				v[1] = z * cosf((2.0f * M_PI * a) / 4.0f);
				v[2] = z;
				GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v);
			}
		}

		SHC.drw_field_cone_limit = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_field_cone_limit;
#undef CIRCLE_RESOL
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Lamps
 * \{ */

Gwn_Batch *DRW_cache_lamp_get(void)
{
#define NSEGMENTS 8
	if (!SHC.drw_lamp) {
		float v[2];

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, NSEGMENTS * 2);

		for (int a = 0; a < NSEGMENTS * 2; a += 2) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)NSEGMENTS * 2));
			v[1] = cosf((2.0f * M_PI * a) / ((float)NSEGMENTS * 2));
			GWN_vertbuf_attr_set(vbo, attr_id.pos, a, v);

			v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)NSEGMENTS * 2));
			v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)NSEGMENTS * 2));
			GWN_vertbuf_attr_set(vbo, attr_id.pos, a + 1, v);
		}

		SHC.drw_lamp = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp;
#undef NSEGMENTS
}

Gwn_Batch *DRW_cache_lamp_shadows_get(void)
{
#define NSEGMENTS 10
	if (!SHC.drw_lamp_shadows) {
		float v[2];

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, NSEGMENTS * 2);

		for (int a = 0; a < NSEGMENTS * 2; a += 2) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)NSEGMENTS * 2));
			v[1] = cosf((2.0f * M_PI * a) / ((float)NSEGMENTS * 2));
			GWN_vertbuf_attr_set(vbo, attr_id.pos, a, v);

			v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)NSEGMENTS * 2));
			v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)NSEGMENTS * 2));
			GWN_vertbuf_attr_set(vbo, attr_id.pos, a + 1, v);
		}

		SHC.drw_lamp_shadows = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_shadows;
#undef NSEGMENTS
}

Gwn_Batch *DRW_cache_lamp_sunrays_get(void)
{
	if (!SHC.drw_lamp_sunrays) {
		float v[2], v1[2], v2[2];

		/* Position Only 2D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 32);

		for (int a = 0; a < 8; a++) {
			v[0] = sinf((2.0f * M_PI * a) / 8.0f);
			v[1] = cosf((2.0f * M_PI * a) / 8.0f);

			mul_v2_v2fl(v1, v, 1.6f);
			mul_v2_v2fl(v2, v, 1.9f);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, a * 4, v1);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, a * 4 + 1, v2);

			mul_v2_v2fl(v1, v, 2.2f);
			mul_v2_v2fl(v2, v, 2.5f);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, a * 4 + 2, v1);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, a * 4 + 3, v2);
		}

		SHC.drw_lamp_sunrays = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_sunrays;
}

Gwn_Batch *DRW_cache_lamp_area_get(void)
{
	if (!SHC.drw_lamp_area) {
		float v1[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 8);

		v1[0] = v1[1] = 0.5f;
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 0, v1);
		v1[0] = -0.5f;
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 1, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 2, v1);
		v1[1] = -0.5f;
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 3, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 4, v1);
		v1[0] = 0.5f;
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 5, v1);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 6, v1);
		v1[1] = 0.5f;
		GWN_vertbuf_attr_set(vbo, attr_id.pos, 7, v1);

		SHC.drw_lamp_area = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_area;
}

Gwn_Batch *DRW_cache_lamp_hemi_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_lamp_hemi) {
		float v[3];
		int vidx = 0;

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2 * 2 - 6 * 2 * 2);

		/* XZ plane */
		for (int a = 3; a < CIRCLE_RESOL / 2 - 3; a++) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL) - M_PI / 2);
			v[2] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL) - M_PI / 2) - 1.0f;
			v[1] = 0.0f;
			GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);

			v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL) - M_PI / 2);
			v[2] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL) - M_PI / 2) - 1.0f;
			v[1] = 0.0f;
			GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
		}

		/* XY plane */
		for (int a = 3; a < CIRCLE_RESOL / 2 - 3; a++) {
			v[2] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL)) - 1.0f;
			v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[0] = 0.0f;
			GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);

			v[2] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL)) - 1.0f;
			v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[0] = 0.0f;
			GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
		}

		/* YZ plane full circle */
		/* lease v[2] as it is */
		const float rad = cosf((2.0f * M_PI * 3) / ((float)CIRCLE_RESOL));
		for (int a = 0; a < CIRCLE_RESOL; a++) {
			v[1] = rad * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[0] = rad * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);

			v[1] = rad * sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[0] = rad * cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
		}


		SHC.drw_lamp_hemi = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_hemi;
#undef CIRCLE_RESOL
}


Gwn_Batch *DRW_cache_lamp_spot_get(void)
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
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, n1, n2; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.n1 = GWN_vertformat_attr_add(&format, "N1", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.n2 = GWN_vertformat_attr_add(&format, "N2", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, NSEGMENTS * 4);

		for (int i = 0; i < NSEGMENTS; ++i) {
			float cv[2], v[3];
			cv[0] = p[i % NSEGMENTS][0];
			cv[1] = p[i % NSEGMENTS][1];

			/* cone sides */
			v[0] = cv[0], v[1] = cv[1], v[2] = -1.0f;
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i * 4, v);
			v[0] = 0.0f, v[1] = 0.0f, v[2] = 0.0f;
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 1, v);

			GWN_vertbuf_attr_set(vbo, attr_id.n1, i * 4,     n[(i) % NSEGMENTS]);
			GWN_vertbuf_attr_set(vbo, attr_id.n1, i * 4 + 1, n[(i) % NSEGMENTS]);
			GWN_vertbuf_attr_set(vbo, attr_id.n2, i * 4,     n[(i + 1) % NSEGMENTS]);
			GWN_vertbuf_attr_set(vbo, attr_id.n2, i * 4 + 1, n[(i + 1) % NSEGMENTS]);

			/* end ring */
			v[0] = cv[0], v[1] = cv[1], v[2] = -1.0f;
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 2, v);
			cv[0] = p[(i + 1) % NSEGMENTS][0];
			cv[1] = p[(i + 1) % NSEGMENTS][1];
			v[0] = cv[0], v[1] = cv[1], v[2] = -1.0f;
			GWN_vertbuf_attr_set(vbo, attr_id.pos, i * 4 + 3, v);

			GWN_vertbuf_attr_set(vbo, attr_id.n1, i * 4 + 2, n[(i) % NSEGMENTS]);
			GWN_vertbuf_attr_set(vbo, attr_id.n1, i * 4 + 3, n[(i) % NSEGMENTS]);
			GWN_vertbuf_attr_set(vbo, attr_id.n2, i * 4 + 2, neg[(i) % NSEGMENTS]);
			GWN_vertbuf_attr_set(vbo, attr_id.n2, i * 4 + 3, neg[(i) % NSEGMENTS]);
		}

		SHC.drw_lamp_spot = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_spot;
#undef NSEGMENTS
}

Gwn_Batch *DRW_cache_lamp_spot_square_get(void)
{
	if (!SHC.drw_lamp_spot_square) {
		float p[5][3] = {{ 0.0f,  0.0f,  0.0f},
		                 { 1.0f,  1.0f, -1.0f},
		                 { 1.0f, -1.0f, -1.0f},
		                 {-1.0f, -1.0f, -1.0f},
		                 {-1.0f,  1.0f, -1.0f}};

		unsigned int v_idx = 0;

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 16);

		/* piramid sides */
		for (int i = 1; i <= 4; ++i) {
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, p[0]);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, p[i]);

			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, p[(i % 4) + 1]);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, p[((i + 1) % 4) + 1]);
		}

		SHC.drw_lamp_spot_square = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_lamp_spot_square;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Speaker
 * \{ */

Gwn_Batch *DRW_cache_speaker_get(void)
{
	if (!SHC.drw_speaker) {
		float v[3];
		const int segments = 16;
		int vidx = 0;

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 3 * segments * 2 + 4 * 4);

		for (int j = 0; j < 3; j++) {
			float z = 0.25f * j - 0.125f;
			float r = (j == 0 ? 0.5f : 0.25f);

			copy_v3_fl3(v, r, 0.0f, z);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
			for (int i = 1; i < segments; i++) {
				float x = cosf(2.f * (float)M_PI * i / segments) * r;
				float y = sinf(2.f * (float)M_PI * i / segments) * r;
				copy_v3_fl3(v, x, y, z);
				GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
				GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
			}
			copy_v3_fl3(v, r, 0.0f, z);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
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
				GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
				if (i == 1) {
					GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
				}
			}
		}

		SHC.drw_speaker = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_speaker;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Probe
 * \{ */

Gwn_Batch *DRW_cache_lightprobe_cube_get(void)
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
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, (6 + 3) * 2);

		for (int i = 0; i < 6; ++i) {
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[i]);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[(i + 1) % 6]);
		}

		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[1]);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[5]);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[3]);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		SHC.drw_lightprobe_cube = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_lightprobe_cube;
}

Gwn_Batch *DRW_cache_lightprobe_grid_get(void)
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
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, (6 * 2 + 3) * 2);

		for (int i = 0; i < 6; ++i) {
			float tmp_v1[3], tmp_v2[3], tmp_tr[3];
			copy_v3_v3(tmp_v1, v[i]);
			copy_v3_v3(tmp_v2, v[(i + 1) % 6]);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, tmp_v1);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, tmp_v2);

			/* Internal wires. */
			for (int j = 1; j < 2; ++j) {
				mul_v3_v3fl(tmp_tr, v[(i / 2) * 2 + 1], -0.5f * j);
				add_v3_v3v3(tmp_v1, v[i], tmp_tr);
				add_v3_v3v3(tmp_v2, v[(i + 1) % 6], tmp_tr);
				GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, tmp_v1);
				GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, tmp_v2);
			}
		}

		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[1]);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[5]);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[3]);
		GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[6]);

		SHC.drw_lightprobe_grid = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_lightprobe_grid;
}

Gwn_Batch *DRW_cache_lightprobe_planar_get(void)
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
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 4 * 2);

		for (int i = 0; i < 4; ++i) {
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[i]);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, v[(i + 1) % 4]);
		}

		SHC.drw_lightprobe_planar = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
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

static const unsigned int bone_octahedral_wire[24] = {
	0, 1,  1, 5,  5, 3,  3, 0,
	0, 4,  4, 5,  5, 2,  2, 0,
	1, 2,  2, 3,  3, 4,  4, 1,
};

/* aligned with bone_octahedral_wire
 * Contains adjacent normal index */
static const unsigned int bone_octahedral_wire_adjacent_face[24] = {
	0, 3,  4, 7,  5, 6,  1, 2,
	2, 3,  6, 7,  4, 5,  0, 1,
	0, 4,  1, 5,  2, 6,  3, 7,
};

static const unsigned int bone_octahedral_solid_tris[8][3] = {
	{2, 1, 0}, /* bottom */
	{3, 2, 0},
	{4, 3, 0},
	{1, 4, 0},

	{5, 1, 2}, /* top */
	{5, 2, 3},
	{5, 3, 4},
	{5, 4, 1}
};

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

Gwn_Batch *DRW_cache_bone_octahedral_get(void)
{
	if (!SHC.drw_bone_octahedral) {
		unsigned int v_idx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, nor; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.nor = GWN_vertformat_attr_add(&format, "nor", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 24);

		for (int i = 0; i < 8; i++) {
			GWN_vertbuf_attr_set(vbo, attr_id.nor, v_idx, bone_octahedral_solid_normals[i]);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, bone_octahedral_verts[bone_octahedral_solid_tris[i][0]]);
			GWN_vertbuf_attr_set(vbo, attr_id.nor, v_idx, bone_octahedral_solid_normals[i]);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, bone_octahedral_verts[bone_octahedral_solid_tris[i][1]]);
			GWN_vertbuf_attr_set(vbo, attr_id.nor, v_idx, bone_octahedral_solid_normals[i]);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, bone_octahedral_verts[bone_octahedral_solid_tris[i][2]]);
		}

		SHC.drw_bone_octahedral = GWN_batch_create_ex(GWN_PRIM_TRIS, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_octahedral;
}

Gwn_Batch *DRW_cache_bone_octahedral_wire_outline_get(void)
{
	if (!SHC.drw_bone_octahedral_wire) {
		unsigned int v_idx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, n1, n2; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.n1 = GWN_vertformat_attr_add(&format, "N1", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.n2 = GWN_vertformat_attr_add(&format, "N2", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 12 * 2);

		for (int i = 0; i < 12; i++) {
			const float *co1 = bone_octahedral_verts[bone_octahedral_wire[i * 2]];
			const float *co2 = bone_octahedral_verts[bone_octahedral_wire[i * 2 + 1]];
			const float *n1 = bone_octahedral_solid_normals[bone_octahedral_wire_adjacent_face[i * 2]];
			const float *n2 = bone_octahedral_solid_normals[bone_octahedral_wire_adjacent_face[i * 2 + 1]];
			add_fancy_edge(vbo, attr_id.pos, attr_id.n1, attr_id.n2, &v_idx, co1, co2, n1, n2);
		}

		SHC.drw_bone_octahedral_wire = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
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

static const unsigned int bone_box_wire[24] = {
	0, 1,  1, 2,  2, 3,  3, 0,
	4, 5,  5, 6,  6, 7,  7, 4,
	0, 4,  1, 5,  2, 6,  3, 7,
};

/* aligned with bone_octahedral_wire
 * Contains adjacent normal index */
static const unsigned int bone_box_wire_adjacent_face[24] = {
	0,  2,   0,  4,   1,  6,   1,  8,
	3, 10,   5, 10,   7, 11,   9, 11,
	3,  8,   2,  5,   4,  7,   6,  9,
};

static const unsigned int bone_box_solid_tris[12][3] = {
	{0, 1, 2}, /* bottom */
	{0, 2, 3},

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

/* aligned with bone_octahedral_solid_tris */
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

Gwn_Batch *DRW_cache_bone_box_get(void)
{
	if (!SHC.drw_bone_box) {
		unsigned int v_idx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, nor; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.nor = GWN_vertformat_attr_add(&format, "nor", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 36);

		for (int i = 0; i < 12; i++) {
			for (int j = 0; j < 3; j++) {
				GWN_vertbuf_attr_set(vbo, attr_id.nor, v_idx, bone_box_solid_normals[i]);
				GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, bone_box_verts[bone_box_solid_tris[i][j]]);
			}
		}

		SHC.drw_bone_box = GWN_batch_create_ex(GWN_PRIM_TRIS, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_box;
}

Gwn_Batch *DRW_cache_bone_box_wire_outline_get(void)
{
	if (!SHC.drw_bone_box_wire) {
		unsigned int v_idx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, n1, n2; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.n1 = GWN_vertformat_attr_add(&format, "N1", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.n2 = GWN_vertformat_attr_add(&format, "N2", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 12 * 2);

		for (int i = 0; i < 12; i++) {
			const float *co1 = bone_box_verts[bone_box_wire[i * 2]];
			const float *co2 = bone_box_verts[bone_box_wire[i * 2 + 1]];
			const float *n1 = bone_box_solid_normals[bone_box_wire_adjacent_face[i * 2]];
			const float *n2 = bone_box_solid_normals[bone_box_wire_adjacent_face[i * 2 + 1]];
			add_fancy_edge(vbo, attr_id.pos, attr_id.n1, attr_id.n2, &v_idx, co1, co2, n1, n2);
		}

		SHC.drw_bone_box_wire = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_box_wire;
}


Gwn_Batch *DRW_cache_bone_wire_wire_outline_get(void)
{
	if (!SHC.drw_bone_wire_wire) {
		unsigned int v_idx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, n1, n2; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.n1 = GWN_vertformat_attr_add(&format, "N1", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.n2 = GWN_vertformat_attr_add(&format, "N2", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 2);

		const float co1[3] = {0.0f, 0.0f, 0.0f};
		const float co2[3] = {0.0f, 1.0f, 0.0f};
		const float n[3] = {1.0f, 0.0f, 0.0f};
		add_fancy_edge(vbo, attr_id.pos, attr_id.n1, attr_id.n2, &v_idx, co1, co2, n, n);

		SHC.drw_bone_wire_wire = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_wire_wire;
}


/* Helpers for envelope bone's solid sphere-with-hidden-equatorial-cylinder.
 * Note that here we only encode head/tail in forth component of the vector. */
static void benv_lat_lon_to_co(const float lat, const float lon, float r_nor[3])
{
	/* Poles are along Y axis. */
	r_nor[0] = sinf(lat) * cosf(lon);
	r_nor[1] = cosf(lat);
	r_nor[2] = sinf(lat) * sinf(lon);
}

static void benv_add_tri(Gwn_VertBuf *vbo, uint pos_id, uint *v_idx, float *co1, float *co2, float *co3)
{
	/* Given tri and its seven other mirrors along X/Y/Z axes. */
	for (int x = -1; x <= 1; x += 2) {
		for (int y = -1; y <= 1; y += 2) {
			const float head_tail = (y == -1) ? 0.0f : 1.0f;
			for (int z = -1; z <= 1; z += 2) {
				GWN_vertbuf_attr_set(vbo, pos_id, (*v_idx)++,
				                     (const float[4]){co1[0] * x, co1[1] * y, co1[2] * z, head_tail});
				GWN_vertbuf_attr_set(vbo, pos_id, (*v_idx)++,
				                     (const float[4]){co2[0] * x, co2[1] * y, co2[2] * z, head_tail});
				GWN_vertbuf_attr_set(vbo, pos_id, (*v_idx)++,
				                     (const float[4]){co3[0] * x, co3[1] * y, co3[2] * z, head_tail});
			}
		}
	}
}

Gwn_Batch *DRW_cache_bone_envelope_solid_get(void)
{
#define CIRCLE_RESOL 32  /* Must be multiple of 4 */
	if (!SHC.drw_bone_envelope) {
		const int lon_res = CIRCLE_RESOL / 4;
		const int lat_res = CIRCLE_RESOL / 4;
		const float lon_inc = M_PI_2 / lon_res;
		const float lat_inc = M_PI_2 / lat_res;
		unsigned int v_idx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 4, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, lat_res * lon_res * 8 * 6);

		float lon = 0.0f;
		for (int i = 0; i < lon_res; i++, lon += lon_inc) {
			float lat = 0.0f;
			float co1[3], co2[3], co3[3], co4[3];

			for (int j = 0; j < lat_res; j++, lat += lat_inc) {
				benv_lat_lon_to_co(lat,           lon,           co1);
				benv_lat_lon_to_co(lat,           lon + lon_inc, co2);
				benv_lat_lon_to_co(lat + lat_inc, lon + lon_inc, co3);
				benv_lat_lon_to_co(lat + lat_inc, lon,           co4);

				if (j != 0) {  /* At pole, n1 and n2 are identical. */
					benv_add_tri(vbo, attr_id.pos, &v_idx, co1, co2, co3);
				}
				benv_add_tri(vbo, attr_id.pos, &v_idx, co1, co3, co4);
			}

			/* lat is at equator (i.e. lat == pi / 2). */
			/* We need to add 'cylinder' part between the equators (along XZ plane). */
			for (int x = -1; x <= 1; x += 2) {
				for (int z = -1; z <= 1; z += 2) {
					GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++,
					                     (const float[4]){co3[0] * x, co3[1], co3[2] * z, 0.0f});
					GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++,
					                     (const float[4]){co4[0] * x, co4[1], co4[2] * z, 0.0f});
					GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++,
					                     (const float[4]){co4[0] * x, co4[1], co4[2] * z, 1.0f});

					GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++,
					                     (const float[4]){co3[0] * x, co3[1], co3[2] * z, 0.0f});
					GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++,
					                     (const float[4]){co4[0] * x, co4[1], co4[2] * z, 1.0f});
					GWN_vertbuf_attr_set(vbo, attr_id.pos, v_idx++,
					                     (const float[4]){co3[0] * x, co3[1], co3[2] * z, 1.0f});
				}
			}
		}

		SHC.drw_bone_envelope = GWN_batch_create_ex(GWN_PRIM_TRIS, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_envelope;
}


Gwn_Batch *DRW_cache_bone_envelope_distance_outline_get(void)
{
#define CIRCLE_RESOL 32  /* Must be multiple of 2 */
	if (!SHC.drw_bone_envelope_distance) {
		unsigned int v_idx = 0;

		static Gwn_VertFormat format = { 0 };
		static unsigned int pos_id;
		if (format.attrib_ct == 0) {
			pos_id = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 4, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, CIRCLE_RESOL * 2 + 6);

		/* Encoded triangle strip, vertex shader gives them final correct value. */
		for (int i = 0; i < CIRCLE_RESOL + 1; i++) {
			const bool is_headtail_transition = ELEM(i, CIRCLE_RESOL / 2, CIRCLE_RESOL);
			const float head_tail = (i > CIRCLE_RESOL / 2) ? 1.0f : 0.0f;
			const float alpha = 2.0f * M_PI * i / CIRCLE_RESOL;
			const float x = cosf(alpha);
			const float y = -sinf(alpha);

			/*                                                        { X, Y, head/tail, inner/outer border } */
			GWN_vertbuf_attr_set(vbo, pos_id, v_idx++, (const float[4]){x, y, head_tail, 0.0f});
			GWN_vertbuf_attr_set(vbo, pos_id, v_idx++, (const float[4]){x, y, head_tail, 1.0f});
			if (is_headtail_transition) {
				GWN_vertbuf_attr_set(vbo, pos_id, v_idx++, (const float[4]){x, y, 1.0f - head_tail, 0.0f});
				GWN_vertbuf_attr_set(vbo, pos_id, v_idx++, (const float[4]){x, y, 1.0f - head_tail, 1.0f});
			}
		}

		SHC.drw_bone_envelope_distance = GWN_batch_create_ex(GWN_PRIM_TRI_STRIP, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_envelope_distance;
#undef CIRCLE_RESOL
}


/* Bone body. */
Gwn_Batch *DRW_cache_bone_envelope_wire_outline_get(void)
{
	if (!SHC.drw_bone_envelope_wire) {
		unsigned int v_idx = 0;

		static Gwn_VertFormat format = { 0 };
		static unsigned int pos_id;
		if (format.attrib_ct == 0) {
			pos_id = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 4, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 4);

		/* Two lines between head and tail circles. */
		/* Encoded lines, vertex shader gives them final correct value. */
		/*                                                         { X,    Y, head/tail, inner/outer border } */
		GWN_vertbuf_attr_set(vbo, pos_id, v_idx++, (const float[4]){ 1.0f, 0.0f,       0.0f, 0.0f});
		GWN_vertbuf_attr_set(vbo, pos_id, v_idx++, (const float[4]){ 1.0f, 0.0f,       1.0f, 0.0f});
		GWN_vertbuf_attr_set(vbo, pos_id, v_idx++, (const float[4]){-1.0f, 0.0f,       0.0f, 0.0f});
		GWN_vertbuf_attr_set(vbo, pos_id, v_idx++, (const float[4]){-1.0f, 0.0f,       1.0f, 0.0f});

		SHC.drw_bone_envelope_wire = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_envelope_wire;
}


/* Bone head and tail. */
Gwn_Batch *DRW_cache_bone_envelope_head_wire_outline_get(void)
{
#define CIRCLE_RESOL 32  /* Must be multiple of 2 */
	if (!SHC.drw_bone_envelope_head_wire) {
		unsigned int v_idx = 0;

		static Gwn_VertFormat format = { 0 };
		static unsigned int pos_id;
		if (format.attrib_ct == 0) {
			pos_id = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 4, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, CIRCLE_RESOL);

		/* Encoded lines, vertex shader gives them final correct value. */
		/* Only head circle (tail is drawn in disp_tail_mat space as a head one by draw_armature.c's draw_point()). */
		for (int i = 0; i < CIRCLE_RESOL; i++) {
			const float alpha = 2.0f * M_PI * i / CIRCLE_RESOL;
			const float x = cosf(alpha);
			const float y = -sinf(alpha);

			/*                                                         {     X,      Y, head/tail, inner/outer border } */
			GWN_vertbuf_attr_set(vbo, pos_id, v_idx++, (const float[4]){     x,      y,      0.0f, 0.0f});
		}

		SHC.drw_bone_envelope_head_wire = GWN_batch_create_ex(GWN_PRIM_LINE_LOOP, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_envelope_head_wire;
#undef CIRCLE_RESOL
}


Gwn_Batch *DRW_cache_bone_point_get(void)
{
	if (!SHC.drw_bone_point) {
		const int lon_res = 16;
		const int lat_res = 8;
		const float rad = 0.05f;
		const float lon_inc = 2 * M_PI / lon_res;
		const float lat_inc = M_PI / lat_res;
		unsigned int v_idx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, nor; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.nor = GWN_vertformat_attr_add(&format, "nor", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, (lat_res - 1) * lon_res * 6);

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

		SHC.drw_bone_point = GWN_batch_create_ex(GWN_PRIM_TRIS, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_point;
}

Gwn_Batch *DRW_cache_bone_point_wire_outline_get(void)
{
	if (!SHC.drw_bone_point_wire) {
		Gwn_VertBuf *vbo = sphere_wire_vbo(0.05f);
		SHC.drw_bone_point_wire = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_bone_point_wire;
}

Gwn_Batch *DRW_cache_bone_arrows_get(void)
{
	if (!SHC.drw_bone_arrows) {
		Gwn_VertBuf *vbo = fill_arrows_vbo(0.25f);
		SHC.drw_bone_arrows = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
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
        Gwn_VertBufRaw *pos_step,
        const float *coords, const uint coords_len)
{
	for (uint i = 0, i_prev = coords_len - 1; i < coords_len; i_prev = i++) {
		*((float *)GWN_vertbuf_raw_step(pos_step)) = coords[i_prev];
		*((float *)GWN_vertbuf_raw_step(pos_step)) = coords[i];
	}
}

/** Fan lines out from the first vertex. */
static void camera_fill_lines_fan_fl_v1(
        Gwn_VertBufRaw *pos_step,
        const float *coords, const uint coords_len)
{
	for (uint i = 1; i < coords_len; i++) {
		*((float *)GWN_vertbuf_raw_step(pos_step)) = coords[0];
		*((float *)GWN_vertbuf_raw_step(pos_step)) = coords[i];
	}
}

/** Simply fill the array. */
static void camera_fill_array_fl_v1(
        Gwn_VertBufRaw *pos_step,
        const float *coords, const uint coords_len)
{
	for (uint i = 0; i < coords_len; i++) {
		*((float *)GWN_vertbuf_raw_step(pos_step)) = coords[i];
	}
}


Gwn_Batch *DRW_cache_camera_get(void)
{
	if (!SHC.drw_camera) {
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 1, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		const int vbo_len_capacity = 22;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);
		Gwn_VertBufRaw pos_step;
		GWN_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);

		/* camera cone (from center to frame) */
		camera_fill_lines_fan_fl_v1(&pos_step, camera_coords_frame_bounds, ARRAY_SIZE(camera_coords_frame_bounds));

		/* camera frame (skip center) */
		camera_fill_lines_loop_fl_v1(&pos_step, &camera_coords_frame_bounds[1], ARRAY_SIZE(camera_coords_frame_bounds) - 1);

		/* camera triangle (above the frame) */
		camera_fill_lines_loop_fl_v1(&pos_step, camera_coords_frame_tri, ARRAY_SIZE(camera_coords_frame_tri));

		BLI_assert(vbo_len_capacity == GWN_vertbuf_raw_used(&pos_step));

		SHC.drw_camera = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_camera;
}

Gwn_Batch *DRW_cache_camera_frame_get(void)
{
	if (!SHC.drw_camera_frame) {

		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 1, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		const int vbo_len_capacity = 8;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);
		Gwn_VertBufRaw pos_step;
		GWN_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);

		/* camera frame (skip center) */
		camera_fill_lines_loop_fl_v1(&pos_step, &camera_coords_frame_bounds[1], ARRAY_SIZE(camera_coords_frame_bounds) - 1);

		BLI_assert(vbo_len_capacity == GWN_vertbuf_raw_used(&pos_step));

		SHC.drw_camera_frame = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_camera_frame;
}

Gwn_Batch *DRW_cache_camera_tria_get(void)
{
	if (!SHC.drw_camera_tria) {
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 1, GWN_FETCH_FLOAT);
		}

		/* Vertices */
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		const int vbo_len_capacity = 3;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);
		Gwn_VertBufRaw pos_step;
		GWN_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);

		/* camera triangle (above the frame) */
		camera_fill_array_fl_v1(&pos_step, camera_coords_frame_tri, ARRAY_SIZE(camera_coords_frame_tri));

		BLI_assert(vbo_len_capacity == GWN_vertbuf_raw_used(&pos_step));

		SHC.drw_camera_tria = GWN_batch_create_ex(GWN_PRIM_TRIS, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_camera_tria;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Object Mode Helpers
 * \{ */

/* Object Center */
Gwn_Batch *DRW_cache_single_vert_get(void)
{
	if (!SHC.drw_single_vertice) {
		float v1[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, 1);

		GWN_vertbuf_attr_set(vbo, attr_id.pos, 0, v1);

		SHC.drw_single_vertice = GWN_batch_create_ex(GWN_PRIM_POINTS, vbo, NULL, GWN_BATCH_OWNS_VBO);
	}
	return SHC.drw_single_vertice;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Meshes
 * \{ */

Gwn_Batch *DRW_cache_mesh_surface_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);
	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_all_triangles(me);
}

void DRW_cache_mesh_wire_overlay_get(
        Object *ob,
        Gwn_Batch **r_tris, Gwn_Batch **r_ledges, Gwn_Batch **r_lverts)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;

	*r_tris = DRW_mesh_batch_cache_get_overlay_triangles(me);
	*r_ledges = DRW_mesh_batch_cache_get_overlay_loose_edges(me);
	*r_lverts = DRW_mesh_batch_cache_get_overlay_loose_verts(me);
}

void DRW_cache_mesh_normals_overlay_get(
        Object *ob,
        Gwn_Batch **r_tris, Gwn_Batch **r_ledges, Gwn_Batch **r_lverts)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;

	*r_tris = DRW_mesh_batch_cache_get_overlay_triangles_nor(me);
	*r_ledges = DRW_mesh_batch_cache_get_overlay_loose_edges_nor(me);
	*r_lverts = DRW_mesh_batch_cache_get_overlay_loose_verts(me);
}

Gwn_Batch *DRW_cache_face_centers_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;

	return DRW_mesh_batch_cache_get_overlay_facedots(me);
}

Gwn_Batch *DRW_cache_mesh_wire_outline_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_fancy_edges(me);
}

Gwn_Batch *DRW_cache_mesh_surface_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_triangles_with_normals(me);
}

Gwn_Batch *DRW_cache_mesh_surface_weights_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_triangles_with_normals_and_weights(me, ob->actdef - 1);
}

Gwn_Batch *DRW_cache_mesh_surface_vert_colors_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_triangles_with_normals_and_vert_colors(me);
}

/* Return list of batches */
Gwn_Batch **DRW_cache_mesh_surface_shaded_get(
        Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_surface_shaded(me, gpumat_array, gpumat_array_len);
}

/* Return list of batches */
Gwn_Batch **DRW_cache_mesh_surface_texpaint_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_surface_texpaint(me);
}

Gwn_Batch *DRW_cache_mesh_surface_texpaint_single_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_surface_texpaint_single(me);
}

Gwn_Batch *DRW_cache_mesh_surface_verts_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_points_with_normals(me);
}

Gwn_Batch *DRW_cache_mesh_edges_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_all_edges(me);
}

Gwn_Batch *DRW_cache_mesh_verts_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_all_verts(me);
}

Gwn_Batch *DRW_cache_mesh_edges_paint_overlay_get(Object *ob, bool use_wire, bool use_sel)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_weight_overlay_edges(me, use_wire, use_sel);
}

Gwn_Batch *DRW_cache_mesh_faces_weight_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	return DRW_mesh_batch_cache_get_weight_overlay_faces(me);
}

Gwn_Batch *DRW_cache_mesh_verts_weight_overlay_get(Object *ob)
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

Gwn_Batch *DRW_cache_curve_edge_wire_get(Object *ob)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_wire_edge(cu, ob->curve_cache);
}

Gwn_Batch *DRW_cache_curve_edge_normal_get(Object *ob, float normal_size)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_normal_edge(cu, ob->curve_cache, normal_size);
}

Gwn_Batch *DRW_cache_curve_edge_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_overlay_edges(cu);
}

Gwn_Batch *DRW_cache_curve_vert_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_overlay_verts(cu);
}

Gwn_Batch *DRW_cache_curve_surface_get(Object *ob)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_triangles_with_normals(cu, ob->curve_cache);
}

/* Return list of batches */
Gwn_Batch **DRW_cache_curve_surface_shaded_get(
        Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len)
{
	BLI_assert(ob->type == OB_CURVE);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_surface_shaded(cu, ob->curve_cache, gpumat_array, gpumat_array_len);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name MetaBall
 * \{ */

Gwn_Batch *DRW_cache_mball_surface_get(Object *ob)
{
	BLI_assert(ob->type == OB_MBALL);
	return DRW_metaball_batch_cache_get_triangles_with_normals(ob);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Font
 * \{ */

Gwn_Batch *DRW_cache_text_edge_wire_get(Object *ob)
{
	BLI_assert(ob->type == OB_FONT);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_wire_edge(cu, ob->curve_cache);
}

Gwn_Batch *DRW_cache_text_surface_get(Object *ob)
{
	BLI_assert(ob->type == OB_FONT);
	struct Curve *cu = ob->data;
	if (cu->editfont && (cu->flag & CU_FAST)) {
		return NULL;
	}
	return DRW_curve_batch_cache_get_triangles_with_normals(cu, ob->curve_cache);
}

Gwn_Batch **DRW_cache_text_surface_shaded_get(
        Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len)
{
	BLI_assert(ob->type == OB_FONT);
	struct Curve *cu = ob->data;
	if (cu->editfont && (cu->flag & CU_FAST)) {
		return NULL;
	}
	return DRW_curve_batch_cache_get_surface_shaded(cu, ob->curve_cache, gpumat_array, gpumat_array_len);
}

Gwn_Batch *DRW_cache_text_cursor_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_FONT);
	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_overlay_cursor(cu);
}

Gwn_Batch *DRW_cache_text_select_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_FONT);
	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_overlay_select(cu);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Surface
 * \{ */

Gwn_Batch *DRW_cache_surf_surface_get(Object *ob)
{
	BLI_assert(ob->type == OB_SURF);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_triangles_with_normals(cu, ob->curve_cache);
}

/* Return list of batches */
Gwn_Batch **DRW_cache_surf_surface_shaded_get(
        Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len)
{
	BLI_assert(ob->type == OB_SURF);

	struct Curve *cu = ob->data;
	return DRW_curve_batch_cache_get_surface_shaded(cu, ob->curve_cache, gpumat_array, gpumat_array_len);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Lattice
 * \{ */

Gwn_Batch *DRW_cache_lattice_verts_get(Object *ob)
{
	BLI_assert(ob->type == OB_LATTICE);

	struct Lattice *lt = ob->data;
	return DRW_lattice_batch_cache_get_all_verts(lt);
}

Gwn_Batch *DRW_cache_lattice_wire_get(Object *ob, bool use_weight)
{
	BLI_assert(ob->type == OB_LATTICE);

	Lattice *lt = ob->data;
	int actdef = -1;

	if (use_weight && ob->defbase.first && lt->editlatt->latt->dvert) {
		actdef = ob->actdef - 1;
	}

	return DRW_lattice_batch_cache_get_all_edges(lt, use_weight, actdef);
}

Gwn_Batch *DRW_cache_lattice_vert_overlay_get(Object *ob)
{
	BLI_assert(ob->type == OB_LATTICE);

	struct Lattice *lt = ob->data;
	return DRW_lattice_batch_cache_get_overlay_verts(lt);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Particles
 * \{ */

Gwn_Batch *DRW_cache_particles_get_hair(ParticleSystem *psys, ModifierData *md)
{
	return DRW_particles_batch_cache_get_hair(psys, md);
}

Gwn_Batch *DRW_cache_particles_get_dots(ParticleSystem *psys)
{
	return DRW_particles_batch_cache_get_dots(psys);
}

Gwn_Batch *DRW_cache_particles_get_prim(int type)
{
	switch (type) {
		case PART_DRAW_CROSS:
			if (!SHC.drw_particle_cross) {
				static Gwn_VertFormat format = { 0 };
				static unsigned pos_id, axis_id;

				if (format.attrib_ct == 0) {
					pos_id = GWN_vertformat_attr_add(&format, "inst_pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
					axis_id = GWN_vertformat_attr_add(&format, "axis", GWN_COMP_I32, 1, GWN_FETCH_INT);
				}

				Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
				GWN_vertbuf_data_alloc(vbo, 6);

				/* X axis */
				float co[3] = {-1.0f, 0.0f, 0.0f};
				int axis = -1;
				GWN_vertbuf_attr_set(vbo, pos_id, 0, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 0, &axis);

				co[0] = 1.0f;
				GWN_vertbuf_attr_set(vbo, pos_id, 1, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 1, &axis);

				/* Y axis */
				co[0] = 0.0f;
				co[1] = -1.0f;
				GWN_vertbuf_attr_set(vbo, pos_id, 2, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 2, &axis);

				co[1] = 1.0f;
				GWN_vertbuf_attr_set(vbo, pos_id, 3, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 3, &axis);

				/* Z axis */
				co[1] = 0.0f;
				co[2] = -1.0f;
				GWN_vertbuf_attr_set(vbo, pos_id, 4, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 4, &axis);

				co[2] = 1.0f;
				GWN_vertbuf_attr_set(vbo, pos_id, 5, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 5, &axis);

				SHC.drw_particle_cross = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
			}

			return SHC.drw_particle_cross;
		case PART_DRAW_AXIS:
			if (!SHC.drw_particle_axis) {
				static Gwn_VertFormat format = { 0 };
				static unsigned pos_id, axis_id;

				if (format.attrib_ct == 0) {
					pos_id = GWN_vertformat_attr_add(&format, "inst_pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
					axis_id = GWN_vertformat_attr_add(&format, "axis", GWN_COMP_I32, 1, GWN_FETCH_INT);
				}

				Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
				GWN_vertbuf_data_alloc(vbo, 6);

				/* X axis */
				float co[3] = {0.0f, 0.0f, 0.0f};
				int axis = 0;
				GWN_vertbuf_attr_set(vbo, pos_id, 0, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 0, &axis);

				co[0] = 1.0f;
				GWN_vertbuf_attr_set(vbo, pos_id, 1, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 1, &axis);

				/* Y axis */
				co[0] = 0.0f;
				axis = 1;
				GWN_vertbuf_attr_set(vbo, pos_id, 2, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 2, &axis);

				co[1] = 1.0f;
				GWN_vertbuf_attr_set(vbo, pos_id, 3, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 3, &axis);

				/* Z axis */
				co[1] = 0.0f;
				axis = 2;
				GWN_vertbuf_attr_set(vbo, pos_id, 4, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 4, &axis);

				co[2] = 1.0f;
				GWN_vertbuf_attr_set(vbo, pos_id, 5, co);
				GWN_vertbuf_attr_set(vbo, axis_id, 5, &axis);

				SHC.drw_particle_axis = GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
			}

			return SHC.drw_particle_axis;
		case PART_DRAW_CIRC:
#define CIRCLE_RESOL 32
			if (!SHC.drw_particle_circle) {
				float v[3] = {0.0f, 0.0f, 0.0f};
				int axis = -1;

				static Gwn_VertFormat format = { 0 };
				static unsigned pos_id, axis_id;

				if (format.attrib_ct == 0) {
					pos_id = GWN_vertformat_attr_add(&format, "inst_pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
					axis_id = GWN_vertformat_attr_add(&format, "axis", GWN_COMP_I32, 1, GWN_FETCH_INT);
				}

				Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
				GWN_vertbuf_data_alloc(vbo, CIRCLE_RESOL);

				for (int a = 0; a < CIRCLE_RESOL; a++) {
					v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
					v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
					v[2] = 0.0f;
					GWN_vertbuf_attr_set(vbo, pos_id, a, v);
					GWN_vertbuf_attr_set(vbo, axis_id, a, &axis);
				}

				SHC.drw_particle_circle = GWN_batch_create_ex(GWN_PRIM_LINE_LOOP, vbo, NULL, GWN_BATCH_OWNS_VBO);
			}

			return SHC.drw_particle_circle;
#undef CIRCLE_RESOL
		default:
			BLI_assert(false);
			break;
	}

	return NULL;
}
