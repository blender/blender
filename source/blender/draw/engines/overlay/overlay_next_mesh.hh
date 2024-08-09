/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "DNA_mesh_types.h"

#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_mesh_types.hh"
#include "BKE_subdiv_modifier.hh"

#include "draw_cache_impl.hh"

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class Meshes {
 private:
  PassSimple edit_mesh_normals_ps_ = {"Normals"};
  PassSimple::Sub *face_normals_ = nullptr;
  PassSimple::Sub *face_normals_subdiv_ = nullptr;
  PassSimple::Sub *loop_normals_ = nullptr;
  PassSimple::Sub *loop_normals_subdiv_ = nullptr;
  PassSimple::Sub *vert_normals_ = nullptr;

  PassSimple edit_mesh_analysis_ps_ = {"Mesh Analysis"};

  PassSimple edit_mesh_edges_ps_ = {"Edges"};
  PassSimple edit_mesh_faces_ps_ = {"Faces"};
  PassSimple edit_mesh_cages_ps_ = {"Cages"}; /* Same as faces but with a different offset. */
  PassSimple edit_mesh_verts_ps_ = {"Verts"};
  PassSimple edit_mesh_facedots_ps_ = {"FaceDots"};
  PassSimple edit_mesh_skin_roots_ps_ = {"SkinRoots"};

  /* Depth pre-pass to cull edit cage in case the object is not opaque. */
  PassSimple edit_mesh_prepass_ps_ = {"Prepass"};

  bool xray_enabled = false;

  bool show_retopology = false;
  bool show_mesh_analysis = false;
  bool show_face = false;
  bool show_face_dots = false;

  bool select_edge = false;
  bool select_face = false;
  bool select_vert = false;

  /* TODO(fclem): This is quite wasteful and expensive, prefer in shader Z modification like the
   * retopology offset. */
  View view_edit_cage = {"view_edit_cage"};
  View view_edit_edge = {"view_edit_edge"};
  View view_edit_vert = {"view_edit_vert"};
  float view_dist = 0.0f;

 public:
  void begin_sync(Resources &res, const State &state, const View &view)
  {
    view_dist = state.view_dist_get(view.winmat());
    xray_enabled = state.xray_enabled;

    ToolSettings *tsettings = state.scene->toolsettings;
    select_edge = (tsettings->selectmode & SCE_SELECT_EDGE);
    select_face = (tsettings->selectmode & SCE_SELECT_FACE);
    select_vert = (tsettings->selectmode & SCE_SELECT_VERTEX);

    int edit_flag = state.v3d->overlay.edit_flag;
    show_retopology = (edit_flag & V3D_OVERLAY_EDIT_RETOPOLOGY) && !state.xray_enabled;
    show_mesh_analysis = (edit_flag & V3D_OVERLAY_EDIT_STATVIS);
    show_face = (edit_flag & V3D_OVERLAY_EDIT_FACES);
    show_face_dots = ((edit_flag & V3D_OVERLAY_EDIT_FACE_DOT) || state.xray_enabled) & select_face;

    const bool show_face_nor = (edit_flag & V3D_OVERLAY_EDIT_FACE_NORMALS);
    const bool show_loop_nor = (edit_flag & V3D_OVERLAY_EDIT_LOOP_NORMALS);
    const bool show_vert_nor = (edit_flag & V3D_OVERLAY_EDIT_VERT_NORMALS);

    const bool do_smooth_wire = (U.gpu_flag & USER_GPU_FLAG_NO_EDIT_MODE_SMOOTH_WIRE) == 0;
    const bool is_wire_shading_mode = (state.v3d->shading.type == OB_WIRE);

    uint4 data_mask = data_mask_get(edit_flag);

    float backwire_opacity = (state.xray_enabled) ? 0.5f : 1.0f;
    float face_alpha = (show_face) ? 1.0f : 0.0f;
    float retopology_offset = RETOPOLOGY_OFFSET(state.v3d);
    /* Cull back-faces for retopology face pass. This makes it so back-faces are not drawn.
     * Doing so lets us distinguish back-faces from front-faces. */
    DRWState face_culling = (show_retopology) ? DRW_STATE_CULL_BACK : DRWState(0);

    GPUTexture **depth_tex = (state.xray_enabled) ? &res.depth_tx : &res.dummy_depth_tx;

    {
      auto &pass = edit_mesh_prepass_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | face_culling |
                     state.clipping_state);
      pass.shader_set(res.shaders.mesh_edit_depth.get());
      pass.push_constant("retopologyOffset", retopology_offset);
    }
    {
      /* Normals */
      const bool use_screen_size = (edit_flag & V3D_OVERLAY_EDIT_CONSTANT_SCREEN_SIZE_NORMALS);
      const bool use_hq_normals = state.scene->r.perf_flag & SCE_PERF_HQ_NORMALS;

      DRWState pass_state = DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR |
                            DRW_STATE_DEPTH_LESS_EQUAL | state.clipping_state;
      if (state.xray_enabled) {
        pass_state |= DRW_STATE_BLEND_ALPHA;
      }

      auto &pass = edit_mesh_normals_ps_;
      pass.init();
      pass.state_set(pass_state);

      auto shader_pass = [&](GPUShader *shader, const char *name) {
        auto &sub = pass.sub(name);
        sub.shader_set(shader);
        sub.bind_ubo("globalsBlock", &res.globals_buf);
        sub.bind_texture("depthTex", depth_tex);
        sub.push_constant("alpha", backwire_opacity);
        sub.push_constant("isConstantScreenSizeNormals", use_screen_size);
        sub.push_constant("normalSize", state.overlay.normals_length);
        sub.push_constant("normalScreenSize", state.overlay.normals_constant_screen_size);
        sub.push_constant("retopologyOffset", retopology_offset);
        sub.push_constant("hq_normals", use_hq_normals);
        return &sub;
      };

      face_normals_ = loop_normals_ = vert_normals_ = nullptr;

      if (show_face_nor) {
        face_normals_subdiv_ = shader_pass(res.shaders.mesh_face_normal_subdiv.get(), "SubdFNor");
        face_normals_ = shader_pass(res.shaders.mesh_face_normal.get(), "FaceNor");
      }
      if (show_loop_nor) {
        loop_normals_subdiv_ = shader_pass(res.shaders.mesh_loop_normal_subdiv.get(), "SubdLNor");
        loop_normals_ = shader_pass(res.shaders.mesh_loop_normal.get(), "LoopNor");
      }
      if (show_vert_nor) {
        vert_normals_ = shader_pass(res.shaders.mesh_vert_normal.get(), "VertexNor");
      }
    }
    {
      auto &pass = edit_mesh_analysis_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                     state.clipping_state);
      pass.shader_set(res.shaders.mesh_analysis.get());
      pass.bind_texture("weightTex", res.weight_ramp_tx);
    }

    auto mesh_edit_common_resource_bind = [&](PassSimple &pass, float alpha) {
      pass.bind_texture("depthTex", depth_tex);
      /* TODO(fclem): UBO. */
      pass.push_constant("wireShading", is_wire_shading_mode);
      pass.push_constant("selectFace", select_face);
      pass.push_constant("selectEdge", select_edge);
      pass.push_constant("alpha", alpha);
      pass.push_constant("retopologyOffset", retopology_offset);
      pass.push_constant("dataMask", int4(data_mask));
      pass.bind_ubo("globalsBlock", &res.globals_buf);
    };

    {
      auto &pass = edit_mesh_edges_ps_;
      pass.init();
      /* Change first vertex convention to match blender loop structure. */
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                     DRW_STATE_FIRST_VERTEX_CONVENTION | state.clipping_state);
      pass.shader_set(res.shaders.mesh_edit_edge.get());
      pass.push_constant("do_smooth_wire", do_smooth_wire);
      pass.push_constant("use_vertex_selection", select_vert);
      mesh_edit_common_resource_bind(pass, backwire_opacity);
    }
    {
      auto &pass = edit_mesh_faces_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                     face_culling | state.clipping_state);
      pass.shader_set(res.shaders.mesh_edit_face.get());
      mesh_edit_common_resource_bind(pass, face_alpha);
    }
    {
      auto &pass = edit_mesh_cages_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                     state.clipping_state);
      pass.shader_set(res.shaders.mesh_edit_face.get());
      mesh_edit_common_resource_bind(pass, face_alpha);
    }
    {
      auto &pass = edit_mesh_verts_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                     DRW_STATE_WRITE_DEPTH | state.clipping_state);
      pass.shader_set(res.shaders.mesh_edit_vert.get());
      mesh_edit_common_resource_bind(pass, backwire_opacity);
    }
    {
      auto &pass = edit_mesh_facedots_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                     DRW_STATE_WRITE_DEPTH | state.clipping_state);
      pass.shader_set(res.shaders.mesh_edit_facedot.get());
      mesh_edit_common_resource_bind(pass, backwire_opacity);
    }
    {
      auto &pass = edit_mesh_skin_roots_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                     DRW_STATE_WRITE_DEPTH | state.clipping_state);
      pass.shader_set(res.shaders.mesh_edit_skin_root.get());
      pass.push_constant("retopologyOffset", retopology_offset);
      pass.bind_ubo("globalsBlock", &res.globals_buf);
    }
  }

  void edit_object_sync(Manager &manager, const ObjectRef &ob_ref, Resources & /*res*/)
  {
    ResourceHandle res_handle = manager.resource_handle(ob_ref);

    Object *ob = ob_ref.object;
    Mesh &mesh = *static_cast<Mesh *>(ob->data);
    /* WORKAROUND: GPU subdiv uses a different normal format. Remove this once GPU subdiv is
     * refactored. */
    const bool use_gpu_subdiv = BKE_subsurf_modifier_has_gpu_subdiv(static_cast<Mesh *>(ob->data));
    const bool draw_as_solid = (ob->dt > OB_WIRE);

    if (show_retopology) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_triangles(mesh);
      edit_mesh_prepass_ps_.draw(geom, res_handle);
    }
    if (draw_as_solid) {
      gpu::Batch *geom = DRW_cache_mesh_surface_get(ob);
      edit_mesh_prepass_ps_.draw(geom, res_handle);
    }

    if (show_mesh_analysis) {
      gpu::Batch *geom = DRW_cache_mesh_surface_mesh_analysis_get(ob);
      edit_mesh_analysis_ps_.draw(geom, res_handle);
    }

    if (face_normals_) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_facedots(mesh);
      (use_gpu_subdiv ? face_normals_subdiv_ : face_normals_)
          ->draw_expand(geom, GPU_PRIM_LINES, 1, 1, res_handle);
    }
    if (loop_normals_) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_loop_normals(mesh);
      (use_gpu_subdiv ? loop_normals_subdiv_ : loop_normals_)
          ->draw_expand(geom, GPU_PRIM_LINES, 1, 1, res_handle);
    }
    if (vert_normals_) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_vert_normals(mesh);
      vert_normals_->draw_expand(geom, GPU_PRIM_LINES, 1, 1, res_handle);
    }

    {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_edges(mesh);
      edit_mesh_edges_ps_.draw_expand(geom, GPU_PRIM_TRIS, 2, 1, res_handle);
    }
    {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_triangles(mesh);
      (mesh_has_edit_cage(ob) ? &edit_mesh_cages_ps_ : &edit_mesh_faces_ps_)
          ->draw(geom, res_handle);
    }
    if (select_vert) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_vertices(mesh);
      edit_mesh_verts_ps_.draw(geom, res_handle);
    }
    if (show_face_dots) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_facedots(mesh);
      edit_mesh_facedots_ps_.draw(geom, res_handle);
    }

    if (mesh_has_skin_roots(ob)) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_skin_roots(mesh);
      edit_mesh_skin_roots_ps_.draw_expand(geom, GPU_PRIM_LINES, 32, 1, res_handle);
    }
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    GPU_debug_group_begin("Mesh Edit");

    GPU_framebuffer_bind(framebuffer);
    manager.submit(edit_mesh_prepass_ps_, view);
    manager.submit(edit_mesh_analysis_ps_, view);

    if (xray_enabled) {
      GPU_debug_group_end();
      return;
    }

    view_edit_cage.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist, 0.5f));
    view_edit_edge.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist, 1.0f));
    view_edit_vert.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist, 1.5f));

    manager.submit(edit_mesh_normals_ps_, view);
    manager.submit(edit_mesh_faces_ps_, view);
    manager.submit(edit_mesh_cages_ps_, view_edit_cage);
    manager.submit(edit_mesh_edges_ps_, view_edit_edge);
    manager.submit(edit_mesh_verts_ps_, view_edit_vert);
    manager.submit(edit_mesh_skin_roots_ps_, view_edit_vert);
    manager.submit(edit_mesh_facedots_ps_, view_edit_vert);

    GPU_debug_group_end();
  }

  void draw_color_only(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!xray_enabled) {
      return;
    }

    GPU_debug_group_begin("Mesh Edit Color Only");

    view_edit_cage.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist, 0.5f));
    view_edit_edge.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist, 1.0f));
    view_edit_vert.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist, 1.5f));

    GPU_framebuffer_bind(framebuffer);
    manager.submit(edit_mesh_normals_ps_, view);
    manager.submit(edit_mesh_faces_ps_, view);
    manager.submit(edit_mesh_cages_ps_, view_edit_cage);
    manager.submit(edit_mesh_edges_ps_, view_edit_edge);
    manager.submit(edit_mesh_verts_ps_, view_edit_vert);
    manager.submit(edit_mesh_skin_roots_ps_, view_edit_vert);
    manager.submit(edit_mesh_facedots_ps_, view_edit_vert);

    GPU_debug_group_end();
  }

 private:
  uint4 data_mask_get(const int flag)
  {
    uint4 mask = {0xFF, 0xFF, 0x00, 0x00};
    SET_FLAG_FROM_TEST(mask[0], flag & V3D_OVERLAY_EDIT_FACES, VFLAG_FACE_SELECTED);
    SET_FLAG_FROM_TEST(mask[0], flag & V3D_OVERLAY_EDIT_FREESTYLE_FACE, VFLAG_FACE_FREESTYLE);
    SET_FLAG_FROM_TEST(mask[1], flag & V3D_OVERLAY_EDIT_FREESTYLE_EDGE, VFLAG_EDGE_FREESTYLE);
    SET_FLAG_FROM_TEST(mask[1], flag & V3D_OVERLAY_EDIT_SEAMS, VFLAG_EDGE_SEAM);
    SET_FLAG_FROM_TEST(mask[1], flag & V3D_OVERLAY_EDIT_SHARP, VFLAG_EDGE_SHARP);
    SET_FLAG_FROM_TEST(mask[2], flag & V3D_OVERLAY_EDIT_CREASES, 0xFF);
    SET_FLAG_FROM_TEST(mask[3], flag & V3D_OVERLAY_EDIT_BWEIGHTS, 0xFF);
    return mask;
  }

  static bool mesh_has_skin_roots(const Object *ob)
  {
    const Mesh &mesh = *static_cast<const Mesh *>(ob->data);
    if (BMEditMesh *em = mesh.runtime->edit_mesh.get()) {
      return CustomData_get_offset(&em->bm->vdata, CD_MVERT_SKIN) != -1;
    }
    return false;
  }

  static bool mesh_has_edit_cage(const Object *ob)
  {
    const Mesh &mesh = *static_cast<const Mesh *>(ob->data);
    if (mesh.runtime->edit_mesh.get() != nullptr) {
      const Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(ob);
      const Mesh *editmesh_eval_cage = BKE_object_get_editmesh_eval_cage(ob);

      return (editmesh_eval_cage != nullptr) && (editmesh_eval_cage != editmesh_eval_final);
    }
    return false;
  }
};

}  // namespace blender::draw::overlay
