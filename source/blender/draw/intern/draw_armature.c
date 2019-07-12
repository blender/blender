/*
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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_object_types.h"

#include "DRW_render.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_armature.h"

#include "DEG_depsgraph_query.h"

#include "ED_armature.h"

#include "UI_resources.h"

#include "draw_common.h"
#include "draw_manager_text.h"

#define BONE_VAR(eBone, pchan, var) ((eBone) ? (eBone->var) : (pchan->var))
#define BONE_FLAG(eBone, pchan) ((eBone) ? (eBone->flag) : (pchan->bone->flag))

#define PT_DEFAULT_RAD 0.05f /* radius of the point batch. */

/* For now just match 2.7x where possible. */
// #define USE_SOLID_COLOR

/* Reset for drawing each armature object */
static struct {
  /* Current armature object */
  Object *ob;
  /* Reset when changing current_armature */
  DRWCallBuffer *bone_octahedral_solid;
  DRWCallBuffer *bone_octahedral_wire;
  DRWCallBuffer *bone_octahedral_outline;
  DRWCallBuffer *bone_box_solid;
  DRWCallBuffer *bone_box_wire;
  DRWCallBuffer *bone_box_outline;
  DRWCallBuffer *bone_wire;
  DRWCallBuffer *bone_stick;
  DRWCallBuffer *bone_dof_sphere;
  DRWCallBuffer *bone_dof_lines;
  DRWCallBuffer *bone_envelope_solid;
  DRWCallBuffer *bone_envelope_distance;
  DRWCallBuffer *bone_envelope_wire;
  DRWCallBuffer *bone_point_solid;
  DRWCallBuffer *bone_point_wire;
  DRWCallBuffer *bone_axes;
  DRWCallBuffer *lines_relationship;
  DRWCallBuffer *lines_ik;
  DRWCallBuffer *lines_ik_no_target;
  DRWCallBuffer *lines_ik_spline;

  DRWEmptiesBufferList empties;

  DRWArmaturePasses passes;

  bool transparent;
} g_data = {NULL};

/**
 * Follow `TH_*` naming except for mixed colors.
 */
static struct {
  float select_color[4];
  float edge_select_color[4];
  float bone_select_color[4]; /* tint */
  float wire_color[4];
  float wire_edit_color[4];
  float bone_solid_color[4];
  float bone_active_unselect_color[4]; /* mix */
  float bone_pose_color[4];
  float bone_pose_active_color[4];
  float bone_pose_active_unselect_color[4]; /* mix */
  float text_hi_color[4];
  float text_color[4];
  float vertex_select_color[4];
  float vertex_color[4];

  /* not a theme, this is an override */
  const float *const_color;
  float const_wire;
} g_theme;

/* -------------------------------------------------------------------- */
/** \name Shader Groups (DRW_shgroup)
 * \{ */

/* Octahedral */
static void drw_shgroup_bone_octahedral(const float (*bone_mat)[4],
                                        const float bone_color[4],
                                        const float hint_color[4],
                                        const float outline_color[4],
                                        const eGPUShaderConfig sh_cfg)
{
  if (g_data.bone_octahedral_outline == NULL) {
    struct GPUBatch *geom = DRW_cache_bone_octahedral_wire_get();
    g_data.bone_octahedral_outline = buffer_instance_bone_shape_outline(
        g_data.passes.bone_outline, geom, sh_cfg);
  }
  if (g_data.bone_octahedral_solid == NULL && g_data.passes.bone_solid != NULL) {
    struct GPUBatch *geom = DRW_cache_bone_octahedral_get();
    g_data.bone_octahedral_solid = buffer_instance_bone_shape_solid(
        g_data.passes.bone_solid, geom, g_data.transparent, sh_cfg);
  }
  float final_bonemat[4][4];
  mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
  if (g_data.bone_octahedral_solid != NULL) {
    DRW_buffer_add_entry(g_data.bone_octahedral_solid, final_bonemat, bone_color, hint_color);
  }
  if (outline_color[3] > 0.0f) {
    DRW_buffer_add_entry(g_data.bone_octahedral_outline, final_bonemat, outline_color);
  }
}

/* Box / B-Bone */
static void drw_shgroup_bone_box(const float (*bone_mat)[4],
                                 const float bone_color[4],
                                 const float hint_color[4],
                                 const float outline_color[4],
                                 const eGPUShaderConfig sh_cfg)
{
  if (g_data.bone_box_wire == NULL) {
    struct GPUBatch *geom = DRW_cache_bone_box_wire_get();
    g_data.bone_box_outline = buffer_instance_bone_shape_outline(
        g_data.passes.bone_outline, geom, sh_cfg);
  }
  if (g_data.bone_box_solid == NULL && g_data.passes.bone_solid != NULL) {
    struct GPUBatch *geom = DRW_cache_bone_box_get();
    g_data.bone_box_solid = buffer_instance_bone_shape_solid(
        g_data.passes.bone_solid, geom, g_data.transparent, sh_cfg);
  }
  float final_bonemat[4][4];
  mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
  if (g_data.bone_box_solid != NULL) {
    DRW_buffer_add_entry(g_data.bone_box_solid, final_bonemat, bone_color, hint_color);
  }
  if (outline_color[3] > 0.0f) {
    DRW_buffer_add_entry(g_data.bone_box_outline, final_bonemat, outline_color);
  }
}

/* Wire */
static void drw_shgroup_bone_wire(const float (*bone_mat)[4],
                                  const float color[4],
                                  const eGPUShaderConfig sh_cfg)
{
  if (g_data.bone_wire == NULL) {
    g_data.bone_wire = buffer_dynlines_flat_color(g_data.passes.bone_wire, sh_cfg);
  }
  float head[3], tail[3];
  mul_v3_m4v3(head, g_data.ob->obmat, bone_mat[3]);
  DRW_buffer_add_entry(g_data.bone_wire, head, color);

  add_v3_v3v3(tail, bone_mat[3], bone_mat[1]);
  mul_m4_v3(g_data.ob->obmat, tail);
  DRW_buffer_add_entry(g_data.bone_wire, tail, color);
}

/* Stick */
static void drw_shgroup_bone_stick(const float (*bone_mat)[4],
                                   const float col_wire[4],
                                   const float col_bone[4],
                                   const float col_head[4],
                                   const float col_tail[4],
                                   const eGPUShaderConfig sh_cfg)
{
  if (g_data.bone_stick == NULL) {
    g_data.bone_stick = buffer_instance_bone_stick(g_data.passes.bone_wire, sh_cfg);
  }
  float final_bonemat[4][4], tail[4];
  mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
  add_v3_v3v3(tail, final_bonemat[3], final_bonemat[1]);
  DRW_buffer_add_entry(
      g_data.bone_stick, final_bonemat[3], tail, col_wire, col_bone, col_head, col_tail);
}

