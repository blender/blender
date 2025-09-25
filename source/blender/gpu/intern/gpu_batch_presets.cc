/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_threads.h"

#include "MEM_guardedalloc.h"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh" /* Own include. */

/* -------------------------------------------------------------------- */
/** \name Local Structures
 * \{ */

/* Struct to store 3D Batches and their format */
static struct {
  struct {
    blender::gpu::Batch *sphere_high;
    blender::gpu::Batch *sphere_med;
    blender::gpu::Batch *sphere_low;
    blender::gpu::Batch *sphere_wire_low;
    blender::gpu::Batch *sphere_wire_med;
  } batch;

  GPUVertFormat format;

  struct {
    uint pos, nor;
  } attr_id;

  ThreadMutex mutex;
} g_presets_3d = {{nullptr}};

static struct {
  struct {
    blender::gpu::Batch *quad;
  } batch;

  GPUVertFormat format;

  struct {
    uint pos, col;
  } attr_id;
} g_presets_2d = {{nullptr}};

static ListBase presets_list = {nullptr, nullptr};
static ListBase buffer_list = {nullptr, nullptr};

/** \} */

/* -------------------------------------------------------------------- */
/** \name 3D Primitives
 * \{ */

static GPUVertFormat &preset_3d_format()
{
  if (g_presets_3d.format.attr_len == 0) {
    GPUVertFormat *format = &g_presets_3d.format;
    g_presets_3d.attr_id.pos = GPU_vertformat_attr_add(
        format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
    g_presets_3d.attr_id.nor = GPU_vertformat_attr_add(
        format, "nor", blender::gpu::VertAttrType::SFLOAT_32_32_32);
  }
  return g_presets_3d.format;
}

static GPUVertFormat &preset_2d_format()
{
  if (g_presets_2d.format.attr_len == 0) {
    GPUVertFormat *format = &g_presets_2d.format;
    g_presets_2d.attr_id.pos = GPU_vertformat_attr_add(
        format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    g_presets_2d.attr_id.col = GPU_vertformat_attr_add(
        format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32_32);
  }
  return g_presets_2d.format;
}

static void batch_sphere_lat_lon_vert(GPUVertBufRaw *pos_step,
                                      GPUVertBufRaw *nor_step,
                                      float lat,
                                      float lon)
{
  float pos[3];
  pos[0] = sinf(lat) * cosf(lon);
  pos[1] = cosf(lat);
  pos[2] = sinf(lat) * sinf(lon);
  copy_v3_v3(static_cast<float *>(GPU_vertbuf_raw_step(pos_step)), pos);
  copy_v3_v3(static_cast<float *>(GPU_vertbuf_raw_step(nor_step)), pos);
}
blender::gpu::Batch *GPU_batch_preset_sphere(int lod)
{
  BLI_assert(lod >= 0 && lod <= 2);
  BLI_assert(BLI_thread_is_main());

  if (lod == 0) {
    return g_presets_3d.batch.sphere_low;
  }
  if (lod == 1) {
    return g_presets_3d.batch.sphere_med;
  }

  return g_presets_3d.batch.sphere_high;
}

blender::gpu::Batch *GPU_batch_preset_sphere_wire(int lod)
{
  BLI_assert(lod >= 0 && lod <= 1);
  BLI_assert(BLI_thread_is_main());

  if (lod == 0) {
    return g_presets_3d.batch.sphere_wire_low;
  }

  return g_presets_3d.batch.sphere_wire_med;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Create Sphere (3D)
 * \{ */

static blender::gpu::Batch *gpu_batch_sphere(int lat_res, int lon_res)
{
  const float lon_inc = 2 * M_PI / lon_res;
  const float lat_inc = M_PI / lat_res;
  float lon, lat;

  blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(preset_3d_format());
  const uint vbo_len = (lat_res - 1) * lon_res * 6;
  GPU_vertbuf_data_alloc(*vbo, vbo_len);

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
        batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat, lon);
      }

      if (j != 0) { /* Pole */
        batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat, lon + lon_inc);
        batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat + lat_inc, lon + lon_inc);
        batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat, lon);
      }
    }
  }

  BLI_assert(vbo_len == GPU_vertbuf_raw_used(&pos_step));
  BLI_assert(vbo_len == GPU_vertbuf_raw_used(&nor_step));

  return GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, nullptr, GPU_BATCH_OWNS_VBO);
}

