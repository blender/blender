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

static struct DRWShapeCache{
	Batch *drw_single_vertice;
	Batch *drw_fullscreen_quad;
	Batch *drw_plain_axes;
	Batch *drw_single_arrow;
	Batch *drw_single_arrow_line;
	Batch *drw_cube;
	Batch *drw_circle;
	Batch *drw_empty_sphere;
	Batch *drw_empty_cone;
	Batch *drw_arrows;
	Batch *drw_lamp;
	Batch *drw_lamp_sunrays;
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
	if (SHC.drw_single_arrow_line)
		Batch_discard_all(SHC.drw_single_arrow_line);
	if (SHC.drw_cube)
		Batch_discard_all(SHC.drw_cube);
	if (SHC.drw_circle)
		Batch_discard_all(SHC.drw_circle);
	if (SHC.drw_empty_sphere)
		Batch_discard_all(SHC.drw_empty_sphere);
	if (SHC.drw_empty_cone)
		Batch_discard_all(SHC.drw_empty_cone);
	if (SHC.drw_arrows)
		Batch_discard_all(SHC.drw_arrows);
	if (SHC.drw_lamp)
		Batch_discard_all(SHC.drw_lamp);
	if (SHC.drw_lamp_sunrays)
		Batch_discard_all(SHC.drw_lamp_sunrays);
}

/* Quads */
Batch *DRW_cache_fullscreen_quad_get(void)
{
	if (!SHC.drw_fullscreen_quad) {
		float v1[2] = {-1.0f, -1.0f};
		float v2[2] = { 1.0f, -1.0f};
		float v3[2] = {-1.0f,  1.0f};
		float v4[2] = { 1.0f,  1.0f};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 6);

		setAttrib(vbo, pos_id, 0, v1);
		setAttrib(vbo, pos_id, 1, v2);
		setAttrib(vbo, pos_id, 2, v3);

		setAttrib(vbo, pos_id, 3, v2);
		setAttrib(vbo, pos_id, 4, v3);
		setAttrib(vbo, pos_id, 5, v4);

		SHC.drw_fullscreen_quad = Batch_create(GL_TRIANGLES, vbo, NULL);
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

		const GLubyte indices[24] = {0,1,1,3,3,2,2,0,0,4,4,5,5,7,7,6,6,4,1,5,3,7,2,6};

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
			v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[2] = 0.0f;
			setAttrib(vbo, pos_id, a * 2, v);

			v[0] = sinf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[1] = cosf((2.0f * M_PI * (a + 1)) / ((float)CIRCLE_RESOL));
			v[2] = 0.0f;
			setAttrib(vbo, pos_id, a * 2 + 1, v);
		}

		SHC.drw_circle = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_circle;
#undef CIRCLE_RESOL
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

Batch *DRW_cache_single_arrow_get(Batch **line)
{
	if (!SHC.drw_single_arrow_line || !SHC.drw_single_arrow) {
		float v1[3] = {0.0f, 0.0f, 0.0f}, v2[3], v3[3];

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		/* Line */
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 2);

		setAttrib(vbo, pos_id, 0, v1);
		v1[2] = 1.0f;
		setAttrib(vbo, pos_id, 1, v1);

		SHC.drw_single_arrow_line = Batch_create(GL_LINES, vbo, NULL);

		/* Square Pyramid */
		vbo = VertexBuffer_create_with_format(&format);
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
	*line = SHC.drw_single_arrow_line;
	return SHC.drw_single_arrow;
}

Batch *DRW_cache_empty_sphere_get(void)
{
#define NSEGMENTS 16
	if (!SHC.drw_empty_sphere) {
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
		VertexBuffer_allocate_data(vbo, NSEGMENTS * 2 * 3);

		for (int axis = 0; axis < 3; ++axis) {
			for (int i = 0; i < NSEGMENTS; ++i) {
				for (int j = 0; j < 2; ++j) {
					float cv[2], v[3];

					cv[0] = p[(i+j) % NSEGMENTS][0];
					cv[1] = p[(i+j) % NSEGMENTS][1];

					if (axis == 0)
						v[0] = cv[0], v[1] = cv[1], v[2] = 0.0f;
					else if (axis == 1)
						v[0] = cv[0], v[1] = 0.0f,  v[2] = cv[1];
					else
						v[0] = 0.0f,  v[1] = cv[0], v[2] = cv[1];

					setAttrib(vbo, pos_id, i*2 + j + (NSEGMENTS * 2 * axis), v);
				}
			}
		}

		SHC.drw_empty_sphere = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_empty_sphere;
#undef NSEGMENTS
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
			setAttrib(vbo, pos_id, i*4, v);
			v[0] = 0.0f, v[1] = 2.0f, v[2] = 0.0f;
			setAttrib(vbo, pos_id, i*4 + 1, v);

			/* end ring */
			v[0] = cv[0], v[1] = 0.0f, v[2] = cv[1];
			setAttrib(vbo, pos_id, i*4 + 2, v);
			cv[0] = p[(i+1) % NSEGMENTS][0];
			cv[1] = p[(i+1) % NSEGMENTS][1];
			v[0] = cv[0], v[1] = 0.0f, v[2] = cv[1];
			setAttrib(vbo, pos_id, i*4 + 3, v);
		}

		SHC.drw_empty_cone = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_empty_cone;
#undef NSEGMENTS
}

Batch *DRW_cache_arrows_get(void)
{
	if (!SHC.drw_arrows) {
		float v1[3] = {0.0, 0.0, 0.0};
		float v2[3] = {0.0, 0.0, 0.0};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		/* Line */
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 6 * 3);

		for (int axis = 0; axis < 3; axis++) {
			const int arrow_axis = (axis == 0) ? 1 : 0;

			v2[axis] = 1.0f;
			setAttrib(vbo, pos_id, axis * 6 + 0, v1);
			setAttrib(vbo, pos_id, axis * 6 + 1, v2);

			v1[axis] = 0.85f;
			v1[arrow_axis] = -0.08f;
			setAttrib(vbo, pos_id, axis * 6 + 2, v1);
			setAttrib(vbo, pos_id, axis * 6 + 3, v2);

			v1[arrow_axis] = 0.08f;
			setAttrib(vbo, pos_id, axis * 6 + 4, v1);
			setAttrib(vbo, pos_id, axis * 6 + 5, v2);

			/* reset v1 & v2 to zero */
			v1[arrow_axis] = v1[axis] = v2[axis] = 0.0f;
		}

		SHC.drw_arrows = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_arrows;
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
Batch *DRW_cache_wire_overlay_get(Object *ob)
{
	Batch *overlay_wire = NULL;

	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
#if 1 /* new version not working */
	overlay_wire = BKE_mesh_batch_cache_get_overlay_edges(me);
#else
	overlay_wire = BKE_mesh_batch_cache_get_all_edges(me);
#endif
	return overlay_wire;
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
	surface = BKE_mesh_batch_cache_get_all_triangles(me);

	return surface;
}

#if 0 /* TODO */
struct Batch *DRW_cache_surface_material_get(Object *ob, int nr) {
	/* TODO */
	return NULL;
}
#endif