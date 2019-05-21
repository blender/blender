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

#include "draw_manager.h"

#include "BLI_math_bits.h"
#include "BLI_memblock.h"

#include "BKE_global.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "intern/gpu_shader_private.h"
#include "intern/gpu_primitive_private.h"

#ifdef USE_GPU_SELECT
#  include "GPU_select.h"
#endif

#ifdef USE_GPU_SELECT
void DRW_select_load_id(uint id)
{
  BLI_assert(G.f & G_FLAG_PICKSEL);
  DST.select_id = id;
}
#endif

#define DEBUG_UBO_BINDING

/* -------------------------------------------------------------------- */
/** \name Draw State (DRW_state)
 * \{ */

void drw_state_set(DRWState state)
{
  if (DST.state == state) {
    return;
  }

#define CHANGED_TO(f) \
  ((DST.state_lock & (f)) ? \
       0 : \
       (((DST.state & (f)) ? ((state & (f)) ? 0 : -1) : ((state & (f)) ? 1 : 0))))

#define CHANGED_ANY(f) (((DST.state & (f)) != (state & (f))) && ((DST.state_lock & (f)) == 0))

#define CHANGED_ANY_STORE_VAR(f, enabled) \
  (((DST.state & (f)) != (enabled = (state & (f)))) && (((DST.state_lock & (f)) == 0)))

  /* Depth Write */
  {
    int test;
    if ((test = CHANGED_TO(DRW_STATE_WRITE_DEPTH))) {
      if (test == 1) {
        glDepthMask(GL_TRUE);
      }
      else {
        glDepthMask(GL_FALSE);
      }
    }
  }

  /* Color Write */
  {
    int test;
    if ((test = CHANGED_TO(DRW_STATE_WRITE_COLOR))) {
      if (test == 1) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }
      else {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      }
    }
  }

  /* Raster Discard */
  {
    if (CHANGED_ANY(DRW_STATE_RASTERIZER_ENABLED)) {
      if ((state & DRW_STATE_RASTERIZER_ENABLED) != 0) {
        glDisable(GL_RASTERIZER_DISCARD);
      }
      else {
        glEnable(GL_RASTERIZER_DISCARD);
      }
    }
  }

  /* Cull */
  {
    DRWState test;
    if (CHANGED_ANY_STORE_VAR(DRW_STATE_CULL_BACK | DRW_STATE_CULL_FRONT, test)) {
      if (test) {
        glEnable(GL_CULL_FACE);

        if ((state & DRW_STATE_CULL_BACK) != 0) {
          glCullFace(GL_BACK);
        }
        else if ((state & DRW_STATE_CULL_FRONT) != 0) {
          glCullFace(GL_FRONT);
        }
        else {
          BLI_assert(0);
        }
      }
      else {
        glDisable(GL_CULL_FACE);
      }
    }
  }

  /* Depth Test */
  {
    DRWState test;
    if (CHANGED_ANY_STORE_VAR(DRW_STATE_DEPTH_LESS | DRW_STATE_DEPTH_LESS_EQUAL |
                                  DRW_STATE_DEPTH_EQUAL | DRW_STATE_DEPTH_GREATER |
                                  DRW_STATE_DEPTH_GREATER_EQUAL | DRW_STATE_DEPTH_ALWAYS,
                              test)) {
      if (test) {
        glEnable(GL_DEPTH_TEST);

        if (state & DRW_STATE_DEPTH_LESS) {
          glDepthFunc(GL_LESS);
        }
        else if (state & DRW_STATE_DEPTH_LESS_EQUAL) {
          glDepthFunc(GL_LEQUAL);
        }
        else if (state & DRW_STATE_DEPTH_EQUAL) {
          glDepthFunc(GL_EQUAL);
        }
        else if (state & DRW_STATE_DEPTH_GREATER) {
          glDepthFunc(GL_GREATER);
        }
        else if (state & DRW_STATE_DEPTH_GREATER_EQUAL) {
          glDepthFunc(GL_GEQUAL);
        }
        else if (state & DRW_STATE_DEPTH_ALWAYS) {
          glDepthFunc(GL_ALWAYS);
        }
        else {
          BLI_assert(0);
        }
      }
      else {
        glDisable(GL_DEPTH_TEST);
      }
    }
  }

  /* Wire Width */
  {
    int test;
    if ((test = CHANGED_TO(DRW_STATE_WIRE_SMOOTH))) {
      if (test == 1) {
        GPU_line_width(2.0f);
        GPU_line_smooth(true);
      }
      else {
        GPU_line_width(1.0f);
        GPU_line_smooth(false);
      }
    }
  }

  /* Blending (all buffer) */
  {
    int test;
    if (CHANGED_ANY_STORE_VAR(DRW_STATE_BLEND | DRW_STATE_BLEND_PREMUL | DRW_STATE_ADDITIVE |
                                  DRW_STATE_MULTIPLY | DRW_STATE_ADDITIVE_FULL |
                                  DRW_STATE_BLEND_OIT | DRW_STATE_BLEND_PREMUL_UNDER,
                              test)) {
      if (test) {
        glEnable(GL_BLEND);

        if ((state & DRW_STATE_BLEND) != 0) {
          glBlendFuncSeparate(GL_SRC_ALPHA,
                              GL_ONE_MINUS_SRC_ALPHA, /* RGB */
                              GL_ONE,
                              GL_ONE_MINUS_SRC_ALPHA); /* Alpha */
        }
        else if ((state & DRW_STATE_BLEND_PREMUL_UNDER) != 0) {
          glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
        }
        else if ((state & DRW_STATE_BLEND_PREMUL) != 0) {
          glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
        else if ((state & DRW_STATE_MULTIPLY) != 0) {
          glBlendFunc(GL_DST_COLOR, GL_ZERO);
        }
        else if ((state & DRW_STATE_BLEND_OIT) != 0) {
          glBlendFuncSeparate(GL_ONE,
                              GL_ONE, /* RGB */
                              GL_ZERO,
                              GL_ONE_MINUS_SRC_ALPHA); /* Alpha */
        }
        else if ((state & DRW_STATE_ADDITIVE) != 0) {
          /* Do not let alpha accumulate but premult the source RGB by it. */
          glBlendFuncSeparate(GL_SRC_ALPHA,
                              GL_ONE, /* RGB */
                              GL_ZERO,
                              GL_ONE); /* Alpha */
        }
        else if ((state & DRW_STATE_ADDITIVE_FULL) != 0) {
          /* Let alpha accumulate. */
          glBlendFunc(GL_ONE, GL_ONE);
        }
        else {
          BLI_assert(0);
        }
      }
      else {
        glDisable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE); /* Don't multiply incoming color by alpha. */
      }
    }
  }

  /* Clip Planes */
  {
    int test;
    if ((test = CHANGED_TO(DRW_STATE_CLIP_PLANES))) {
      if (test == 1) {
        for (int i = 0; i < DST.view_active->clip_planes_len; ++i) {
          glEnable(GL_CLIP_DISTANCE0 + i);
        }
      }
      else {
        for (int i = 0; i < MAX_CLIP_PLANES; ++i) {
          glDisable(GL_CLIP_DISTANCE0 + i);
        }
      }
    }
  }

  /* Stencil */
  {
    DRWState test;
    if (CHANGED_ANY_STORE_VAR(DRW_STATE_WRITE_STENCIL | DRW_STATE_WRITE_STENCIL_SHADOW_PASS |
                                  DRW_STATE_WRITE_STENCIL_SHADOW_FAIL | DRW_STATE_STENCIL_EQUAL |
                                  DRW_STATE_STENCIL_NEQUAL,
                              test)) {
      if (test) {
        glEnable(GL_STENCIL_TEST);
        /* Stencil Write */
        if ((state & DRW_STATE_WRITE_STENCIL) != 0) {
          glStencilMask(0xFF);
          glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        }
        else if ((state & DRW_STATE_WRITE_STENCIL_SHADOW_PASS) != 0) {
          glStencilMask(0xFF);
          glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
          glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
        }
        else if ((state & DRW_STATE_WRITE_STENCIL_SHADOW_FAIL) != 0) {
          glStencilMask(0xFF);
          glStencilOpSeparate(GL_BACK, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
          glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
        }
        /* Stencil Test */
        else if ((state & (DRW_STATE_STENCIL_EQUAL | DRW_STATE_STENCIL_NEQUAL)) != 0) {
          glStencilMask(0x00); /* disable write */
          DST.stencil_mask = STENCIL_UNDEFINED;
        }
        else {
          BLI_assert(0);
        }
      }
      else {
        /* disable write & test */
        DST.stencil_mask = 0;
        glStencilMask(0x00);
        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glDisable(GL_STENCIL_TEST);
      }
    }
  }

  /* Provoking Vertex */
  {
    int test;
    if ((test = CHANGED_TO(DRW_STATE_FIRST_VERTEX_CONVENTION))) {
      if (test == 1) {
        glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
      }
      else {
        glProvokingVertex(GL_LAST_VERTEX_CONVENTION);
      }
    }
  }

  /* Polygon Offset */
  {
    int test;
    if (CHANGED_ANY_STORE_VAR(DRW_STATE_OFFSET_POSITIVE | DRW_STATE_OFFSET_NEGATIVE, test)) {
      if (test) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glEnable(GL_POLYGON_OFFSET_POINT);
        /* Stencil Write */
        if ((state & DRW_STATE_OFFSET_POSITIVE) != 0) {
          glPolygonOffset(1.0f, 1.0f);
        }
        else if ((state & DRW_STATE_OFFSET_NEGATIVE) != 0) {
          glPolygonOffset(-1.0f, -1.0f);
        }
        else {
          BLI_assert(0);
        }
      }
      else {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
        glDisable(GL_POLYGON_OFFSET_POINT);
      }
    }
  }

