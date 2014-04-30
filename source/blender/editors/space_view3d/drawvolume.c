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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Daniel Genrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawvolume.c
 *  \ingroup spview3d
 */



#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_smoke_types.h"
#include "DNA_view3d_types.h"
#include "DNA_property_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_particle.h"

#include "smoke_API.h"

#include "BIF_gl.h"

#include "GPU_extensions.h"

#include "ED_mesh.h"

#include "BLF_api.h"

#include "view3d_intern.h"  // own include

struct GPUTexture;

// #define DEBUG_DRAW_TIME

#ifdef DEBUG_DRAW_TIME
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

static int intersect_edges(float (*points)[3], float a, float b, float c, float d, float edges[12][2][3])
{
	int i;
	float t;
	int numpoints = 0;

	for (i = 0; i < 12; i++) {
		t = -(a * edges[i][0][0] + b * edges[i][0][1] + c * edges[i][0][2] + d) /
		     (a * edges[i][1][0] + b * edges[i][1][1] + c * edges[i][1][2]);
		if ((t > 0) && (t < 1)) {
			points[numpoints][0] = edges[i][0][0] + edges[i][1][0] * t;
			points[numpoints][1] = edges[i][0][1] + edges[i][1][1] * t;
			points[numpoints][2] = edges[i][0][2] + edges[i][1][2] * t;
			numpoints++;
		}
	}
	return numpoints;
}

static bool convex(const float p0[3], const float up[3], const float a[3], const float b[3])
{
	/* Vec3 va = a-p0, vb = b-p0; */
	float va[3], vb[3], tmp[3];
	sub_v3_v3v3(va, a, p0);
	sub_v3_v3v3(vb, b, p0);
	cross_v3_v3v3(tmp, va, vb);
	return dot_v3v3(up, tmp) >= 0;
}

