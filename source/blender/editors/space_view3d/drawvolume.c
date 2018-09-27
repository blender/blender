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

#include "BKE_DerivedMesh.h"
#include "BKE_colorband.h"
#include "BKE_particle.h"

#include "smoke_API.h"

#include "BIF_gl.h"

#include "GPU_debug.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "view3d_intern.h"  // own include

struct GPUTexture;

// #define DEBUG_DRAW_TIME

#ifdef DEBUG_DRAW_TIME
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

/* *************************** Transfer functions *************************** */

enum {
	TFUNC_FLAME_SPECTRUM = 0,
	TFUNC_COLOR_RAMP     = 1,
};

#define TFUNC_WIDTH 256

static void create_flame_spectrum_texture(float *data)
{
#define FIRE_THRESH 7
#define MAX_FIRE_ALPHA 0.06f
#define FULL_ON_FIRE 100

	float *spec_pixels = MEM_mallocN(TFUNC_WIDTH * 4 * 16 * 16 * sizeof(float), "spec_pixels");

	blackbody_temperature_to_rgb_table(data, TFUNC_WIDTH, 1500, 3000);

	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 16; j++) {
			for (int k = 0; k < TFUNC_WIDTH; k++) {
				int index = (j * TFUNC_WIDTH * 16 + i * TFUNC_WIDTH + k) * 4;
				if (k >= FIRE_THRESH) {
					spec_pixels[index] = (data[k * 4]);
					spec_pixels[index + 1] = (data[k * 4 + 1]);
					spec_pixels[index + 2] = (data[k * 4 + 2]);
					spec_pixels[index + 3] = MAX_FIRE_ALPHA * (
					        (k > FULL_ON_FIRE) ? 1.0f : (k - FIRE_THRESH) / ((float)FULL_ON_FIRE - FIRE_THRESH));
				}
				else {
					zero_v4(&spec_pixels[index]);
				}
			}
		}
	}

	memcpy(data, spec_pixels, sizeof(float) * 4 * TFUNC_WIDTH);

	MEM_freeN(spec_pixels);

#undef FIRE_THRESH
#undef MAX_FIRE_ALPHA
#undef FULL_ON_FIRE
}

static void create_color_ramp(const ColorBand *coba, float *data)
{
	for (int i = 0; i < TFUNC_WIDTH; i++) {
		BKE_colorband_evaluate(coba, (float)i / TFUNC_WIDTH, &data[i * 4]);
	}
}

static GPUTexture *create_transfer_function(int type, const ColorBand *coba)
{
	float *data = MEM_mallocN(sizeof(float) * 4 * TFUNC_WIDTH, __func__);

	switch (type) {
		case TFUNC_FLAME_SPECTRUM:
			create_flame_spectrum_texture(data);
			break;
		case TFUNC_COLOR_RAMP:
			create_color_ramp(coba, data);
			break;
	}

	GPUTexture *tex = GPU_texture_create_1D(TFUNC_WIDTH, data, NULL);

	MEM_freeN(data);

	return tex;
}

static GPUTexture *create_field_texture(SmokeDomainSettings *sds)
{
	float *field = NULL;

	switch (sds->coba_field) {
#ifdef WITH_SMOKE
		case FLUID_FIELD_DENSITY:    field = smoke_get_density(sds->fluid); break;
		case FLUID_FIELD_HEAT:       field = smoke_get_heat(sds->fluid); break;
		case FLUID_FIELD_FUEL:       field = smoke_get_fuel(sds->fluid); break;
		case FLUID_FIELD_REACT:      field = smoke_get_react(sds->fluid); break;
		case FLUID_FIELD_FLAME:      field = smoke_get_flame(sds->fluid); break;
		case FLUID_FIELD_VELOCITY_X: field = smoke_get_velocity_x(sds->fluid); break;
		case FLUID_FIELD_VELOCITY_Y: field = smoke_get_velocity_y(sds->fluid); break;
		case FLUID_FIELD_VELOCITY_Z: field = smoke_get_velocity_z(sds->fluid); break;
		case FLUID_FIELD_COLOR_R:    field = smoke_get_color_r(sds->fluid); break;
		case FLUID_FIELD_COLOR_G:    field = smoke_get_color_g(sds->fluid); break;
		case FLUID_FIELD_COLOR_B:    field = smoke_get_color_b(sds->fluid); break;
		case FLUID_FIELD_FORCE_X:    field = smoke_get_force_x(sds->fluid); break;
		case FLUID_FIELD_FORCE_Y:    field = smoke_get_force_y(sds->fluid); break;
		case FLUID_FIELD_FORCE_Z:    field = smoke_get_force_z(sds->fluid); break;
#endif
		default: return NULL;
	}

	return GPU_texture_create_3D(sds->res[0], sds->res[1], sds->res[2], 1, field);
}

