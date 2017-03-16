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
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_mesh_render.h"

#include "GPU_batch.h"

#include "draw_cache.h"

static struct DRWShapeCache {
	Batch *drw_single_vertice;
	Batch *drw_fullscreen_quad;
	Batch *drw_plain_axes;
	Batch *drw_single_arrow;
	Batch *drw_cube;
	Batch *drw_circle;
	Batch *drw_square;
	Batch *drw_line;
	Batch *drw_line_endpoints;
	Batch *drw_empty_sphere;
	Batch *drw_empty_cone;
	Batch *drw_arrows;
	Batch *drw_axis_names;
	Batch *drw_lamp;
	Batch *drw_lamp_sunrays;
	Batch *drw_lamp_area;
	Batch *drw_lamp_hemi;
	Batch *drw_lamp_spot;
	Batch *drw_lamp_spot_square;
	Batch *drw_speaker;
	Batch *drw_bone_octahedral;
	Batch *drw_bone_octahedral_wire;
	Batch *drw_bone_point;
	Batch *drw_bone_point_wire;
	Batch *drw_bone_arrows;
	Batch *drw_camera;
	Batch *drw_camera_tria;
	Batch *drw_camera_focus;
} SHC = {NULL};

void DRW_shape_cache_free(void)
{
	if (SHC.drw_single_vertice)
		Batch_discard_all(SHC.drw_single_vertice);
	if (SHC.drw_fullscreen_quad)
		Batch_discard_all(SHC.drw_fullscreen_quad);
	if (SHC.drw_plain_axes)
		Batch_discard_all(SHC.drw_plain_axes);
	if (SHC.drw_single_arrow)
		Batch_discard_all(SHC.drw_single_arrow);
	if (SHC.drw_cube)
		Batch_discard_all(SHC.drw_cube);
	if (SHC.drw_circle)
		Batch_discard_all(SHC.drw_circle);
	if (SHC.drw_square)
		Batch_discard_all(SHC.drw_square);
	if (SHC.drw_line)
		Batch_discard_all(SHC.drw_line);
	if (SHC.drw_line_endpoints)
		Batch_discard_all(SHC.drw_line_endpoints);
	if (SHC.drw_empty_sphere)
		Batch_discard_all(SHC.drw_empty_sphere);
	if (SHC.drw_empty_cone)
		Batch_discard_all(SHC.drw_empty_cone);
	if (SHC.drw_arrows)
		Batch_discard_all(SHC.drw_arrows);
	if (SHC.drw_axis_names)
		Batch_discard_all(SHC.drw_axis_names);
	if (SHC.drw_lamp)
		Batch_discard_all(SHC.drw_lamp);
	if (SHC.drw_lamp_sunrays)
		Batch_discard_all(SHC.drw_lamp_sunrays);
	if (SHC.drw_lamp_area)
		Batch_discard_all(SHC.drw_lamp_area);
	if (SHC.drw_lamp_hemi)
		Batch_discard_all(SHC.drw_lamp_hemi);
	if (SHC.drw_lamp_spot)
		Batch_discard_all(SHC.drw_lamp_spot);
	if (SHC.drw_lamp_spot_square)
		Batch_discard_all(SHC.drw_lamp_spot_square);
	if (SHC.drw_speaker)
		Batch_discard_all(SHC.drw_speaker);
	if (SHC.drw_bone_octahedral)
		Batch_discard_all(SHC.drw_bone_octahedral);
	if (SHC.drw_bone_octahedral_wire)
		Batch_discard_all(SHC.drw_bone_octahedral_wire);
	if (SHC.drw_bone_point)
		Batch_discard_all(SHC.drw_bone_point);
	if (SHC.drw_bone_point_wire)
		Batch_discard_all(SHC.drw_bone_point_wire);
	if (SHC.drw_bone_arrows)
		Batch_discard_all(SHC.drw_bone_arrows);
	if (SHC.drw_camera)
		Batch_discard_all(SHC.drw_camera);
	if (SHC.drw_camera_tria)
		Batch_discard_all(SHC.drw_camera_tria);
	if (SHC.drw_camera_focus)
		Batch_discard_all(SHC.drw_camera_focus);
}

/* Helper functions */

