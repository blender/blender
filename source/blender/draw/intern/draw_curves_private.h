/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_THICKRES 2    /* see eHairType */
#define MAX_HAIR_SUBDIV 4 /* see hair_subdiv rna */

typedef enum CurvesEvalShader {
  CURVES_EVAL_CATMULL_ROM = 0,
  CURVES_EVAL_BEZIER = 1,
} CurvesEvalShader;
#define CURVES_EVAL_SHADER_NUM 3

struct GPUVertBuf;
struct GPUIndexBuf;
struct GPUBatch;
struct GPUTexture;

typedef struct CurvesEvalFinalCache {
  /* Output of the subdivision stage: vertex buff sized to subdiv level. */
  GPUVertBuf *proc_buf;
  GPUTexture *proc_tex;

  /* Just contains a huge index buffer used to draw the final hair. */
  GPUBatch *proc_hairs[MAX_THICKRES];

  int strands_res; /* points per hair, at least 2 */
} CurvesEvalFinalCache;

typedef struct CurvesEvalCache {
  GPUVertBuf *pos;
  GPUIndexBuf *indices;
  GPUBatch *hairs;

  /* Hair Procedural display: Interpolation is done on the GPU. */
  GPUVertBuf *proc_point_buf; /* Input control points */
  GPUTexture *point_tex;

  /** Infos of control points strands (segment count and base index) */
  GPUVertBuf *proc_strand_buf;
  GPUTexture *strand_tex;

  /* Hair Length */
  GPUVertBuf *proc_length_buf;
  GPUTexture *length_tex;

  GPUVertBuf *proc_strand_seg_buf;
  GPUTexture *strand_seg_tex;

  CurvesEvalFinalCache final[MAX_HAIR_SUBDIV];

  int strands_len;
  int elems_len;
  int point_len;
} CurvesEvalCache;

/**
 * Ensure all textures and buffers needed for GPU accelerated drawing.
 */
bool curves_ensure_procedural_data(struct Object *object,
                                   struct CurvesEvalCache **r_hair_cache,
                                   struct GPUMaterial *gpu_material,
                                   int subdiv,
                                   int thickness_res);

#ifdef __cplusplus
}
#endif
