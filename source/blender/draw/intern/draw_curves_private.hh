/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "GPU_shader.hh"

#include "draw_attributes.hh"

struct Curves;
struct GPUVertBuf;
struct GPUBatch;
struct GPUMaterial;

namespace blender::draw {

#define MAX_THICKRES 2    /* see eHairType */
#define MAX_HAIR_SUBDIV 4 /* see hair_subdiv rna */

enum CurvesEvalShader {
  CURVES_EVAL_CATMULL_ROM = 0,
  CURVES_EVAL_BEZIER = 1,
};
#define CURVES_EVAL_SHADER_NUM 3

struct CurvesEvalFinalCache {
  /* Output of the subdivision stage: vertex buffer sized to subdiv level. */
  GPUVertBuf *proc_buf;

  /** Just contains a huge index buffer used to draw the final curves. */
  GPUBatch *proc_hairs[MAX_THICKRES];

  /** Points per curve, at least 2. */
  int resolution;

  /** Attributes currently being drawn or about to be drawn. */
  DRW_Attributes attr_used;

  /**
   * Attributes that were used at some point. This is used for garbage collection, to remove
   * attributes that are not used in shaders anymore due to user edits.
   */
  DRW_Attributes attr_used_over_time;

  /**
   * The last time in seconds that the `attr_used` and `attr_used_over_time` were exactly the same.
   * If the delta between this time and the current scene time is greater than the timeout set in
   * user preferences (`U.vbotimeout`) then garbage collection is performed.
   */
  int last_attr_matching_time;

  /* Output of the subdivision stage: vertex buffers sized to subdiv level. This is only attributes
   * on point domain. */
  GPUVertBuf *attributes_buf[GPU_MAX_ATTR];
};

/* Curves procedural display: Evaluation is done on the GPU. */
struct CurvesEvalCache {
  /* Control point positions on evaluated data-block combined with parameter data. */
  GPUVertBuf *proc_point_buf;

  /** Info of control points strands (segment count and base index) */
  GPUVertBuf *proc_strand_buf;

  /* Curve length data. */
  GPUVertBuf *proc_length_buf;

  GPUVertBuf *proc_strand_seg_buf;

  CurvesEvalFinalCache final[MAX_HAIR_SUBDIV];

  /* For point attributes, which need subdivision, these buffers contain the input data.
   * For curve domain attributes, which do not need subdivision, these are the final data. */
  GPUVertBuf *proc_attributes_buf[GPU_MAX_ATTR];

  int curves_num;
  int points_num;
};

/**
 * Ensure all necessary textures and buffers exist for GPU accelerated drawing.
 */
bool curves_ensure_procedural_data(Curves *curves_id,
                                   CurvesEvalCache **r_cache,
                                   const GPUMaterial *gpu_material,
                                   int subdiv,
                                   int thickness_res);

void drw_curves_get_attribute_sampler_name(const char *layer_name, char r_sampler_name[32]);

}  // namespace blender::draw