#undef CHANGED_TO
#undef CHANGED_ANY
#undef CHANGED_ANY_STORE_VAR

  DST.state = state;
}

static void drw_stencil_set(uint mask)
{
  if (DST.stencil_mask != mask) {
    DST.stencil_mask = mask;
    /* Stencil Write */
    if ((DST.state & DRW_STATE_WRITE_STENCIL) != 0) {
      glStencilFunc(GL_ALWAYS, mask, 0xFF);
    }
    /* Stencil Test */
    else if ((DST.state & DRW_STATE_STENCIL_EQUAL) != 0) {
      glStencilFunc(GL_EQUAL, mask, 0xFF);
    }
    else if ((DST.state & DRW_STATE_STENCIL_NEQUAL) != 0) {
      glStencilFunc(GL_NOTEQUAL, mask, 0xFF);
    }
  }
}

/* Reset state to not interfer with other UI drawcall */
void DRW_state_reset_ex(DRWState state)
{
  DST.state = ~state;
  drw_state_set(state);
}

/**
 * Use with care, intended so selection code can override passes depth settings,
 * which is important for selection to work properly.
 *
 * Should be set in main draw loop, cleared afterwards
 */
void DRW_state_lock(DRWState state)
{
  DST.state_lock = state;
}

void DRW_state_reset(void)
{
  DRW_state_reset_ex(DRW_STATE_DEFAULT);

  GPU_point_size(5);
  GPU_enable_program_point_size();

  /* Reset blending function */
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

/**
 * This only works if DRWPasses have been tagged with DRW_STATE_CLIP_PLANES,
 * and if the shaders have support for it (see usage of gl_ClipDistance).
 */
void DRW_state_clip_planes_len_set(uint plane_len)
{
  BLI_assert(plane_len <= MAX_CLIP_PLANES);
  /* DUMMY TO REMOVE */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Culling (DRW_culling)
 * \{ */

static bool draw_call_is_culled(DRWCall *call, DRWView *view)
{
  return (call->state->culling->mask & view->culling_mask) != 0;
}

/* Set active view for rendering. */
void DRW_view_set_active(DRWView *view)
{
  DST.view_active = (view) ? view : DST.view_default;
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
  if (center_dist_sq > SQUARE(radius_sum)) {
    return false;
  }
  /* TODO we could test against the inscribed sphere of the frustum to early out positively. */

  /* Test against the 6 frustum planes. */
  /* TODO order planes with sides first then far then near clip. Should be better culling
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
      else if (v == 7) {
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

/* Return True if the given BoundSphere intersect the current view frustum.
 * bsphere must be in world space. */
bool DRW_culling_sphere_test(const DRWView *view, const BoundSphere *bsphere)
{
  view = view ? view : DST.view_default;
  return draw_culling_sphere_test(&view->frustum_bsphere, view->frustum_planes, bsphere);
}

/* Return True if the given BoundBox intersect the current view frustum.
 * bbox must be in world space. */
bool DRW_culling_box_test(const DRWView *view, const BoundBox *bbox)
{
  view = view ? view : DST.view_default;
  return draw_culling_box_test(view->frustum_planes, bbox);
}

/* Return True if the view frustum is inside or intersect the given plane.
 * plane must be in world space. */
bool DRW_culling_plane_test(const DRWView *view, const float plane[4])
{
  view = view ? view : DST.view_default;
  return draw_culling_plane_test(&view->frustum_corners, plane);
}

void DRW_culling_frustum_corners_get(BoundBox *corners)
{
  *corners = DST.view_active->frustum_corners;
}

void DRW_culling_frustum_planes_get(float planes[6][4])
{
  memcpy(planes, DST.view_active->frustum_planes, sizeof(float) * 6 * 4);
}

static void draw_compute_culling(DRWView *view)
{
  view = view->parent ? view->parent : view;

  /* TODO(fclem) multithread this. */
  /* TODO(fclem) compute all dirty views at once. */
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

static void draw_geometry_prepare(DRWShadingGroup *shgroup, DRWCall *call)
{
  BLI_assert(call);
  DRWCallState *state = call->state;

  if (shgroup->model != -1) {
    GPU_shader_uniform_vector(shgroup->shader, shgroup->model, 16, 1, (float *)state->model);
  }
  if (shgroup->modelinverse != -1) {
    GPU_shader_uniform_vector(
        shgroup->shader, shgroup->modelinverse, 16, 1, (float *)state->modelinverse);
  }
  if (shgroup->objectinfo != -1) {
    float infos[4];
    infos[0] = state->ob_index;
    // infos[1]; /* UNUSED. */
    infos[2] = state->ob_random;
    infos[3] = (state->flag & DRW_CALL_NEGSCALE) ? -1.0f : 1.0f;
    GPU_shader_uniform_vector(shgroup->shader, shgroup->objectinfo, 4, 1, (float *)infos);
  }
  if (shgroup->orcotexfac != -1) {
    GPU_shader_uniform_vector(
        shgroup->shader, shgroup->orcotexfac, 3, 2, (float *)state->orcotexfac);
  }
  /* Still supported for compatibility with gpu_shader_* but should be forbidden
   * and is slow (since it does not cache the result). */
  if (shgroup->modelviewprojection != -1) {
    float mvp[4][4];
    mul_m4_m4m4(mvp, DST.view_active->storage.matstate.persmat, state->model);
    GPU_shader_uniform_vector(shgroup->shader, shgroup->modelviewprojection, 16, 1, (float *)mvp);
  }
}

static void draw_geometry_execute(
    DRWShadingGroup *shgroup, GPUBatch *geom, uint start, uint count, bool draw_instance)
{
  /* step 2 : bind vertex array & draw */
  GPU_batch_program_set_no_use(
      geom, GPU_shader_get_program(shgroup->shader), GPU_shader_get_interface(shgroup->shader));
  /* XXX hacking gawain. we don't want to call glUseProgram! (huge performance loss) */
  geom->program_in_use = true;

  GPU_batch_draw_range_ex(geom, start, count, draw_instance);

  geom->program_in_use = false; /* XXX hacking gawain */
}

enum {
  BIND_NONE = 0,
  BIND_TEMP = 1,    /* Release slot after this shading group. */
  BIND_PERSIST = 2, /* Release slot only after the next shader change. */
};

static void set_bound_flags(uint64_t *slots, uint64_t *persist_slots, int slot_idx, char bind_type)
{
  uint64_t slot = 1lu << (unsigned long)slot_idx;
  *slots |= slot;
  if (bind_type == BIND_PERSIST) {
    *persist_slots |= slot;
  }
}

static int get_empty_slot_index(uint64_t slots)
{
  uint64_t empty_slots = ~slots;
  /* Find first empty slot using bitscan. */
  if (empty_slots != 0) {
    if ((empty_slots & 0xFFFFFFFFlu) != 0) {
      return (int)bitscan_forward_uint(empty_slots);
    }
    else {
      return (int)bitscan_forward_uint(empty_slots >> 32) + 32;
    }
  }
  else {
    /* Greater than GPU_max_textures() */
    return 99999;
  }
}

static void bind_texture(GPUTexture *tex, char bind_type)
{
  int idx = GPU_texture_bound_number(tex);
  if (idx == -1) {
    /* Texture isn't bound yet. Find an empty slot and bind it. */
    idx = get_empty_slot_index(DST.RST.bound_tex_slots);

    if (idx < GPU_max_textures()) {
      GPUTexture **gpu_tex_slot = &DST.RST.bound_texs[idx];
      /* Unbind any previous texture. */
      if (*gpu_tex_slot != NULL) {
        GPU_texture_unbind(*gpu_tex_slot);
      }
      GPU_texture_bind(tex, idx);
      *gpu_tex_slot = tex;
    }
    else {
      printf("Not enough texture slots! Reduce number of textures used by your shader.\n");
      return;
    }
  }
  else {
    /* This texture slot was released but the tex
     * is still bound. Just flag the slot again. */
    BLI_assert(DST.RST.bound_texs[idx] == tex);
  }
  set_bound_flags(&DST.RST.bound_tex_slots, &DST.RST.bound_tex_slots_persist, idx, bind_type);
}

static void bind_ubo(GPUUniformBuffer *ubo, char bind_type)
{
  int idx = GPU_uniformbuffer_bindpoint(ubo);
  if (idx == -1) {
    /* UBO isn't bound yet. Find an empty slot and bind it. */
    idx = get_empty_slot_index(DST.RST.bound_ubo_slots);

    if (idx < GPU_max_ubo_binds()) {
      GPUUniformBuffer **gpu_ubo_slot = &DST.RST.bound_ubos[idx];
      /* Unbind any previous UBO. */
      if (*gpu_ubo_slot != NULL) {
        GPU_uniformbuffer_unbind(*gpu_ubo_slot);
      }
      GPU_uniformbuffer_bind(ubo, idx);
      *gpu_ubo_slot = ubo;
    }
    else {
      /* printf so user can report bad behavior */
      printf("Not enough ubo slots! This should not happen!\n");
      /* This is not depending on user input.
       * It is our responsibility to make sure there is enough slots. */
      BLI_assert(0);
      return;
    }
  }
  else {
    /* This UBO slot was released but the UBO is
     * still bound here. Just flag the slot again. */
    BLI_assert(DST.RST.bound_ubos[idx] == ubo);
  }
  set_bound_flags(&DST.RST.bound_ubo_slots, &DST.RST.bound_ubo_slots_persist, idx, bind_type);
}

#ifndef NDEBUG
/**
 * Opengl specification is strict on buffer binding.
 *
 * " If any active uniform block is not backed by a
 * sufficiently large buffer object, the results of shader
 * execution are undefined, and may result in GL interruption or
 * termination. " - Opengl 3.3 Core Specification
 *
 * For now we only check if the binding is correct. Not the size of
 * the bound ubo.
 *
 * See T55475.
 * */
static bool ubo_bindings_validate(DRWShadingGroup *shgroup)
{
  bool valid = true;
#  ifdef DEBUG_UBO_BINDING
  /* Check that all active uniform blocks have a non-zero buffer bound. */
  GLint program = 0;
  GLint active_blocks = 0;

  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &active_blocks);

  for (uint i = 0; i < active_blocks; ++i) {
    int binding = 0;
    int buffer = 0;

    glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_BINDING, &binding);
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, binding, &buffer);

    if (buffer == 0) {
      char blockname[64];
      glGetActiveUniformBlockName(program, i, sizeof(blockname), NULL, blockname);

      if (valid) {
        printf("Trying to draw with missing UBO binding.\n");
        valid = false;
      }
      printf("Pass : %s, Shader : %s, Block : %s\n",
             shgroup->pass_parent->name,
             shgroup->shader->name,
             blockname);
    }
  }
#  endif
  return valid;
}
#endif

static void release_texture_slots(bool with_persist)
{
  if (with_persist) {
    DST.RST.bound_tex_slots = 0;
    DST.RST.bound_tex_slots_persist = 0;
  }
  else {
    DST.RST.bound_tex_slots &= DST.RST.bound_tex_slots_persist;
  }
}

static void release_ubo_slots(bool with_persist)
{
  if (with_persist) {
    DST.RST.bound_ubo_slots = 0;
    DST.RST.bound_ubo_slots_persist = 0;
  }
  else {
    DST.RST.bound_ubo_slots &= DST.RST.bound_ubo_slots_persist;
  }
}

static void draw_update_uniforms(DRWShadingGroup *shgroup)
{
  for (DRWUniform *uni = shgroup->uniforms; uni; uni = uni->next) {
    GPUTexture *tex;
    GPUUniformBuffer *ubo;
    if (uni->location == -2) {
      uni->location = GPU_shader_get_uniform_ensure(shgroup->shader,
                                                    DST.uniform_names.buffer + uni->name_ofs);
      if (uni->location == -1) {
        continue;
      }
    }
    const void *data = uni->pvalue;
    if (ELEM(uni->type, DRW_UNIFORM_INT_COPY, DRW_UNIFORM_FLOAT_COPY)) {
      data = uni->fvalue;
    }
    switch (uni->type) {
      case DRW_UNIFORM_INT_COPY:
      case DRW_UNIFORM_INT:
        GPU_shader_uniform_vector_int(
            shgroup->shader, uni->location, uni->length, uni->arraysize, data);
        break;
      case DRW_UNIFORM_FLOAT_COPY:
      case DRW_UNIFORM_FLOAT:
        GPU_shader_uniform_vector(
            shgroup->shader, uni->location, uni->length, uni->arraysize, data);
        break;
      case DRW_UNIFORM_TEXTURE:
        tex = (GPUTexture *)uni->pvalue;
        BLI_assert(tex);
        bind_texture(tex, BIND_TEMP);
        GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
        break;
      case DRW_UNIFORM_TEXTURE_PERSIST:
        tex = (GPUTexture *)uni->pvalue;
        BLI_assert(tex);
        bind_texture(tex, BIND_PERSIST);
        GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
        break;
      case DRW_UNIFORM_TEXTURE_REF:
        tex = *((GPUTexture **)uni->pvalue);
        BLI_assert(tex);
        bind_texture(tex, BIND_TEMP);
        GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
        break;
      case DRW_UNIFORM_BLOCK:
        ubo = (GPUUniformBuffer *)uni->pvalue;
        bind_ubo(ubo, BIND_TEMP);
        GPU_shader_uniform_buffer(shgroup->shader, uni->location, ubo);
        break;
      case DRW_UNIFORM_BLOCK_PERSIST:
        ubo = (GPUUniformBuffer *)uni->pvalue;
        bind_ubo(ubo, BIND_PERSIST);
        GPU_shader_uniform_buffer(shgroup->shader, uni->location, ubo);
        break;
    }
  }

  BLI_assert(ubo_bindings_validate(shgroup));
}

BLI_INLINE bool draw_select_do_call(DRWShadingGroup *shgroup, DRWCall *call)
{
#ifdef USE_GPU_SELECT
  if ((G.f & G_FLAG_PICKSEL) == 0) {
    return false;
  }
  if (call->inst_selectid != NULL) {
    const bool is_instancing = (call->inst_count != 0);
    uint start = 0;
    uint count = 1;
    uint tot = is_instancing ? call->inst_count : call->vert_count;
    /* Hack : get vbo data without actually drawing. */
    GPUVertBufRaw raw;
    GPU_vertbuf_attr_get_raw_data(call->inst_selectid, 0, &raw);
    int *select_id = GPU_vertbuf_raw_step(&raw);

    /* Batching */
    if (!is_instancing) {
      /* FIXME: Meh a bit nasty. */
      if (call->batch->gl_prim_type == convert_prim_type_to_gl(GPU_PRIM_TRIS)) {
        count = 3;
      }
      else if (call->batch->gl_prim_type == convert_prim_type_to_gl(GPU_PRIM_LINES)) {
        count = 2;
      }
    }

    while (start < tot) {
      GPU_select_load_id(select_id[start]);
      draw_geometry_execute(shgroup, call->batch, start, count, is_instancing);
      start += count;
    }
    return true;
  }
  else {
    GPU_select_load_id(call->select_id);
    return false;
  }
#else
  return false;
#endif
}

static void draw_shgroup(DRWShadingGroup *shgroup, DRWState pass_state)
{
  BLI_assert(shgroup->shader);

  const bool shader_changed = (DST.shader != shgroup->shader);
  bool use_tfeedback = false;

  if (shader_changed) {
    if (DST.shader) {
      GPU_shader_unbind();
    }
    GPU_shader_bind(shgroup->shader);
    DST.shader = shgroup->shader;
  }

  if (shgroup->tfeedback_target != NULL) {
    use_tfeedback = GPU_shader_transform_feedback_enable(shgroup->shader,
                                                         shgroup->tfeedback_target->vbo_id);
  }

  release_ubo_slots(shader_changed);
  release_texture_slots(shader_changed);

  drw_state_set((pass_state & shgroup->state_extra_disable) | shgroup->state_extra);
  drw_stencil_set(shgroup->stencil_mask);

  draw_update_uniforms(shgroup);

  /* Rendering Calls */
  {
    bool prev_neg_scale = false;
    int callid = 0;
    for (DRWCall *call = shgroup->calls.first; call; call = call->next) {

      if (draw_call_is_culled(call, DST.view_active)) {
        continue;
      }

      /* XXX small exception/optimisation for outline rendering. */
      if (shgroup->callid != -1) {
        GPU_shader_uniform_vector_int(shgroup->shader, shgroup->callid, 1, 1, &callid);
        callid += 1;
      }

      /* Negative scale objects */
      bool neg_scale = call->state->flag & DRW_CALL_NEGSCALE;
      if (neg_scale != prev_neg_scale) {
        glFrontFace((neg_scale) ? GL_CW : GL_CCW);
        prev_neg_scale = neg_scale;
      }

      draw_geometry_prepare(shgroup, call);

      if (draw_select_do_call(shgroup, call)) {
        continue;
      }

      /* TODO revisit when DRW_SHG_INSTANCE and the like is gone. */
      if (call->inst_count == 0) {
        draw_geometry_execute(shgroup, call->batch, call->vert_first, call->vert_count, false);
      }
      else {
        draw_geometry_execute(shgroup, call->batch, 0, call->inst_count, true);
      }
    }
    /* Reset state */
    glFrontFace(GL_CCW);
  }

  if (use_tfeedback) {
    GPU_shader_transform_feedback_disable(shgroup->shader);
  }
}

static void drw_update_view(void)
{
  /* TODO(fclem) update a big UBO and only bind ranges here. */
  DRW_uniformbuffer_update(G_draw.view_ubo, &DST.view_active->storage);

  /* TODO get rid of this. */
  DST.view_storage_cpy = DST.view_active->storage;

  draw_compute_culling(DST.view_active);
}

static void drw_draw_pass_ex(DRWPass *pass,
                             DRWShadingGroup *start_group,
                             DRWShadingGroup *end_group)
{
  if (start_group == NULL) {
    return;
  }

  DST.shader = NULL;

  BLI_assert(DST.buffer_finish_called &&
             "DRW_render_instance_buffer_finish had not been called before drawing");

  drw_update_view();

  /* GPU_framebuffer_clear calls can change the state outside the DRW module.
   * Force reset the affected states to avoid problems later. */
  drw_state_set(DST.state | DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR);

  drw_state_set(pass->state);

  DRW_stats_query_start(pass->name);

  for (DRWShadingGroup *shgroup = start_group; shgroup; shgroup = shgroup->next) {
    draw_shgroup(shgroup, pass->state);
    /* break if upper limit */
    if (shgroup == end_group) {
      break;
    }
  }

  /* Clear Bound textures */
  for (int i = 0; i < DST_MAX_SLOTS; i++) {
    if (DST.RST.bound_texs[i] != NULL) {
      GPU_texture_unbind(DST.RST.bound_texs[i]);
      DST.RST.bound_texs[i] = NULL;
    }
  }

  /* Clear Bound Ubos */
  for (int i = 0; i < DST_MAX_SLOTS; i++) {
    if (DST.RST.bound_ubos[i] != NULL) {
      GPU_uniformbuffer_unbind(DST.RST.bound_ubos[i]);
      DST.RST.bound_ubos[i] = NULL;
    }
  }

  if (DST.shader) {
    GPU_shader_unbind();
    DST.shader = NULL;
  }

  /* HACK: Rasterized discard can affect clear commands which are not
   * part of a DRWPass (as of now). So disable rasterized discard here
   * if it has been enabled. */
  if ((DST.state & DRW_STATE_RASTERIZER_ENABLED) == 0) {
    drw_state_set((DST.state & ~DRW_STATE_RASTERIZER_ENABLED) | DRW_STATE_DEFAULT);
  }

  DRW_stats_query_end();
}

void DRW_draw_pass(DRWPass *pass)
{
  drw_draw_pass_ex(pass, pass->shgroups.first, pass->shgroups.last);
}

/* Draw only a subset of shgroups. Used in special situations as grease pencil strokes */
void DRW_draw_pass_subset(DRWPass *pass, DRWShadingGroup *start_group, DRWShadingGroup *end_group)
{
  drw_draw_pass_ex(pass, start_group, end_group);
}

/** \} */