typedef struct VolumeSlicer {
	float size[3];
	float min[3];
	float max[3];
	float (*verts)[3];
} VolumeSlicer;

/* *************************** Axis Aligned Slicing ************************** */

static void create_single_slice(VolumeSlicer *slicer, const float depth,
                                const int axis, const int idx)
{
	const float vertices[3][4][3] = {
	    {
	        { depth, slicer->min[1], slicer->min[2] },
	        { depth, slicer->max[1], slicer->min[2] },
	        { depth, slicer->max[1], slicer->max[2] },
	        { depth, slicer->min[1], slicer->max[2] }
	    },
	    {
	        { slicer->min[0], depth, slicer->min[2] },
	        { slicer->min[0], depth, slicer->max[2] },
	        { slicer->max[0], depth, slicer->max[2] },
	        { slicer->max[0], depth, slicer->min[2] }
	    },
	    {
	        { slicer->min[0], slicer->min[1], depth },
	        { slicer->min[0], slicer->max[1], depth },
	        { slicer->max[0], slicer->max[1], depth },
	        { slicer->max[0], slicer->min[1], depth }
	    }
	};

	copy_v3_v3(slicer->verts[idx + 0], vertices[axis][0]);
	copy_v3_v3(slicer->verts[idx + 1], vertices[axis][1]);
	copy_v3_v3(slicer->verts[idx + 2], vertices[axis][2]);
	copy_v3_v3(slicer->verts[idx + 3], vertices[axis][0]);
	copy_v3_v3(slicer->verts[idx + 4], vertices[axis][2]);
	copy_v3_v3(slicer->verts[idx + 5], vertices[axis][3]);
}

static void create_axis_aligned_slices(VolumeSlicer *slicer, const int num_slices,
                                       const float view_dir[3], const int axis)
{
	float depth, slice_size = slicer->size[axis] / num_slices;

	/* always process slices in back to front order! */
	if (view_dir[axis] > 0.0f) {
		depth = slicer->min[axis];
	}
	else {
		depth = slicer->max[axis];
		slice_size = -slice_size;
	}

	int count = 0;
	for (int slice = 0; slice < num_slices; slice++) {
		create_single_slice(slicer, depth, axis, count);

		count += 6;
		depth += slice_size;
	}
}

/* *************************** View Aligned Slicing ************************** */

/* Code adapted from:
 * "GPU-based Volume Rendering, Real-time Volume Graphics", AK Peters/CRC Press
 */
