/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "draw_manager.h"

#include "BLI_alloca.h"
#include "BLI_math.h"
#include "BLI_math_bits.h"
#include "BLI_memblock.h"

#include "BKE_global.h"

#include "GPU_compute.h"
#include "GPU_platform.h"
#include "GPU_shader.h"
#include "GPU_state.h"

#ifdef USE_GPU_SELECT
#  include "GPU_select.h"
#endif

void DRW_select_load_id(uint id)
{
#ifdef USE_GPU_SELECT
  BLI_assert(G.f & G_FLAG_PICKSEL);
  DST.select_id = id;
#endif
}

#define DEBUG_UBO_BINDING

typedef struct DRWCommandsState {
  GPUBatch *batch;
  int resource_chunk;
  int resource_id;
  int base_inst;
  int inst_count;
  bool neg_scale;
  /* Resource location. */
  int obmats_loc;
  int obinfos_loc;
  int obattrs_loc;
  int vlattrs_loc;
  int baseinst_loc;
  int chunkid_loc;
  int resourceid_loc;
  /* Legacy matrix support. */
  int obmat_loc;
  int obinv_loc;
  /* Uniform Attributes. */
  DRWSparseUniformBuf *obattrs_ubo;
  /* Selection ID state. */
  GPUVertBuf *select_buf;
  uint select_id;
  /* Drawing State */
  DRWState drw_state_enabled;
  DRWState drw_state_disabled;
} DRWCommandsState;

/* -------------------------------------------------------------------- */
/** \name Draw State (DRW_state)
 * \{ */

void drw_state_set(DRWState state)
{
  /* Mask locked state. */
  state = (~DST.state_lock & state) | (DST.state_lock & DST.state);

  if (DST.state == state) {
    return;
  }

  eGPUWriteMask write_mask = 0;
  eGPUBlend blend = 0;
  eGPUFaceCullTest culling_test = 0;
  eGPUDepthTest depth_test = 0;
  eGPUStencilTest stencil_test = 0;
  eGPUStencilOp stencil_op = 0;
  eGPUProvokingVertex provoking_vert = 0;

  if (state & DRW_STATE_WRITE_DEPTH) {
    write_mask |= GPU_WRITE_DEPTH;
  }
  if (state & DRW_STATE_WRITE_COLOR) {
    write_mask |= GPU_WRITE_COLOR;
  }
  if (state & DRW_STATE_WRITE_STENCIL_ENABLED) {
    write_mask |= GPU_WRITE_STENCIL;
  }

  switch (state & (DRW_STATE_CULL_BACK | DRW_STATE_CULL_FRONT)) {
    case DRW_STATE_CULL_BACK:
      culling_test = GPU_CULL_BACK;
      break;
    case DRW_STATE_CULL_FRONT:
      culling_test = GPU_CULL_FRONT;
      break;
    default:
      culling_test = GPU_CULL_NONE;
      break;
  }

  switch (state & DRW_STATE_DEPTH_TEST_ENABLED) {
    case DRW_STATE_DEPTH_LESS:
      depth_test = GPU_DEPTH_LESS;
      break;
    case DRW_STATE_DEPTH_LESS_EQUAL:
      depth_test = GPU_DEPTH_LESS_EQUAL;
      break;
    case DRW_STATE_DEPTH_EQUAL:
      depth_test = GPU_DEPTH_EQUAL;
      break;
    case DRW_STATE_DEPTH_GREATER:
      depth_test = GPU_DEPTH_GREATER;
      break;
    case DRW_STATE_DEPTH_GREATER_EQUAL:
      depth_test = GPU_DEPTH_GREATER_EQUAL;
      break;
    case DRW_STATE_DEPTH_ALWAYS:
      depth_test = GPU_DEPTH_ALWAYS;
      break;
    default:
      depth_test = GPU_DEPTH_NONE;
      break;
  }

  switch (state & DRW_STATE_WRITE_STENCIL_ENABLED) {
    case DRW_STATE_WRITE_STENCIL:
      stencil_op = GPU_STENCIL_OP_REPLACE;
      GPU_stencil_write_mask_set(0xFF);
      break;
    case DRW_STATE_WRITE_STENCIL_SHADOW_PASS:
      stencil_op = GPU_STENCIL_OP_COUNT_DEPTH_PASS;
      GPU_stencil_write_mask_set(0xFF);
      break;
    case DRW_STATE_WRITE_STENCIL_SHADOW_FAIL:
      stencil_op = GPU_STENCIL_OP_COUNT_DEPTH_FAIL;
      GPU_stencil_write_mask_set(0xFF);
      break;
    default:
      stencil_op = GPU_STENCIL_OP_NONE;
      GPU_stencil_write_mask_set(0x00);
      break;
  }

  switch (state & DRW_STATE_STENCIL_TEST_ENABLED) {
    case DRW_STATE_STENCIL_ALWAYS:
      stencil_test = GPU_STENCIL_ALWAYS;
      break;
    case DRW_STATE_STENCIL_EQUAL:
      stencil_test = GPU_STENCIL_EQUAL;
      break;
    case DRW_STATE_STENCIL_NEQUAL:
      stencil_test = GPU_STENCIL_NEQUAL;
      break;
    default:
      stencil_test = GPU_STENCIL_NONE;
      break;
  }

  switch (state & DRW_STATE_BLEND_ENABLED) {
    case DRW_STATE_BLEND_ADD:
      blend = GPU_BLEND_ADDITIVE;
      break;
    case DRW_STATE_BLEND_ADD_FULL:
      blend = GPU_BLEND_ADDITIVE_PREMULT;
      break;
    case DRW_STATE_BLEND_ALPHA:
      blend = GPU_BLEND_ALPHA;
      break;
    case DRW_STATE_BLEND_ALPHA_PREMUL:
      blend = GPU_BLEND_ALPHA_PREMULT;
      break;
    case DRW_STATE_BLEND_BACKGROUND:
      blend = GPU_BLEND_BACKGROUND;
      break;
    case DRW_STATE_BLEND_OIT:
      blend = GPU_BLEND_OIT;
      break;
    case DRW_STATE_BLEND_MUL:
      blend = GPU_BLEND_MULTIPLY;
      break;
    case DRW_STATE_BLEND_SUB:
      blend = GPU_BLEND_SUBTRACT;
      break;
    case DRW_STATE_BLEND_CUSTOM:
      blend = GPU_BLEND_CUSTOM;
      break;
    case DRW_STATE_LOGIC_INVERT:
      blend = GPU_BLEND_INVERT;
      break;
    case DRW_STATE_BLEND_ALPHA_UNDER_PREMUL:
      blend = GPU_BLEND_ALPHA_UNDER_PREMUL;
      break;
    default:
      blend = GPU_BLEND_NONE;
      break;
  }

  GPU_state_set(
      write_mask, blend, culling_test, depth_test, stencil_test, stencil_op, provoking_vert);

  if (state & DRW_STATE_SHADOW_OFFSET) {
    GPU_shadow_offset(true);
  }
  else {
    GPU_shadow_offset(false);
  }

  /* TODO: this should be part of shader state. */
  if (state & DRW_STATE_CLIP_PLANES) {
    GPU_clip_distances(DST.view_active->clip_planes_len);
  }
  else {
    GPU_clip_distances(0);
  }

  if (state & DRW_STATE_IN_FRONT_SELECT) {
    /* XXX `GPU_depth_range` is not a perfect solution
     * since very distant geometries can still be occluded.
     * Also the depth test precision of these geometries is impaired.
     * However, it solves the selection for the vast majority of cases. */
    GPU_depth_range(0.0f, 0.01f);
  }
  else {
    GPU_depth_range(0.0f, 1.0f);
  }

  if (state & DRW_STATE_PROGRAM_POINT_SIZE) {
    GPU_program_point_size(true);
  }
  else {
    GPU_program_point_size(false);
  }

  if (state & DRW_STATE_FIRST_VERTEX_CONVENTION) {
    GPU_provoking_vertex(GPU_VERTEX_FIRST);
  }
  else {
    GPU_provoking_vertex(GPU_VERTEX_LAST);
  }

  DST.state = state;
}