void draw_smoke_volume(SmokeDomainSettings *sds, Object *ob,
                       GPUTexture *tex, const float min[3], const float max[3],
                       const int res[3], float dx, float UNUSED(base_scale), const float viewnormal[3],
                       GPUTexture *tex_shadow, GPUTexture *tex_flame)
{
	const float ob_sizei[3] = {
	    1.0f / fabsf(ob->size[0]),
	    1.0f / fabsf(ob->size[1]),
	    1.0f / fabsf(ob->size[2])};

	int i, j, k, n, good_index;
	float d /*, d0 */ /* UNUSED */, dd, ds;
	float (*points)[3] = NULL;
	int numpoints = 0;
	float cor[3] = {1.0f, 1.0f, 1.0f};
	int gl_depth = 0, gl_blend = 0;

	int use_fire = (sds->active_fields & SM_ACTIVE_FIRE);

	/* draw slices of smoke is adapted from c++ code authored
	 * by: Johannes Schmid and Ingemar Rask, 2006, johnny@grob.org */
	float cv[][3] = {
		{1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f},
		{1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}
	};

	/* edges have the form edges[n][0][xyz] + t*edges[n][1][xyz] */
	float edges[12][2][3] = {
		{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 2.0f}},
		{{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 2.0f}},
		{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 2.0f}},
		{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 2.0f}},

		{{1.0f, -1.0f, 1.0f}, {0.0f, 2.0f, 0.0f}},
		{{-1.0f, -1.0f, 1.0f}, {0.0f, 2.0f, 0.0f}},
		{{-1.0f, -1.0f, -1.0f}, {0.0f, 2.0f, 0.0f}},
		{{1.0f, -1.0f, -1.0f}, {0.0f, 2.0f, 0.0f}},

		{{-1.0f, 1.0f, 1.0f}, {2.0f, 0.0f, 0.0f}},
		{{-1.0f, -1.0f, 1.0f}, {2.0f, 0.0f, 0.0f}},
		{{-1.0f, -1.0f, -1.0f}, {2.0f, 0.0f, 0.0f}},
		{{-1.0f, 1.0f, -1.0f}, {2.0f, 0.0f, 0.0f}}
	};

	unsigned char *spec_data;
	float *spec_pixels;
	GPUTexture *tex_spec;

	/* Fragment program to calculate the view3d of smoke */
	/* using 4 textures, density, shadow, flame and flame spectrum */
	const char *shader_basic =
	        "!!ARBfp1.0\n"
	        "PARAM dx = program.local[0];\n"
	        "PARAM darkness = program.local[1];\n"
	        "PARAM render = program.local[2];\n"
	        "PARAM f = {1.442695041, 1.442695041, 1.442695041, 0.01};\n"
	        "TEMP temp, shadow, flame, spec, value;\n"
	        "TEX temp, fragment.texcoord[0], texture[0], 3D;\n"
	        "TEX shadow, fragment.texcoord[0], texture[1], 3D;\n"
	        "TEX flame, fragment.texcoord[0], texture[2], 3D;\n"
	        "TEX spec, flame.r, texture[3], 1D;\n"
	        /* calculate shading factor from density */
	        "MUL value.r, temp.a, darkness.a;\n"
	        "MUL value.r, value.r, dx.r;\n"
	        "MUL value.r, value.r, f.r;\n"
	        "EX2 temp, -value.r;\n"
	        /* alpha */
	        "SUB temp.a, 1.0, temp.r;\n"
	        /* shade colors */
	        "MUL temp.r, temp.r, shadow.r;\n"
	        "MUL temp.g, temp.g, shadow.r;\n"
	        "MUL temp.b, temp.b, shadow.r;\n"
	        "MUL temp.r, temp.r, darkness.r;\n"
	        "MUL temp.g, temp.g, darkness.g;\n"
	        "MUL temp.b, temp.b, darkness.b;\n"
	        /* for now this just replace smoke shading if rendering fire */
	        "CMP result.color, render.r, temp, spec;\n"
	        "END\n";

	/* color shader */
	const char *shader_color =
	        "!!ARBfp1.0\n"
	        "PARAM dx = program.local[0];\n"
	        "PARAM darkness = program.local[1];\n"
	        "PARAM render = program.local[2];\n"
	        "PARAM f = {1.442695041, 1.442695041, 1.442695041, 1.442695041};\n"
	        "TEMP temp, shadow, flame, spec, value;\n"
	        "TEX temp, fragment.texcoord[0], texture[0], 3D;\n"
	        "TEX shadow, fragment.texcoord[0], texture[1], 3D;\n"
	        "TEX flame, fragment.texcoord[0], texture[2], 3D;\n"
	        "TEX spec, flame.r, texture[3], 1D;\n"
	        /* unpremultiply volume texture */
	        "RCP value.r, temp.a;\n"
	        "MUL temp.r, temp.r, value.r;\n"
	        "MUL temp.g, temp.g, value.r;\n"
	        "MUL temp.b, temp.b, value.r;\n"
	        /* calculate shading factor from density */
	        "MUL value.r, temp.a, darkness.a;\n"
	        "MUL value.r, value.r, dx.r;\n"
	        "MUL value.r, value.r, f.r;\n"
	        "EX2 value.r, -value.r;\n"
	        /* alpha */
	        "SUB temp.a, 1.0, value.r;\n"
	        /* shade colors */
	        "MUL temp.r, temp.r, shadow.r;\n"
	        "MUL temp.g, temp.g, shadow.r;\n"
	        "MUL temp.b, temp.b, shadow.r;\n"
	        "MUL temp.r, temp.r, value.r;\n"
	        "MUL temp.g, temp.g, value.r;\n"
	        "MUL temp.b, temp.b, value.r;\n"
	        /* for now this just replace smoke shading if rendering fire */
	        "CMP result.color, render.r, temp, spec;\n"
	        "END\n";

	GLuint prog;

	
	float size[3];

	if (!tex) {
		printf("Could not allocate 3D texture for 3D View smoke drawing.\n");
		return;
	}

#ifdef DEBUG_DRAW_TIME
	TIMEIT_START(draw);
#endif

	/* generate flame spectrum texture */