static int create_view_aligned_slices(VolumeSlicer *slicer,
                                      const int num_slices,
                                      const float view_dir[3])
{
	const int indices[] = { 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5 };

	const float vertices[8][3] = {
	    { slicer->min[0], slicer->min[1], slicer->min[2] },
	    { slicer->max[0], slicer->min[1], slicer->min[2] },
	    { slicer->max[0], slicer->max[1], slicer->min[2] },
	    { slicer->min[0], slicer->max[1], slicer->min[2] },
	    { slicer->min[0], slicer->min[1], slicer->max[2] },
	    { slicer->max[0], slicer->min[1], slicer->max[2] },
	    { slicer->max[0], slicer->max[1], slicer->max[2] },
	    { slicer->min[0], slicer->max[1], slicer->max[2] }
	};

	const int edges[12][2] = {
	    { 0, 1 }, { 1, 2 }, { 2, 3 },
	    { 3, 0 }, { 0, 4 }, { 1, 5 },
	    { 2, 6 }, { 3, 7 }, { 4, 5 },
	    { 5, 6 }, { 6, 7 }, { 7, 4 }
	};

	const int edge_list[8][12] = {
	    { 0, 1, 5, 6, 4, 8, 11, 9, 3, 7, 2, 10 },
	    { 0, 4, 3, 11, 1, 2, 6, 7, 5, 9, 8, 10 },
	    { 1, 5, 0, 8, 2, 3, 7, 4, 6, 10, 9, 11 },
	    { 7, 11, 10, 8, 2, 6, 1, 9, 3, 0, 4, 5 },
	    { 8, 5, 9, 1, 11, 10, 7, 6, 4, 3, 0, 2 },
	    { 9, 6, 10, 2, 8, 11, 4, 7, 5, 0, 1, 3 },
	    { 9, 8, 5, 4, 6, 1, 2, 0, 10, 7, 11, 3 },
	    { 10, 9, 6, 5, 7, 2, 3, 1, 11, 4, 8, 0 }
	};

	/* find vertex that is the furthest from the view plane */
	int max_index = 0;
	float max_dist, min_dist;
	min_dist = max_dist = dot_v3v3(view_dir, vertices[0]);

	for (int i = 1; i < 8; i++) {
		float dist = dot_v3v3(view_dir, vertices[i]);

		if (dist > max_dist) {
			max_dist = dist;
			max_index = i;
		}

		if (dist < min_dist) {
			min_dist = dist;
		}
	}

	max_dist -= FLT_EPSILON;
	min_dist += FLT_EPSILON;

	/* start and direction vectors */
	float vec_start[12][3], vec_dir[12][3];
	/* lambda intersection values */
	float lambda[12], lambda_inc[12];
	float denom = 0.0f;

	float plane_dist = min_dist;
	float plane_dist_inc = (max_dist - min_dist) / (float)num_slices;

	/* for all edges */
	for (int i = 0; i < 12; i++) {
		copy_v3_v3(vec_start[i], vertices[edges[edge_list[max_index][i]][0]]);
		copy_v3_v3(vec_dir[i],   vertices[edges[edge_list[max_index][i]][1]]);
		sub_v3_v3(vec_dir[i], vec_start[i]);

		denom = dot_v3v3(vec_dir[i], view_dir);

		if (1.0f + denom != 1.0f) {
			lambda_inc[i] = plane_dist_inc / denom;
			lambda[i] = (plane_dist - dot_v3v3(vec_start[i], view_dir)) / denom;
		}
		else {
			lambda[i] = -1.0f;
			lambda_inc[i] = 0.0f;
		}
	}

	float intersections[6][3];
	float dL[12];
	int num_points = 0;
	/* find intersections for each slice, process them in back to front order */
	for (int i = 0; i < num_slices; i++) {
		for (int e = 0; e < 12; e++) {
			dL[e] = lambda[e] + i * lambda_inc[e];
		}

		if ((dL[0] >= 0.0f) && (dL[0] < 1.0f)) {
			madd_v3_v3v3fl(intersections[0], vec_start[0], vec_dir[0], dL[0]);
		}
		else if ((dL[1] >= 0.0f) && (dL[1] < 1.0f)) {
			madd_v3_v3v3fl(intersections[0], vec_start[1], vec_dir[1], dL[1]);
		}
		else if ((dL[3] >= 0.0f) && (dL[3] < 1.0f)) {
			madd_v3_v3v3fl(intersections[0], vec_start[3], vec_dir[3], dL[3]);
		}
		else continue;

		if ((dL[2] >= 0.0f) && (dL[2] < 1.0f)) {
			madd_v3_v3v3fl(intersections[1], vec_start[2], vec_dir[2], dL[2]);
		}
		else if ((dL[0] >= 0.0f) && (dL[0] < 1.0f)) {
			madd_v3_v3v3fl(intersections[1], vec_start[0], vec_dir[0], dL[0]);
		}
		else if ((dL[1] >= 0.0f) && (dL[1] < 1.0f)) {
			madd_v3_v3v3fl(intersections[1], vec_start[1], vec_dir[1], dL[1]);
		}
		else {
			madd_v3_v3v3fl(intersections[1], vec_start[3], vec_dir[3], dL[3]);
		}

		if ((dL[4] >= 0.0f) && (dL[4] < 1.0f)) {
			madd_v3_v3v3fl(intersections[2], vec_start[4], vec_dir[4], dL[4]);
		}
		else if ((dL[5] >= 0.0f) && (dL[5] < 1.0f)) {
			madd_v3_v3v3fl(intersections[2], vec_start[5], vec_dir[5], dL[5]);
		}
		else {
			madd_v3_v3v3fl(intersections[2], vec_start[7], vec_dir[7], dL[7]);
		}

		if ((dL[6] >= 0.0f) && (dL[6] < 1.0f)) {
			madd_v3_v3v3fl(intersections[3], vec_start[6], vec_dir[6], dL[6]);
		}
		else if ((dL[4] >= 0.0f) && (dL[4] < 1.0f)) {
			madd_v3_v3v3fl(intersections[3], vec_start[4], vec_dir[4], dL[4]);
		}
		else if ((dL[5] >= 0.0f) && (dL[5] < 1.0f)) {
			madd_v3_v3v3fl(intersections[3], vec_start[5], vec_dir[5], dL[5]);
		}
		else {
			madd_v3_v3v3fl(intersections[3], vec_start[7], vec_dir[7], dL[7]);
		}

		if ((dL[8] >= 0.0f) && (dL[8] < 1.0f)) {
			madd_v3_v3v3fl(intersections[4], vec_start[8], vec_dir[8], dL[8]);
		}
		else if ((dL[9] >= 0.0f) && (dL[9] < 1.0f)) {
			madd_v3_v3v3fl(intersections[4], vec_start[9], vec_dir[9], dL[9]);
		}
		else {
			madd_v3_v3v3fl(intersections[4], vec_start[11], vec_dir[11], dL[11]);
		}

		if ((dL[10] >= 0.0f) && (dL[10] < 1.0f)) {
			madd_v3_v3v3fl(intersections[5], vec_start[10], vec_dir[10], dL[10]);
		}
		else if ((dL[8] >= 0.0f) && (dL[8] < 1.0f)) {
			madd_v3_v3v3fl(intersections[5], vec_start[8], vec_dir[8], dL[8]);
		}
		else if ((dL[9] >= 0.0f) && (dL[9] < 1.0f)) {
			madd_v3_v3v3fl(intersections[5], vec_start[9], vec_dir[9], dL[9]);
		}
		else {
			madd_v3_v3v3fl(intersections[5], vec_start[11], vec_dir[11], dL[11]);
		}

		for (int e = 0; e < 12; e++) {
			copy_v3_v3(slicer->verts[num_points++], intersections[indices[e]]);
		}
	}

	return num_points;
}

