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

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_particle.h"

#include "smoke_API.h"

#include "BIF_gl.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "view3d_intern.h"  // own include

struct GPUTexture;

// #define DEBUG_DRAW_TIME

#ifdef DEBUG_DRAW_TIME
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

static int intersect_edges(float (*points)[3], float a, float b, float c, float d, const float edges[12][2][3])
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

static GPUTexture *create_flame_spectrum_texture(void)
{
#define SPEC_WIDTH 256
#define FIRE_THRESH 7
#define MAX_FIRE_ALPHA 0.06f
#define FULL_ON_FIRE 100

	GPUTexture *tex;
	int i, j, k;
	unsigned char *spec_data = malloc(SPEC_WIDTH * 4 * sizeof(unsigned char));
	float *spec_pixels = malloc(SPEC_WIDTH * 4 * 16 * 16 * sizeof(float));

	flame_get_spectrum(spec_data, SPEC_WIDTH, 1500, 3000);

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
					zero_v4(&spec_pixels[index]);
				}
			}
		}
	}

	tex = GPU_texture_create_1D(SPEC_WIDTH, spec_pixels, NULL);

	free(spec_data);
	free(spec_pixels);

#undef SPEC_WIDTH
#undef FIRE_THRESH
#undef MAX_FIRE_ALPHA
#undef FULL_ON_FIRE

	return tex;
}