static void add_fancy_edge(VertexBuffer *vbo, unsigned int pos_id, unsigned int n1_id, unsigned int n2_id,
                           unsigned int *v_idx, const float co1[3], const float co2[3],
                           const float n1[3], const float n2[3])
{
	setAttrib(vbo, n1_id, *v_idx, n1);
	setAttrib(vbo, n2_id, *v_idx, n2);
	setAttrib(vbo, pos_id, (*v_idx)++, co1);

	setAttrib(vbo, n1_id, *v_idx, n1);
	setAttrib(vbo, n2_id, *v_idx, n2);
	setAttrib(vbo, pos_id, (*v_idx)++, co2);
}

static void add_lat_lon_vert(VertexBuffer *vbo, unsigned int pos_id, unsigned int nor_id,
                             unsigned int *v_idx, const float rad, const float lat, const float lon)
{
	float pos[3], nor[3];
	nor[0] = sinf(lat) * cosf(lon);
	nor[1] = cosf(lat);
	nor[2] = sinf(lat) * sinf(lon);
	mul_v3_v3fl(pos, nor, rad);

	setAttrib(vbo, nor_id, *v_idx, nor);
	setAttrib(vbo, pos_id, (*v_idx)++, pos);
}

static VertexBuffer *fill_arrows_vbo(const float scale)
{
	/* Position Only 3D format */
	static VertexFormat format = { 0 };
	static unsigned pos_id;
	if (format.attrib_ct == 0) {
		pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
	}

	/* Line */
	VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
	VertexBuffer_allocate_data(vbo, 6 * 3);

	float v1[3] = {0.0, 0.0, 0.0};
	float v2[3] = {0.0, 0.0, 0.0};
	float vtmp1[3], vtmp2[3];

	for (int axis = 0; axis < 3; axis++) {
		const int arrow_axis = (axis == 0) ? 1 : 0;

		v2[axis] = 1.0f;
		mul_v3_v3fl(vtmp1, v1, scale);
		mul_v3_v3fl(vtmp2, v2, scale);
		setAttrib(vbo, pos_id, axis * 6 + 0, vtmp1);
		setAttrib(vbo, pos_id, axis * 6 + 1, vtmp2);

		v1[axis] = 0.85f;
		v1[arrow_axis] = -0.08f;
		mul_v3_v3fl(vtmp1, v1, scale);
		mul_v3_v3fl(vtmp2, v2, scale);
		setAttrib(vbo, pos_id, axis * 6 + 2, vtmp1);
		setAttrib(vbo, pos_id, axis * 6 + 3, vtmp2);

		v1[arrow_axis] = 0.08f;
		mul_v3_v3fl(vtmp1, v1, scale);
		mul_v3_v3fl(vtmp2, v2, scale);
		setAttrib(vbo, pos_id, axis * 6 + 4, vtmp1);
		setAttrib(vbo, pos_id, axis * 6 + 5, vtmp2);

		/* reset v1 & v2 to zero */
		v1[arrow_axis] = v1[axis] = v2[axis] = 0.0f;
	}

	return vbo;
}

static VertexBuffer *sphere_wire_vbo(const float rad)
{
#define NSEGMENTS 16
	/* Position Only 3D format */
	static VertexFormat format = { 0 };
	static unsigned pos_id;
	if (format.attrib_ct == 0) {
		pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
	}

	VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
	VertexBuffer_allocate_data(vbo, NSEGMENTS * 2 * 3);

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

				setAttrib(vbo, pos_id, i * 2 + j + (NSEGMENTS * 2 * axis), v);
			}
		}
	}

	return vbo;
#undef NSEGMENTS
}

/* Quads */
Batch *DRW_cache_fullscreen_quad_get(void)
{
	if (!SHC.drw_fullscreen_quad) {
		float pos[4][2] = {{-1.0f, -1.0f}, { 1.0f, -1.0f}, {-1.0f,  1.0f}, { 1.0f,  1.0f}};
		float uvs[4][2] = {{ 0.0f,  0.0f}, { 1.0f,  0.0f}, { 0.0f,  1.0f}, { 1.0f,  1.0f}};

		/* Position Only 2D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id, uvs_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 2, KEEP_FLOAT);
			uvs_id = add_attrib(&format, "uvs", GL_FLOAT, 2, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 4);

		for (int i = 0; i < 4; ++i)	{
			setAttrib(vbo, pos_id, i, pos[i]);
			setAttrib(vbo, uvs_id, i, uvs[i]);
		}

		SHC.drw_fullscreen_quad = Batch_create(GL_TRIANGLE_STRIP, vbo, NULL);
	}
	return SHC.drw_fullscreen_quad;
}

/* Common */

Batch *DRW_cache_cube_get(void)
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
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 24);

		for (int i = 0; i < 24; ++i) {
			setAttrib(vbo, pos_id, i, verts[indices[i]]);
		}

		SHC.drw_cube = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_cube;
}