static void drw_stencil_state_set(uint write_mask, uint reference, uint compare_mask)
{
  /* Reminders:
   * - (compare_mask & reference) is what is tested against (compare_mask & stencil_value)
   *   stencil_value being the value stored in the stencil buffer.
   * - (write-mask & reference) is what gets written if the test condition is fulfilled.
   */
  GPU_stencil_write_mask_set(write_mask);
  GPU_stencil_reference_set(reference);
  GPU_stencil_compare_mask_set(compare_mask);
}

void DRW_state_reset_ex(DRWState state)
{
  DST.state = ~state;
  drw_state_set(state);
}

static void drw_state_validate(void)
{
  /* Cannot write to stencil buffer without stencil test. */
  if (DST.state & DRW_STATE_WRITE_STENCIL_ENABLED) {
    BLI_assert(DST.state & DRW_STATE_STENCIL_TEST_ENABLED);
  }
  /* Cannot write to depth buffer without depth test. */
  if (DST.state & DRW_STATE_WRITE_DEPTH) {
    BLI_assert(DST.state & DRW_STATE_DEPTH_TEST_ENABLED);
  }
}

void DRW_state_lock(DRWState state)
{
  DST.state_lock = state;

  /* We must get the current state to avoid overriding it. */
  /* Not complete, but that just what we need for now. */
  if (state & DRW_STATE_WRITE_DEPTH) {
    SET_FLAG_FROM_TEST(DST.state, GPU_depth_mask_get(), DRW_STATE_WRITE_DEPTH);
  }
  if (state & DRW_STATE_DEPTH_TEST_ENABLED) {
    DST.state &= ~DRW_STATE_DEPTH_TEST_ENABLED;

    switch (GPU_depth_test_get()) {
      case GPU_DEPTH_ALWAYS:
        DST.state |= DRW_STATE_DEPTH_ALWAYS;
        break;
      case GPU_DEPTH_LESS:
        DST.state |= DRW_STATE_DEPTH_LESS;
        break;
      case GPU_DEPTH_LESS_EQUAL:
        DST.state |= DRW_STATE_DEPTH_LESS_EQUAL;
        break;
      case GPU_DEPTH_EQUAL:
        DST.state |= DRW_STATE_DEPTH_EQUAL;
        break;
      case GPU_DEPTH_GREATER:
        DST.state |= DRW_STATE_DEPTH_GREATER;
        break;
      case GPU_DEPTH_GREATER_EQUAL:
        DST.state |= DRW_STATE_DEPTH_GREATER_EQUAL;
        break;
      default:
        break;
    }
  }
}

