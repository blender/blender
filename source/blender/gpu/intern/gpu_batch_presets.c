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

#include "UI_interface.h"

#include "GPU_batch.h"
#include "gpu_shader_private.h"

/* Struct to store 3D Batches and their format */
static struct {
	struct {
		Gwn_Batch *sphere_high;
		Gwn_Batch *sphere_med;
		Gwn_Batch *sphere_low;
		Gwn_Batch *sphere_wire_low;
		Gwn_Batch *sphere_wire_med;
	} batch;

	Gwn_VertFormat format;

	struct {
		uint pos, nor;
	} attr_id;
} g_presets_3d = {{0}};

/* We may want 2D presets later. */

/* -------------------------------------------------------------------- */
/** \name 3D Primitives
 * \{ */

static void batch_sphere_lat_lon_vert(
        Gwn_VertBufRaw *pos_step, Gwn_VertBufRaw *nor_step,
        float lat, float lon)
{
	float pos[3];
	pos[0] = sinf(lat) * cosf(lon);
	pos[1] = cosf(lat);
	pos[2] = sinf(lat) * sinf(lon);
	copy_v3_v3(GWN_vertbuf_raw_step(pos_step), pos);
	copy_v3_v3(GWN_vertbuf_raw_step(nor_step), pos);
}

/* Replacement for gluSphere */
Gwn_Batch *gpu_batch_sphere(int lat_res, int lon_res)
{
	const float lon_inc = 2 * M_PI / lon_res;
	const float lat_inc = M_PI / lat_res;
	float lon, lat;

	Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&g_presets_3d.format);
	const uint vbo_len = (lat_res - 1) * lon_res * 6;
	GWN_vertbuf_data_alloc(vbo, vbo_len);

	Gwn_VertBufRaw pos_step, nor_step;
	GWN_vertbuf_attr_get_raw_data(vbo, g_presets_3d.attr_id.pos, &pos_step);
	GWN_vertbuf_attr_get_raw_data(vbo, g_presets_3d.attr_id.nor, &nor_step);

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

	BLI_assert(vbo_len == GWN_vertbuf_raw_used(&pos_step));
	BLI_assert(vbo_len == GWN_vertbuf_raw_used(&nor_step));

	return GWN_batch_create_ex(GWN_PRIM_TRIS, vbo, NULL, GWN_BATCH_OWNS_VBO);
}

static Gwn_Batch *batch_sphere_wire(int lat_res, int lon_res)
{
	const float lon_inc = 2 * M_PI / lon_res;
	const float lat_inc = M_PI / lat_res;
	float lon, lat;

	Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&g_presets_3d.format);
	const uint vbo_len = (lat_res * lon_res * 2) + ((lat_res - 1) * lon_res * 2);
	GWN_vertbuf_data_alloc(vbo, vbo_len);

	Gwn_VertBufRaw pos_step, nor_step;
	GWN_vertbuf_attr_get_raw_data(vbo, g_presets_3d.attr_id.pos, &pos_step);
	GWN_vertbuf_attr_get_raw_data(vbo, g_presets_3d.attr_id.nor, &nor_step);

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

	BLI_assert(vbo_len == GWN_vertbuf_raw_used(&pos_step));
	BLI_assert(vbo_len == GWN_vertbuf_raw_used(&nor_step));

	return GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
}

Gwn_Batch *GPU_batch_preset_sphere(int lod)
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

Gwn_Batch *GPU_batch_preset_sphere_wire(int lod)
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


void gpu_batch_presets_init(void)
{
	if (g_presets_3d.format.attrib_ct == 0) {
		Gwn_VertFormat *format = &g_presets_3d.format;
		g_presets_3d.attr_id.pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		g_presets_3d.attr_id.nor = GWN_vertformat_attr_add(format, "nor", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	}

	/* Hard coded resolution */
	g_presets_3d.batch.sphere_low = gpu_batch_sphere(8, 16);
	g_presets_3d.batch.sphere_med = gpu_batch_sphere(16, 10);
	g_presets_3d.batch.sphere_high = gpu_batch_sphere(32, 24);

	g_presets_3d.batch.sphere_wire_low = batch_sphere_wire(6, 8);
	g_presets_3d.batch.sphere_wire_med = batch_sphere_wire(8, 16);
}

void gpu_batch_presets_reset(void)
{
	/* Reset vao caches for these every time we switch opengl context.
	 * This way they will draw correctly for each window. */
	gwn_batch_vao_cache_clear(g_presets_3d.batch.sphere_low);
	gwn_batch_vao_cache_clear(g_presets_3d.batch.sphere_med);
	gwn_batch_vao_cache_clear(g_presets_3d.batch.sphere_high);
	gwn_batch_vao_cache_clear(g_presets_3d.batch.sphere_wire_low);
	gwn_batch_vao_cache_clear(g_presets_3d.batch.sphere_wire_med);

	UI_widget_batch_preset_reset();
}

void gpu_batch_presets_exit(void)
{
	GWN_batch_discard(g_presets_3d.batch.sphere_low);
	GWN_batch_discard(g_presets_3d.batch.sphere_med);
	GWN_batch_discard(g_presets_3d.batch.sphere_high);
	GWN_batch_discard(g_presets_3d.batch.sphere_wire_low);
	GWN_batch_discard(g_presets_3d.batch.sphere_wire_med);

	UI_widget_batch_preset_exit();
}