Batch *DRW_cache_circle_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_circle) {
		float v[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, CIRCLE_RESOL * 2);

		for (int a = 0; a < CIRCLE_RESOL; a++) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[2] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[1] = 0.0f;
			setAttrib(vbo, pos_id, a * 2, v);

			v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[2] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[1] = 0.0f;
			setAttrib(vbo, pos_id, a * 2 + 1, v);
		}

		SHC.drw_circle = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_circle;
#undef CIRCLE_RESOL
}

Batch *DRW_cache_square_get(void)
{
	if (!SHC.drw_square) {
		float p[4][3] = {{ 1.0f, 0.0f,  1.0f},
		                 { 1.0f, 0.0f, -1.0f},
		                 {-1.0f, 0.0f, -1.0f},
		                 {-1.0f, 0.0f,  1.0f}};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 8);

		for (int i = 0; i < 4; i++) {
			setAttrib(vbo, pos_id, i * 2,     p[i % 4]);
			setAttrib(vbo, pos_id, i * 2 + 1, p[(i+1) % 4]);
		}

		SHC.drw_square = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_square;
}

Batch *DRW_cache_single_line_get(void)
{
	/* Z axis line */
	if (!SHC.drw_line) {
		float v1[3] = {0.0f, 0.0f, 0.0f};
		float v2[3] = {0.0f, 0.0f, 1.0f};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 2);

		setAttrib(vbo, pos_id, 0, v1);
		setAttrib(vbo, pos_id, 1, v2);

		SHC.drw_line = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_line;
}


Batch *DRW_cache_single_line_endpoints_get(void)
{
	/* Z axis line */
	if (!SHC.drw_line_endpoints) {
		float v1[3] = {0.0f, 0.0f, 0.0f};
		float v2[3] = {0.0f, 0.0f, 1.0f};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 2);

		setAttrib(vbo, pos_id, 0, v1);
		setAttrib(vbo, pos_id, 1, v2);

		SHC.drw_line_endpoints = Batch_create(GL_POINTS, vbo, NULL);
	}
	return SHC.drw_line_endpoints;
}

/* Empties */
Batch *DRW_cache_plain_axes_get(void)
{
	if (!SHC.drw_plain_axes) {
		int axis;
		float v1[3] = {0.0f, 0.0f, 0.0f};
		float v2[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 6);

		for (axis = 0; axis < 3; axis++) {
			v1[axis] = 1.0f;
			v2[axis] = -1.0f;

			setAttrib(vbo, pos_id, axis * 2, v1);
			setAttrib(vbo, pos_id, axis * 2 + 1, v2);

			/* reset v1 & v2 to zero for next axis */
			v1[axis] = v2[axis] = 0.0f;
		}

		SHC.drw_plain_axes = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_plain_axes;
}

Batch *DRW_cache_single_arrow_get(void)
{
	if (!SHC.drw_single_arrow) {
		float v1[3] = {0.0f, 0.0f, 1.0f}, v2[3], v3[3];

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		/* Square Pyramid */
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 12);

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

			setAttrib(vbo, pos_id, sides * 3 + 0, v1);
			setAttrib(vbo, pos_id, sides * 3 + 1, v2);
			setAttrib(vbo, pos_id, sides * 3 + 2, v3);
		}

		SHC.drw_single_arrow = Batch_create(GL_TRIANGLES, vbo, NULL);
	}
	return SHC.drw_single_arrow;
}