static void bind_shader(SmokeDomainSettings *sds, GPUShader *shader, GPUTexture *tex_spec,
                        GPUTexture *tex_tfunc, GPUTexture *tex_coba,
                        bool use_fire, const float min[3],
                        const float ob_sizei[3], const float invsize[3])
{
	int invsize_location = GPU_shader_get_uniform(shader, "invsize");
	int ob_sizei_location = GPU_shader_get_uniform(shader, "ob_sizei");
	int min_location = GPU_shader_get_uniform(shader, "min_location");

	int soot_location;
	int stepsize_location;
	int densityscale_location;
	int spec_location, flame_location;
	int shadow_location, actcol_location;
	int tfunc_location = 0;
	int coba_location = 0;

	if (use_fire) {
		spec_location = GPU_shader_get_uniform(shader, "spectrum_texture");
		flame_location = GPU_shader_get_uniform(shader, "flame_texture");
	}
	else {
		shadow_location = GPU_shader_get_uniform(shader, "shadow_texture");
		actcol_location = GPU_shader_get_uniform(shader, "active_color");
		soot_location = GPU_shader_get_uniform(shader, "soot_texture");
		stepsize_location = GPU_shader_get_uniform(shader, "step_size");
		densityscale_location = GPU_shader_get_uniform(shader, "density_scale");

		if (sds->use_coba) {
			tfunc_location = GPU_shader_get_uniform(shader, "transfer_texture");
			coba_location = GPU_shader_get_uniform(shader, "color_band_texture");
		}
	}

	GPU_shader_bind(shader);

	if (use_fire) {
		GPU_texture_bind(sds->tex_flame, 2);
		GPU_shader_uniform_texture(shader, flame_location, sds->tex_flame);

		GPU_texture_bind(tex_spec, 3);
		GPU_shader_uniform_texture(shader, spec_location, tex_spec);
	}
	else {
		float density_scale = 10.0f * sds->display_thickness;

		GPU_shader_uniform_vector(shader, stepsize_location, 1, 1, &sds->dx);
		GPU_shader_uniform_vector(shader, densityscale_location, 1, 1, &density_scale);

		GPU_texture_bind(sds->tex, 0);
		GPU_shader_uniform_texture(shader, soot_location, sds->tex);

		GPU_texture_bind(sds->tex_shadow, 1);
		GPU_shader_uniform_texture(shader, shadow_location, sds->tex_shadow);

		float active_color[3] = { 0.9, 0.9, 0.9 };
		if ((sds->active_fields & SM_ACTIVE_COLORS) == 0)
			mul_v3_v3(active_color, sds->active_color);
		GPU_shader_uniform_vector(shader, actcol_location, 3, 1, active_color);

		if (sds->use_coba) {
			GPU_texture_bind(tex_tfunc, 4);
			GPU_shader_uniform_texture(shader, tfunc_location, tex_tfunc);

			GPU_texture_bind(tex_coba, 5);
			GPU_shader_uniform_texture(shader, coba_location, tex_coba);
		}
	}

	GPU_shader_uniform_vector(shader, min_location, 3, 1, min);
	GPU_shader_uniform_vector(shader, ob_sizei_location, 3, 1, ob_sizei);
	GPU_shader_uniform_vector(shader, invsize_location, 3, 1, invsize);
}