#define SPEC_WIDTH 256
#define FIRE_THRESH 7
#define MAX_FIRE_ALPHA 0.06f
#define FULL_ON_FIRE 100
	spec_data = malloc(SPEC_WIDTH * 4 * sizeof(unsigned char));
	flame_get_spectrum(spec_data, SPEC_WIDTH, 1500, 3000);
	spec_pixels = malloc(SPEC_WIDTH * 4 * 16 * 16 * sizeof(float));
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 16; j++) {
			for (k = 0; k < SPEC_WIDTH; k++) {
				int index = (j * SPEC_WIDTH * 16 + i * SPEC_WIDTH + k) * 4;
				if (k >= FIRE_THRESH) {
					spec_pixels[index] = ((float)spec_data[k * 4]) / 255.0f;
					spec_pixels[index + 1] = ((float)spec_data[k * 4 + 1]) / 255.0f;
					spec_pixels[index + 2] = ((float)spec_data[k * 4 + 2]) / 255.0f;
					spec_pixels[index + 3] = MAX_FIRE_ALPHA * (
					        (k > FULL_ON_FIRE) ? 1.0f : (k - FIRE_THRESH) / ((float)FULL_ON_FIRE - FIRE_THRESH));
				}
				else {
					spec_pixels[index] = spec_pixels[index + 1] = spec_pixels[index + 2] = spec_pixels[index + 3] = 0.0f;
				}
			}
		}
	}

	tex_spec = GPU_texture_create_1D(SPEC_WIDTH, spec_pixels, NULL);