Batch *DRW_cache_empty_sphere_get(void)
{
	if (!SHC.drw_empty_sphere) {
		VertexBuffer *vbo = sphere_wire_vbo(1.0f);
		SHC.drw_empty_sphere = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_empty_sphere;
}

Batch *DRW_cache_empty_cone_get(void)
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
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, NSEGMENTS * 4);

		for (int i = 0; i < NSEGMENTS; ++i) {
			float cv[2], v[3];
			cv[0] = p[(i) % NSEGMENTS][0];
			cv[1] = p[(i) % NSEGMENTS][1];

			/* cone sides */
			v[0] = cv[0], v[1] = 0.0f, v[2] = cv[1];
			setAttrib(vbo, pos_id, i * 4, v);
			v[0] = 0.0f, v[1] = 2.0f, v[2] = 0.0f;
			setAttrib(vbo, pos_id, i * 4 + 1, v);

			/* end ring */
			v[0] = cv[0], v[1] = 0.0f, v[2] = cv[1];
			setAttrib(vbo, pos_id, i * 4 + 2, v);
			cv[0] = p[(i + 1) % NSEGMENTS][0];
			cv[1] = p[(i + 1) % NSEGMENTS][1];
			v[0] = cv[0], v[1] = 0.0f, v[2] = cv[1];
			setAttrib(vbo, pos_id, i * 4 + 3, v);
		}

		SHC.drw_empty_cone = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_empty_cone;
#undef NSEGMENTS
}

Batch *DRW_cache_arrows_get(void)
{
	if (!SHC.drw_arrows) {
		VertexBuffer *vbo = fill_arrows_vbo(1.0f);

		SHC.drw_arrows = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_arrows;
}

Batch *DRW_cache_axis_names_get(void)
{
	if (!SHC.drw_axis_names) {
		const float size = 0.1f;
		float v1[3], v2[3];

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			/* Using 3rd component as axis indicator */
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		/* Line */
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 14);

		/* X */
		copy_v3_fl3(v1, -size,  size, 0.0f);
		copy_v3_fl3(v2,  size, -size, 0.0f);
		setAttrib(vbo, pos_id, 0, v1);
		setAttrib(vbo, pos_id, 1, v2);

		copy_v3_fl3(v1,  size,  size, 0.0f);
		copy_v3_fl3(v2, -size, -size, 0.0f);
		setAttrib(vbo, pos_id, 2, v1);
		setAttrib(vbo, pos_id, 3, v2);

		/* Y */
		copy_v3_fl3(v1, -size + 0.25f * size,  size, 1.0f);
		copy_v3_fl3(v2,  0.0f,  0.0f, 1.0f);
		setAttrib(vbo, pos_id, 4, v1);
		setAttrib(vbo, pos_id, 5, v2);

		copy_v3_fl3(v1,  size - 0.25f * size,  size, 1.0f);
		copy_v3_fl3(v2, -size + 0.25f * size, -size, 1.0f);
		setAttrib(vbo, pos_id, 6, v1);
		setAttrib(vbo, pos_id, 7, v2);

		/* Z */
		copy_v3_fl3(v1, -size,  size, 2.0f);
		copy_v3_fl3(v2,  size,  size, 2.0f);
		setAttrib(vbo, pos_id, 8, v1);
		setAttrib(vbo, pos_id, 9, v2);

		copy_v3_fl3(v1,  size,  size, 2.0f);
		copy_v3_fl3(v2, -size, -size, 2.0f);
		setAttrib(vbo, pos_id, 10, v1);
		setAttrib(vbo, pos_id, 11, v2);

		copy_v3_fl3(v1, -size, -size, 2.0f);
		copy_v3_fl3(v2,  size, -size, 2.0f);
		setAttrib(vbo, pos_id, 12, v1);
		setAttrib(vbo, pos_id, 13, v2);

		SHC.drw_axis_names = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_axis_names;
}

/* Lamps */
Batch *DRW_cache_lamp_get(void)
{
#define NSEGMENTS 8
	if (!SHC.drw_lamp) {
		float v[2];

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 2, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, NSEGMENTS * 2);

		for (int a = 0; a < NSEGMENTS; a++) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)NSEGMENTS));
			v[1] = cosf((2.0f * M_PI * a) / ((float)NSEGMENTS));
			setAttrib(vbo, pos_id, a * 2, v);

			v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)NSEGMENTS));
			v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)NSEGMENTS));
			setAttrib(vbo, pos_id, a * 2 + 1, v);
		}

		SHC.drw_lamp = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_lamp;
#undef NSEGMENTS
}

Batch *DRW_cache_lamp_sunrays_get(void)
{
	if (!SHC.drw_lamp_sunrays) {
		float v[2], v1[2], v2[2];

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 2, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 16);

		for (int a = 0; a < 8; a++) {
			v[0] = sinf((2.0f * M_PI * a) / 8.0f);
			v[1] = cosf((2.0f * M_PI * a) / 8.0f);

			mul_v2_v2fl(v1, v, 1.2f);
			mul_v2_v2fl(v2, v, 2.5f);

			setAttrib(vbo, pos_id, a * 2, v1);
			setAttrib(vbo, pos_id, a * 2 + 1, v2);
		}

		SHC.drw_lamp_sunrays = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_lamp_sunrays;
}