/* Envelope */
static void drw_shgroup_bone_envelope_distance(const float (*bone_mat)[4],
                                               const float *radius_head,
                                               const float *radius_tail,
                                               const float *distance,
                                               const eGPUShaderConfig sh_cfg)
{
  if (g_data.passes.bone_envelope != NULL) {
    if (g_data.bone_envelope_distance == NULL) {
      g_data.bone_envelope_distance = buffer_instance_bone_envelope_distance(
          g_data.passes.bone_envelope, sh_cfg);
      /* passes.bone_envelope should have the DRW_STATE_CULL_FRONT state enabled. */
    }
    float head_sphere[4] = {0.0f, 0.0f, 0.0f, 1.0f}, tail_sphere[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float final_bonemat[4][4];
    mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
    /* We need matrix mul because we need shear applied. */
    /* NOTE: could be done in shader if that becomes a bottleneck. */
    mul_m4_v4(final_bonemat, head_sphere);
    mul_m4_v4(final_bonemat, tail_sphere);
    head_sphere[3] = *radius_head;
    head_sphere[3] += *distance;
    tail_sphere[3] = *radius_tail;
    tail_sphere[3] += *distance;
    DRW_buffer_add_entry(
        g_data.bone_envelope_distance, head_sphere, tail_sphere, final_bonemat[0]);
  }
}

static void drw_shgroup_bone_envelope(const float (*bone_mat)[4],
                                      const float bone_color[4],
                                      const float hint_color[4],
                                      const float outline_color[4],
                                      const float *radius_head,
                                      const float *radius_tail,
                                      const eGPUShaderConfig sh_cfg)
{
  if (g_data.bone_point_wire == NULL) {
    g_data.bone_point_wire = buffer_instance_bone_sphere_outline(g_data.passes.bone_wire, sh_cfg);
  }
  if (g_data.bone_point_solid == NULL && g_data.passes.bone_solid != NULL) {
    g_data.bone_point_solid = buffer_instance_bone_sphere_solid(
        g_data.passes.bone_solid, g_data.transparent, sh_cfg);
  }
  if (g_data.bone_envelope_wire == NULL) {
    g_data.bone_envelope_wire = buffer_instance_bone_envelope_outline(g_data.passes.bone_wire,
                                                                      sh_cfg);
  }
  if (g_data.bone_envelope_solid == NULL && g_data.passes.bone_solid != NULL) {
    g_data.bone_envelope_solid = buffer_instance_bone_envelope_solid(
        g_data.passes.bone_solid, g_data.transparent, sh_cfg);
  }

  float head_sphere[4] = {0.0f, 0.0f, 0.0f, 1.0f}, tail_sphere[4] = {0.0f, 1.0f, 0.0f, 1.0f};
  float final_bonemat[4][4];
  mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
  mul_m4_v4(final_bonemat, head_sphere);
  mul_m4_v4(final_bonemat, tail_sphere);
  head_sphere[3] = *radius_head;
  tail_sphere[3] = *radius_tail;

  if (head_sphere[3] < 0.0f) {
    /* Draw Tail only */
    float tmp[4][4] = {{0.0f}};
    tmp[0][0] = tmp[1][1] = tmp[2][2] = tail_sphere[3] / PT_DEFAULT_RAD;
    tmp[3][3] = 1.0f;
    copy_v3_v3(tmp[3], tail_sphere);
    if (g_data.bone_point_solid != NULL) {
      DRW_buffer_add_entry(g_data.bone_point_solid, tmp, bone_color, hint_color);
    }
    if (outline_color[3] > 0.0f) {
      DRW_buffer_add_entry(g_data.bone_point_wire, tmp, outline_color);
    }
  }
  else if (tail_sphere[3] < 0.0f) {
    /* Draw Head only */
    float tmp[4][4] = {{0.0f}};
    tmp[0][0] = tmp[1][1] = tmp[2][2] = head_sphere[3] / PT_DEFAULT_RAD;
    tmp[3][3] = 1.0f;
    copy_v3_v3(tmp[3], head_sphere);
    if (g_data.bone_point_solid != NULL) {
      DRW_buffer_add_entry(g_data.bone_point_solid, tmp, bone_color, hint_color);
    }
    if (outline_color[3] > 0.0f) {
      DRW_buffer_add_entry(g_data.bone_point_wire, tmp, outline_color);
    }
  }
  else {
    /* Draw Body */
    float tmp_sphere[4];
    float len = len_v3v3(tail_sphere, head_sphere);
    float fac_head = (len - head_sphere[3]) / len;
    float fac_tail = (len - tail_sphere[3]) / len;

    /* Small epsilon to avoid problem with float precision in shader. */
    if (len > (tail_sphere[3] + head_sphere[3]) + 1e-8f) {

      copy_v4_v4(tmp_sphere, head_sphere);
      interp_v4_v4v4(head_sphere, tail_sphere, head_sphere, fac_head);
      interp_v4_v4v4(tail_sphere, tmp_sphere, tail_sphere, fac_tail);
      if (g_data.bone_envelope_solid != NULL) {
        DRW_buffer_add_entry(g_data.bone_envelope_solid,
                             head_sphere,
                             tail_sphere,
                             bone_color,
                             hint_color,
                             final_bonemat[0]);
      }
      if (outline_color[3] > 0.0f) {
        DRW_buffer_add_entry(
            g_data.bone_envelope_wire, head_sphere, tail_sphere, outline_color, final_bonemat[0]);
      }
    }
    else {
      float tmp[4][4] = {{0.0f}};
      float fac = max_ff(fac_head, 1.0f - fac_tail);
      interp_v4_v4v4(tmp_sphere, tail_sphere, head_sphere, clamp_f(fac, 0.0f, 1.0f));
      tmp[0][0] = tmp[1][1] = tmp[2][2] = tmp_sphere[3] / PT_DEFAULT_RAD;
      tmp[3][3] = 1.0f;
      copy_v3_v3(tmp[3], tmp_sphere);
      if (g_data.bone_point_solid != NULL) {
        DRW_buffer_add_entry(g_data.bone_point_solid, tmp, bone_color, hint_color);
      }
      if (outline_color[3] > 0.0f) {
        DRW_buffer_add_entry(g_data.bone_point_wire, tmp, outline_color);
      }
    }
  }
}

/* Custom (geometry) */

extern void drw_batch_cache_validate(Object *custom);
extern void drw_batch_cache_generate_requested(Object *custom);

static void drw_shgroup_bone_custom_solid(const float (*bone_mat)[4],
                                          const float bone_color[4],
                                          const float hint_color[4],
                                          const float outline_color[4],
                                          const eGPUShaderConfig sh_cfg,
                                          Object *custom)
{
  /* TODO(fclem) arg... less than ideal but we never iter on this object
   * to assure batch cache is valid. */
  drw_batch_cache_validate(custom);

  struct GPUBatch *surf = DRW_cache_object_surface_get(custom);
  struct GPUBatch *edges = DRW_cache_object_edge_detection_get(custom, NULL);
  struct GPUBatch *ledges = DRW_cache_object_loose_edges_get(custom);
  float final_bonemat[4][4];

  if (surf || edges || ledges) {
    mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
  }

  BLI_assert(g_data.passes.custom_shapes != NULL);

  if (surf && g_data.passes.bone_solid != NULL) {
    DRWCallBuffer *buf_geom_solid = BLI_ghash_lookup(g_data.passes.custom_shapes, surf);

    if (buf_geom_solid == NULL) {
      /* TODO(fclem) needs to be moved elsewhere. */
      drw_batch_cache_generate_requested(custom);

      /* NOTE! g_data.transparent require a separate shading group if the
       * object is transparent. This is done by passing a different ghash
       * for transparent armature in pose mode. */
      buf_geom_solid = buffer_instance_bone_shape_solid(
          g_data.passes.bone_solid, surf, g_data.transparent, sh_cfg);
      BLI_ghash_insert(g_data.passes.custom_shapes, surf, buf_geom_solid);
    }
    DRW_buffer_add_entry(buf_geom_solid, final_bonemat, bone_color, hint_color);
  }

  if (edges && outline_color[3] > 0.0f) {
    DRWCallBuffer *buf_geom_wire = BLI_ghash_lookup(g_data.passes.custom_shapes, edges);

    if (buf_geom_wire == NULL) {
      /* TODO(fclem) needs to be moved elsewhere. */
      drw_batch_cache_generate_requested(custom);

      buf_geom_wire = buffer_instance_bone_shape_outline(
          g_data.passes.bone_outline, edges, sh_cfg);

      BLI_ghash_insert(g_data.passes.custom_shapes, edges, buf_geom_wire);
    }
    DRW_buffer_add_entry(buf_geom_wire, final_bonemat, outline_color);
  }

  if (ledges) {
    DRWCallBuffer *buf_geom_ledges = BLI_ghash_lookup(g_data.passes.custom_shapes, ledges);

    if (buf_geom_ledges == NULL) {
      /* TODO(fclem) needs to be moved elsewhere. */
      drw_batch_cache_generate_requested(custom);

      buf_geom_ledges = buffer_instance_wire(g_data.passes.bone_wire, ledges);

      BLI_ghash_insert(g_data.passes.custom_shapes, ledges, buf_geom_ledges);
    }
    float final_color[4] = {outline_color[0], outline_color[1], outline_color[2], 1.0f};
    DRW_buffer_add_entry(buf_geom_ledges, final_bonemat, final_color);
  }
}

static void drw_shgroup_bone_custom_wire(const float (*bone_mat)[4],
                                         const float color[4],
                                         Object *custom)
{
  /* TODO(fclem) arg... less than ideal but we never iter on this object
   * to assure batch cache is valid. */
  drw_batch_cache_validate(custom);

  struct GPUBatch *geom = DRW_cache_object_all_edges_get(custom);

  if (geom) {
    DRWCallBuffer *buf_geom_wire = BLI_ghash_lookup(g_data.passes.custom_shapes, geom);

    if (buf_geom_wire == NULL) {
      /* TODO(fclem) needs to be moved elsewhere. */
      drw_batch_cache_generate_requested(custom);

      buf_geom_wire = buffer_instance_wire(g_data.passes.bone_wire, geom);

      BLI_ghash_insert(g_data.passes.custom_shapes, geom, buf_geom_wire);
    }
    float final_color[4] = {color[0], color[1], color[2], 1.0f};
    float final_bonemat[4][4];
    mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
    DRW_buffer_add_entry(buf_geom_wire, final_bonemat, final_color);
  }
}

static void drw_shgroup_bone_custom_empty(const float (*bone_mat)[4],
                                          const float color[4],
                                          const eGPUShaderConfig sh_cfg,
                                          Object *custom)
{
  DRWEmptiesBufferList *buffers = &g_data.empties;
  const float *draw_size = &custom->empty_drawsize;

  if (g_data.empties.plain_axes == NULL) {
    empties_callbuffers_create(g_data.passes.bone_wire, buffers, sh_cfg);
  }

  float final_color[4] = {color[0], color[1], color[2], 1.0f};
  float final_bonemat[4][4];
  mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);

  switch (custom->empty_drawtype) {
    case OB_PLAINAXES:
      DRW_buffer_add_entry(buffers->plain_axes, final_color, draw_size, final_bonemat);
      break;
    case OB_SINGLE_ARROW:
      DRW_buffer_add_entry(buffers->single_arrow, final_color, draw_size, final_bonemat);
      DRW_buffer_add_entry(buffers->single_arrow_line, final_color, draw_size, final_bonemat);
      break;
    case OB_CUBE:
      DRW_buffer_add_entry(buffers->cube, final_color, draw_size, final_bonemat);
      break;
    case OB_CIRCLE:
      DRW_buffer_add_entry(buffers->circle, final_color, draw_size, final_bonemat);
      break;
    case OB_EMPTY_SPHERE:
      DRW_buffer_add_entry(buffers->sphere, final_color, draw_size, final_bonemat);
      break;
    case OB_EMPTY_CONE:
      DRW_buffer_add_entry(buffers->cone, final_color, draw_size, final_bonemat);
      break;
    case OB_ARROWS:
      DRW_buffer_add_entry(buffers->empty_axes, final_color, draw_size, final_bonemat);
      break;
    case OB_EMPTY_IMAGE:
      break;
  }
}

/* Head and tail sphere */
static void drw_shgroup_bone_point(const float (*bone_mat)[4],
                                   const float bone_color[4],
                                   const float hint_color[4],
                                   const float outline_color[4],
                                   const eGPUShaderConfig sh_cfg)
{
  if (g_data.bone_point_wire == NULL) {
    g_data.bone_point_wire = buffer_instance_bone_sphere_outline(g_data.passes.bone_wire, sh_cfg);
  }
  if (g_data.bone_point_solid == NULL && g_data.passes.bone_solid != NULL) {
    g_data.bone_point_solid = buffer_instance_bone_sphere_solid(
        g_data.passes.bone_solid, g_data.transparent, sh_cfg);
  }
  float final_bonemat[4][4];
  mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
  if (g_data.bone_point_solid != NULL) {
    DRW_buffer_add_entry(g_data.bone_point_solid, final_bonemat, bone_color, hint_color);
  }
  if (outline_color[3] > 0.0f) {
    DRW_buffer_add_entry(g_data.bone_point_wire, final_bonemat, outline_color);
  }
}

