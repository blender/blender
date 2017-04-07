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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/intern/gpu_immediate_util.c
 *  \ingroup gpu
 */

#include <stdio.h>
#include <string.h>

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "GPU_basic_shader.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"

/**
* Pack color into 3 bytes
*
* \param x color.
*/
void imm_cpack(unsigned int x)
{
	immUniformColor3ub(((x) & 0xFF),
	                   (((x) >> 8) & 0xFF),
	                   (((x) >> 16) & 0xFF));
}

static void imm_draw_circle(PrimitiveType prim_type, unsigned pos, float x, float y, float rad, int nsegments)
{
	immBegin(prim_type, nsegments);
	for (int i = 0; i < nsegments; ++i) {
		float angle = 2 * M_PI * ((float)i / (float)nsegments);
		immVertex2f(pos, x + rad * cosf(angle), y + rad * sinf(angle));
	}
	immEnd();
}

/**
 * Draw a circle outline with the given \a radius.
 * The circle is centered at \a x, \a y and drawn in the XY plane.
 *
 * \param pos The vertex attribute number for position.
 * \param x Horizontal center.
 * \param y Vertical center.
 * \param radius The circle's radius.
 * \param nsegments The number of segments to use in drawing (more = smoother).
 */
void imm_draw_circle_wire(unsigned pos, float x, float y, float rad, int nsegments)
{
	imm_draw_circle(PRIM_LINE_LOOP, pos, x, y, rad, nsegments);
}

/**
 * Draw a filled circle with the given \a radius.
 * The circle is centered at \a x, \a y and drawn in the XY plane.
 *
 * \param pos The vertex attribute number for position.
 * \param x Horizontal center.
 * \param y Vertical center.
 * \param radius The circle's radius.
 * \param nsegments The number of segments to use in drawing (more = smoother).
 */
void imm_draw_circle_fill(unsigned pos, float x, float y, float rad, int nsegments)
{
	imm_draw_circle(PRIM_TRIANGLE_FAN, pos, x, y, rad, nsegments);
}

/**
 * \note We could have `imm_draw_lined_disk_partial` but currently there is no need.
 */
static void imm_draw_disk_partial(
        PrimitiveType prim_type, unsigned pos, float x, float y,
        float rad_inner, float rad_outer, int nsegments, float start, float sweep)
{
	/* shift & reverse angle, increase 'nsegments' to match gluPartialDisk */
	const float angle_start = -(DEG2RADF(start)) + (M_PI / 2);
	const float angle_end   = -(DEG2RADF(sweep) - angle_start);
	nsegments += 1;
	immBegin(prim_type, nsegments * 2);
	for (int i = 0; i < nsegments; ++i) {
		const float angle = interpf(angle_start, angle_end, ((float)i / (float)(nsegments - 1)));
		const float angle_sin = sinf(angle);
		const float angle_cos = cosf(angle);
		immVertex2f(pos, x + rad_inner * angle_cos, y + rad_inner * angle_sin);
		immVertex2f(pos, x + rad_outer * angle_cos, y + rad_outer * angle_sin);
	}
	immEnd();
}

/**
 * Draw a filled arc with the given inner and outer radius.
 * The circle is centered at \a x, \a y and drawn in the XY plane.
 *
 * \note Arguments are `gluPartialDisk` compatible.
 *
 * \param pos: The vertex attribute number for position.
 * \param x: Horizontal center.
 * \param y: Vertical center.
 * \param radius_inner: The inner circle's radius.
 * \param radius_outer: The outer circle's radius (can be zero).
 * \param nsegments: The number of segments to use in drawing (more = smoother).
 * \param start: Specifies the starting angle, in degrees, of the disk portion.
 * \param sweep: Specifies the sweep angle, in degrees, of the disk portion.
 */
void imm_draw_disk_partial_fill(
        unsigned pos, float x, float y,
        float rad_inner, float rad_outer, int nsegments, float start, float sweep)
{
	imm_draw_disk_partial(PRIM_TRIANGLE_STRIP, pos, x, y, rad_inner, rad_outer, nsegments, start, sweep);
}