Batch *DRW_cache_lamp_area_get(void)
{
	if (!SHC.drw_lamp_area) {
		float v1[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 8);

		v1[0] = v1[1] = 0.5f;
		setAttrib(vbo, pos_id, 0, v1);
		v1[0] = -0.5f;
		setAttrib(vbo, pos_id, 1, v1);
		setAttrib(vbo, pos_id, 2, v1);
		v1[1] = -0.5f;
		setAttrib(vbo, pos_id, 3, v1);
		setAttrib(vbo, pos_id, 4, v1);
		v1[0] = 0.5f;
		setAttrib(vbo, pos_id, 5, v1);
		setAttrib(vbo, pos_id, 6, v1);
		v1[1] = 0.5f;
		setAttrib(vbo, pos_id, 7, v1);

		SHC.drw_lamp_area = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_lamp_area;
}

Batch *DRW_cache_lamp_hemi_get(void)
{
#define CIRCLE_RESOL 32
	if (!SHC.drw_lamp_hemi) {
		float v[3];
		int vidx = 0;

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, CIRCLE_RESOL * 2 * 2 - 6 * 2 * 2);

		/* XZ plane */
		for (int a = 3; a < CIRCLE_RESOL / 2 - 3; a++) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL) - M_PI / 2);
			v[2] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL) - M_PI / 2) - 1.0f;
			v[1] = 0.0f;
			setAttrib(vbo, pos_id, vidx++, v);

			v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL) - M_PI / 2);
			v[2] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL) - M_PI / 2) - 1.0f;
			v[1] = 0.0f;
			setAttrib(vbo, pos_id, vidx++, v);
		}

		/* XY plane */
		for (int a = 3; a < CIRCLE_RESOL / 2 - 3; a++) {
			v[2] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL)) - 1.0f;
			v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[0] = 0.0f;
			setAttrib(vbo, pos_id, vidx++, v);

			v[2] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL)) - 1.0f;
			v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[0] = 0.0f;
			setAttrib(vbo, pos_id, vidx++, v);
		}

		/* YZ plane full circle */
		/* lease v[2] as it is */
		const float rad = cosf((2.0f * M_PI * 3) / ((float)CIRCLE_RESOL));
		for (int a = 0; a < CIRCLE_RESOL; a++) {
			v[1] = rad * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[0] = rad * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			setAttrib(vbo, pos_id, vidx++, v);

			v[1] = rad * sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[0] = rad * cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			setAttrib(vbo, pos_id, vidx++, v);
		}


		SHC.drw_lamp_hemi = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_lamp_hemi;
#undef CIRCLE_RESOL
}


Batch *DRW_cache_lamp_spot_get(void)
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
		static VertexFormat format = { 0 };
		static unsigned int pos_id, n1_id, n2_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
			n1_id = add_attrib(&format, "N1", GL_FLOAT, 3, KEEP_FLOAT);
			n2_id = add_attrib(&format, "N2", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, NSEGMENTS * 4);

		for (int i = 0; i < NSEGMENTS; ++i) {
			float cv[2], v[3];
			cv[0] = p[i % NSEGMENTS][0];
			cv[1] = p[i % NSEGMENTS][1];

			/* cone sides */
			v[0] = cv[0], v[1] = cv[1], v[2] = -1.0f;
			setAttrib(vbo, pos_id, i * 4, v);
			v[0] = 0.0f, v[1] = 0.0f, v[2] = 0.0f;
			setAttrib(vbo, pos_id, i * 4 + 1, v);

			setAttrib(vbo, n1_id, i * 4,     n[(i) % NSEGMENTS]);
			setAttrib(vbo, n1_id, i * 4 + 1, n[(i) % NSEGMENTS]);
			setAttrib(vbo, n2_id, i * 4,     n[(i+1) % NSEGMENTS]);
			setAttrib(vbo, n2_id, i * 4 + 1, n[(i+1) % NSEGMENTS]);

			/* end ring */
			v[0] = cv[0], v[1] = cv[1], v[2] = -1.0f;
			setAttrib(vbo, pos_id, i * 4 + 2, v);
			cv[0] = p[(i + 1) % NSEGMENTS][0];
			cv[1] = p[(i + 1) % NSEGMENTS][1];
			v[0] = cv[0], v[1] = cv[1], v[2] = -1.0f;
			setAttrib(vbo, pos_id, i * 4 + 3, v);

			setAttrib(vbo, n1_id, i * 4 + 2, n[(i) % NSEGMENTS]);
			setAttrib(vbo, n1_id, i * 4 + 3, n[(i) % NSEGMENTS]);
			setAttrib(vbo, n2_id, i * 4 + 2, neg[(i) % NSEGMENTS]);
			setAttrib(vbo, n2_id, i * 4 + 3, neg[(i) % NSEGMENTS]);
		}

		SHC.drw_lamp_spot = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_lamp_spot;