static blender::gpu::Batch *batch_sphere_wire(int lat_res, int lon_res)
{
  const float lon_inc = 2 * M_PI / lon_res;
  const float lat_inc = M_PI / lat_res;
  float lon, lat;

  blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(preset_3d_format());
  const uint vbo_len = (lat_res * lon_res * 2) + ((lat_res - 1) * lon_res * 2);
  GPU_vertbuf_data_alloc(*vbo, vbo_len);

  GPUVertBufRaw pos_step, nor_step;
  GPU_vertbuf_attr_get_raw_data(vbo, g_presets_3d.attr_id.pos, &pos_step);
  GPU_vertbuf_attr_get_raw_data(vbo, g_presets_3d.attr_id.nor, &nor_step);

  lon = 0.0f;
  for (int i = 0; i < lon_res; i++, lon += lon_inc) {
    lat = 0.0f;
    for (int j = 0; j < lat_res; j++, lat += lat_inc) {
      batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat + lat_inc, lon);
      batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat, lon);

      if (j != lat_res - 1) { /* Pole */
        batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat + lat_inc, lon + lon_inc);
        batch_sphere_lat_lon_vert(&pos_step, &nor_step, lat + lat_inc, lon);
      }
    }
  }

  BLI_assert(vbo_len == GPU_vertbuf_raw_used(&pos_step));
  BLI_assert(vbo_len == GPU_vertbuf_raw_used(&nor_step));

  return GPU_batch_create_ex(GPU_PRIM_LINES, vbo, nullptr, GPU_BATCH_OWNS_VBO);
}

blender::gpu::Batch *GPU_batch_preset_quad()
{
  if (!g_presets_2d.batch.quad) {
    blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(preset_2d_format());
    GPU_vertbuf_data_alloc(*vbo, 4);

    float pos_data[4][2] = {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}};
    GPU_vertbuf_attr_fill(vbo, g_presets_2d.attr_id.pos, pos_data);
    /* Don't fill the color. */

    g_presets_2d.batch.quad = GPU_batch_create_ex(
        GPU_PRIM_TRI_STRIP, vbo, nullptr, GPU_BATCH_OWNS_VBO);

    gpu_batch_presets_register(g_presets_2d.batch.quad);
  }
  return g_presets_2d.batch.quad;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preset Registration Management
 * \{ */

void gpu_batch_presets_init()
{
  BLI_mutex_init(&g_presets_3d.mutex);

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

void gpu_batch_presets_register(blender::gpu::Batch *preset_batch)
{
  BLI_mutex_lock(&g_presets_3d.mutex);
  BLI_addtail(&presets_list, BLI_genericNodeN(preset_batch));
  BLI_mutex_unlock(&g_presets_3d.mutex);
}

void gpu_batch_storage_buffer_register(blender::gpu::StorageBuf *preset_buffer)
{
  BLI_mutex_lock(&g_presets_3d.mutex);
  BLI_addtail(&buffer_list, BLI_genericNodeN(preset_buffer));
  BLI_mutex_unlock(&g_presets_3d.mutex);
}

void gpu_batch_presets_exit()
{
  while (LinkData *link = static_cast<LinkData *>(BLI_pophead(&presets_list))) {
    blender::gpu::Batch *preset = static_cast<blender::gpu::Batch *>(link->data);
    GPU_batch_discard(preset);
    MEM_freeN(link);
  }

  while (LinkData *link = static_cast<LinkData *>(BLI_pophead(&buffer_list))) {
    blender::gpu::StorageBuf *preset = static_cast<blender::gpu::StorageBuf *>(link->data);
    GPU_storagebuf_free(preset);
    MEM_freeN(link);
  }

  /* Reset pointers to null for subsequent initializations after tear-down. */
  g_presets_2d = {{nullptr}};
  g_presets_3d = {{nullptr}};
  presets_list = {nullptr, nullptr};

  BLI_mutex_end(&g_presets_3d.mutex);
}

/** \} */