#undef SPEC_WIDTH
#undef FIRE_THRESH
#undef MAX_FIRE_ALPHA
#undef FULL_ON_FIRE

	sub_v3_v3v3(size, max, min);

	/* maxx, maxy, maxz */
	cv[0][0] = max[0];
	cv[0][1] = max[1];
	cv[0][2] = max[2];
	/* minx, maxy, maxz */
	cv[1][0] = min[0];
	cv[1][1] = max[1];
	cv[1][2] = max[2];
	/* minx, miny, maxz */
	cv[2][0] = min[0];
	cv[2][1] = min[1];
	cv[2][2] = max[2];
	/* maxx, miny, maxz */
	cv[3][0] = max[0];
	cv[3][1] = min[1];
	cv[3][2] = max[2];

	/* maxx, maxy, minz */
	cv[4][0] = max[0];
	cv[4][1] = max[1];
	cv[4][2] = min[2];
	/* minx, maxy, minz */
	cv[5][0] = min[0];
	cv[5][1] = max[1];
	cv[5][2] = min[2];
	/* minx, miny, minz */
	cv[6][0] = min[0];
	cv[6][1] = min[1];
	cv[6][2] = min[2];
	/* maxx, miny, minz */
	cv[7][0] = max[0];
	cv[7][1] = min[1];
	cv[7][2] = min[2];

	copy_v3_v3(edges[0][0], cv[4]); /* maxx, maxy, minz */
	copy_v3_v3(edges[1][0], cv[5]); /* minx, maxy, minz */
	copy_v3_v3(edges[2][0], cv[6]); /* minx, miny, minz */
	copy_v3_v3(edges[3][0], cv[7]); /* maxx, miny, minz */

	copy_v3_v3(edges[4][0], cv[3]); /* maxx, miny, maxz */
	copy_v3_v3(edges[5][0], cv[2]); /* minx, miny, maxz */
	copy_v3_v3(edges[6][0], cv[6]); /* minx, miny, minz */
	copy_v3_v3(edges[7][0], cv[7]); /* maxx, miny, minz */

	copy_v3_v3(edges[8][0], cv[1]); /* minx, maxy, maxz */
	copy_v3_v3(edges[9][0], cv[2]); /* minx, miny, maxz */
	copy_v3_v3(edges[10][0], cv[6]); /* minx, miny, minz */
	copy_v3_v3(edges[11][0], cv[5]); /* minx, maxy, minz */

	// printf("size x: %f, y: %f, z: %f\n", size[0], size[1], size[2]);
	// printf("min[2]: %f, max[2]: %f\n", min[2], max[2]);

	edges[0][1][2] = size[2];
	edges[1][1][2] = size[2];
	edges[2][1][2] = size[2];
	edges[3][1][2] = size[2];

	edges[4][1][1] = size[1];
	edges[5][1][1] = size[1];
	edges[6][1][1] = size[1];
	edges[7][1][1] = size[1];

	edges[8][1][0] = size[0];
	edges[9][1][0] = size[0];
	edges[10][1][0] = size[0];
	edges[11][1][0] = size[0];

	glGetBooleanv(GL_BLEND, (GLboolean *)&gl_blend);
	glGetBooleanv(GL_DEPTH_TEST, (GLboolean *)&gl_depth);

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	/* find cube vertex that is closest to the viewer */
	for (i = 0; i < 8; i++) {
		float x, y, z;

		x = cv[i][0] - viewnormal[0] * size[0] * 0.5f;
		y = cv[i][1] - viewnormal[1] * size[1] * 0.5f;
		z = cv[i][2] - viewnormal[2] * size[2] * 0.5f;

		if ((x >= min[0]) && (x <= max[0]) &&
		    (y >= min[1]) && (y <= max[1]) &&
		    (z >= min[2]) && (z <= max[2]))
		{
			break;
		}
	}

	if (i >= 8) {
		/* fallback, avoid using buffer over-run */
		i = 0;
	}

	// printf("i: %d\n", i);
	// printf("point %f, %f, %f\n", cv[i][0], cv[i][1], cv[i][2]);

	if (GL_TRUE == glewIsSupported("GL_ARB_fragment_program")) {
		glEnable(GL_FRAGMENT_PROGRAM_ARB);
		glGenProgramsARB(1, &prog);

		glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, prog);
		/* set shader */
		if (sds->active_fields & SM_ACTIVE_COLORS)
			glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(shader_color), shader_color);
		else
			glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(shader_basic), shader_basic);

		/* cell spacing */
		glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, dx, dx, dx, 1.0);
		/* custom parameter for smoke style (higher = thicker) */
		if (sds->active_fields & SM_ACTIVE_COLORS)
			glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 1, 1.0, 1.0, 1.0, 10.0);
		else
			glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 1, sds->active_color[0], sds->active_color[1], sds->active_color[2], 10.0);
	}
	else
		printf("Your gfx card does not support 3D View smoke drawing.\n");

	GPU_texture_bind(tex, 0);
	if (tex_shadow)
		GPU_texture_bind(tex_shadow, 1);
	else
		printf("No volume shadow\n");

	if (tex_flame) {
		GPU_texture_bind(tex_flame, 2);
		GPU_texture_bind(tex_spec, 3);
	}

	if (!GPU_non_power_of_two_support()) {
		cor[0] = (float)res[0] / (float)power_of_2_max_u(res[0]);
		cor[1] = (float)res[1] / (float)power_of_2_max_u(res[1]);
		cor[2] = (float)res[2] / (float)power_of_2_max_u(res[2]);
	}

	cor[0] /= size[0];
	cor[1] /= size[1];
	cor[2] /= size[2];

	/* our slices are defined by the plane equation a*x + b*y +c*z + d = 0
	 * (a,b,c), the plane normal, are given by viewdir
	 * d is the parameter along the view direction. the first d is given by
	 * inserting previously found vertex into the plane equation */

	/* d0 = (viewnormal[0]*cv[i][0] + viewnormal[1]*cv[i][1] + viewnormal[2]*cv[i][2]); */ /* UNUSED */
	ds = (fabsf(viewnormal[0]) * size[0] + fabsf(viewnormal[1]) * size[1] + fabsf(viewnormal[2]) * size[2]);
	dd = max_fff(sds->global_size[0], sds->global_size[1], sds->global_size[2]) / 128.f;
	n = 0;
	good_index = i;

	// printf("d0: %f, dd: %f, ds: %f\n\n", d0, dd, ds);

	points = MEM_callocN(sizeof(*points) * 12, "smoke_points_preview");

	while (1) {
		float p0[3];
		float tmp_point[3], tmp_point2[3];

		if (dd * (float)n > ds)
			break;

		copy_v3_v3(tmp_point, viewnormal);
		mul_v3_fl(tmp_point, -dd * ((ds / dd) - (float)n));
		add_v3_v3v3(tmp_point2, cv[good_index], tmp_point);
		d = dot_v3v3(tmp_point2, viewnormal);

		// printf("my d: %f\n", d);

		/* intersect_edges returns the intersection points of all cube edges with
		 * the given plane that lie within the cube */
		numpoints = intersect_edges(points, viewnormal[0], viewnormal[1], viewnormal[2], -d, edges);

		// printf("points: %d\n", numpoints);

		if (numpoints > 2) {
			copy_v3_v3(p0, points[0]);

			/* sort points to get a convex polygon */
			for (i = 1; i < numpoints - 1; i++) {
				for (j = i + 1; j < numpoints; j++) {
					if (!convex(p0, viewnormal, points[j], points[i])) {
						swap_v3_v3(points[i], points[j]);
					}
				}
			}

			/* render fire slice */
			if (use_fire) {
				if (GLEW_VERSION_1_4)
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
				else
					glBlendFunc(GL_SRC_ALPHA, GL_ONE);

				glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 2, 1.0, 0.0, 0.0, 0.0);
				glBegin(GL_POLYGON);
				glColor3f(1.0, 1.0, 1.0);
				for (i = 0; i < numpoints; i++) {
					glTexCoord3d((points[i][0] - min[0]) * cor[0],
					             (points[i][1] - min[1]) * cor[1],
					             (points[i][2] - min[2]) * cor[2]);
					glVertex3f(points[i][0] * ob_sizei[0],
					           points[i][1] * ob_sizei[1],
					           points[i][2] * ob_sizei[2]);
				}
				glEnd();
			}

			/* render smoke slice */
			if (GLEW_VERSION_1_4)
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			else
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 2, -1.0, 0.0, 0.0, 0.0);
			glBegin(GL_POLYGON);
			glColor3f(1.0, 1.0, 1.0);
			for (i = 0; i < numpoints; i++) {
				glTexCoord3d((points[i][0] - min[0]) * cor[0],
				             (points[i][1] - min[1]) * cor[1],
				             (points[i][2] - min[2]) * cor[2]);
				glVertex3f(points[i][0] * ob_sizei[0],
				           points[i][1] * ob_sizei[1],
				           points[i][2] * ob_sizei[2]);
			}
			glEnd();
		}
		n++;
	}

