/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Shadow:
 *
 * Use stencil shadow buffer to cast a sharp shadow over opaque surfaces.
 *
 * After the main pre-pass we render shadow volumes using custom depth & stencil states to
 * set the stencil of shadowed area to anything but 0.
 *
 * Then the shading pass will shade the areas with stencil not equal 0 differently.
 */

#include "BKE_object.hh"
#include "DNA_scene_types.h"
#include "DRW_render.hh"
#include "GPU_compute.hh"

#include "draw_cache.hh"

#include "workbench_private.hh"

namespace blender::workbench {

void ShadowPass::ShadowView::setup(View &view, float3 light_direction, bool force_fail_method)
{
  force_fail_method_ = force_fail_method;
  light_direction_ = light_direction;
  sync(view.viewmat(), view.winmat());

  /* Prepare frustum extruded in the negative light direction,
   * so we can test regular bounding boxes against it for culling. */

  /* Frustum Corners indices
   * Z  Y
   * | /
   * |/
   * .-----X
   *     3----------7
   *    /|         /|
   *   / |        / |
   *  0----------4  |
   *  |  |       |  |
   *  |  2-------|--6
   *  | /        | /
   *  |/         |/
   *  1----------5
   */

  /* Frustum Planes indices */
  const int x_neg = 0; /* Left */
  const int x_pos = 5; /* Right */
  const int y_neg = 1; /* Bottom */
  const int y_pos = 3; /* Top */
  const int z_pos = 4; /* Near */
  const int z_neg = 2; /* Far */

  const int3 corner_faces[8] = {
      {x_neg, y_neg, z_pos},
      {x_neg, y_neg, z_neg},
      {x_neg, y_pos, z_neg},
      {x_neg, y_pos, z_pos},
      {x_pos, y_neg, z_pos},
      {x_pos, y_neg, z_neg},
      {x_pos, y_pos, z_neg},
      {x_pos, y_pos, z_pos},
  };

  const int2 edge_faces[12] = {
      {x_neg, y_neg},
      {x_neg, z_neg},
      {x_neg, y_pos},
      {x_neg, z_pos},
      {y_neg, x_pos},
      {z_neg, x_pos},
      {y_pos, x_pos},
      {z_pos, x_pos},
      {y_neg, z_pos},
      {z_neg, y_neg},
      {y_pos, z_neg},
      {z_pos, y_pos},
  };

  const int2 edge_corners[12] = {
      {0, 1},
      {1, 2},
      {2, 3},
      {3, 0},
      {4, 5},
      {5, 6},
      {6, 7},
      {7, 4},
      {0, 4},
      {1, 5},
      {2, 6},
      {3, 7},
  };

  std::array<float3, 8> frustum_corners = this->frustum_corners_get();
  std::array<float4, 6> frustum_planes = this->frustum_planes_get();

  Vector<float4> faces_result;
  Vector<float3> corners_result;

  /* "Unlit" frustum faces are left "as-is" */

  bool face_lit[6];
  for (int i : IndexRange(6)) {
    /* Make the frustum normals face outwards */
    frustum_planes[i] *= float4(-1, -1, -1, 1);

    face_lit[i] = math::dot(float3(frustum_planes[i]), light_direction_) < 0;
    if (!face_lit[i]) {
      faces_result.append(frustum_planes[i]);
    }
  }

  /* Edges between lit and unlit faces are extruded "infinitely" towards the light source */

  for (int i : IndexRange(12)) {
    int2 f = edge_faces[i];
    bool a_lit = face_lit[f[0]];
    bool b_lit = face_lit[f[1]];
    if (a_lit != b_lit) {
      /* Extrude Face */
      float3 corner_a = frustum_corners[edge_corners[i][0]];
      float3 corner_b = frustum_corners[edge_corners[i][1]];
      float3 edge_direction = math::normalize(corner_b - corner_a);
      float3 normal = math::normalize(math::cross(light_direction_, edge_direction));

      float4 extruded_face = float4(UNPACK3(normal), math::dot(normal, corner_a));

      /* Ensure the plane faces outwards */
      bool flipped = false;
      for (float3 corner : frustum_corners) {
        if (math::dot(float3(extruded_face), corner) > (extruded_face.w + 0.1)) {
          BLI_assert(!flipped);
          UNUSED_VARS_NDEBUG(flipped);
          flipped = true;
          extruded_face *= -1;
        }
      }

      faces_result.append(extruded_face);
    }
  }

  for (int i_corner : IndexRange(8)) {
    int lit_faces = 0;
    for (int i_face : IndexRange(3)) {
      lit_faces += face_lit[corner_faces[i_corner][i_face]] ? 1 : 0;
    }
    if (lit_faces < 3) {
      /* Add original corner */
      corners_result.append(frustum_corners[i_corner]);

      if (lit_faces > 0) {
        /* Add extruded corner */
        corners_result.append(float3(frustum_corners[i_corner]) - (light_direction_ * 1e4f));
      }
    }
  }

  for (int i : corners_result.index_range()) {
    extruded_frustum_.corners[i] = float4(corners_result[i], 1);
  }
  extruded_frustum_.corners_count = corners_result.size();

  for (int i : faces_result.index_range()) {
    extruded_frustum_.planes[i] = faces_result[i];
  }
  extruded_frustum_.planes_count = faces_result.size();

  extruded_frustum_.push_update();
}

bool ShadowPass::ShadowView::debug_object_culling(Object *ob)
{
  printf("Test %s\n", ob->id.name);
  const Bounds<float3> bounds = *BKE_object_boundbox_get(ob);
  const std::array<float3, 8> corners = bounds::corners(bounds);
  for (int p : IndexRange(extruded_frustum_.planes_count)) {
    float4 plane = extruded_frustum_.planes[p];
    bool separating_axis = true;
    for (float3 corner : corners) {
      corner = math::transform_point(ob->object_to_world(), corner);
      float signed_distance = math::dot(corner, float3(plane)) - plane.w;
      if (signed_distance <= 0) {
        separating_axis = false;
        break;
      }
    }
    if (separating_axis) {
      printf("Separating Axis >>> x: %f, y: %f, z: %f, w: %f \n", UNPACK4(plane));
      return true;
    }
  }
  return false;
}

void ShadowPass::ShadowView::set_mode(ShadowPass::PassType type)
{
  current_pass_type_ = type;
  /* Ensure compute_visibility runs again after updating the mode. */
  manager_fingerprint_ = 0;
}

void ShadowPass::ShadowView::compute_visibility(ObjectBoundsBuf &bounds,
                                                ObjectInfosBuf & /*infos*/,
                                                uint resource_len,
                                                bool /*debug_freeze*/)
{
  /* TODO (Miguel Pozo): Add debug_freeze support */

  GPU_debug_group_begin("ShadowView.compute_visibility");

  uint word_per_draw = this->visibility_word_per_draw();
  /* Switch between tightly packed and set of whole word per instance. */
  uint words_len = (view_len_ == 1) ? divide_ceil_u(resource_len, 32) :
                                      resource_len * word_per_draw;
  words_len = ceil_to_multiple_u(max_ii(1, words_len), 4);
  const uint32_t data = 0xFFFFFFFFu;

  if (current_pass_type_ == ShadowPass::PASS) {
    /* TODO(fclem): Resize to nearest pow2 to reduce fragmentation. */
    pass_visibility_buf_.resize(words_len);
    GPU_storagebuf_clear(pass_visibility_buf_, data);
    fail_visibility_buf_.resize(words_len);
    GPU_storagebuf_clear(fail_visibility_buf_, data);
  }
  else if (current_pass_type_ == ShadowPass::FAIL) {
    /* Already computed in the ShadowPass::PASS */
    GPU_debug_group_end();
    return;
  }
  else {
    visibility_buf_.resize(words_len);
    GPU_storagebuf_clear(visibility_buf_, data);
  }

  if (do_visibility_) {
    /* TODO(@pragma37): Use regular culling for the caps pass. */
    gpu::Shader *shader = current_pass_type_ == ShadowPass::FORCED_FAIL ?
                              ShaderCache::get().shadow_visibility_static.get() :
                              ShaderCache::get().shadow_visibility_dynamic.get();
    GPU_shader_bind(shader);
    GPU_shader_uniform_1i(shader, "resource_len", resource_len);
    GPU_shader_uniform_1i(shader, "view_len", view_len_);
    GPU_shader_uniform_1i(shader, "visibility_word_per_draw", word_per_draw);
    GPU_shader_uniform_1b(shader, "force_fail_method", force_fail_method_);
    GPU_shader_uniform_3fv(shader, "shadow_direction", light_direction_);
    GPU_uniformbuf_bind(extruded_frustum_, GPU_shader_get_ubo_binding(shader, "extruded_frustum"));
    GPU_storagebuf_bind(bounds, GPU_shader_get_ssbo_binding(shader, "bounds_buf"));
    if (current_pass_type_ == ShadowPass::FORCED_FAIL) {
      GPU_storagebuf_bind(visibility_buf_, GPU_shader_get_ssbo_binding(shader, "visibility_buf"));
    }
    else {
      GPU_storagebuf_bind(pass_visibility_buf_,
                          GPU_shader_get_ssbo_binding(shader, "pass_visibility_buf"));
      GPU_storagebuf_bind(fail_visibility_buf_,
                          GPU_shader_get_ssbo_binding(shader, "fail_visibility_buf"));
    }
    GPU_uniformbuf_bind(data_, DRW_VIEW_UBO_SLOT);
    GPU_compute_dispatch(shader, divide_ceil_u(resource_len, DRW_VISIBILITY_GROUP_SIZE), 1, 1);
    GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
  }

  GPU_debug_group_end();
}

VisibilityBuf &ShadowPass::ShadowView::get_visibility_buffer()
{
  switch (current_pass_type_) {
    case ShadowPass::PASS:
      return pass_visibility_buf_;
    case ShadowPass::FAIL:
      return fail_visibility_buf_;
    case ShadowPass::FORCED_FAIL:
      return visibility_buf_;
    default:
      BLI_assert_unreachable();
  }
  return visibility_buf_;
}

PassMain::Sub *&ShadowPass::get_pass_ptr(PassType type, bool manifold, bool cap /*=false*/)
{
  return passes_[type][manifold][cap];
}

void ShadowPass::init(const SceneState &scene_state, SceneResources &resources)
{
  enabled_ = scene_state.draw_shadows;
  if (!enabled_) {
    resources.world_buf.shadow_mul = 0.0f;
    resources.world_buf.shadow_add = 1.0f;
    return;
  }
  const Scene &scene = *scene_state.scene;

  float3 direction_ws = scene.display.light_direction;
  /* Turn the light in a way where it's more user friendly to control. */
  std::swap(direction_ws.y, direction_ws.z);
  direction_ws *= float3(-1, 1, -1);

  std::array<float4, 6> planes = View::default_get().frustum_planes_get();

  pass_data_.light_direction_ws = direction_ws;
  pass_data_.far_plane = planes[2] * float4(-1, -1, -1, 1);
  pass_data_.push_update();

  /* Shadow direction. */
  float4x4 view_matrix = blender::draw::View::default_get().viewmat();
  resources.world_buf.shadow_direction_vs = float4(
      math::transform_direction(view_matrix, direction_ws), 0.0f);

  /* Clamp to avoid overshadowing and shading errors. */
  float focus = clamp_f(scene.display.shadow_focus, 0.0001f, 0.99999f);
  resources.world_buf.shadow_shift = scene.display.shadow_shift;
  resources.world_buf.shadow_focus = 1.0f - focus * (1.0f - resources.world_buf.shadow_shift);
  resources.world_buf.shadow_mul = scene_state.shading.shadow_intensity;
  resources.world_buf.shadow_add = 1.0f - resources.world_buf.shadow_mul;
}

void ShadowPass::sync()
{
  if (!enabled_) {
    return;
  }

#if DEBUG_SHADOW_VOLUME
  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL;
  DRWState depth_pass_state = state | DRW_STATE_DEPTH_LESS;
  DRWState depth_fail_state = state | DRW_STATE_DEPTH_GREATER_EQUAL;
#else
  DRWState state = DRW_STATE_DEPTH_LESS | DRW_STATE_STENCIL_ALWAYS;
  DRWState depth_pass_state = state | DRW_STATE_WRITE_STENCIL_SHADOW_PASS;
  DRWState depth_fail_state = state | DRW_STATE_WRITE_STENCIL_SHADOW_FAIL;
#endif

  pass_ps_.init();
  pass_ps_.state_set(depth_pass_state);
  pass_ps_.state_stencil(0xFF, 0xFF, 0xFF);

  fail_ps_.init();
  fail_ps_.state_set(depth_fail_state);
  fail_ps_.state_stencil(0xFF, 0xFF, 0xFF);

  forced_fail_ps_.init();
  forced_fail_ps_.state_set(depth_fail_state);
  forced_fail_ps_.state_stencil(0xFF, 0xFF, 0xFF);

  /* Stencil Shadow passes. */
  for (bool manifold : {false, true}) {
    PassMain::Sub *&ps = get_pass_ptr(PASS, manifold);
    ps = &pass_ps_.sub(manifold ? "manifold" : "non_manifold");
    ps->shader_set(ShaderCache::get().shadow_get(true, manifold));
    ps->bind_ubo("pass_data", pass_data_);

    for (PassType fail_type : {FAIL, FORCED_FAIL}) {
      PassMain &ps_main = fail_type == FAIL ? fail_ps_ : forced_fail_ps_;

      PassMain::Sub *&ps = get_pass_ptr(fail_type, manifold, false);
      ps = &ps_main.sub(manifold ? "NoCaps.manifold" : "NoCaps.non_manifold");
      ps->shader_set(ShaderCache::get().shadow_get(false, manifold, false));
      ps->bind_ubo("pass_data", pass_data_);

      PassMain::Sub *&caps_ps = get_pass_ptr(fail_type, manifold, true);
      caps_ps = &ps_main.sub(manifold ? "Caps.manifold" : "Caps.non_manifold");
      caps_ps->shader_set(ShaderCache::get().shadow_get(false, manifold, true));
      caps_ps->bind_ubo("pass_data", pass_data_);
    }
  }
}

void ShadowPass::object_sync(SceneState &scene_state,
                             ObjectRef &ob_ref,
                             ResourceHandleRange handle,
                             const bool has_transp_mat)
{
  if (!enabled_) {
    return;
  }

  Object *ob = ob_ref.object;
  bool is_manifold;
  blender::gpu::Batch *geom_shadow = DRW_cache_object_edge_detection_get(ob, &is_manifold);
  if (geom_shadow == nullptr) {
    return;
  }

#define DEBUG_CULLING 0
#if DEBUG_CULLING
  View view = View("View", DRW_view_default_get());
  ShadowView shadow_view = ShadowView("ShadowView", view, pass_data_.light_direction_ws);
  printf(
      "%s culling : %s\n", ob->id.name, shadow_view.debug_object_culling(ob) ? "true" : "false");
#endif

  /* Shadow pass technique needs object to be have all its surface opaque. */
  /* We cannot use the PASS technique on non-manifold object (see #76168). */
  bool force_fail_pass = has_transp_mat || (!is_manifold && (scene_state.cull_state != 0));

  PassType fail_type = force_fail_pass ? FORCED_FAIL : FAIL;

  /* Unless we force the FAIL Method we add draw commands to both methods,
   * then the visibility compute shader selects the one needed */

  GPUPrimType prim = GPU_PRIM_TRIS;
  int tri_len = is_manifold ? 2 : 4;

  if (!force_fail_pass) {
    PassMain::Sub &ps = *get_pass_ptr(PASS, is_manifold);
    ps.draw_expand(geom_shadow, prim, tri_len, 1, handle);
  }

  blender::gpu::Batch *geom_faces = DRW_cache_object_surface_get(ob);
  /* Caps. */
  get_pass_ptr(fail_type, is_manifold, true)->draw_expand(geom_faces, prim, 2, 1, handle);
  /* Sides extrusion. */
  get_pass_ptr(fail_type, is_manifold, false)->draw_expand(geom_shadow, prim, tri_len, 1, handle);
}

void ShadowPass::draw(Manager &manager,
                      View &view,
                      SceneResources &resources,
                      gpu::Texture &depth_stencil_tx,
                      bool force_fail_method)
{
  if (!enabled_) {
    return;
  }

  fb_.ensure(GPU_ATTACHMENT_TEXTURE(&depth_stencil_tx),
             GPU_ATTACHMENT_TEXTURE(resources.color_tx));
  fb_.bind();

  view_.setup(view, pass_data_.light_direction_ws, force_fail_method);

  view_.set_mode(PASS);
  manager.submit(pass_ps_, view_);
  view_.set_mode(FAIL);
  manager.submit(fail_ps_, view_);
  view_.set_mode(FORCED_FAIL);
  manager.submit(forced_fail_ps_, view_);
}

bool ShadowPass::is_debug()
{
  return DEBUG_SHADOW_VOLUME;
}

}  // namespace blender::workbench