#undef NSEGMENTS
}

Batch *DRW_cache_lamp_spot_square_get(void)
{
	if (!SHC.drw_lamp_spot_square) {
		float p[5][3] = {{ 0.0f,  0.0f,  0.0f},
		                 { 1.0f,  1.0f, -1.0f},
		                 { 1.0f, -1.0f, -1.0f},
		                 {-1.0f, -1.0f, -1.0f},
		                 {-1.0f,  1.0f, -1.0f}};

		unsigned int v_idx = 0;

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned int pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 16);

		/* piramid sides */
		for (int i = 1; i <= 4; ++i) {
			setAttrib(vbo, pos_id, v_idx++, p[0]);
			setAttrib(vbo, pos_id, v_idx++, p[i]);

			setAttrib(vbo, pos_id, v_idx++, p[(i % 4)+1]);
			setAttrib(vbo, pos_id, v_idx++, p[((i+1) % 4)+1]);
		}

		SHC.drw_lamp_spot_square = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_lamp_spot_square;
}

/* Speaker */
Batch *DRW_cache_speaker_get(void)
{
	if (!SHC.drw_speaker) {
		float v[3];
		const int segments = 16;
		int vidx = 0;

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 3 * segments * 2 + 4 * 4);

		for (int j = 0; j < 3; j++) {
			float z = 0.25f * j - 0.125f;
			float r = (j == 0 ? 0.5f : 0.25f);

			copy_v3_fl3(v, r, 0.0f, z);
			setAttrib(vbo, pos_id, vidx++, v);
			for (int i = 1; i < segments; i++) {
				float x = cosf(2.f * (float)M_PI * i / segments) * r;
				float y = sinf(2.f * (float)M_PI * i / segments) * r;
				copy_v3_fl3(v, x, y, z);
				setAttrib(vbo, pos_id, vidx++, v);
				setAttrib(vbo, pos_id, vidx++, v);
			}
			copy_v3_fl3(v, r, 0.0f, z);
			setAttrib(vbo, pos_id, vidx++, v);
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
				setAttrib(vbo, pos_id, vidx++, v);
				if (i == 1) {
					setAttrib(vbo, pos_id, vidx++, v);
				}
			}
		}

		SHC.drw_speaker = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_speaker;
}

/* Armature bones */
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

Batch *DRW_cache_bone_octahedral_get(void)
{
	if (!SHC.drw_bone_octahedral) {
		unsigned int v_idx = 0;

		static VertexFormat format = { 0 };
		static unsigned pos_id, nor_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
			nor_id = add_attrib(&format, "nor", GL_FLOAT, 3, KEEP_FLOAT);
		}

		/* Vertices */
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 24);

		for (int i = 0; i < 8; i++) {
			setAttrib(vbo, nor_id, v_idx, bone_octahedral_solid_normals[i]);
			setAttrib(vbo, pos_id, v_idx++, bone_octahedral_verts[bone_octahedral_solid_tris[i][0]]);
			setAttrib(vbo, nor_id, v_idx, bone_octahedral_solid_normals[i]);
			setAttrib(vbo, pos_id, v_idx++, bone_octahedral_verts[bone_octahedral_solid_tris[i][1]]);
			setAttrib(vbo, nor_id, v_idx, bone_octahedral_solid_normals[i]);
			setAttrib(vbo, pos_id, v_idx++, bone_octahedral_verts[bone_octahedral_solid_tris[i][2]]);
		}

		SHC.drw_bone_octahedral = Batch_create(GL_TRIANGLES, vbo, NULL);
	}
	return SHC.drw_bone_octahedral;
}