static void unbind_shader(SmokeDomainSettings *sds, GPUTexture *tex_spec,
                          GPUTexture *tex_tfunc, GPUTexture *tex_coba, bool use_fire)
{
	GPU_shader_unbind();

	GPU_texture_unbind(sds->tex);

	if (use_fire) {
		GPU_texture_unbind(sds->tex_flame);
		GPU_texture_unbind(tex_spec);
		GPU_texture_free(tex_spec);
	}
	else {
		GPU_texture_unbind(sds->tex_shadow);

		if (sds->use_coba) {
			GPU_texture_unbind(tex_tfunc);
			GPU_texture_free(tex_tfunc);

			GPU_texture_unbind(tex_coba);
			GPU_texture_free(tex_coba);
		}
	}
}

static void draw_buffer(SmokeDomainSettings *sds, GPUShader *shader, const VolumeSlicer *slicer,
                        const float ob_sizei[3], const float invsize[3], const int num_points, const bool do_fire)
{
	GPUTexture *tex_spec = (do_fire) ? create_transfer_function(TFUNC_FLAME_SPECTRUM, NULL) : NULL;
	GPUTexture *tex_tfunc = (sds->use_coba) ? create_transfer_function(TFUNC_COLOR_RAMP, sds->coba) : NULL;
	GPUTexture *tex_coba = (sds->use_coba) ? create_field_texture(sds) : NULL;

	GLuint vertex_buffer;
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * num_points, &slicer->verts[0][0], GL_STATIC_DRAW);

	bind_shader(sds, shader, tex_spec, tex_tfunc, tex_coba, do_fire, slicer->min, ob_sizei, invsize);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, NULL);

	glDrawArrays(GL_TRIANGLES, 0, num_points);

	glDisableClientState(GL_VERTEX_ARRAY);

	unbind_shader(sds, tex_spec, tex_tfunc, tex_coba, do_fire);

	/* cleanup */

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDeleteBuffers(1, &vertex_buffer);
}