#ifdef DEBUG_DRAW_TIME
	printf("Draw Time: %f\n", (float)TIMEIT_VALUE(draw));
	TIMEIT_END(draw);
#endif

	if (tex_shadow)
		GPU_texture_unbind(tex_shadow);
	GPU_texture_unbind(tex);
	if (tex_flame) {
		GPU_texture_unbind(tex_flame);
		GPU_texture_unbind(tex_spec);
	}
	GPU_texture_free(tex_spec);

	free(spec_data);
	free(spec_pixels);

	if (GLEW_ARB_fragment_program) {
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
		glDeleteProgramsARB(1, &prog);
	}


	MEM_freeN(points);

	if (!gl_blend) {
		glDisable(GL_BLEND);
	}

	if (gl_depth) {
		glEnable(GL_DEPTH_TEST);
	}

	glDepthMask(GL_TRUE);
}

#ifdef SMOKE_DEBUG_VELOCITY
void draw_smoke_velocity(SmokeDomainSettings *domain, Object *ob)
{
	float x, y, z;
	float x0, y0, z0;
	int *base_res = domain->base_res;
	int *res = domain->res;
	int *res_min = domain->res_min;
	int *res_max = domain->res_max;
	float *vel_x = smoke_get_velocity_x(domain->fluid);
	float *vel_y = smoke_get_velocity_y(domain->fluid);
	float *vel_z = smoke_get_velocity_z(domain->fluid);

	float min[3];
	float *cell_size = domain->cell_size;
	float step_size = ((float)max_iii(base_res[0], base_res[1], base_res[2])) / 16.f;
	float vf = domain->scale / 16.f * 2.f; /* velocity factor */

	glLineWidth(1.0f);

	/* set first position so that it doesn't jump when domain moves */
	x0 = res_min[0] + fmod(-(float)domain->shift[0] + res_min[0], step_size);
	y0 = res_min[1] + fmod(-(float)domain->shift[1] + res_min[1], step_size);
	z0 = res_min[2] + fmod(-(float)domain->shift[2] + res_min[2], step_size);
	if (x0 < res_min[0]) x0 += step_size;
	if (y0 < res_min[1]) y0 += step_size;
	if (z0 < res_min[2]) z0 += step_size;
	add_v3_v3v3(min, domain->p0, domain->obj_shift_f);

	for (x = floor(x0); x < res_max[0]; x += step_size)
		for (y = floor(y0); y < res_max[1]; y += step_size)
			for (z = floor(z0); z < res_max[2]; z += step_size) {
				int index = (floor(x) - res_min[0]) + (floor(y) - res_min[1]) * res[0] + (floor(z) - res_min[2]) * res[0] * res[1];

				float pos[3] = {min[0] + ((float)x + 0.5f) * cell_size[0], min[1] + ((float)y + 0.5f) * cell_size[1], min[2] + ((float)z + 0.5f) * cell_size[2]};
				float vel = sqrtf(vel_x[index] * vel_x[index] + vel_y[index] * vel_y[index] + vel_z[index] * vel_z[index]);

				/* draw heat as scaled "arrows" */
				if (vel >= 0.01f) {
					float col_g = 1.0f - vel;
					CLAMP(col_g, 0.0f, 1.0f);
					glColor3f(1.0f, col_g, 0.0f);
					glPointSize(10.0f * vel);

					glBegin(GL_LINES);
					glVertex3f(pos[0], pos[1], pos[2]);
					glVertex3f(pos[0] + vel_x[index] * vf, pos[1] + vel_y[index] * vf, pos[2] + vel_z[index] * vf);
					glEnd();
					glBegin(GL_POINTS);
					glVertex3f(pos[0] + vel_x[index] * vf, pos[1] + vel_y[index] * vf, pos[2] + vel_z[index] * vf);
					glEnd();
				}
			}
}
#endif