void draw_smoke_volume(SmokeDomainSettings *sds, Object *ob,
                       const float min[3], const float max[3],
	                   const float viewnormal[3])
{
	GPUTexture *tex_spec = NULL;
	GPUProgram *smoke_program;
	const int progtype = (sds->active_fields & SM_ACTIVE_COLORS) ? GPU_PROGRAM_SMOKE_COLORED
	                                                             : GPU_PROGRAM_SMOKE;

	const float ob_sizei[3] = {
	    1.0f / fabsf(ob->size[0]),
	    1.0f / fabsf(ob->size[1]),
	    1.0f / fabsf(ob->size[2])
	};

	int i, j, n, good_index;
	float d /*, d0 */ /* UNUSED */, dd, ds;
	float (*points)[3] = NULL;
	int numpoints = 0;
	int gl_depth = 0, gl_blend = 0;

	const bool use_fire = (sds->active_fields & SM_ACTIVE_FIRE) != 0;

	/* draw slices of smoke is adapted from c++ code authored
	 * by: Johannes Schmid and Ingemar Rask, 2006, johnny@grob.org */
	const float verts[8][3] = {
		{ max[0], max[1], max[2] },
	    { min[0], max[1], max[2] },
	    { min[0], min[1], max[2] },
	    { max[0], min[1], max[2] },
		{ max[0], max[1], min[2] },
	    { min[0], max[1], min[2] },
	    { min[0], min[1], min[2] },
	    { max[0], min[1], min[2] }
	};

	const float size[3] = { max[0] - min[0], max[1] - min[1], max[2] - min[2] };
	const float invsize[3] = { 1.0f / size[0], 1.0f / size[1], 1.0f / size[2] };

	/* edges have the form edges[n][0][xyz] + t*edges[n][1][xyz] */
	const float edges[12][2][3] = {
	    {{verts[4][0], verts[4][1], verts[4][2]}, {0.0f, 0.0f, size[2]}},
		{{verts[5][0], verts[5][1], verts[5][2]}, {0.0f, 0.0f, size[2]}},
		{{verts[6][0], verts[6][1], verts[6][2]}, {0.0f, 0.0f, size[2]}},
		{{verts[7][0], verts[7][1], verts[7][2]}, {0.0f, 0.0f, size[2]}},

		{{verts[3][0], verts[3][1], verts[3][2]}, {0.0f, size[1], 0.0f}},
		{{verts[2][0], verts[2][1], verts[2][2]}, {0.0f, size[1], 0.0f}},
		{{verts[6][0], verts[6][1], verts[6][2]}, {0.0f, size[1], 0.0f}},
		{{verts[7][0], verts[7][1], verts[7][2]}, {0.0f, size[1], 0.0f}},

		{{verts[1][0], verts[1][1], verts[1][2]}, {size[0], 0.0f, 0.0f}},
		{{verts[2][0], verts[2][1], verts[2][2]}, {size[0], 0.0f, 0.0f}},
		{{verts[6][0], verts[6][1], verts[6][2]}, {size[0], 0.0f, 0.0f}},
		{{verts[5][0], verts[5][1], verts[5][2]}, {size[0], 0.0f, 0.0f}}
	};

	if (!sds->tex) {
		printf("Could not allocate 3D texture for 3D View smoke drawing.\n");
		return;
	}

#ifdef DEBUG_DRAW_TIME
	TIMEIT_START(draw);
#endif

	// printf("size x: %f, y: %f, z: %f\n", size[0], size[1], size[2]);
	// printf("min[2]: %f, max[2]: %f\n", min[2], max[2]);

	glGetBooleanv(GL_BLEND, (GLboolean *)&gl_blend);
	glGetBooleanv(GL_DEPTH_TEST, (GLboolean *)&gl_depth);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	/* find cube vertex that is closest to the viewer */
	for (i = 0; i < 8; i++) {
		float x = verts[i][0] - viewnormal[0] * size[0] * 0.5f;
		float y = verts[i][1] - viewnormal[1] * size[1] * 0.5f;
		float z = verts[i][2] - viewnormal[2] * size[2] * 0.5f;

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

	smoke_program = GPU_shader_get_builtin_program(progtype);
	if (smoke_program) {
		GPU_program_bind(smoke_program);

		/* cell spacing */
		GPU_program_parameter_4f(smoke_program, 0, sds->dx, sds->dx, sds->dx, 1.0);
		/* custom parameter for smoke style (higher = thicker) */
		if (sds->active_fields & SM_ACTIVE_COLORS)
			GPU_program_parameter_4f(smoke_program, 1, 1.0, 1.0, 1.0, 10.0);
		else
			GPU_program_parameter_4f(smoke_program, 1, sds->active_color[0], sds->active_color[1], sds->active_color[2], 10.0);
	}
	else
		printf("Your gfx card does not support 3D View smoke drawing.\n");

	GPU_texture_bind(sds->tex, 0);
	if (sds->tex_shadow)
		GPU_texture_bind(sds->tex_shadow, 1);
	else
		printf("No volume shadow\n");

	if (sds->tex_flame) {
		GPU_texture_bind(sds->tex_flame, 2);
		tex_spec = create_flame_spectrum_texture();
		GPU_texture_bind(tex_spec, 3);
	}

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
		float p0[3], tmp_point[3];

		if (dd * (float)n > ds)
			break;

		madd_v3_v3v3fl(tmp_point, verts[good_index], viewnormal, -dd * ((ds / dd) - (float)n));
		d = dot_v3v3(tmp_point, viewnormal);

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
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);

				GPU_program_parameter_4f(smoke_program, 2, 1.0, 0.0, 0.0, 0.0);
				glBegin(GL_POLYGON);
				for (i = 0; i < numpoints; i++) {
					glTexCoord3d((points[i][0] - min[0]) * invsize[0],
					             (points[i][1] - min[1]) * invsize[1],
					             (points[i][2] - min[2]) * invsize[2]);
					glVertex3f(points[i][0] * ob_sizei[0],
					           points[i][1] * ob_sizei[1],
					           points[i][2] * ob_sizei[2]);
				}
				glEnd();
			}

			/* render smoke slice */
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

			GPU_program_parameter_4f(smoke_program, 2, -1.0, 0.0, 0.0, 0.0);
			glBegin(GL_POLYGON);
			for (i = 0; i < numpoints; i++) {
				glTexCoord3d((points[i][0] - min[0]) * invsize[0],
				             (points[i][1] - min[1]) * invsize[1],
				             (points[i][2] - min[2]) * invsize[2]);
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

	GPU_texture_unbind(sds->tex);

	if (sds->tex_shadow)
		GPU_texture_unbind(sds->tex_shadow);

	if (sds->tex_flame) {
		GPU_texture_unbind(sds->tex_flame);
		GPU_texture_unbind(tex_spec);
		GPU_texture_free(tex_spec);
	}

	if (smoke_program)
		GPU_program_unbind(smoke_program);

	MEM_freeN(points);

	if (!gl_blend) {
		glDisable(GL_BLEND);
	}

	if (gl_depth) {
		glEnable(GL_DEPTH_TEST);
	}
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