void draw_smoke_volume(SmokeDomainSettings *sds, Object *ob,
                       const float min[3], const float max[3],
                        const float viewnormal[3])
{
	if (!sds->tex || !sds->tex_shadow) {
		fprintf(stderr, "Could not allocate 3D texture for volume rendering!\n");
		return;
	}

	const bool use_fire = (sds->active_fields & SM_ACTIVE_FIRE) && sds->tex_flame;

	GPUBuiltinShader builtin_shader;

	if (sds->use_coba) {
		builtin_shader = GPU_SHADER_SMOKE_COBA;
	}
	else {
		builtin_shader = GPU_SHADER_SMOKE;
	}

	GPUShader *shader = GPU_shader_get_builtin_shader(builtin_shader);

	if (!shader) {
		fprintf(stderr, "Unable to create GLSL smoke shader.\n");
		return;
	}

	GPUShader *fire_shader = NULL;
	if (use_fire) {
		fire_shader = GPU_shader_get_builtin_shader(GPU_SHADER_SMOKE_FIRE);

		if (!fire_shader) {
			fprintf(stderr, "Unable to create GLSL fire shader.\n");
			return;
		}
	}

	const float ob_sizei[3] = {
	    1.0f / fabsf(ob->size[0]),
	    1.0f / fabsf(ob->size[1]),
	    1.0f / fabsf(ob->size[2])
	};

	const float size[3] = { max[0] - min[0], max[1] - min[1], max[2] - min[2] };
	const float invsize[3] = { 1.0f / size[0], 1.0f / size[1], 1.0f / size[2] };

#ifdef DEBUG_DRAW_TIME
	TIMEIT_START(draw);
#endif

	/* setup slicing information */

	const bool view_aligned = (sds->slice_method == MOD_SMOKE_SLICE_VIEW_ALIGNED);
	int max_slices, max_points, axis = 0;

	if (view_aligned) {
		max_slices = max_iii(sds->res[0], sds->res[1], sds->res[2]) * sds->slice_per_voxel;
		max_points = max_slices * 12;
	}
	else {
		if (sds->axis_slice_method == AXIS_SLICE_FULL) {
			axis = axis_dominant_v3_single(viewnormal);
			max_slices = sds->res[axis] * sds->slice_per_voxel;
		}
		else {
			axis = (sds->slice_axis == SLICE_AXIS_AUTO) ? axis_dominant_v3_single(viewnormal) : sds->slice_axis - 1;
			max_slices = 1;
		}

		max_points = max_slices * 6;
	}

	VolumeSlicer slicer;
	copy_v3_v3(slicer.min, min);
	copy_v3_v3(slicer.max, max);
	copy_v3_v3(slicer.size, size);
	slicer.verts = MEM_mallocN(sizeof(float) * 3 * max_points, "smoke_slice_vertices");

	int num_points;

	if (view_aligned) {
		num_points = create_view_aligned_slices(&slicer, max_slices, viewnormal);
	}
	else {
		num_points = max_points;

		if (sds->axis_slice_method == AXIS_SLICE_FULL) {
			create_axis_aligned_slices(&slicer, max_slices, viewnormal, axis);
		}
		else {
			const float depth = (sds->slice_depth - 0.5f) * size[axis];
			create_single_slice(&slicer, depth, axis, 0);
		}
	}

	/* setup buffer and draw */

	int gl_depth = 0, gl_blend = 0, gl_depth_write = 0;
	glGetBooleanv(GL_BLEND, (GLboolean *)&gl_blend);
	glGetBooleanv(GL_DEPTH_TEST, (GLboolean *)&gl_depth);
	glGetBooleanv(GL_DEPTH_WRITEMASK, (GLboolean *)&gl_depth_write);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	draw_buffer(sds, shader, &slicer, ob_sizei, invsize, num_points, false);

	/* Draw fire separately (T47639). */
	if (use_fire && !sds->use_coba) {
		glBlendFunc(GL_ONE, GL_ONE);
		draw_buffer(sds, fire_shader, &slicer, ob_sizei, invsize, num_points, true);
	}

#ifdef DEBUG_DRAW_TIME
	printf("Draw Time: %f\n", (float)TIMEIT_VALUE(draw));
	TIMEIT_END(draw);
#endif

	MEM_freeN(slicer.verts);

	glDepthMask(gl_depth_write);

	if (!gl_blend) {
		glDisable(GL_BLEND);
	}

	if (gl_depth) {
		glEnable(GL_DEPTH_TEST);
	}
}