/* Axes */
static void drw_shgroup_bone_axes(const float (*bone_mat)[4],
                                  const float color[4],
                                  const eGPUShaderConfig sh_cfg)
{
  if (g_data.bone_axes == NULL) {
    g_data.bone_axes = buffer_instance_bone_axes(g_data.passes.bone_axes, sh_cfg);
  }
  float final_bonemat[4][4];
  mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
  DRW_buffer_add_entry(g_data.bone_axes, final_bonemat, color);
}

/* Relationship lines */
static void drw_shgroup_bone_relationship_lines(const float start[3],
                                                const float end[3],
                                                const eGPUShaderConfig sh_cfg)
{
  if (g_data.lines_relationship == NULL) {
    g_data.lines_relationship = buffer_dynlines_dashed_uniform_color(
        g_data.passes.relationship_lines, g_theme.wire_color, sh_cfg);
  }
  /* reverse order to have less stipple overlap */
  float v[3];
  mul_v3_m4v3(v, g_data.ob->obmat, end);
  DRW_buffer_add_entry(g_data.lines_relationship, v);
  mul_v3_m4v3(v, g_data.ob->obmat, start);
  DRW_buffer_add_entry(g_data.lines_relationship, v);
}

static void drw_shgroup_bone_ik_lines(const float start[3],
                                      const float end[3],
                                      const eGPUShaderConfig sh_cfg)
{
  if (g_data.lines_ik == NULL) {
    static float fcolor[4] = {0.8f, 0.5f, 0.0f, 1.0f}; /* add theme! */
    g_data.lines_ik = buffer_dynlines_dashed_uniform_color(
        g_data.passes.relationship_lines, fcolor, sh_cfg);
  }
  /* reverse order to have less stipple overlap */
  float v[3];
  mul_v3_m4v3(v, g_data.ob->obmat, end);
  DRW_buffer_add_entry(g_data.lines_ik, v);
  mul_v3_m4v3(v, g_data.ob->obmat, start);
  DRW_buffer_add_entry(g_data.lines_ik, v);
}

static void drw_shgroup_bone_ik_no_target_lines(const float start[3],
                                                const float end[3],
                                                const eGPUShaderConfig sh_cfg)
{
  if (g_data.lines_ik_no_target == NULL) {
    static float fcolor[4] = {0.8f, 0.8f, 0.2f, 1.0f}; /* add theme! */
    g_data.lines_ik_no_target = buffer_dynlines_dashed_uniform_color(
        g_data.passes.relationship_lines, fcolor, sh_cfg);
  }
  /* reverse order to have less stipple overlap */
  float v[3];
  mul_v3_m4v3(v, g_data.ob->obmat, end);
  DRW_buffer_add_entry(g_data.lines_ik_no_target, v);
  mul_v3_m4v3(v, g_data.ob->obmat, start);
  DRW_buffer_add_entry(g_data.lines_ik_no_target, v);
}