Batch *DRW_cache_bone_octahedral_wire_outline_get(void)
{
	if (!SHC.drw_bone_octahedral_wire) {
		unsigned int v_idx = 0;

		static VertexFormat format = { 0 };
		static unsigned pos_id, n1_id, n2_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
			n1_id = add_attrib(&format, "N1", COMP_F32, 3, KEEP_FLOAT);
			n2_id = add_attrib(&format, "N2", COMP_F32, 3, KEEP_FLOAT);
		}

		/* Vertices */
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 12 * 2);

		for (int i = 0; i < 12; i++) {
			const float *co1 = bone_octahedral_verts[bone_octahedral_wire[i * 2]];
			const float *co2 = bone_octahedral_verts[bone_octahedral_wire[i * 2 + 1]];
			const float *n1 = bone_octahedral_solid_normals[bone_octahedral_wire_adjacent_face[i * 2]];
			const float *n2 = bone_octahedral_solid_normals[bone_octahedral_wire_adjacent_face[i * 2 + 1]];
			add_fancy_edge(vbo, pos_id, n1_id, n2_id, &v_idx, co1, co2, n1, n2);
		}

		SHC.drw_bone_octahedral_wire = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_bone_octahedral_wire;
}

Batch *DRW_cache_bone_point_get(void)
{
	if (!SHC.drw_bone_point) {
		const int lon_res = 16;
		const int lat_res = 8;
		const float rad = 0.05f;
		const float lon_inc = 2 * M_PI / lon_res;
		const float lat_inc = M_PI / lat_res;
		unsigned int v_idx = 0;

		static VertexFormat format = { 0 };
		static unsigned pos_id, nor_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
			nor_id = add_attrib(&format, "nor", GL_FLOAT, 3, KEEP_FLOAT);
		}

		/* Vertices */
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, (lat_res - 1) * lon_res * 6);

		float lon = 0.0f;
		for (int i = 0; i < lon_res; i++, lon += lon_inc) {
			float lat = 0.0f;
			for (int j = 0; j < lat_res; j++, lat += lat_inc) {
				if (j != lat_res - 1) { /* Pole */
					add_lat_lon_vert(vbo, pos_id, nor_id, &v_idx, rad, lat + lat_inc, lon + lon_inc);
					add_lat_lon_vert(vbo, pos_id, nor_id, &v_idx, rad, lat + lat_inc, lon);
					add_lat_lon_vert(vbo, pos_id, nor_id, &v_idx, rad, lat,           lon);
				}

				if (j != 0) { /* Pole */
					add_lat_lon_vert(vbo, pos_id, nor_id, &v_idx, rad, lat,           lon + lon_inc);
					add_lat_lon_vert(vbo, pos_id, nor_id, &v_idx, rad, lat + lat_inc, lon + lon_inc);
					add_lat_lon_vert(vbo, pos_id, nor_id, &v_idx, rad, lat,           lon);
				}
			}
		}

		SHC.drw_bone_point = Batch_create(GL_TRIANGLES, vbo, NULL);
	}
	return SHC.drw_bone_point;
}

