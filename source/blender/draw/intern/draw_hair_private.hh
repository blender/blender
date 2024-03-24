/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#define MAX_LAYER_NAME_CT 4 /* `u0123456789, u, au, a0123456789`. */
#define MAX_LAYER_NAME_LEN (GPU_MAX_SAFE_ATTR_NAME + 2)
#define MAX_THICKRES 2    /* see eHairType */
#define MAX_HAIR_SUBDIV 4 /* see hair_subdiv rna */

enum ParticleRefineShader {
  PART_REFINE_CATMULL_ROM = 0,
  PART_REFINE_MAX_SHADER,
};

struct ModifierData;
struct Object;
struct ParticleHairCache;
struct ParticleSystem;

struct ParticleHairFinalCache {
  /* Output of the subdivision stage: vertex buff sized to subdiv level. */
  blender::gpu::VertBuf *proc_buf;

  /* Just contains a huge index buffer used to draw the final hair. */
  GPUBatch *proc_hairs[MAX_THICKRES];

  int strands_res; /* points per hair, at least 2 */
};

struct ParticleHairCache {
  blender::gpu::VertBuf *pos;
  blender::gpu::IndexBuf *indices;
  GPUBatch *hairs;

  /* Hair Procedural display: Interpolation is done on the GPU. */
  blender::gpu::VertBuf *proc_point_buf; /* Input control points */

  /** Infos of control points strands (segment count and base index) */
  blender::gpu::VertBuf *proc_strand_buf;

  /* Hair Length */
  blender::gpu::VertBuf *proc_length_buf;

  blender::gpu::VertBuf *proc_strand_seg_buf;

  blender::gpu::VertBuf *proc_uv_buf[MAX_MTFACE];
  GPUTexture *uv_tex[MAX_MTFACE];
  char uv_layer_names[MAX_MTFACE][MAX_LAYER_NAME_CT][MAX_LAYER_NAME_LEN];

  blender::gpu::VertBuf **proc_col_buf;
  GPUTexture **col_tex;
  char (*col_layer_names)[MAX_LAYER_NAME_CT][MAX_LAYER_NAME_LEN];

  int num_uv_layers;
  int num_col_layers;

  ParticleHairFinalCache final[MAX_HAIR_SUBDIV];

  int strands_len;
  int elems_len;
  int point_len;
};

namespace blender::draw {

/**
 * Ensure all textures and buffers needed for GPU accelerated drawing.
 */
bool particles_ensure_procedural_data(Object *object,
                                      ParticleSystem *psys,
                                      ModifierData *md,
                                      ParticleHairCache **r_hair_cache,
                                      GPUMaterial *gpu_material,
                                      int subdiv,
                                      int thickness_res);

}  // namespace blender::draw