static void imm_draw_circle_3D(
        PrimitiveType prim_type, unsigned pos, float x, float y,
        float rad, int nsegments)
{
	immBegin(prim_type, nsegments);
	for (int i = 0; i < nsegments; ++i) {
		float angle = 2 * M_PI * ((float)i / (float)nsegments);
		immVertex3f(pos, x + rad * cosf(angle), y + rad * sinf(angle), 0.0f);
	}
	immEnd();
}

void imm_draw_circle_wire_3d(unsigned pos, float x, float y, float rad, int nsegments)
{
	imm_draw_circle_3D(PRIM_LINE_LOOP, pos, x, y, rad, nsegments);
}

void imm_draw_circle_fill_3d(unsigned pos, float x, float y, float rad, int nsegments)
{
	imm_draw_circle_3D(PRIM_TRIANGLE_FAN, pos, x, y, rad, nsegments);
}

/**
* Draw a lined box.
*
* \param pos The vertex attribute number for position.
* \param x1 left.
* \param y1 bottom.
* \param x2 right.
* \param y2 top.
*/
void imm_draw_line_box(unsigned pos, float x1, float y1, float x2, float y2)
{
	immBegin(PRIM_LINE_LOOP, 4);
	immVertex2f(pos, x1, y1);
	immVertex2f(pos, x1, y2);
	immVertex2f(pos, x2, y2);
	immVertex2f(pos, x2, y1);
	immEnd();
}

void imm_draw_line_box_3d(unsigned pos, float x1, float y1, float x2, float y2)
{
	/* use this version when VertexFormat has a vec3 position */
	immBegin(PRIM_LINE_LOOP, 4);
	immVertex3f(pos, x1, y1, 0.0f);
	immVertex3f(pos, x1, y2, 0.0f);
	immVertex3f(pos, x2, y2, 0.0f);
	immVertex3f(pos, x2, y1, 0.0f);
	immEnd();
}

/**
 * Draw a standard checkerboard to indicate transparent backgrounds.
 */
void imm_draw_checker_box(float x1, float y1, float x2, float y2)
{
	unsigned int pos = VertexFormat_add_attrib(immVertexFormat(), "pos", COMP_F32, 2, KEEP_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_CHECKER);

	immUniform4f("color1", 0.15f, 0.15f, 0.15f, 1.0f);
	immUniform4f("color2", 0.2f, 0.2f, 0.2f, 1.0f);
	immUniform1i("size", 8);

	immRectf(pos, x1, y1, x2, y2);

	immUnbindProgram();
}

/**
* Draw a cylinder. Replacement for gluCylinder.
* _warning_ : Slow, better use it only if you no other choices.
*
* \param pos The vertex attribute number for position.
* \param nor The vertex attribute number for normal.
* \param base Specifies the radius of the cylinder at z = 0.
* \param top Specifies the radius of the cylinder at z = height.
* \param height Specifies the height of the cylinder.
* \param slices Specifies the number of subdivisions around the z axis.
* \param stacks Specifies the number of subdivisions along the z axis.
*/
void imm_draw_cylinder_fill_normal_3d(
        unsigned int pos, unsigned int nor, float base, float top, float height, int slices, int stacks)
{
	immBegin(PRIM_TRIANGLES, 6 * slices * stacks);
	for (int i = 0; i < slices; ++i) {
		const float angle1 = 2 * M_PI * ((float)i / (float)slices);
		const float angle2 = 2 * M_PI * ((float)(i + 1) / (float)slices);
		const float cos1 = cosf(angle1);
		const float sin1 = sinf(angle1);
		const float cos2 = cosf(angle2);
		const float sin2 = sinf(angle2);

		for (int j = 0; j < stacks; ++j) {
			float fac1 = (float)j / (float)stacks;
			float fac2 = (float)(j + 1) / (float)stacks;
			float r1 = base * (1.f - fac1) + top * fac1;
			float r2 = base * (1.f - fac2) + top * fac2;
			float h1 = height * ((float)j / (float)stacks);
			float h2 = height * ((float)(j + 1) / (float)stacks);

			float v1[3] = {r1 *cos2, r1 * sin2, h1};
			float v2[3] = {r2 *cos2, r2 * sin2, h2};
			float v3[3] = {r2 *cos1, r2 * sin1, h2};
			float v4[3] = {r1 *cos1, r1 * sin1, h1};
			float n1[3], n2[3];

			/* calc normals */
			sub_v3_v3v3(n1, v2, v1);
			normalize_v3(n1);
			n1[0] = cos1; n1[1] = sin1; n1[2] = 1 - n1[2];

			sub_v3_v3v3(n2, v3, v4);
			normalize_v3(n2);
			n2[0] = cos2; n2[1] = sin2; n2[2] = 1 - n2[2];

			/* first tri */
			immAttrib3fv(nor, n2);
			immVertex3fv(pos, v1);
			immVertex3fv(pos, v2);
			immAttrib3fv(nor, n1);
			immVertex3fv(pos, v3);

			/* second tri */
			immVertex3fv(pos, v3);
			immVertex3fv(pos, v4);
			immAttrib3fv(nor, n2);
			immVertex3fv(pos, v1);
		}
	}
	immEnd();
}

