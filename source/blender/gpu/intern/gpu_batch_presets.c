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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_batch_presets.c
 *  \ingroup gpu
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_listbase.h"
#include "MEM_guardedalloc.h"

#include "UI_interface.h"

#include "GPU_batch.h"
#include "GPU_batch_utils.h"
#include "GPU_batch_presets.h"  /* own include */
#include "gpu_shader_private.h"

/* Struct to store 3D Batches and their format */
static struct {
	struct {
		GPUBatch *sphere_high;
		GPUBatch *sphere_med;
		GPUBatch *sphere_low;
		GPUBatch *sphere_wire_low;
		GPUBatch *sphere_wire_med;
	} batch;

	GPUVertFormat format;

	struct {
		uint pos, nor;
	} attr_id;
} g_presets_3d = {{0}};

static ListBase presets_list = {NULL, NULL};


/* -------------------------------------------------------------------- */
/** \name 3D Primitives
 * \{ */

static GPUVertFormat *preset_3d_format(void)
{
	if (g_presets_3d.format.attr_len == 0) {
		GPUVertFormat *format = &g_presets_3d.format;
		g_presets_3d.attr_id.pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		g_presets_3d.attr_id.nor = GPU_vertformat_attr_add(format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	}
	return &g_presets_3d.format;
}

static void batch_sphere_lat_lon_vert(
        GPUVertBufRaw *pos_step, GPUVertBufRaw *nor_step,
        float lat, float lon)
{
	float pos[3];
	pos[0] = sinf(lat) * cosf(lon);
	pos[1] = cosf(lat);
	pos[2] = sinf(lat) * sinf(lon);
	copy_v3_v3(GPU_vertbuf_raw_step(pos_step), pos);
	copy_v3_v3(GPU_vertbuf_raw_step(nor_step), pos);
}
GPUBatch *GPU_batch_preset_sphere(int lod)
{
	BLI_assert(lod >= 0 && lod <= 2);
	BLI_assert(BLI_thread_is_main());

	if (lod == 0) {
		return g_presets_3d.batch.sphere_low;
	}
	else if (lod == 1) {
		return g_presets_3d.batch.sphere_med;
	}
	else {
		return g_presets_3d.batch.sphere_high;
	}
}

GPUBatch *GPU_batch_preset_sphere_wire(int lod)
{
	BLI_assert(lod >= 0 && lod <= 1);
	BLI_assert(BLI_thread_is_main());

	if (lod == 0) {
		return g_presets_3d.batch.sphere_wire_low;
	}
	else {
		return g_presets_3d.batch.sphere_wire_med;
	}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Create Sphere (3D)
 * \{ */

/* Replacement for gluSphere */
GPUBatch *gpu_batch_sphere(int lat_res, int lon_res)
{
	const float lon_inc = 2 * M_PI / lon_res;
	const float lat_inc = M_PI / lat_res;
	float lon, lat;

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(preset_3d_format());
	const uint vbo_len = (lat_res - 1) * lon_res * 6;
	GPU_vertbuf_data_alloc(vbo, vbo_len);

	GPUVertBufRaw pos_step, nor_step;
	GPU_vertbuf_attr_get_raw_data(vbo, g_presets_3d.attr_id.pos, &pos_step);
	GPU_vertbuf_attr_get_raw_data(vbo, g_presets_3d.attr_id.nor, &nor_step);

	lon = 0.0f;
	for (int i = 0; i < lon_res; i++, lon += lon_inc) {
		lat = 0.0f;
		for (int j = 0; j < lat_res; j++, lat += lat_inc) {
			if (j != lat_res - 1) { /* Pole */
				batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat + lat_inc, lon + lon_inc);
				batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat + lat_inc, lon);
				batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat,           lon);
			}

			if (j != 0) { /* Pole */
				batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat,           lon + lon_inc);
				batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat + lat_inc, lon + lon_inc);
				batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat,           lon);
			}
		}
	}

	BLI_assert(vbo_len == GPU_vertbuf_raw_used(&pos_step));
	BLI_assert(vbo_len == GPU_vertbuf_raw_used(&nor_step));

	return GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

static GPUBatch *batch_sphere_wire(int lat_res, int lon_res)
{
	const float lon_inc = 2 * M_PI / lon_res;
	const float lat_inc = M_PI / lat_res;
	float lon, lat;

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(preset_3d_format());
	const uint vbo_len = (lat_res * lon_res * 2) + ((lat_res - 1) * lon_res * 2);
	GPU_vertbuf_data_alloc(vbo, vbo_len);

	GPUVertBufRaw pos_step, nor_step;
	GPU_vertbuf_attr_get_raw_data(vbo, g_presets_3d.attr_id.pos, &pos_step);
	GPU_vertbuf_attr_get_raw_data(vbo, g_presets_3d.attr_id.nor, &nor_step);

	lon = 0.0f;
	for (int i = 0; i < lon_res; i++, lon += lon_inc) {
		lat = 0.0f;
		for (int j = 0; j < lat_res; j++, lat += lat_inc) {
			batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat + lat_inc, lon);
			batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat,           lon);

			if (j != lat_res - 1) { /* Pole */
				batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat + lat_inc, lon + lon_inc);
				batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat + lat_inc, lon);
			}
		}
	}

	BLI_assert(vbo_len == GPU_vertbuf_raw_used(&pos_step));
	BLI_assert(vbo_len == GPU_vertbuf_raw_used(&nor_step));

	return GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/** \} */

void gpu_batch_presets_init(void)
{
	/* Hard coded resolution */
	g_presets_3d.batch.sphere_low = gpu_batch_sphere(8, 16);
	gpu_batch_presets_register(g_presets_3d.batch.sphere_low);

	g_presets_3d.batch.sphere_med = gpu_batch_sphere(16, 10);
	gpu_batch_presets_register(g_presets_3d.batch.sphere_med);

	g_presets_3d.batch.sphere_high = gpu_batch_sphere(32, 24);
	gpu_batch_presets_register(g_presets_3d.batch.sphere_high);

	g_presets_3d.batch.sphere_wire_low = batch_sphere_wire(6, 8);
	gpu_batch_presets_register(g_presets_3d.batch.sphere_wire_low);

	g_presets_3d.batch.sphere_wire_med = batch_sphere_wire(8, 16);
	gpu_batch_presets_register(g_presets_3d.batch.sphere_wire_med);
}

void gpu_batch_presets_register(GPUBatch *preset_batch)
{
	BLI_addtail(&presets_list, BLI_genericNodeN(preset_batch));
}

void gpu_batch_presets_reset(void)
{
	/* Reset vao caches for these every time we switch opengl context.
	 * This way they will draw correctly for each window. */
	LinkData *link = presets_list.first;
	for (link = presets_list.first; link; link = link->next) {
		GPUBatch *preset = link->data;
		GPU_batch_vao_cache_clear(preset);
	}
}

void gpu_batch_presets_exit(void)
{
	LinkData *link;
	while ((link = BLI_pophead(&presets_list))) {
		GPUBatch *preset = link->data;
		GPU_batch_discard(preset);
		MEM_freeN(link);
	}
}