static void drw_shgroup_bone_ik_spline_lines(const float start[3],
                                             const float end[3],
                                             const eGPUShaderConfig sh_cfg)
{
  if (g_data.lines_ik_spline == NULL) {
    static float fcolor[4] = {0.8f, 0.8f, 0.2f, 1.0f}; /* add theme! */
    g_data.lines_ik_spline = buffer_dynlines_dashed_uniform_color(
        g_data.passes.relationship_lines, fcolor, sh_cfg);
  }
  /* reverse order to have less stipple overlap */
  float v[3];
  mul_v3_m4v3(v, g_data.ob->obmat, end);
  DRW_buffer_add_entry(g_data.lines_ik_spline, v);
  mul_v3_m4v3(v, g_data.ob->obmat, start);
  DRW_buffer_add_entry(g_data.lines_ik_spline, v);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing Theme Helpers
 *
 * Note, this section is duplicate of code in 'drawarmature.c'.
 *
 * \{ */

/* global here is reset before drawing each bone */
static struct {
  const ThemeWireColor *bcolor;
} g_color;

/* values of colCode for set_pchan_color */
enum {
  PCHAN_COLOR_NORMAL = 0, /* normal drawing */
  PCHAN_COLOR_SOLID,      /* specific case where "solid" color is needed */
  PCHAN_COLOR_CONSTS,     /* "constraint" colors (which may/may-not be suppressed) */

  PCHAN_COLOR_SPHEREBONE_BASE, /* for the 'stick' of sphere (envelope) bones */
  PCHAN_COLOR_SPHEREBONE_END,  /* for the ends of sphere (envelope) bones */
  PCHAN_COLOR_LINEBONE,        /* for the middle of line-bones */
};

/* This function sets the color-set for coloring a certain bone */
static void set_pchan_colorset(Object *ob, bPoseChannel *pchan)
{
  bPose *pose = (ob) ? ob->pose : NULL;
  bArmature *arm = (ob) ? ob->data : NULL;
  bActionGroup *grp = NULL;
  short color_index = 0;

  /* sanity check */
  if (ELEM(NULL, ob, arm, pose, pchan)) {
    g_color.bcolor = NULL;
    return;
  }

  /* only try to set custom color if enabled for armature */
  if (arm->flag & ARM_COL_CUSTOM) {
    /* currently, a bone can only use a custom color set if it's group (if it has one),
     * has been set to use one
     */
    if (pchan->agrp_index) {
      grp = (bActionGroup *)BLI_findlink(&pose->agroups, (pchan->agrp_index - 1));
      if (grp) {
        color_index = grp->customCol;
      }
    }
  }

  /* bcolor is a pointer to the color set to use. If NULL, then the default
   * color set (based on the theme colors for 3d-view) is used.
   */
  if (color_index > 0) {
    bTheme *btheme = UI_GetTheme();
    g_color.bcolor = &btheme->tarm[(color_index - 1)];
  }
  else if (color_index == -1) {
    /* use the group's own custom color set (grp is always != NULL here) */
    g_color.bcolor = &grp->cs;
  }
  else {
    g_color.bcolor = NULL;
  }
}

/* This function is for brightening/darkening a given color (like UI_GetThemeColorShade3ubv()) */
static void cp_shade_color3ub(uchar cp[3], const int offset)
{
  int r, g, b;

  r = offset + (int)cp[0];
  CLAMP(r, 0, 255);
  g = offset + (int)cp[1];
  CLAMP(g, 0, 255);
  b = offset + (int)cp[2];
  CLAMP(b, 0, 255);

  cp[0] = r;
  cp[1] = g;
  cp[2] = b;
}

static void cp_shade_color3f(float cp[3], const float offset)
{
  add_v3_fl(cp, offset);
  CLAMP(cp[0], 0, 255);
  CLAMP(cp[1], 0, 255);
  CLAMP(cp[2], 0, 255);
}

/* This function sets the gl-color for coloring a certain bone (based on bcolor) */
static bool set_pchan_color(short colCode,
                            const int boneflag,
                            const short constflag,
                            float r_color[4])
{
  float *fcolor = r_color;
  const ThemeWireColor *bcolor = g_color.bcolor;

  switch (colCode) {
    case PCHAN_COLOR_NORMAL: {
      if (bcolor) {
        uchar cp[4] = {255};

        if (boneflag & BONE_DRAW_ACTIVE) {
          copy_v3_v3_char((char *)cp, bcolor->active);
          if (!(boneflag & BONE_SELECTED)) {
            cp_shade_color3ub(cp, -80);
          }
        }
        else if (boneflag & BONE_SELECTED) {
          copy_v3_v3_char((char *)cp, bcolor->select);
        }
        else {
          /* a bit darker than solid */
          copy_v3_v3_char((char *)cp, bcolor->solid);
          cp_shade_color3ub(cp, -50);
        }

        rgb_uchar_to_float(fcolor, cp);
      }
      else {
        if ((boneflag & BONE_DRAW_ACTIVE) && (boneflag & BONE_SELECTED)) {
          UI_GetThemeColor4fv(TH_BONE_POSE_ACTIVE, fcolor);
        }
        else if (boneflag & BONE_DRAW_ACTIVE) {
          UI_GetThemeColorBlendShade4fv(TH_WIRE, TH_BONE_POSE, 0.15f, 0, fcolor);
        }
        else if (boneflag & BONE_SELECTED) {
          UI_GetThemeColor4fv(TH_BONE_POSE, fcolor);
        }
        else {
          UI_GetThemeColor4fv(TH_WIRE, fcolor);
        }
      }

      return true;
    }
    case PCHAN_COLOR_SOLID: {
      UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);

      if (bcolor) {
        float solid_bcolor[3];
        rgb_uchar_to_float(solid_bcolor, (uchar *)bcolor->solid);
        interp_v3_v3v3(fcolor, fcolor, solid_bcolor, 1.0f);
      }

      return true;
    }
    case PCHAN_COLOR_CONSTS: {
      if ((bcolor == NULL) || (bcolor->flag & TH_WIRECOLOR_CONSTCOLS)) {
        uchar cp[4];
        if (constflag & PCHAN_HAS_TARGET) {
          rgba_char_args_set((char *)cp, 255, 150, 0, 80);
        }
        else if (constflag & PCHAN_HAS_IK) {
          rgba_char_args_set((char *)cp, 255, 255, 0, 80);
        }
        else if (constflag & PCHAN_HAS_SPLINEIK) {
          rgba_char_args_set((char *)cp, 200, 255, 0, 80);
        }
        else if (constflag & PCHAN_HAS_CONST) {
          rgba_char_args_set((char *)cp, 0, 255, 120, 80);
        }
        else {
          return false;
        }

        rgba_uchar_to_float(fcolor, cp);

        return true;
      }
      return false;
    }
    case PCHAN_COLOR_SPHEREBONE_BASE: {
      if (bcolor) {
        uchar cp[4] = {255};

        if (boneflag & BONE_DRAW_ACTIVE) {
          copy_v3_v3_char((char *)cp, bcolor->active);
        }
        else if (boneflag & BONE_SELECTED) {
          copy_v3_v3_char((char *)cp, bcolor->select);
        }
        else {
          copy_v3_v3_char((char *)cp, bcolor->solid);
        }

        rgb_uchar_to_float(fcolor, cp);
      }
      else {
        if (boneflag & BONE_DRAW_ACTIVE) {
          UI_GetThemeColorShade4fv(TH_BONE_POSE, 40, fcolor);
        }
        else if (boneflag & BONE_SELECTED) {
          UI_GetThemeColor4fv(TH_BONE_POSE, fcolor);
        }
        else {
          UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);
        }
      }

      return true;
    }
    case PCHAN_COLOR_SPHEREBONE_END: {
      if (bcolor) {
        uchar cp[4] = {255};

        if (boneflag & BONE_DRAW_ACTIVE) {
          copy_v3_v3_char((char *)cp, bcolor->active);
          cp_shade_color3ub(cp, 10);
        }
        else if (boneflag & BONE_SELECTED) {
          copy_v3_v3_char((char *)cp, bcolor->select);
          cp_shade_color3ub(cp, -30);
        }
        else {
          copy_v3_v3_char((char *)cp, bcolor->solid);
          cp_shade_color3ub(cp, -30);
        }

        rgb_uchar_to_float(fcolor, cp);
      }
      else {
        if (boneflag & BONE_DRAW_ACTIVE) {
          UI_GetThemeColorShade4fv(TH_BONE_POSE, 10, fcolor);
        }
        else if (boneflag & BONE_SELECTED) {
          UI_GetThemeColorShade4fv(TH_BONE_POSE, -30, fcolor);
        }
        else {
          UI_GetThemeColorShade4fv(TH_BONE_SOLID, -30, fcolor);
        }
      }
      break;
    }
    case PCHAN_COLOR_LINEBONE: {
      /* inner part in background color or constraint */
      if ((constflag) && ((bcolor == NULL) || (bcolor->flag & TH_WIRECOLOR_CONSTCOLS))) {
        uchar cp[4];
        if (constflag & PCHAN_HAS_TARGET) {
          rgba_char_args_set((char *)cp, 255, 150, 0, 255);
        }
        else if (constflag & PCHAN_HAS_IK) {
          rgba_char_args_set((char *)cp, 255, 255, 0, 255);
        }
        else if (constflag & PCHAN_HAS_SPLINEIK) {
          rgba_char_args_set((char *)cp, 200, 255, 0, 255);
        }
        else if (constflag & PCHAN_HAS_CONST) {
          rgba_char_args_set((char *)cp, 0, 255, 120, 255);
        }
        else if (constflag) {
          UI_GetThemeColor4ubv(TH_BONE_POSE, cp);
        } /* PCHAN_HAS_ACTION */

        rgb_uchar_to_float(fcolor, cp);
      }
      else {
        if (bcolor) {
          const char *cp = bcolor->solid;
          rgb_uchar_to_float(fcolor, (uchar *)cp);
          fcolor[3] = 204.f / 255.f;
        }
        else {
          UI_GetThemeColorShade4fv(TH_BACK, -30, fcolor);
        }
      }

      return true;
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing Color Helpers
 * \{ */

/** See: 'set_pchan_color'*/
static void update_color(const Object *ob, const float const_color[4])
{
  const bArmature *arm = ob->data;
  g_theme.const_color = const_color;
  g_theme.const_wire = (((ob->base_flag & BASE_SELECTED) || (arm->drawtype == ARM_WIRE)) ?
                            1.5f :
                            ((g_data.transparent) ? 1.0f : 0.0f));

#define NO_ALPHA(c) (((c)[3] = 1.0f), (c))

  UI_GetThemeColor3fv(TH_SELECT, NO_ALPHA(g_theme.select_color));
  UI_GetThemeColorShade3fv(TH_EDGE_SELECT, 60, NO_ALPHA(g_theme.edge_select_color));
  UI_GetThemeColorShade3fv(TH_EDGE_SELECT, -20, NO_ALPHA(g_theme.bone_select_color));
  UI_GetThemeColor3fv(TH_WIRE, NO_ALPHA(g_theme.wire_color));
  UI_GetThemeColor3fv(TH_WIRE_EDIT, NO_ALPHA(g_theme.wire_edit_color));
  UI_GetThemeColor3fv(TH_BONE_SOLID, NO_ALPHA(g_theme.bone_solid_color));
  UI_GetThemeColorBlendShade3fv(
      TH_WIRE_EDIT, TH_EDGE_SELECT, 0.15f, 0, NO_ALPHA(g_theme.bone_active_unselect_color));
  UI_GetThemeColor3fv(TH_BONE_POSE, NO_ALPHA(g_theme.bone_pose_color));
  UI_GetThemeColor3fv(TH_BONE_POSE_ACTIVE, NO_ALPHA(g_theme.bone_pose_active_color));
  UI_GetThemeColorBlendShade3fv(
      TH_WIRE, TH_BONE_POSE, 0.15f, 0, NO_ALPHA(g_theme.bone_pose_active_unselect_color));
  UI_GetThemeColor3fv(TH_TEXT_HI, NO_ALPHA(g_theme.text_hi_color));
  UI_GetThemeColor3fv(TH_TEXT, NO_ALPHA(g_theme.text_color));
  UI_GetThemeColor3fv(TH_VERTEX_SELECT, NO_ALPHA(g_theme.vertex_select_color));
  UI_GetThemeColor3fv(TH_VERTEX, NO_ALPHA(g_theme.vertex_color));

#undef NO_ALPHA
}

static const float *get_bone_solid_color(const EditBone *UNUSED(eBone),
                                         const bPoseChannel *pchan,
                                         const bArmature *arm,
                                         const int boneflag,
                                         const short constflag)
{
  if (g_theme.const_color) {
    return g_theme.bone_solid_color;
  }

  if (arm->flag & ARM_POSEMODE) {
    static float disp_color[4];
    copy_v4_v4(disp_color, pchan->draw_data->solid_color);
    set_pchan_color(PCHAN_COLOR_SOLID, boneflag, constflag, disp_color);
    return disp_color;
  }

  return g_theme.bone_solid_color;
}

static const float *get_bone_solid_with_consts_color(const EditBone *eBone,
                                                     const bPoseChannel *pchan,
                                                     const bArmature *arm,
                                                     const int boneflag,
                                                     const short constflag)
{
  if (g_theme.const_color) {
    return g_theme.bone_solid_color;
  }

  const float *col = get_bone_solid_color(eBone, pchan, arm, boneflag, constflag);

  static float consts_color[4];
  if (set_pchan_color(PCHAN_COLOR_CONSTS, boneflag, constflag, consts_color)) {
    interp_v3_v3v3(consts_color, col, consts_color, 0.5f);
  }
  else {
    copy_v4_v4(consts_color, col);
  }
  return consts_color;
}

static float get_bone_wire_thickness(int boneflag)
{
  if (g_theme.const_color) {
    return g_theme.const_wire;
  }
  else if (boneflag & (BONE_DRAW_ACTIVE | BONE_SELECTED)) {
    return 2.0f;
  }
  else {
    return 1.0f;
  }
}

static const float *get_bone_wire_color(const EditBone *eBone,
                                        const bPoseChannel *pchan,
                                        const bArmature *arm,
                                        const int boneflag,
                                        const short constflag)
{
  static float disp_color[4];

  if (g_theme.const_color) {
    copy_v3_v3(disp_color, g_theme.const_color);
  }
  else if (eBone) {
    if (boneflag & BONE_SELECTED) {
      if (boneflag & BONE_DRAW_ACTIVE) {
        copy_v3_v3(disp_color, g_theme.edge_select_color);
      }
      else {
        copy_v3_v3(disp_color, g_theme.bone_select_color);
      }
    }
    else {
      if (boneflag & BONE_DRAW_ACTIVE) {
        copy_v3_v3(disp_color, g_theme.bone_active_unselect_color);
      }
      else {
        copy_v3_v3(disp_color, g_theme.wire_edit_color);
      }
    }
  }
  else if (arm->flag & ARM_POSEMODE) {
    copy_v4_v4(disp_color, pchan->draw_data->wire_color);
    set_pchan_color(PCHAN_COLOR_NORMAL, boneflag, constflag, disp_color);
  }
  else {
    copy_v3_v3(disp_color, g_theme.vertex_color);
  }

  disp_color[3] = get_bone_wire_thickness(boneflag);

  return disp_color;
}

#define HINT_MUL 0.5f
#define HINT_SHADE 0.2f

static void bone_hint_color_shade(float hint_color[4], const float color[4])
{
  mul_v3_v3fl(hint_color, color, HINT_MUL);
  cp_shade_color3f(hint_color, -HINT_SHADE);
  hint_color[3] = 1.0f;
}

static const float *get_bone_hint_color(const EditBone *eBone,
                                        const bPoseChannel *pchan,
                                        const bArmature *arm,
                                        const int boneflag,
                                        const short constflag)
{
  static float hint_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  if (g_theme.const_color) {
    bone_hint_color_shade(hint_color, g_theme.bone_solid_color);
  }
  else {
    const float *wire_color = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);
    bone_hint_color_shade(hint_color, wire_color);
  }

  return hint_color;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Helper Utils
 * \{ */

static void pchan_draw_data_init(bPoseChannel *pchan)
{
  if (pchan->draw_data != NULL) {
    if (pchan->draw_data->bbone_matrix_len != pchan->bone->segments) {
      MEM_SAFE_FREE(pchan->draw_data);
    }
  }

  if (pchan->draw_data == NULL) {
    pchan->draw_data = MEM_mallocN(
        sizeof(*pchan->draw_data) + sizeof(Mat4) * pchan->bone->segments, __func__);
    pchan->draw_data->bbone_matrix_len = pchan->bone->segments;
  }
}

static void draw_bone_update_disp_matrix_default(EditBone *eBone, bPoseChannel *pchan)
{
  float s[4][4], ebmat[4][4];
  float length;
  float(*bone_mat)[4];
  float(*disp_mat)[4];
  float(*disp_tail_mat)[4];

  /* TODO : This should be moved to depsgraph or armature refresh
   * and not be tight to the draw pass creation.
   * This would refresh armature without invalidating the draw cache */
  if (pchan) {
    length = pchan->bone->length;
    bone_mat = pchan->pose_mat;
    disp_mat = pchan->disp_mat;
    disp_tail_mat = pchan->disp_tail_mat;
  }
  else {
    eBone->length = len_v3v3(eBone->tail, eBone->head);
    ED_armature_ebone_to_mat4(eBone, ebmat);

    length = eBone->length;
    bone_mat = ebmat;
    disp_mat = eBone->disp_mat;
    disp_tail_mat = eBone->disp_tail_mat;
  }

  scale_m4_fl(s, length);
  mul_m4_m4m4(disp_mat, bone_mat, s);
  copy_m4_m4(disp_tail_mat, disp_mat);
  translate_m4(disp_tail_mat, 0.0f, 1.0f, 0.0f);
}

/* compute connected child pointer for B-Bone drawing */
static void edbo_compute_bbone_child(bArmature *arm)
{
  EditBone *eBone;

  for (eBone = arm->edbo->first; eBone; eBone = eBone->next) {
    eBone->bbone_child = NULL;
  }

  for (eBone = arm->edbo->first; eBone; eBone = eBone->next) {
    if (eBone->parent && (eBone->flag & BONE_CONNECTED)) {
      eBone->parent->bbone_child = eBone;
    }
  }
}

/* A version of BKE_pchan_bbone_spline_setup() for previewing editmode curve settings. */
static void ebone_spline_preview(EditBone *ebone, float result_array[MAX_BBONE_SUBDIV][4][4])
{
  BBoneSplineParameters param;
  EditBone *prev, *next;
  float imat[4][4], bonemat[4][4];
  float tmp[3];

  memset(&param, 0, sizeof(param));

  param.segments = ebone->segments;
  param.length = ebone->length;

  /* Get "next" and "prev" bones - these are used for handle calculations. */
  if (ebone->bbone_prev_type == BBONE_HANDLE_AUTO) {
    /* Use connected parent. */
    if (ebone->flag & BONE_CONNECTED) {
      prev = ebone->parent;
    }
    else {
      prev = NULL;
    }
  }
  else {
    prev = ebone->bbone_prev;
  }

  if (ebone->bbone_next_type == BBONE_HANDLE_AUTO) {
    /* Use connected child. */
    next = ebone->bbone_child;
  }
  else {
    next = ebone->bbone_next;
  }

  /* compute handles from connected bones */
  if (prev || next) {
    ED_armature_ebone_to_mat4(ebone, imat);
    invert_m4(imat);

    if (prev) {
      param.use_prev = true;

      if (ebone->bbone_prev_type == BBONE_HANDLE_RELATIVE) {
        zero_v3(param.prev_h);
      }
      else if (ebone->bbone_prev_type == BBONE_HANDLE_TANGENT) {
        sub_v3_v3v3(tmp, prev->tail, prev->head);
        sub_v3_v3v3(tmp, ebone->head, tmp);
        mul_v3_m4v3(param.prev_h, imat, tmp);
      }
      else {
        param.prev_bbone = (prev->segments > 1);

        mul_v3_m4v3(param.prev_h, imat, prev->head);
      }

      if (!param.prev_bbone) {
        ED_armature_ebone_to_mat4(prev, bonemat);
        mul_m4_m4m4(param.prev_mat, imat, bonemat);
      }
    }

    if (next) {
      param.use_next = true;

      if (ebone->bbone_next_type == BBONE_HANDLE_RELATIVE) {
        copy_v3_fl3(param.next_h, 0.0f, param.length, 0.0);
      }
      else if (ebone->bbone_next_type == BBONE_HANDLE_TANGENT) {
        sub_v3_v3v3(tmp, next->tail, next->head);
        add_v3_v3v3(tmp, ebone->tail, tmp);
        mul_v3_m4v3(param.next_h, imat, tmp);
      }
      else {
        param.next_bbone = (next->segments > 1);

        mul_v3_m4v3(param.next_h, imat, next->tail);
      }

      ED_armature_ebone_to_mat4(next, bonemat);
      mul_m4_m4m4(param.next_mat, imat, bonemat);
    }
  }

  param.ease1 = ebone->ease1;
  param.ease2 = ebone->ease2;
  param.roll1 = ebone->roll1;
  param.roll2 = ebone->roll2;

  if (prev && (ebone->flag & BONE_ADD_PARENT_END_ROLL)) {
    param.roll1 += prev->roll2;
  }

  param.scale_in_x = ebone->scale_in_x;
  param.scale_in_y = ebone->scale_in_y;

  param.scale_out_x = ebone->scale_out_x;
  param.scale_out_y = ebone->scale_out_y;

  param.curve_in_x = ebone->curve_in_x;
  param.curve_in_y = ebone->curve_in_y;

  param.curve_out_x = ebone->curve_out_x;
  param.curve_out_y = ebone->curve_out_y;

  ebone->segments = BKE_pchan_bbone_spline_compute(&param, false, (Mat4 *)result_array);
}

static void draw_bone_update_disp_matrix_bbone(EditBone *eBone, bPoseChannel *pchan)
{
  float s[4][4], ebmat[4][4];
  float length, xwidth, zwidth;
  float(*bone_mat)[4];
  short bbone_segments;

  /* TODO : This should be moved to depsgraph or armature refresh
   * and not be tight to the draw pass creation.
   * This would refresh armature without invalidating the draw cache */
  if (pchan) {
    length = pchan->bone->length;
    xwidth = pchan->bone->xwidth;
    zwidth = pchan->bone->zwidth;
    bone_mat = pchan->pose_mat;
    bbone_segments = pchan->bone->segments;
  }
  else {
    eBone->length = len_v3v3(eBone->tail, eBone->head);
    ED_armature_ebone_to_mat4(eBone, ebmat);

    length = eBone->length;
    xwidth = eBone->xwidth;
    zwidth = eBone->zwidth;
    bone_mat = ebmat;
    bbone_segments = eBone->segments;
  }

  size_to_mat4(s, (const float[3]){xwidth, length / bbone_segments, zwidth});

  /* Compute BBones segment matrices... */
  /* Note that we need this even for one-segment bones, because box drawing need specific weirdo
   * matrix for the box, that we cannot use to draw end points & co. */
  if (pchan) {
    Mat4 *bbones_mat = (Mat4 *)pchan->draw_data->bbone_matrix;
    if (bbone_segments > 1) {
      BKE_pchan_bbone_spline_setup(pchan, false, false, bbones_mat);

      for (int i = bbone_segments; i--; bbones_mat++) {
        mul_m4_m4m4(bbones_mat->mat, bbones_mat->mat, s);
        mul_m4_m4m4(bbones_mat->mat, bone_mat, bbones_mat->mat);
      }
    }
    else {
      mul_m4_m4m4(bbones_mat->mat, bone_mat, s);
    }
  }
  else {
    float(*bbones_mat)[4][4] = eBone->disp_bbone_mat;

    if (bbone_segments > 1) {
      ebone_spline_preview(eBone, bbones_mat);

      for (int i = bbone_segments; i--; bbones_mat++) {
        mul_m4_m4m4(*bbones_mat, *bbones_mat, s);
        mul_m4_m4m4(*bbones_mat, bone_mat, *bbones_mat);
      }
    }
    else {
      mul_m4_m4m4(*bbones_mat, bone_mat, s);
    }
  }

  /* Grrr... We need default display matrix to draw end points, axes, etc. :( */
  draw_bone_update_disp_matrix_default(eBone, pchan);
}

static void draw_bone_update_disp_matrix_custom(bPoseChannel *pchan)
{
  float s[4][4];
  float length;
  float(*bone_mat)[4];
  float(*disp_mat)[4];
  float(*disp_tail_mat)[4];

  /* See TODO above */
  length = PCHAN_CUSTOM_DRAW_SIZE(pchan);
  bone_mat = pchan->custom_tx ? pchan->custom_tx->pose_mat : pchan->pose_mat;
  disp_mat = pchan->disp_mat;
  disp_tail_mat = pchan->disp_tail_mat;

  scale_m4_fl(s, length);
  mul_m4_m4m4(disp_mat, bone_mat, s);
  copy_m4_m4(disp_tail_mat, disp_mat);
  translate_m4(disp_tail_mat, 0.0f, 1.0f, 0.0f);
}

static void draw_axes(EditBone *eBone, bPoseChannel *pchan, const eGPUShaderConfig sh_cfg)
{
  float final_col[4];
  const float *col = (g_theme.const_color) ?
                         g_theme.const_color :
                         (BONE_FLAG(eBone, pchan) & BONE_SELECTED) ? g_theme.text_hi_color :
                                                                     g_theme.text_color;
  copy_v4_v4(final_col, col);
  /* Mix with axes color. */
  final_col[3] = (g_theme.const_color) ? 1.0 :
                                         (BONE_FLAG(eBone, pchan) & BONE_SELECTED) ? 0.3 : 0.8;
  drw_shgroup_bone_axes(BONE_VAR(eBone, pchan, disp_mat), final_col, sh_cfg);
}

static void draw_points(const EditBone *eBone,
                        const bPoseChannel *pchan,
                        const bArmature *arm,
                        const int boneflag,
                        const short constflag,
                        const eGPUShaderConfig sh_cfg,
                        const int select_id)
{
  float col_solid_root[4], col_solid_tail[4], col_wire_root[4], col_wire_tail[4];
  float col_hint_root[4], col_hint_tail[4];

  copy_v4_v4(col_solid_root, g_theme.bone_solid_color);
  copy_v4_v4(col_solid_tail, g_theme.bone_solid_color);
  copy_v4_v4(col_wire_root, (g_theme.const_color) ? g_theme.const_color : g_theme.vertex_color);
  copy_v4_v4(col_wire_tail, (g_theme.const_color) ? g_theme.const_color : g_theme.vertex_color);

  const bool is_envelope_draw = (arm->drawtype == ARM_ENVELOPE);
  static const float envelope_ignore = -1.0f;

  col_wire_tail[3] = col_wire_root[3] = get_bone_wire_thickness(boneflag);

  /* Edit bone points can be selected */
  if (eBone) {
    if (eBone->flag & BONE_ROOTSEL) {
      copy_v3_v3(col_wire_root, g_theme.vertex_select_color);
    }
    if (eBone->flag & BONE_TIPSEL) {
      copy_v3_v3(col_wire_tail, g_theme.vertex_select_color);
    }
  }
  else if (arm->flag & ARM_POSEMODE) {
    const float *solid_color = get_bone_solid_color(eBone, pchan, arm, boneflag, constflag);
    const float *wire_color = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);
    copy_v4_v4(col_wire_tail, wire_color);
    copy_v4_v4(col_wire_root, wire_color);
    copy_v4_v4(col_solid_tail, solid_color);
    copy_v4_v4(col_solid_root, solid_color);
  }

  bone_hint_color_shade(col_hint_root, (g_theme.const_color) ? col_solid_root : col_wire_root);
  bone_hint_color_shade(col_hint_tail, (g_theme.const_color) ? col_solid_tail : col_wire_tail);

  /* Draw root point if we are not connected to our parent */
  if ((BONE_FLAG(eBone, pchan) & BONE_CONNECTED) == 0) {
    if (select_id != -1) {
      DRW_select_load_id(select_id | BONESEL_ROOT);
    }

    if (eBone) {
      if (is_envelope_draw) {
        drw_shgroup_bone_envelope(eBone->disp_mat,
                                  col_solid_root,
                                  col_hint_root,
                                  col_wire_root,
                                  &eBone->rad_head,
                                  &envelope_ignore,
                                  sh_cfg);
      }
      else {
        drw_shgroup_bone_point(
            eBone->disp_mat, col_solid_root, col_hint_root, col_wire_root, sh_cfg);
      }
    }
    else {
      Bone *bone = pchan->bone;
      if (is_envelope_draw) {
        drw_shgroup_bone_envelope(pchan->disp_mat,
                                  col_solid_root,
                                  col_hint_root,
                                  col_wire_root,
                                  &bone->rad_head,
                                  &envelope_ignore,
                                  sh_cfg);
      }
      else {
        drw_shgroup_bone_point(
            pchan->disp_mat, col_solid_root, col_hint_root, col_wire_root, sh_cfg);
      }
    }
  }

  /*  Draw tip point */
  if (select_id != -1) {
    DRW_select_load_id(select_id | BONESEL_TIP);
  }

  if (is_envelope_draw) {
    const float *rad_tail = eBone ? &eBone->rad_tail : &pchan->bone->rad_tail;
    drw_shgroup_bone_envelope(BONE_VAR(eBone, pchan, disp_mat),
                              col_solid_tail,
                              col_hint_tail,
                              col_wire_tail,
                              &envelope_ignore,
                              rad_tail,
                              sh_cfg);
  }
  else {
    drw_shgroup_bone_point(BONE_VAR(eBone, pchan, disp_tail_mat),
                           col_solid_tail,
                           col_hint_tail,
                           col_wire_tail,
                           sh_cfg);
  }

  if (select_id != -1) {
    DRW_select_load_id(-1);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Bones
 * \{ */

static void draw_bone_custom_shape(EditBone *eBone,
                                   bPoseChannel *pchan,
                                   bArmature *arm,
                                   const int boneflag,
                                   const short constflag,
                                   const eGPUShaderConfig sh_cfg,
                                   const int select_id)
{
  const float *col_solid = get_bone_solid_color(eBone, pchan, arm, boneflag, constflag);
  const float *col_wire = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);
  const float *col_hint = get_bone_hint_color(eBone, pchan, arm, boneflag, constflag);
  const float(*disp_mat)[4] = pchan->disp_mat;

  if (select_id != -1) {
    DRW_select_load_id(select_id | BONESEL_BONE);
  }

  if (pchan->custom->type == OB_EMPTY) {
    Object *ob = pchan->custom;
    if (ob->empty_drawtype != OB_EMPTY_IMAGE) {
      drw_shgroup_bone_custom_empty(disp_mat, col_wire, sh_cfg, pchan->custom);
    }
  }
  if ((boneflag & BONE_DRAWWIRE) == 0) {
    drw_shgroup_bone_custom_solid(disp_mat, col_solid, col_hint, col_wire, sh_cfg, pchan->custom);
  }
  else {
    drw_shgroup_bone_custom_wire(disp_mat, col_wire, pchan->custom);
  }

  if (select_id != -1) {
    DRW_select_load_id(-1);
  }
}

static void draw_bone_envelope(EditBone *eBone,
                               bPoseChannel *pchan,
                               bArmature *arm,
                               const int boneflag,
                               const short constflag,
                               const eGPUShaderConfig sh_cfg,
                               const int select_id)
{
  const float *col_solid = get_bone_solid_with_consts_color(
      eBone, pchan, arm, boneflag, constflag);
  const float *col_wire = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);
  const float *col_hint = get_bone_hint_color(eBone, pchan, arm, boneflag, constflag);

  float *rad_head, *rad_tail, *distance;
  if (eBone) {
    rad_tail = &eBone->rad_tail;
    distance = &eBone->dist;
    rad_head = (eBone->parent && (boneflag & BONE_CONNECTED)) ? &eBone->parent->rad_tail :
                                                                &eBone->rad_head;
  }
  else {
    rad_tail = &pchan->bone->rad_tail;
    distance = &pchan->bone->dist;
    rad_head = (pchan->parent && (boneflag & BONE_CONNECTED)) ? &pchan->parent->bone->rad_tail :
                                                                &pchan->bone->rad_head;
  }

  if ((select_id == -1) && (boneflag & BONE_NO_DEFORM) == 0 &&
      ((boneflag & BONE_SELECTED) || (eBone && (boneflag & (BONE_ROOTSEL | BONE_TIPSEL))))) {
    drw_shgroup_bone_envelope_distance(
        BONE_VAR(eBone, pchan, disp_mat), rad_head, rad_tail, distance, sh_cfg);
  }

  if (select_id != -1) {
    DRW_select_load_id(select_id | BONESEL_BONE);
  }

  drw_shgroup_bone_envelope(
      BONE_VAR(eBone, pchan, disp_mat), col_solid, col_hint, col_wire, rad_head, rad_tail, sh_cfg);

  if (select_id != -1) {
    DRW_select_load_id(-1);
  }

  draw_points(eBone, pchan, arm, boneflag, constflag, sh_cfg, select_id);
}

static void draw_bone_line(EditBone *eBone,
                           bPoseChannel *pchan,
                           bArmature *arm,
                           const int boneflag,
                           const short constflag,
                           const eGPUShaderConfig sh_cfg,
                           const int select_id)
{
  const float *col_bone = get_bone_solid_with_consts_color(eBone, pchan, arm, boneflag, constflag);
  const float *col_wire = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);
  const float no_display[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const float *col_head = no_display;
  const float *col_tail = col_bone;

  if (eBone) {
    if (eBone->flag & BONE_TIPSEL) {
      col_tail = g_theme.vertex_select_color;
    }
    if (boneflag & BONE_SELECTED) {
      col_bone = g_theme.edge_select_color;
    }
    col_wire = g_theme.wire_color;
  }

  /* Draw root point if we are not connected to our parent */
  if ((BONE_FLAG(eBone, pchan) & BONE_CONNECTED) == 0) {
    if (eBone) {
      col_head = (eBone->flag & BONE_ROOTSEL) ? g_theme.vertex_select_color : col_bone;
    }
    else if (pchan) {
      col_head = col_bone;
    }
  }

  if (g_theme.const_color != NULL) {
    col_wire = no_display; /* actually shrink the display. */
    col_bone = col_head = col_tail = g_theme.const_color;
  }

  if (select_id == -1) {
    /* Not in selection mode, draw everything at once. */
    drw_shgroup_bone_stick(
        BONE_VAR(eBone, pchan, disp_mat), col_wire, col_bone, col_head, col_tail, sh_cfg);
  }
  else {
    /* In selection mode, draw bone, root and tip separately. */
    DRW_select_load_id(select_id | BONESEL_BONE);
    drw_shgroup_bone_stick(
        BONE_VAR(eBone, pchan, disp_mat), col_wire, col_bone, no_display, no_display, sh_cfg);

    if (col_head[3] > 0.0f) {
      DRW_select_load_id(select_id | BONESEL_ROOT);
      drw_shgroup_bone_stick(
          BONE_VAR(eBone, pchan, disp_mat), col_wire, no_display, col_head, no_display, sh_cfg);
    }

    DRW_select_load_id(select_id | BONESEL_TIP);
    drw_shgroup_bone_stick(
        BONE_VAR(eBone, pchan, disp_mat), col_wire, no_display, no_display, col_tail, sh_cfg);

    DRW_select_load_id(-1);
  }
}

static void draw_bone_wire(EditBone *eBone,
                           bPoseChannel *pchan,
                           bArmature *arm,
                           const int boneflag,
                           const short constflag,
                           const eGPUShaderConfig sh_cfg,
                           const int select_id)
{
  const float *col_wire = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);

  if (select_id != -1) {
    DRW_select_load_id(select_id | BONESEL_BONE);
  }

  if (pchan) {
    Mat4 *bbones_mat = (Mat4 *)pchan->draw_data->bbone_matrix;
    BLI_assert(bbones_mat != NULL);

    for (int i = pchan->bone->segments; i--; bbones_mat++) {
      drw_shgroup_bone_wire(bbones_mat->mat, col_wire, sh_cfg);
    }
  }
  else if (eBone) {
    for (int i = 0; i < eBone->segments; i++) {
      drw_shgroup_bone_wire(eBone->disp_bbone_mat[i], col_wire, sh_cfg);
    }
  }

  if (select_id != -1) {
    DRW_select_load_id(-1);
  }

  if (eBone) {
    draw_points(eBone, pchan, arm, boneflag, constflag, sh_cfg, select_id);
  }
}

static void draw_bone_box(EditBone *eBone,
                          bPoseChannel *pchan,
                          bArmature *arm,
                          const int boneflag,
                          const short constflag,
                          const eGPUShaderConfig sh_cfg,
                          const int select_id)
{
  const float *col_solid = get_bone_solid_with_consts_color(
      eBone, pchan, arm, boneflag, constflag);
  const float *col_wire = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);
  const float *col_hint = get_bone_hint_color(eBone, pchan, arm, boneflag, constflag);

  if (select_id != -1) {
    DRW_select_load_id(select_id | BONESEL_BONE);
  }

  if (pchan) {
    Mat4 *bbones_mat = (Mat4 *)pchan->draw_data->bbone_matrix;
    BLI_assert(bbones_mat != NULL);

    for (int i = pchan->bone->segments; i--; bbones_mat++) {
      drw_shgroup_bone_box(bbones_mat->mat, col_solid, col_hint, col_wire, sh_cfg);
    }
  }
  else if (eBone) {
    for (int i = 0; i < eBone->segments; i++) {
      drw_shgroup_bone_box(eBone->disp_bbone_mat[i], col_solid, col_hint, col_wire, sh_cfg);
    }
  }

  if (select_id != -1) {
    DRW_select_load_id(-1);
  }

  if (eBone) {
    draw_points(eBone, pchan, arm, boneflag, constflag, sh_cfg, select_id);
  }
}

static void draw_bone_octahedral(EditBone *eBone,
                                 bPoseChannel *pchan,
                                 bArmature *arm,
                                 const int boneflag,
                                 const short constflag,
                                 const eGPUShaderConfig sh_cfg,
                                 const int select_id)
{
  const float *col_solid = get_bone_solid_with_consts_color(
      eBone, pchan, arm, boneflag, constflag);
  const float *col_wire = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);
  const float *col_hint = get_bone_hint_color(eBone, pchan, arm, boneflag, constflag);

  if (select_id != -1) {
    DRW_select_load_id(select_id | BONESEL_BONE);
  }

  drw_shgroup_bone_octahedral(
      BONE_VAR(eBone, pchan, disp_mat), col_solid, col_hint, col_wire, sh_cfg);

  if (select_id != -1) {
    DRW_select_load_id(-1);
  }

  draw_points(eBone, pchan, arm, boneflag, constflag, sh_cfg, select_id);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Degrees of Freedom
 * \{ */

static void draw_bone_dofs(bPoseChannel *pchan)
{
  float final_bonemat[4][4], posetrans[4][4], mat[4][4];
  float amin[2], amax[2], xminmax[2], zminmax[2];
  float col_sphere[4] = {0.25f, 0.25f, 0.25f, 0.25f};
  float col_lines[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float col_xaxis[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  float col_zaxis[4] = {0.0f, 0.0f, 1.0f, 1.0f};

  if (g_data.passes.bone_envelope == NULL) {
    return;
  }

  if (g_data.bone_dof_sphere == NULL) {
    g_data.bone_dof_lines = buffer_instance_bone_dof(
        g_data.passes.bone_wire, DRW_cache_bone_dof_lines_get(), false);
    g_data.bone_dof_sphere = buffer_instance_bone_dof(
        g_data.passes.bone_envelope, DRW_cache_bone_dof_sphere_get(), true);
  }

  /* *0.5f here comes from M_PI/360.0f when rotations were still in degrees */
  xminmax[0] = sinf(pchan->limitmin[0] * 0.5f);
  xminmax[1] = sinf(pchan->limitmax[0] * 0.5f);
  zminmax[0] = sinf(pchan->limitmin[2] * 0.5f);
  zminmax[1] = sinf(pchan->limitmax[2] * 0.5f);

  unit_m4(posetrans);
  translate_m4(posetrans, pchan->pose_mat[3][0], pchan->pose_mat[3][1], pchan->pose_mat[3][2]);
  /* in parent-bone pose space... */
  if (pchan->parent) {
    copy_m4_m4(mat, pchan->parent->pose_mat);
    mat[3][0] = mat[3][1] = mat[3][2] = 0.0f;
    mul_m4_m4m4(posetrans, posetrans, mat);
  }
  /* ... but own restspace */
  mul_m4_m4m3(posetrans, posetrans, pchan->bone->bone_mat);

  float scale = pchan->bone->length * pchan->size[1];
  scale_m4_fl(mat, scale);
  mat[1][1] = -mat[1][1];
  mul_m4_m4m4(posetrans, posetrans, mat);

  /* into world space. */
  mul_m4_m4m4(final_bonemat, g_data.ob->obmat, posetrans);

  if ((pchan->ikflag & BONE_IK_XLIMIT) && (pchan->ikflag & BONE_IK_ZLIMIT)) {
    amin[0] = xminmax[0];
    amax[0] = xminmax[1];
    amin[1] = zminmax[0];
    amax[1] = zminmax[1];
    DRW_buffer_add_entry(g_data.bone_dof_sphere, final_bonemat, col_sphere, amin, amax);
    DRW_buffer_add_entry(g_data.bone_dof_lines, final_bonemat, col_lines, amin, amax);
  }
  if (pchan->ikflag & BONE_IK_XLIMIT) {
    amin[0] = xminmax[0];
    amax[0] = xminmax[1];
    amin[1] = amax[1] = 0.0f;
    DRW_buffer_add_entry(g_data.bone_dof_lines, final_bonemat, col_xaxis, amin, amax);
  }
  if (pchan->ikflag & BONE_IK_ZLIMIT) {
    amin[1] = zminmax[0];
    amax[1] = zminmax[1];
    amin[0] = amax[0] = 0.0f;
    DRW_buffer_add_entry(g_data.bone_dof_lines, final_bonemat, col_zaxis, amin, amax);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Relationships
 * \{ */

static void pchan_draw_ik_lines(bPoseChannel *pchan,
                                const bool only_temp,
                                const int constflag,
                                const eGPUShaderConfig sh_cfg)
{
  bConstraint *con;
  bPoseChannel *parchan;
  float *line_start = NULL, *line_end = NULL;

  for (con = pchan->constraints.first; con; con = con->next) {
    if (con->enforce == 0.0f) {
      continue;
    }

    switch (con->type) {
      case CONSTRAINT_TYPE_KINEMATIC: {
        bKinematicConstraint *data = (bKinematicConstraint *)con->data;
        int segcount = 0;

        /* if only_temp, only draw if it is a temporary ik-chain */
        if (only_temp && !(data->flag & CONSTRAINT_IK_TEMP)) {
          continue;
        }

        /* exclude tip from chain? */
        parchan = ((data->flag & CONSTRAINT_IK_TIP) == 0) ? pchan->parent : pchan;
        line_start = parchan->pose_tail;

        /* Find the chain's root */
        while (parchan->parent) {
          segcount++;
          if (segcount == data->rootbone || segcount > 255) {
            break; /* 255 is weak */
          }
          parchan = parchan->parent;
        }

        if (parchan) {
          line_end = parchan->pose_head;

          if (constflag & PCHAN_HAS_TARGET) {
            drw_shgroup_bone_ik_lines(line_start, line_end, sh_cfg);
          }
          else {
            drw_shgroup_bone_ik_no_target_lines(line_start, line_end, sh_cfg);
          }
        }
        break;
      }
      case CONSTRAINT_TYPE_SPLINEIK: {
        bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;
        int segcount = 0;

        /* don't draw if only_temp, as Spline IK chains cannot be temporary */
        if (only_temp) {
          continue;
        }

        parchan = pchan;
        line_start = parchan->pose_tail;

        /* Find the chain's root */
        while (parchan->parent) {
          segcount++;
          /* FIXME: revise the breaking conditions */
          if (segcount == data->chainlen || segcount > 255) {
            break; /* 255 is weak */
          }
          parchan = parchan->parent;
        }
        /* Only draw line in case our chain is more than one bone long! */
        if (parchan != pchan) { /* XXX revise the breaking conditions to only stop at the tail? */
          line_end = parchan->pose_head;
          drw_shgroup_bone_ik_spline_lines(line_start, line_end, sh_cfg);
        }
        break;
      }
    }
  }
}

static void draw_bone_relations(EditBone *ebone,
                                bPoseChannel *pchan,
                                bArmature *arm,
                                const int boneflag,
                                const short constflag,
                                const bool do_relations,
                                const eGPUShaderConfig sh_cfg)
{
  if (g_data.passes.relationship_lines) {
    if (ebone && ebone->parent) {
      if (do_relations) {
        /* Always draw for unconnected bones, regardless of selection,
         * since riggers will want to know about the links between bones
         */
        if ((boneflag & BONE_CONNECTED) == 0) {
          drw_shgroup_bone_relationship_lines(ebone->head, ebone->parent->tail, sh_cfg);
        }
      }
    }
    else if (pchan && pchan->parent) {
      if (do_relations) {
        /* Only draw if bone or its parent is selected - reduces viewport complexity with complex
         * rigs */
        if ((boneflag & BONE_SELECTED) ||
            (pchan->parent->bone && (pchan->parent->bone->flag & BONE_SELECTED))) {
          if ((boneflag & BONE_CONNECTED) == 0) {
            drw_shgroup_bone_relationship_lines(
                pchan->pose_head, pchan->parent->pose_tail, sh_cfg);
          }
        }
      }

      /* Draw a line to IK root bone if bone is selected. */
      if (arm->flag & ARM_POSEMODE) {
        if (constflag & (PCHAN_HAS_IK | PCHAN_HAS_SPLINEIK)) {
          if (boneflag & BONE_SELECTED) {
            pchan_draw_ik_lines(pchan, !do_relations, constflag, sh_cfg);
          }
        }
      }
    }
  }
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Draw Loops
 * \{ */

static void draw_armature_edit(Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EditBone *eBone;
  int index;
  const bool is_select = DRW_state_is_select();

  const bool show_text = DRW_state_show_text();
  const bool show_relations = ((draw_ctx->v3d->flag & V3D_HIDE_HELPLINES) == 0);

  const Object *ob_orig = DEG_get_original_object(ob);
  /* FIXME(campbell): We should be able to use the CoW object,
   * however the active bone isn't updated. Long term solution is an 'EditArmature' struct.
   * for now we can draw from the original armature. See: T66773. */
  // bArmature *arm = ob->data;
  bArmature *arm = ob_orig->data;

  update_color(ob, NULL);
  edbo_compute_bbone_child(arm);

  for (eBone = arm->edbo->first, index = ob_orig->runtime.select_id; eBone;
       eBone = eBone->next, index += 0x10000) {
    if (eBone->layer & arm->layer) {
      if ((eBone->flag & BONE_HIDDEN_A) == 0) {
        const int select_id = is_select ? index : (uint)-1;

        const short constflag = 0;

        /* catch exception for bone with hidden parent */
        int boneflag = eBone->flag;
        if ((eBone->parent) && !EBONE_VISIBLE(arm, eBone->parent)) {
          boneflag &= ~BONE_CONNECTED;
        }

        /* set temporary flag for drawing bone as active, but only if selected */
        if (eBone == arm->act_edbone) {
          boneflag |= BONE_DRAW_ACTIVE;
        }

        draw_bone_relations(
            eBone, NULL, arm, boneflag, constflag, show_relations, draw_ctx->sh_cfg);

        if (arm->drawtype == ARM_ENVELOPE) {
          draw_bone_update_disp_matrix_default(eBone, NULL);
          draw_bone_envelope(eBone, NULL, arm, boneflag, constflag, draw_ctx->sh_cfg, select_id);
        }
        else if (arm->drawtype == ARM_LINE) {
          draw_bone_update_disp_matrix_default(eBone, NULL);
          draw_bone_line(eBone, NULL, arm, boneflag, constflag, draw_ctx->sh_cfg, select_id);
        }
        else if (arm->drawtype == ARM_WIRE) {
          draw_bone_update_disp_matrix_bbone(eBone, NULL);
          draw_bone_wire(eBone, NULL, arm, boneflag, constflag, draw_ctx->sh_cfg, select_id);
        }
        else if (arm->drawtype == ARM_B_BONE) {
          draw_bone_update_disp_matrix_bbone(eBone, NULL);
          draw_bone_box(eBone, NULL, arm, boneflag, constflag, draw_ctx->sh_cfg, select_id);
        }
        else {
          draw_bone_update_disp_matrix_default(eBone, NULL);
          draw_bone_octahedral(eBone, NULL, arm, boneflag, constflag, draw_ctx->sh_cfg, select_id);
        }

        /* Draw names of bone */
        if (show_text && (arm->flag & ARM_DRAWNAMES)) {
          uchar color[4];
          UI_GetThemeColor4ubv((eBone->flag & BONE_SELECTED) ? TH_TEXT_HI : TH_TEXT, color);

          float vec[3];
          mid_v3_v3v3(vec, eBone->head, eBone->tail);
          mul_m4_v3(ob->obmat, vec);

          struct DRWTextStore *dt = DRW_text_cache_ensure();
          DRW_text_cache_add(dt,
                             vec,
                             eBone->name,
                             strlen(eBone->name),
                             10,
                             0,
                             DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
                             color);
        }

        /*  Draw additional axes */
        if (arm->flag & ARM_DRAWAXES) {
          draw_axes(eBone, NULL, draw_ctx->sh_cfg);
        }
      }
    }
  }
}

/* if const_color is NULL do pose mode coloring */
static void draw_armature_pose(Object *ob, const float const_color[4])
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  bArmature *arm = ob->data;
  bPoseChannel *pchan;
  int index = -1;
  Bone *bone;

  update_color(ob, const_color);

  /* We can't safely draw non-updated pose, might contain NULL bone pointers... */
  if (ob->pose->flag & POSE_RECALC) {
    return;
  }

  // if (!(base->flag & OB_FROMDUPLI)) // TODO
  {
    if ((draw_ctx->object_mode & OB_MODE_POSE) || (ob == draw_ctx->object_pose)) {
      arm->flag |= ARM_POSEMODE;
    }

    if (arm->flag & ARM_POSEMODE) {
      const Object *ob_orig = DEG_get_original_object(ob);
      index = ob_orig->runtime.select_id;
    }
  }

  const bool is_pose_select = (arm->flag & ARM_POSEMODE) && DRW_state_is_select();
  const bool show_text = DRW_state_show_text();
  const bool show_relations = ((draw_ctx->v3d->flag & V3D_HIDE_HELPLINES) == 0);

  /* being set below */
  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    bone = pchan->bone;

    /* bone must be visible */
    if ((bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0) {
      if (bone->layer & arm->layer) {
        const int select_id = is_pose_select ? index : (uint)-1;

        const short constflag = pchan->constflag;

        pchan_draw_data_init(pchan);

        if (const_color) {
          /* keep color */
        }
        else {
          /* set color-set to use */
          set_pchan_colorset(ob, pchan);
        }

        int boneflag = bone->flag;
        /* catch exception for bone with hidden parent */
        boneflag = bone->flag;
        if ((bone->parent) && (bone->parent->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG))) {
          boneflag &= ~BONE_CONNECTED;
        }

        /* set temporary flag for drawing bone as active, but only if selected */
        if (bone == arm->act_bone) {
          boneflag |= BONE_DRAW_ACTIVE;
        }

        draw_bone_relations(
            NULL, pchan, arm, boneflag, constflag, show_relations, draw_ctx->sh_cfg);

        if ((pchan->custom) && !(arm->flag & ARM_NO_CUSTOM)) {
          draw_bone_update_disp_matrix_custom(pchan);
          draw_bone_custom_shape(
              NULL, pchan, arm, boneflag, constflag, draw_ctx->sh_cfg, select_id);
        }
        else if (arm->drawtype == ARM_ENVELOPE) {
          draw_bone_update_disp_matrix_default(NULL, pchan);
          draw_bone_envelope(NULL, pchan, arm, boneflag, constflag, draw_ctx->sh_cfg, select_id);
        }
        else if (arm->drawtype == ARM_LINE) {
          draw_bone_update_disp_matrix_default(NULL, pchan);
          draw_bone_line(NULL, pchan, arm, boneflag, constflag, draw_ctx->sh_cfg, select_id);
        }
        else if (arm->drawtype == ARM_WIRE) {
          draw_bone_update_disp_matrix_bbone(NULL, pchan);
          draw_bone_wire(NULL, pchan, arm, boneflag, constflag, draw_ctx->sh_cfg, select_id);
        }
        else if (arm->drawtype == ARM_B_BONE) {
          draw_bone_update_disp_matrix_bbone(NULL, pchan);
          draw_bone_box(NULL, pchan, arm, boneflag, constflag, draw_ctx->sh_cfg, select_id);
        }
        else {
          draw_bone_update_disp_matrix_default(NULL, pchan);
          draw_bone_octahedral(NULL, pchan, arm, boneflag, constflag, draw_ctx->sh_cfg, select_id);
        }

        if (!is_pose_select && show_relations && (arm->flag & ARM_POSEMODE) &&
            (bone->flag & BONE_SELECTED) && ((ob->base_flag & BASE_FROM_DUPLI) == 0) &&
            (pchan->ikflag & (BONE_IK_XLIMIT | BONE_IK_ZLIMIT))) {
          draw_bone_dofs(pchan);
        }

        /* Draw names of bone */
        if (show_text && (arm->flag & ARM_DRAWNAMES)) {
          uchar color[4];
          UI_GetThemeColor4ubv(
              (arm->flag & ARM_POSEMODE) && (bone->flag & BONE_SELECTED) ? TH_TEXT_HI : TH_TEXT,
              color);
          float vec[3];
          mid_v3_v3v3(vec, pchan->pose_head, pchan->pose_tail);
          mul_m4_v3(ob->obmat, vec);

          struct DRWTextStore *dt = DRW_text_cache_ensure();
          DRW_text_cache_add(dt,
                             vec,
                             pchan->name,
                             strlen(pchan->name),
                             10,
                             0,
                             DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
                             color);
        }

        /*  Draw additional axes */
        if (arm->flag & ARM_DRAWAXES) {
          draw_axes(NULL, pchan, draw_ctx->sh_cfg);
        }
      }
    }
    if (is_pose_select) {
      index += 0x10000;
    }
  }

  arm->flag &= ~ARM_POSEMODE;
}

/**
 * This function set the object space to use for all subsequent `DRW_shgroup_bone_*` calls.
 */
static void drw_shgroup_armature(Object *ob, DRWArmaturePasses passes, bool transp)
{
  memset(&g_data, 0x0, sizeof(g_data));
  g_data.ob = ob;
  g_data.passes = passes;
  g_data.transparent = transp;
  memset(&g_color, 0x0, sizeof(g_color));
}

void DRW_shgroup_armature_object(Object *ob,
                                 ViewLayer *view_layer,
                                 DRWArmaturePasses passes,
                                 bool transp)
{
  float *color;
  DRW_object_wire_theme_get(ob, view_layer, &color);
  passes.bone_envelope = NULL; /* Don't do envelope distance in object mode. */
  drw_shgroup_armature(ob, passes, transp);
  draw_armature_pose(ob, color);
}

void DRW_shgroup_armature_pose(Object *ob, DRWArmaturePasses passes, bool transp)
{
  drw_shgroup_armature(ob, passes, transp);
  draw_armature_pose(ob, NULL);
}

void DRW_shgroup_armature_edit(Object *ob, DRWArmaturePasses passes, bool transp)
{
  drw_shgroup_armature(ob, passes, transp);
  draw_armature_edit(ob);
}

/** \} */