void imm_draw_cylinder_wire_3d(unsigned int pos, float base, float top, float height, int slices, int stacks)
{
	immBegin(PRIM_LINES, 6 * slices * stacks);
	for (int i = 0; i < slices; ++i) {
		const float angle1 = 2 * M_PI * ((float)i / (float)slices);
		const float angle2 = 2 * M_PI * ((float)(i + 1) / (float)slices);
		const float cos1 = cosf(angle1);
		const float sin1 = sinf(angle1);
		const float cos2 = cosf(angle2);
		const float sin2 = sinf(angle2);

		for (int j = 0; j < stacks; ++j) {
			float fac1 = (float)j / (float)stacks;
			float fac2 = (float)(j + 1) / (float)stacks;
			float r1 = base * (1.f - fac1) + top * fac1;
			float r2 = base * (1.f - fac2) + top * fac2;
			float h1 = height * ((float)j / (float)stacks);
			float h2 = height * ((float)(j + 1) / (float)stacks);

			float v1[3] = {r1 * cos2, r1 * sin2, h1};
			float v2[3] = {r2 * cos2, r2 * sin2, h2};
			float v3[3] = {r2 * cos1, r2 * sin1, h2};
			float v4[3] = {r1 * cos1, r1 * sin1, h1};

			immVertex3fv(pos, v1);
			immVertex3fv(pos, v2);

			immVertex3fv(pos, v2);
			immVertex3fv(pos, v3);

			immVertex3fv(pos, v1);
			immVertex3fv(pos, v4);
		}
	}
	immEnd();
}

void imm_draw_cylinder_fill_3d(unsigned int pos, float base, float top, float height, int slices, int stacks)
{
	immBegin(PRIM_TRIANGLES, 6 * slices * stacks);
	for (int i = 0; i < slices; ++i) {
		const float angle1 = 2 * M_PI * ((float)i / (float)slices);
		const float angle2 = 2 * M_PI * ((float)(i + 1) / (float)slices);
		const float cos1 = cosf(angle1);
		const float sin1 = sinf(angle1);
		const float cos2 = cosf(angle2);
		const float sin2 = sinf(angle2);

		for (int j = 0; j < stacks; ++j) {
			float fac1 = (float)j / (float)stacks;
			float fac2 = (float)(j + 1) / (float)stacks;
			float r1 = base * (1.f - fac1) + top * fac1;
			float r2 = base * (1.f - fac2) + top * fac2;
			float h1 = height * ((float)j / (float)stacks);
			float h2 = height * ((float)(j + 1) / (float)stacks);

			float v1[3] = {r1 * cos2, r1 * sin2, h1};
			float v2[3] = {r2 * cos2, r2 * sin2, h2};
			float v3[3] = {r2 * cos1, r2 * sin1, h2};
			float v4[3] = {r1 * cos1, r1 * sin1, h1};

			/* first tri */
			immVertex3fv(pos, v1);
			immVertex3fv(pos, v2);
			immVertex3fv(pos, v3);

			/* second tri */
			immVertex3fv(pos, v3);
			immVertex3fv(pos, v4);
			immVertex3fv(pos, v1);
		}
	}
	immEnd();
}