#ifdef WITH_SMOKE
static void add_tri(float (*verts)[3], float(*colors)[3], int *offset,
                    float p1[3], float p2[3], float p3[3], float rgb[3])
{
	copy_v3_v3(verts[*offset + 0], p1);
	copy_v3_v3(verts[*offset + 1], p2);
	copy_v3_v3(verts[*offset + 2], p3);

	copy_v3_v3(colors[*offset + 0], rgb);
	copy_v3_v3(colors[*offset + 1], rgb);
	copy_v3_v3(colors[*offset + 2], rgb);

	*offset += 3;
}

static void add_needle(float (*verts)[3], float (*colors)[3], float center[3],
                       float dir[3], float scale, float voxel_size, int *offset)
{
	float len = len_v3(dir);

	float rgb[3];
	weight_to_rgb(rgb, len);

	if (len != 0.0f) {
		mul_v3_fl(dir, 1.0f / len);
		len *= scale;
	}

	len *= voxel_size;

	float corners[4][3] = {
	    { 0.0f, 0.2f, -0.5f },
	    { -0.2f * 0.866f, -0.2f * 0.5f, -0.5f },
	    { 0.2f * 0.866f, -0.2f * 0.5f, -0.5f },
	    { 0.0f, 0.0f, 0.5f }
	};

	const float up[3] = { 0.0f, 0.0f, 1.0f };
	float rot[3][3];

	rotation_between_vecs_to_mat3(rot, up, dir);
	transpose_m3(rot);

	for (int i = 0; i < 4; i++) {
		mul_m3_v3(rot, corners[i]);
		mul_v3_fl(corners[i], len);
		add_v3_v3(corners[i], center);
	}

	add_tri(verts, colors, offset, corners[0], corners[1], corners[2], rgb);
	add_tri(verts, colors, offset, corners[0], corners[1], corners[3], rgb);
	add_tri(verts, colors, offset, corners[1], corners[2], corners[3], rgb);
	add_tri(verts, colors, offset, corners[2], corners[0], corners[3], rgb);
}

static void add_streamline(float (*verts)[3], float(*colors)[3], float center[3],
                           float dir[3], float scale, float voxel_size, int *offset)
{
	const float len = len_v3(dir);

	float rgb[3];
	weight_to_rgb(rgb, len);

	copy_v3_v3(colors[(*offset)], rgb);
	copy_v3_v3(verts[(*offset)++], center);

	mul_v3_fl(dir, scale * voxel_size);
	add_v3_v3(center, dir);

	copy_v3_v3(colors[(*offset)], rgb);
	copy_v3_v3(verts[(*offset)++], center);
}

typedef void (*vector_draw_func)(float(*)[3], float(*)[3], float *, float *, float, float, int *);
#endif  /* WITH_SMOKE */