Batch *DRW_cache_bone_point_wire_outline_get(void)
{
	if (!SHC.drw_bone_point_wire) {
		VertexBuffer *vbo = sphere_wire_vbo(0.05f);
		SHC.drw_bone_point_wire = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_bone_point_wire;
}

Batch *DRW_cache_bone_arrows_get(void)
{
	if (!SHC.drw_bone_arrows) {
		VertexBuffer *vbo = fill_arrows_vbo(0.25f);
		SHC.drw_bone_arrows = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_bone_arrows;
}

/* Camera */
Batch *DRW_cache_camera_get(void)
{
	if (!SHC.drw_camera) {
		float v0 = 0.0f; /* Center point */
		float v1 = 1.0f; /* + X + Y */
		float v2 = 2.0f; /* + X - Y */
		float v3 = 3.0f; /* - X - Y */
		float v4 = 4.0f; /* - X + Y */
		float v5 = 5.0f; /* tria + X */
		float v6 = 6.0f; /* tria - X */
		float v7 = 7.0f; /* tria + Y */
		int v_idx = 0;

		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			/* use x coordinate to identify the vertex
			 * the vertex shader take care to place it
			 * appropriatelly */
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 1, KEEP_FLOAT);
		}

		/* Vertices */
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 22);

		setAttrib(vbo, pos_id, v_idx++, &v0);
		setAttrib(vbo, pos_id, v_idx++, &v1);

		setAttrib(vbo, pos_id, v_idx++, &v0);
		setAttrib(vbo, pos_id, v_idx++, &v2);

		setAttrib(vbo, pos_id, v_idx++, &v0);
		setAttrib(vbo, pos_id, v_idx++, &v3);

		setAttrib(vbo, pos_id, v_idx++, &v0);
		setAttrib(vbo, pos_id, v_idx++, &v4);

		/* camera frame */
		setAttrib(vbo, pos_id, v_idx++, &v1);
		setAttrib(vbo, pos_id, v_idx++, &v2);

		setAttrib(vbo, pos_id, v_idx++, &v2);
		setAttrib(vbo, pos_id, v_idx++, &v3);

		setAttrib(vbo, pos_id, v_idx++, &v3);
		setAttrib(vbo, pos_id, v_idx++, &v4);

		setAttrib(vbo, pos_id, v_idx++, &v4);
		setAttrib(vbo, pos_id, v_idx++, &v1);

		/* tria */
		setAttrib(vbo, pos_id, v_idx++, &v5);
		setAttrib(vbo, pos_id, v_idx++, &v6);

		setAttrib(vbo, pos_id, v_idx++, &v6);
		setAttrib(vbo, pos_id, v_idx++, &v7);

		setAttrib(vbo, pos_id, v_idx++, &v7);
		setAttrib(vbo, pos_id, v_idx++, &v5);

		SHC.drw_camera = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_camera;
}

Batch *DRW_cache_camera_tria_get(void)
{
	if (!SHC.drw_camera_tria) {
		float v5 = 5.0f; /* tria + X */
		float v6 = 6.0f; /* tria - X */
		float v7 = 7.0f; /* tria + Y */
		int v_idx = 0;

		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			/* use x coordinate to identify the vertex
			 * the vertex shader take care to place it
			 * appropriatelly */
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 1, KEEP_FLOAT);
		}

		/* Vertices */
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 6);

		/* tria */
		setAttrib(vbo, pos_id, v_idx++, &v5);
		setAttrib(vbo, pos_id, v_idx++, &v6);
		setAttrib(vbo, pos_id, v_idx++, &v7);

		SHC.drw_camera_tria = Batch_create(GL_TRIANGLES, vbo, NULL);
	}
	return SHC.drw_camera_tria;
}

/* Object Center */
Batch *DRW_cache_single_vert_get(void)
{
	if (!SHC.drw_single_vertice) {
		float v1[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 1);

		setAttrib(vbo, pos_id, 0, v1);

		SHC.drw_single_vertice = Batch_create(GL_POINTS, vbo, NULL);
	}
	return SHC.drw_single_vertice;
}

/* Meshes */
void DRW_cache_wire_overlay_get(Object *ob, Batch **tris, Batch **ledges, Batch **lverts)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;

	*tris = BKE_mesh_batch_cache_get_overlay_triangles(me);
	*ledges = BKE_mesh_batch_cache_get_overlay_loose_edges(me);
	*lverts = BKE_mesh_batch_cache_get_overlay_loose_verts(me);
}

Batch *DRW_cache_face_centers_get(Object *ob)
{
	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;

	return BKE_mesh_batch_cache_get_overlay_facedots(me);
}

Batch *DRW_cache_wire_outline_get(Object *ob)
{
	Batch *fancy_wire = NULL;

	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	fancy_wire = BKE_mesh_batch_cache_get_fancy_edges(me);

	return fancy_wire;
}

Batch *DRW_cache_surface_get(Object *ob)
{
	Batch *surface = NULL;

	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	surface = BKE_mesh_batch_cache_get_triangles_with_normals(me);

	return surface;
}

Batch *DRW_cache_surface_verts_get(Object *ob)
{
	Batch *surface = NULL;

	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	surface = BKE_mesh_batch_cache_get_points_with_normals(me);

	return surface;
}

Batch *DRW_cache_verts_get(Object *ob)
{
	Batch *surface = NULL;

	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	surface = BKE_mesh_batch_cache_get_all_verts(me);

	return surface;
}

#if 0 /* TODO */
struct Batch *DRW_cache_surface_material_get(Object *ob, int nr) {
	/* TODO */
	return NULL;
}
#endif