void DRW_state_reset(void)
{
  DRW_state_reset_ex(DRW_STATE_DEFAULT);

  GPU_texture_unbind_all();
  GPU_texture_image_unbind_all();
  GPU_uniformbuf_unbind_all();
  GPU_storagebuf_unbind_all();

  /* Should stay constant during the whole rendering. */
  GPU_point_size(5);
  GPU_line_smooth(false);
  /* Bypass #U.pixelsize factor by using a factor of 0.0f. Will be clamped to 1.0f. */
  GPU_line_width(0.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Culling (DRW_culling)
 * \{ */

static bool draw_call_is_culled(const DRWResourceHandle *handle, DRWView *view)
{
  DRWCullingState *culling = DRW_memblock_elem_from_handle(DST.vmempool->cullstates, handle);
  return (culling->mask & view->culling_mask) != 0;
}

void DRW_view_set_active(const DRWView *view)
{
  DST.view_active = (view != NULL) ? ((DRWView *)view) : DST.view_default;
}

const DRWView *DRW_view_get_active(void)
{
  return DST.view_active;
}

/* Return True if the given BoundSphere intersect the current view frustum */
static bool draw_culling_sphere_test(const BoundSphere *frustum_bsphere,
                                     const float (*frustum_planes)[4],
                                     const BoundSphere *bsphere)
{
  /* Bypass test if radius is negative. */
  if (bsphere->radius < 0.0f) {
    return true;
  }

  /* Do a rough test first: Sphere VS Sphere intersect. */
  float center_dist_sq = len_squared_v3v3(bsphere->center, frustum_bsphere->center);
  float radius_sum = bsphere->radius + frustum_bsphere->radius;
  if (center_dist_sq > square_f(radius_sum)) {
    return false;
  }
  /* TODO: we could test against the inscribed sphere of the frustum to early out positively. */

  /* Test against the 6 frustum planes. */
  /* TODO: order planes with sides first then far then near clip. Should be better culling
   * heuristic when sculpting. */
  for (int p = 0; p < 6; p++) {
    float dist = plane_point_side_v3(frustum_planes[p], bsphere->center);
    if (dist < -bsphere->radius) {
      return false;
    }
  }
  return true;
}

static bool draw_culling_box_test(const float (*frustum_planes)[4], const BoundBox *bbox)
{
  /* 6 view frustum planes */
  for (int p = 0; p < 6; p++) {
    /* 8 box vertices. */
    for (int v = 0; v < 8; v++) {
      float dist = plane_point_side_v3(frustum_planes[p], bbox->vec[v]);
      if (dist > 0.0f) {
        /* At least one point in front of this plane.
         * Go to next plane. */
        break;
      }
      if (v == 7) {
        /* 8 points behind this plane. */
        return false;
      }
    }
  }
  return true;
}

static bool draw_culling_plane_test(const BoundBox *corners, const float plane[4])
{
  /* Test against the 8 frustum corners. */
  for (int c = 0; c < 8; c++) {
    float dist = plane_point_side_v3(plane, corners->vec[c]);
    if (dist < 0.0f) {
      return true;
    }
  }
  return false;
}

bool DRW_culling_sphere_test(const DRWView *view, const BoundSphere *bsphere)
{
  view = view ? view : DST.view_default;
  return draw_culling_sphere_test(&view->frustum_bsphere, view->frustum_planes, bsphere);
}

bool DRW_culling_box_test(const DRWView *view, const BoundBox *bbox)
{
  view = view ? view : DST.view_default;
  return draw_culling_box_test(view->frustum_planes, bbox);
}

bool DRW_culling_plane_test(const DRWView *view, const float plane[4])
{
  view = view ? view : DST.view_default;
  return draw_culling_plane_test(&view->frustum_corners, plane);
}

bool DRW_culling_min_max_test(const DRWView *view, float obmat[4][4], float min[3], float max[3])
{
  view = view ? view : DST.view_default;
  float tobmat[4][4];
  transpose_m4_m4(tobmat, obmat);
  for (int i = 6; i--;) {
    float frustum_plane_local[4], bb_near[3], bb_far[3];
    mul_v4_m4v4(frustum_plane_local, tobmat, view->frustum_planes[i]);
    aabb_get_near_far_from_plane(frustum_plane_local, min, max, bb_near, bb_far);

    if (plane_point_side_v3(frustum_plane_local, bb_far) < 0.0f) {
      return false;
    }
  }

  return true;
}

void DRW_culling_frustum_corners_get(const DRWView *view, BoundBox *corners)
{
  view = view ? view : DST.view_default;
  *corners = view->frustum_corners;
}

void DRW_culling_frustum_planes_get(const DRWView *view, float planes[6][4])
{
  view = view ? view : DST.view_default;
  memcpy(planes, view->frustum_planes, sizeof(float[6][4]));
}

static void draw_compute_culling(DRWView *view)
{
  view = view->parent ? view->parent : view;

  /* TODO(fclem): multi-thread this. */
  /* TODO(fclem): compute all dirty views at once. */
  if (!view->is_dirty) {
    return;
  }

  BLI_memblock_iter iter;
  BLI_memblock_iternew(DST.vmempool->cullstates, &iter);
  DRWCullingState *cull;
  while ((cull = BLI_memblock_iterstep(&iter))) {
    if (cull->bsphere.radius < 0.0) {
      cull->mask = 0;
    }
    else {
      bool culled = !draw_culling_sphere_test(
          &view->frustum_bsphere, view->frustum_planes, &cull->bsphere);

#ifdef DRW_DEBUG_CULLING
      if (G.debug_value != 0) {
        if (culled) {
          DRW_debug_sphere(
              cull->bsphere.center, cull->bsphere.radius, (const float[4]){1, 0, 0, 1});
        }
        else {
          DRW_debug_sphere(
              cull->bsphere.center, cull->bsphere.radius, (const float[4]){0, 1, 0, 1});
        }
      }
#endif

      if (view->visibility_fn) {
        culled = !view->visibility_fn(!culled, cull->user_data);
      }

      SET_FLAG_FROM_TEST(cull->mask, culled, view->culling_mask);
    }
  }

  view->is_dirty = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw (DRW_draw)
 * \{ */

BLI_INLINE void draw_legacy_matrix_update(DRWShadingGroup *shgroup,
                                          DRWResourceHandle *handle,
                                          float obmat_loc,
                                          float obinv_loc)
{
  /* Still supported for compatibility with gpu_shader_* but should be forbidden. */
  DRWObjectMatrix *ob_mats = DRW_memblock_elem_from_handle(DST.vmempool->obmats, handle);
  if (obmat_loc != -1) {
    GPU_shader_uniform_float_ex(shgroup->shader, obmat_loc, 16, 1, (float *)ob_mats->model);
  }
  if (obinv_loc != -1) {
    GPU_shader_uniform_float_ex(shgroup->shader, obinv_loc, 16, 1, (float *)ob_mats->modelinverse);
  }
}

BLI_INLINE void draw_geometry_bind(DRWShadingGroup *shgroup, GPUBatch *geom)
{
  DST.batch = geom;

  GPU_batch_set_shader(geom, shgroup->shader);
}

BLI_INLINE void draw_geometry_execute(DRWShadingGroup *shgroup,
                                      GPUBatch *geom,
                                      int vert_first,
                                      int vert_count,
                                      int inst_first,
                                      int inst_count,
                                      int baseinst_loc)
{
  /* inst_count can be -1. */
  inst_count = max_ii(0, inst_count);

  if (baseinst_loc != -1) {
    /* Fallback when ARB_shader_draw_parameters is not supported. */
    GPU_shader_uniform_int_ex(shgroup->shader, baseinst_loc, 1, 1, (int *)&inst_first);
    /* Avoids VAO reconfiguration on older hardware. (see GPU_batch_draw_advanced) */
    inst_first = 0;
  }

  /* bind vertex array */
  if (DST.batch != geom) {
    draw_geometry_bind(shgroup, geom);
  }

  GPU_batch_draw_advanced(geom, vert_first, vert_count, inst_first, inst_count);
}

BLI_INLINE void draw_indirect_call(DRWShadingGroup *shgroup, DRWCommandsState *state)
{
  if (state->inst_count == 0) {
    return;
  }
  if (state->baseinst_loc == -1) {
    /* bind vertex array */
    if (DST.batch != state->batch) {
      GPU_draw_list_submit(DST.draw_list);
      draw_geometry_bind(shgroup, state->batch);
    }
    GPU_draw_list_append(DST.draw_list, state->batch, state->base_inst, state->inst_count);
  }
  /* Fallback when unsupported */
  else {
    draw_geometry_execute(
        shgroup, state->batch, 0, 0, state->base_inst, state->inst_count, state->baseinst_loc);
  }
}

static void draw_update_uniforms(DRWShadingGroup *shgroup,
                                 DRWCommandsState *state,
                                 bool *use_tfeedback)
{
#define MAX_UNIFORM_STACK_SIZE 64

  /* Uniform array elements stored as separate entries. We need to batch these together */
  int array_uniform_loc = -1;
  int array_index = 0;
  float mat4_stack[4 * 4];

  /* Loop through uniforms in reverse order. */
  for (DRWUniformChunk *unichunk = shgroup->uniforms; unichunk; unichunk = unichunk->next) {
    DRWUniform *uni = unichunk->uniforms + unichunk->uniform_used - 1;

    for (int i = 0; i < unichunk->uniform_used; i++, uni--) {
      /* For uniform array copies, copy per-array-element data into local buffer before upload. */
      if (uni->arraysize > 1 && uni->type == DRW_UNIFORM_FLOAT_COPY) {
        /* Only written for mat4 copy for now and is not meant to become generalized. */
        /* TODO(@fclem): Use UBOs/SSBOs instead of inline mat4 copies. */
        BLI_assert(uni->arraysize == 4 && uni->length == 4);
        /* Begin copying uniform array. */
        if (array_uniform_loc == -1) {
          array_uniform_loc = uni->location;
          array_index = uni->arraysize * uni->length;
        }
        /* Debug check same array loc. */
        BLI_assert(array_uniform_loc > -1 && array_uniform_loc == uni->location);
        /* Copy array element data to local buffer. */
        array_index -= uni->length;
        memcpy(&mat4_stack[array_index], uni->fvalue, sizeof(float) * uni->length);
        /* Flush array data to shader. */
        if (array_index <= 0) {
          GPU_shader_uniform_float_ex(shgroup->shader, uni->location, 16, 1, mat4_stack);
          array_uniform_loc = -1;
        }
        continue;
      }

      /* Handle standard cases. */
      switch (uni->type) {
        case DRW_UNIFORM_INT_COPY:
          BLI_assert(uni->arraysize == 1);
          if (uni->arraysize == 1) {
            GPU_shader_uniform_int_ex(
                shgroup->shader, uni->location, uni->length, uni->arraysize, uni->ivalue);
          }
          break;
        case DRW_UNIFORM_INT:
          GPU_shader_uniform_int_ex(
              shgroup->shader, uni->location, uni->length, uni->arraysize, uni->pvalue);
          break;
        case DRW_UNIFORM_FLOAT_COPY:
          BLI_assert(uni->arraysize == 1);
          if (uni->arraysize == 1) {
            GPU_shader_uniform_float_ex(
                shgroup->shader, uni->location, uni->length, uni->arraysize, uni->fvalue);
          }
          break;
        case DRW_UNIFORM_FLOAT:
          GPU_shader_uniform_float_ex(
              shgroup->shader, uni->location, uni->length, uni->arraysize, uni->pvalue);
          break;
        case DRW_UNIFORM_TEXTURE:
          GPU_texture_bind_ex(uni->texture, uni->sampler_state, uni->location);
          break;
        case DRW_UNIFORM_TEXTURE_REF:
          GPU_texture_bind_ex(*uni->texture_ref, uni->sampler_state, uni->location);
          break;
        case DRW_UNIFORM_IMAGE:
          GPU_texture_image_bind(uni->texture, uni->location);
          break;
        case DRW_UNIFORM_IMAGE_REF:
          GPU_texture_image_bind(*uni->texture_ref, uni->location);
          break;
        case DRW_UNIFORM_BLOCK:
          GPU_uniformbuf_bind(uni->block, uni->location);
          break;
        case DRW_UNIFORM_BLOCK_REF:
          GPU_uniformbuf_bind(*uni->block_ref, uni->location);
          break;
        case DRW_UNIFORM_STORAGE_BLOCK:
          GPU_storagebuf_bind(uni->ssbo, uni->location);
          break;
        case DRW_UNIFORM_STORAGE_BLOCK_REF:
          GPU_storagebuf_bind(*uni->ssbo_ref, uni->location);
          break;
        case DRW_UNIFORM_BLOCK_OBMATS:
          state->obmats_loc = uni->location;
          GPU_uniformbuf_bind(DST.vmempool->matrices_ubo[0], uni->location);
          break;
        case DRW_UNIFORM_BLOCK_OBINFOS:
          state->obinfos_loc = uni->location;
          GPU_uniformbuf_bind(DST.vmempool->obinfos_ubo[0], uni->location);
          break;
        case DRW_UNIFORM_BLOCK_OBATTRS:
          state->obattrs_loc = uni->location;
          state->obattrs_ubo = DRW_uniform_attrs_pool_find_ubo(DST.vmempool->obattrs_ubo_pool,
                                                               uni->uniform_attrs);
          DRW_sparse_uniform_buffer_bind(state->obattrs_ubo, 0, uni->location);
          break;
        case DRW_UNIFORM_BLOCK_VLATTRS:
          state->vlattrs_loc = uni->location;
          GPU_uniformbuf_bind(drw_ensure_layer_attribute_buffer(), uni->location);
          break;
        case DRW_UNIFORM_RESOURCE_CHUNK: {
          state->chunkid_loc = uni->location;
          int zero = 0;
          GPU_shader_uniform_int_ex(shgroup->shader, uni->location, 1, 1, &zero);
          break;
        }
        case DRW_UNIFORM_RESOURCE_ID:
          state->resourceid_loc = uni->location;
          break;
        case DRW_UNIFORM_TFEEDBACK_TARGET:
          BLI_assert(uni->pvalue && (*use_tfeedback == false));
          *use_tfeedback = GPU_shader_transform_feedback_enable(shgroup->shader,
                                                                ((GPUVertBuf *)uni->pvalue));
          break;
        case DRW_UNIFORM_VERTEX_BUFFER_AS_TEXTURE_REF:
          GPU_vertbuf_bind_as_texture(*uni->vertbuf_ref, uni->location);
          break;
        case DRW_UNIFORM_VERTEX_BUFFER_AS_TEXTURE:
          GPU_vertbuf_bind_as_texture(uni->vertbuf, uni->location);
          break;
        case DRW_UNIFORM_VERTEX_BUFFER_AS_STORAGE_REF:
          GPU_vertbuf_bind_as_ssbo(*uni->vertbuf_ref, uni->location);
          break;
        case DRW_UNIFORM_VERTEX_BUFFER_AS_STORAGE:
          GPU_vertbuf_bind_as_ssbo(uni->vertbuf, uni->location);
          break;
          /* Legacy/Fallback support. */
        case DRW_UNIFORM_BASE_INSTANCE:
          state->baseinst_loc = uni->location;
          break;
        case DRW_UNIFORM_MODEL_MATRIX:
          state->obmat_loc = uni->location;
          break;
        case DRW_UNIFORM_MODEL_MATRIX_INVERSE:
          state->obinv_loc = uni->location;
          break;
      }
    }
  }
  /* Ensure uniform arrays copied. */
  BLI_assert(array_index == 0);
  BLI_assert(array_uniform_loc == -1);
  UNUSED_VARS_NDEBUG(array_uniform_loc);
}

BLI_INLINE void draw_select_buffer(DRWShadingGroup *shgroup,
                                   DRWCommandsState *state,
                                   GPUBatch *batch,
                                   const DRWResourceHandle *handle)
{
  const bool is_instancing = (batch->inst[0] != NULL);
  int start = 0;
  int count = 1;
  int tot = is_instancing ? GPU_vertbuf_get_vertex_len(batch->inst[0]) :
                            GPU_vertbuf_get_vertex_len(batch->verts[0]);
  /* HACK: get VBO data without actually drawing. */
  int *select_id = (void *)GPU_vertbuf_get_data(state->select_buf);

  /* Batching */
  if (!is_instancing) {
    /* FIXME: Meh a bit nasty. */
    if (batch->prim_type == GPU_PRIM_TRIS) {
      count = 3;
    }
    else if (batch->prim_type == GPU_PRIM_LINES) {
      count = 2;
    }
  }

  while (start < tot) {
    GPU_select_load_id(select_id[start]);
    if (is_instancing) {
      draw_geometry_execute(shgroup, batch, 0, 0, start, count, state->baseinst_loc);
    }
    else {
      draw_geometry_execute(
          shgroup, batch, start, count, DRW_handle_id_get(handle), 0, state->baseinst_loc);
    }
    start += count;
  }
}

typedef struct DRWCommandIterator {
  int cmd_index;
  DRWCommandChunk *curr_chunk;
} DRWCommandIterator;

static void draw_command_iter_begin(DRWCommandIterator *iter, DRWShadingGroup *shgroup)
{
  iter->curr_chunk = shgroup->cmd.first;
  iter->cmd_index = 0;
}

static DRWCommand *draw_command_iter_step(DRWCommandIterator *iter, eDRWCommandType *cmd_type)
{
  if (iter->curr_chunk) {
    if (iter->cmd_index == iter->curr_chunk->command_len) {
      iter->curr_chunk = iter->curr_chunk->next;
      iter->cmd_index = 0;
    }
    if (iter->curr_chunk) {
      *cmd_type = command_type_get(iter->curr_chunk->command_type, iter->cmd_index);
      if (iter->cmd_index < iter->curr_chunk->command_used) {
        return iter->curr_chunk->commands + iter->cmd_index++;
      }
    }
  }
  return NULL;
}

static void draw_call_resource_bind(DRWCommandsState *state, const DRWResourceHandle *handle)
{
  /* Front face is not a resource but it is inside the resource handle. */
  bool neg_scale = DRW_handle_negative_scale_get(handle);
  if (neg_scale != state->neg_scale) {
    state->neg_scale = neg_scale;
    GPU_front_facing(neg_scale != DST.view_active->is_inverted);
  }

  int chunk = DRW_handle_chunk_get(handle);
  if (state->resource_chunk != chunk) {
    if (state->chunkid_loc != -1) {
      GPU_shader_uniform_int_ex(DST.shader, state->chunkid_loc, 1, 1, &chunk);
    }
    if (state->obmats_loc != -1) {
      GPU_uniformbuf_unbind(DST.vmempool->matrices_ubo[state->resource_chunk]);
      GPU_uniformbuf_bind(DST.vmempool->matrices_ubo[chunk], state->obmats_loc);
    }
    if (state->obinfos_loc != -1) {
      GPU_uniformbuf_unbind(DST.vmempool->obinfos_ubo[state->resource_chunk]);
      GPU_uniformbuf_bind(DST.vmempool->obinfos_ubo[chunk], state->obinfos_loc);
    }
    if (state->obattrs_loc != -1) {
      DRW_sparse_uniform_buffer_unbind(state->obattrs_ubo, state->resource_chunk);
      DRW_sparse_uniform_buffer_bind(state->obattrs_ubo, chunk, state->obattrs_loc);
    }
    state->resource_chunk = chunk;
  }

  if (state->resourceid_loc != -1) {
    int id = DRW_handle_id_get(handle);
    if (state->resource_id != id) {
      GPU_shader_uniform_int_ex(DST.shader, state->resourceid_loc, 1, 1, &id);
      state->resource_id = id;
    }
  }
}

static void draw_call_batching_flush(DRWShadingGroup *shgroup, DRWCommandsState *state)
{
  draw_indirect_call(shgroup, state);
  GPU_draw_list_submit(DST.draw_list);

  state->batch = NULL;
  state->inst_count = 0;
  state->base_inst = -1;
}

static void draw_call_single_do(DRWShadingGroup *shgroup,
                                DRWCommandsState *state,
                                GPUBatch *batch,
                                DRWResourceHandle handle,
                                int vert_first,
                                int vert_count,
                                int inst_first,
                                int inst_count,
                                bool do_base_instance)
{
  draw_call_batching_flush(shgroup, state);

  draw_call_resource_bind(state, &handle);

  /* TODO: This is Legacy. Need to be removed. */
  if (state->obmats_loc == -1 && (state->obmat_loc != -1 || state->obinv_loc != -1)) {
    draw_legacy_matrix_update(shgroup, &handle, state->obmat_loc, state->obinv_loc);
  }

  if (G.f & G_FLAG_PICKSEL) {
    if (state->select_buf != NULL) {
      draw_select_buffer(shgroup, state, batch, &handle);
      return;
    }

    GPU_select_load_id(state->select_id);
  }

  draw_geometry_execute(shgroup,
                        batch,
                        vert_first,
                        vert_count,
                        do_base_instance ? DRW_handle_id_get(&handle) : inst_first,
                        inst_count,
                        state->baseinst_loc);
}

/* Not to be mistaken with draw_indirect_call which does batch many drawcalls together. This one
 * only execute an indirect drawcall with user indirect buffer. */
static void draw_call_indirect(DRWShadingGroup *shgroup,
                               DRWCommandsState *state,
                               GPUBatch *batch,
                               DRWResourceHandle handle,
                               GPUStorageBuf *indirect_buf)
{
  draw_call_batching_flush(shgroup, state);
  draw_call_resource_bind(state, &handle);

  if (G.f & G_FLAG_PICKSEL) {
    GPU_select_load_id(state->select_id);
  }

  GPU_batch_set_shader(batch, shgroup->shader);
  GPU_batch_draw_indirect(batch, indirect_buf, 0);
}

static void draw_call_batching_start(DRWCommandsState *state)
{
  state->neg_scale = false;
  state->resource_chunk = 0;
  state->resource_id = -1;
  state->base_inst = 0;
  state->inst_count = 0;
  state->batch = NULL;

  state->select_id = -1;
  state->select_buf = NULL;
}

/* NOTE: Does not support batches with instancing VBOs. */
static void draw_call_batching_do(DRWShadingGroup *shgroup,
                                  DRWCommandsState *state,
                                  DRWCommandDraw *call)
{
  /* If any condition requires to interrupt the merging. */
  bool neg_scale = DRW_handle_negative_scale_get(&call->handle);
  int chunk = DRW_handle_chunk_get(&call->handle);
  int id = DRW_handle_id_get(&call->handle);
  if ((state->neg_scale != neg_scale) ||  /* Need to change state. */
      (state->resource_chunk != chunk) || /* Need to change UBOs. */
      (state->batch != call->batch)       /* Need to change VAO. */
  )
  {
    draw_call_batching_flush(shgroup, state);

    state->batch = call->batch;
    state->inst_count = 1;
    state->base_inst = id;

    draw_call_resource_bind(state, &call->handle);
  }
  /* Is the id consecutive? */
  else if (id != state->base_inst + state->inst_count) {
    /* We need to add a draw command for the pending instances. */
    draw_indirect_call(shgroup, state);
    state->inst_count = 1;
    state->base_inst = id;
  }
  /* We avoid a drawcall by merging with the precedent
   * drawcall using instancing. */
  else {
    state->inst_count++;
  }
}

/* Flush remaining pending drawcalls. */
static void draw_call_batching_finish(DRWShadingGroup *shgroup, DRWCommandsState *state)
{
  draw_call_batching_flush(shgroup, state);

  /* Reset state */
  if (state->neg_scale) {
    GPU_front_facing(DST.view_active->is_inverted);
  }
  if (state->obmats_loc != -1) {
    GPU_uniformbuf_unbind(DST.vmempool->matrices_ubo[state->resource_chunk]);
  }
  if (state->obinfos_loc != -1) {
    GPU_uniformbuf_unbind(DST.vmempool->obinfos_ubo[state->resource_chunk]);
  }
  if (state->obattrs_loc != -1) {
    DRW_sparse_uniform_buffer_unbind(state->obattrs_ubo, state->resource_chunk);
  }
  if (state->vlattrs_loc != -1) {
    GPU_uniformbuf_unbind(DST.vmempool->vlattrs_ubo);
  }
}

static void draw_shgroup(DRWShadingGroup *shgroup, DRWState pass_state)
{
  BLI_assert(shgroup->shader);

  DRWCommandsState state = {
      .obmats_loc = -1,
      .obinfos_loc = -1,
      .obattrs_loc = -1,
      .vlattrs_loc = -1,
      .baseinst_loc = -1,
      .chunkid_loc = -1,
      .resourceid_loc = -1,
      .obmat_loc = -1,
      .obinv_loc = -1,
      .obattrs_ubo = NULL,
      .drw_state_enabled = 0,
      .drw_state_disabled = 0,
  };

  const bool shader_changed = (DST.shader != shgroup->shader);
  bool use_tfeedback = false;

  if (shader_changed) {
    if (DST.shader) {
      GPU_shader_unbind();

      /* Unbinding can be costly. Skip in normal condition. */
      if (G.debug & G_DEBUG_GPU) {
        GPU_texture_unbind_all();
        GPU_texture_image_unbind_all();
        GPU_uniformbuf_unbind_all();
        GPU_storagebuf_unbind_all();
      }
    }
    GPU_shader_bind(shgroup->shader);
    DST.shader = shgroup->shader;
    DST.batch = NULL;
  }

  draw_update_uniforms(shgroup, &state, &use_tfeedback);

  drw_state_set(pass_state);

  /* Rendering Calls */
  {
    DRWCommandIterator iter;
    DRWCommand *cmd;
    eDRWCommandType cmd_type;

    draw_command_iter_begin(&iter, shgroup);

    draw_call_batching_start(&state);

    while ((cmd = draw_command_iter_step(&iter, &cmd_type))) {

      switch (cmd_type) {
        case DRW_CMD_DRAW_PROCEDURAL:
        case DRW_CMD_DRWSTATE:
        case DRW_CMD_STENCIL:
          draw_call_batching_flush(shgroup, &state);
          break;
        case DRW_CMD_DRAW:
        case DRW_CMD_DRAW_INDIRECT:
        case DRW_CMD_DRAW_INSTANCE:
          if (draw_call_is_culled(&cmd->instance.handle, DST.view_active)) {
            continue;
          }
          break;
        default:
          break;
      }

      switch (cmd_type) {
        case DRW_CMD_CLEAR:
          GPU_framebuffer_clear(GPU_framebuffer_active_get(),
                                cmd->clear.clear_channels,
                                (float[4]){cmd->clear.r / 255.0f,
                                           cmd->clear.g / 255.0f,
                                           cmd->clear.b / 255.0f,
                                           cmd->clear.a / 255.0f},
                                cmd->clear.depth,
                                cmd->clear.stencil);
          break;
        case DRW_CMD_DRWSTATE:
          state.drw_state_enabled |= cmd->state.enable;
          state.drw_state_disabled |= cmd->state.disable;
          drw_state_set((pass_state & ~state.drw_state_disabled) | state.drw_state_enabled);
          break;
        case DRW_CMD_STENCIL:
          drw_stencil_state_set(cmd->stencil.write_mask, cmd->stencil.ref, cmd->stencil.comp_mask);
          break;
        case DRW_CMD_SELECTID:
          state.select_id = cmd->select_id.select_id;
          state.select_buf = cmd->select_id.select_buf;
          break;
        case DRW_CMD_DRAW:
          if (!USE_BATCHING || state.obmats_loc == -1 || (G.f & G_FLAG_PICKSEL) ||
              cmd->draw.batch->inst[0]) {
            draw_call_single_do(
                shgroup, &state, cmd->draw.batch, cmd->draw.handle, 0, 0, 0, 0, true);
          }
          else {
            draw_call_batching_do(shgroup, &state, &cmd->draw);
          }
          break;
        case DRW_CMD_DRAW_PROCEDURAL:
          draw_call_single_do(shgroup,
                              &state,
                              cmd->procedural.batch,
                              cmd->procedural.handle,
                              0,
                              cmd->procedural.vert_count,
                              0,
                              1,
                              true);
          break;
        case DRW_CMD_DRAW_INDIRECT:
          draw_call_indirect(shgroup,
                             &state,
                             cmd->draw_indirect.batch,
                             cmd->draw_indirect.handle,
                             cmd->draw_indirect.indirect_buf);
          break;
        case DRW_CMD_DRAW_INSTANCE:
          draw_call_single_do(shgroup,
                              &state,
                              cmd->instance.batch,
                              cmd->instance.handle,
                              0,
                              0,
                              0,
                              cmd->instance.inst_count,
                              cmd->instance.use_attrs == 0);
          break;
        case DRW_CMD_DRAW_RANGE:
          draw_call_single_do(shgroup,
                              &state,
                              cmd->range.batch,
                              cmd->range.handle,
                              cmd->range.vert_first,
                              cmd->range.vert_count,
                              0,
                              1,
                              true);
          break;
        case DRW_CMD_DRAW_INSTANCE_RANGE:
          draw_call_single_do(shgroup,
                              &state,
                              cmd->instance_range.batch,
                              cmd->instance_range.handle,
                              0,
                              0,
                              cmd->instance_range.inst_first,
                              cmd->instance_range.inst_count,
                              false);
          break;
        case DRW_CMD_COMPUTE:
          GPU_compute_dispatch(shgroup->shader,
                               cmd->compute.groups_x_len,
                               cmd->compute.groups_y_len,
                               cmd->compute.groups_z_len);
          break;
        case DRW_CMD_COMPUTE_REF:
          GPU_compute_dispatch(shgroup->shader,
                               cmd->compute_ref.groups_ref[0],
                               cmd->compute_ref.groups_ref[1],
                               cmd->compute_ref.groups_ref[2]);
          break;
        case DRW_CMD_COMPUTE_INDIRECT:
          GPU_compute_dispatch_indirect(shgroup->shader, cmd->compute_indirect.indirect_buf);
          break;
        case DRW_CMD_BARRIER:
          GPU_memory_barrier(cmd->barrier.type);
          break;
      }
    }

    draw_call_batching_finish(shgroup, &state);
  }

  if (use_tfeedback) {
    GPU_shader_transform_feedback_disable(shgroup->shader);
  }
}

static void drw_update_view(void)
{
  /* TODO(fclem): update a big UBO and only bind ranges here. */
  GPU_uniformbuf_update(G_draw.view_ubo, &DST.view_active->storage);
  GPU_uniformbuf_update(G_draw.clipping_ubo, &DST.view_active->clip_planes);

  draw_compute_culling(DST.view_active);
}

static void drw_draw_pass_ex(DRWPass *pass,
                             DRWShadingGroup *start_group,
                             DRWShadingGroup *end_group)
{
  if (pass->original) {
    start_group = pass->original->shgroups.first;
    end_group = pass->original->shgroups.last;
  }

  if (start_group == NULL) {
    return;
  }

  DST.shader = NULL;

  BLI_assert(DST.buffer_finish_called &&
             "DRW_render_instance_buffer_finish had not been called before drawing");

  if (DST.view_previous != DST.view_active || DST.view_active->is_dirty) {
    drw_update_view();
    DST.view_active->is_dirty = false;
    DST.view_previous = DST.view_active;
  }

  /* GPU_framebuffer_clear calls can change the state outside the DRW module.
   * Force reset the affected states to avoid problems later. */
  drw_state_set(DST.state | DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR);

  drw_state_set(pass->state);
  drw_state_validate();

  if (DST.view_active->is_inverted) {
    GPU_front_facing(true);
  }

  DRW_stats_query_start(pass->name);

  for (DRWShadingGroup *shgroup = start_group; shgroup; shgroup = shgroup->next) {
    draw_shgroup(shgroup, pass->state);
    /* break if upper limit */
    if (shgroup == end_group) {
      break;
    }
  }

  if (DST.shader) {
    GPU_shader_unbind();
    DST.shader = NULL;
  }

  if (DST.batch) {
    DST.batch = NULL;
  }

  /* Fix #67342 for some reason. AMD Pro driver bug. */
  if ((DST.state & DRW_STATE_BLEND_CUSTOM) != 0 &&
      GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OFFICIAL))
  {
    drw_state_set(DST.state & ~DRW_STATE_BLEND_CUSTOM);
  }

  /* HACK: Rasterized discard can affect clear commands which are not
   * part of a DRWPass (as of now). So disable rasterized discard here
   * if it has been enabled. */
  if ((DST.state & DRW_STATE_RASTERIZER_ENABLED) == 0) {
    drw_state_set((DST.state & ~DRW_STATE_RASTERIZER_ENABLED) | DRW_STATE_DEFAULT);
  }

  /* Reset default. */
  if (DST.view_active->is_inverted) {
    GPU_front_facing(false);
  }

  DRW_stats_query_end();
}

void DRW_draw_pass(DRWPass *pass)
{
  for (; pass; pass = pass->next) {
    drw_draw_pass_ex(pass, pass->shgroups.first, pass->shgroups.last);
  }
}

void DRW_draw_pass_subset(DRWPass *pass, DRWShadingGroup *start_group, DRWShadingGroup *end_group)
{
  drw_draw_pass_ex(pass, start_group, end_group);
}

/** \} */