void draw_smoke_velocity(SmokeDomainSettings *domain, float viewnormal[3])
{
#ifdef WITH_SMOKE
	const float *vel_x = smoke_get_velocity_x(domain->fluid);
	const float *vel_y = smoke_get_velocity_y(domain->fluid);
	const float *vel_z = smoke_get_velocity_z(domain->fluid);

	if (ELEM(NULL, vel_x, vel_y, vel_z)) {
		return;
	}

	const int *base_res = domain->base_res;
	const int *res = domain->res;
	const int *res_min = domain->res_min;

	int res_max[3];
	copy_v3_v3_int(res_max, domain->res_max);

	const float *cell_size = domain->cell_size;
	const float step_size = ((float)max_iii(base_res[0], base_res[1], base_res[2])) / 16.0f;

	/* set first position so that it doesn't jump when domain moves */
	float xyz[3] = {
	    res_min[0] + fmod(-(float)domain->shift[0] + res_min[0], step_size),
	    res_min[1] + fmod(-(float)domain->shift[1] + res_min[1], step_size),
	    res_min[2] + fmod(-(float)domain->shift[2] + res_min[2], step_size)
	};

	if (xyz[0] < res_min[0]) xyz[0] += step_size;
	if (xyz[1] < res_min[1]) xyz[1] += step_size;
	if (xyz[2] < res_min[2]) xyz[2] += step_size;

	float min[3] = {
	    domain->p0[0] - domain->cell_size[0] * domain->adapt_res,
	    domain->p0[1] - domain->cell_size[1] * domain->adapt_res,
	    domain->p0[2] - domain->cell_size[2] * domain->adapt_res,
	};

	int num_points_v[3] = {
	    ((float)(res_max[0] - floor(xyz[0])) / step_size) + 0.5f,
	    ((float)(res_max[1] - floor(xyz[1])) / step_size) + 0.5f,
	    ((float)(res_max[2] - floor(xyz[2])) / step_size) + 0.5f
	};

	if (domain->slice_method == MOD_SMOKE_SLICE_AXIS_ALIGNED &&
	    domain->axis_slice_method == AXIS_SLICE_SINGLE)
	{
		const int axis = (domain->slice_axis == SLICE_AXIS_AUTO) ?
		                     axis_dominant_v3_single(viewnormal) : domain->slice_axis - 1;

		xyz[axis] = (float)base_res[axis] * domain->slice_depth;
		num_points_v[axis] = 1;
		res_max[axis] = xyz[axis] + 1;
	}

	vector_draw_func func;
	int max_points;

	if (domain->vector_draw_type == VECTOR_DRAW_NEEDLE) {
		func = add_needle;
		max_points = (num_points_v[0] * num_points_v[1] * num_points_v[2]) * 4 * 3;
	}
	else {
		func = add_streamline;
		max_points = (num_points_v[0] * num_points_v[1] * num_points_v[2]) * 2;
	}

	float (*verts)[3] = MEM_mallocN(sizeof(float) * 3 * max_points, "");
	float (*colors)[3] = MEM_mallocN(sizeof(float) * 3 * max_points, "");

	int num_points = 0;

	for (float x = floor(xyz[0]); x < res_max[0]; x += step_size) {
		for (float y = floor(xyz[1]); y < res_max[1]; y += step_size) {
			for (float z = floor(xyz[2]); z < res_max[2]; z += step_size) {
				int index = (floor(x) - res_min[0]) + (floor(y) - res_min[1]) * res[0] + (floor(z) - res_min[2]) * res[0] * res[1];

				float pos[3] = {
				    min[0] + ((float)x + 0.5f) * cell_size[0],
				    min[1] + ((float)y + 0.5f) * cell_size[1],
				    min[2] + ((float)z + 0.5f) * cell_size[2]
				};

				float vel[3] = {
				    vel_x[index], vel_y[index], vel_z[index]
				};

				func(verts, colors, pos, vel, domain->vector_scale, cell_size[0], &num_points);
			}
		}
	}

	glLineWidth(1.0f);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);

	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(3, GL_FLOAT, 0, colors);

	glDrawArrays(GL_LINES, 0, num_points);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	MEM_freeN(verts);
	MEM_freeN(colors);
#else
	UNUSED_VARS(domain, viewnormal);
#endif
}