#ifdef SMOKE_DEBUG_HEAT
void draw_smoke_heat(SmokeDomainSettings *domain, Object *ob)
{
	float x, y, z;
	float x0, y0, z0;
	int *base_res = domain->base_res;
	int *res = domain->res;
	int *res_min = domain->res_min;
	int *res_max = domain->res_max;
	float *heat = smoke_get_heat(domain->fluid);

	float min[3];
	float *cell_size = domain->cell_size;
	float step_size = ((float)max_iii(base_res[0], base_res[1], base_res[2])) / 16.f;
	float vf = domain->scale / 16.f * 2.f; /* velocity factor */

	/* set first position so that it doesn't jump when domain moves */
	x0 = res_min[0] + fmod(-(float)domain->shift[0] + res_min[0], step_size);
	y0 = res_min[1] + fmod(-(float)domain->shift[1] + res_min[1], step_size);
	z0 = res_min[2] + fmod(-(float)domain->shift[2] + res_min[2], step_size);
	if (x0 < res_min[0]) x0 += step_size;
	if (y0 < res_min[1]) y0 += step_size;
	if (z0 < res_min[2]) z0 += step_size;
	add_v3_v3v3(min, domain->p0, domain->obj_shift_f);

	for (x = floor(x0); x < res_max[0]; x += step_size)
		for (y = floor(y0); y < res_max[1]; y += step_size)
			for (z = floor(z0); z < res_max[2]; z += step_size) {
				int index = (floor(x) - res_min[0]) + (floor(y) - res_min[1]) * res[0] + (floor(z) - res_min[2]) * res[0] * res[1];

				float pos[3] = {min[0] + ((float)x + 0.5f) * cell_size[0], min[1] + ((float)y + 0.5f) * cell_size[1], min[2] + ((float)z + 0.5f) * cell_size[2]};

				/* draw heat as different sized points */
				if (heat[index] >= 0.01f) {
					float col_gb = 1.0f - heat[index];
					CLAMP(col_gb, 0.0f, 1.0f);
					glColor3f(1.0f, col_gb, col_gb);
					glPointSize(24.0f * heat[index]);

					glBegin(GL_POINTS);
					glVertex3f(pos[0], pos[1], pos[2]);
					glEnd();
				}
			}
}
#endif
