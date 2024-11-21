/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include <string>

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_mask.h"
#include "BKE_mesh_types.hh"
#include "BKE_subdiv_modifier.hh"

#include "ED_image.hh"

#include "GPU_capabilities.hh"

#include "draw_cache_impl.hh"

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

constexpr int overlay_edit_text = V3D_OVERLAY_EDIT_EDGE_LEN | V3D_OVERLAY_EDIT_FACE_AREA |
                                  V3D_OVERLAY_EDIT_FACE_ANG | V3D_OVERLAY_EDIT_EDGE_ANG |
                                  V3D_OVERLAY_EDIT_INDICES;

class Meshes {
 private:
  PassSimple edit_mesh_normals_ps_ = {"Normals"};
  PassSimple::Sub *face_normals_ = nullptr;
  PassSimple::Sub *face_normals_subdiv_ = nullptr;
  PassSimple::Sub *loop_normals_ = nullptr;
  PassSimple::Sub *loop_normals_subdiv_ = nullptr;
  PassSimple::Sub *vert_normals_ = nullptr;

  PassSimple edit_mesh_analysis_ps_ = {"Mesh Analysis"};
  PassSimple edit_mesh_weight_ps_ = {"Edit Weight"};

  PassSimple edit_mesh_edges_ps_ = {"Edges"};
  PassSimple edit_mesh_faces_ps_ = {"Faces"};
  PassSimple edit_mesh_cages_ps_ = {"Cages"}; /* Same as faces but with a different offset. */
  PassSimple edit_mesh_verts_ps_ = {"Verts"};
  PassSimple edit_mesh_facedots_ps_ = {"FaceDots"};
  PassSimple edit_mesh_skin_roots_ps_ = {"SkinRoots"};

  /* Depth pre-pass to cull edit cage in case the object is not opaque. */
  PassSimple edit_mesh_prepass_ps_ = {"Prepass"};

  bool xray_enabled_ = false;

  bool show_retopology_ = false;
  bool show_mesh_analysis_ = false;
  bool show_face_ = false;
  bool show_face_dots_ = false;
  bool show_weight_ = false;

  bool select_edge_ = false;
  bool select_face_ = false;
  bool select_vert_ = false;

  /* TODO(fclem): This is quite wasteful and expensive, prefer in shader Z modification like the
   * retopology offset. */
  View view_edit_cage_ = {"view_edit_cage"};
  View view_edit_edge_ = {"view_edit_edge"};
  View view_edit_vert_ = {"view_edit_vert"};
  float view_dist_ = 0.0f;

  bool enabled_ = false;

 public:
  void begin_sync(Resources &res, const State &state, const View &view)
  {
    enabled_ = state.space_type == SPACE_VIEW3D;

    if (!enabled_) {
      return;
    }

    view_dist_ = state.view_dist_get(view.winmat());
    xray_enabled_ = state.xray_enabled;

    ToolSettings *tsettings = state.scene->toolsettings;
    select_edge_ = (tsettings->selectmode & SCE_SELECT_EDGE);
    select_face_ = (tsettings->selectmode & SCE_SELECT_FACE);
    select_vert_ = (tsettings->selectmode & SCE_SELECT_VERTEX);

    int edit_flag = state.v3d->overlay.edit_flag;
    show_retopology_ = (edit_flag & V3D_OVERLAY_EDIT_RETOPOLOGY) && !state.xray_enabled;
    show_mesh_analysis_ = (edit_flag & V3D_OVERLAY_EDIT_STATVIS);
    show_face_ = (edit_flag & V3D_OVERLAY_EDIT_FACES);
    show_face_dots_ = ((edit_flag & V3D_OVERLAY_EDIT_FACE_DOT) || state.xray_enabled) &
                      select_face_;
    show_weight_ = (edit_flag & V3D_OVERLAY_EDIT_WEIGHT);

    const bool show_face_nor = (edit_flag & V3D_OVERLAY_EDIT_FACE_NORMALS);
    const bool show_loop_nor = (edit_flag & V3D_OVERLAY_EDIT_LOOP_NORMALS);
    const bool show_vert_nor = (edit_flag & V3D_OVERLAY_EDIT_VERT_NORMALS);

    const bool do_smooth_wire = (U.gpu_flag & USER_GPU_FLAG_NO_EDIT_MODE_SMOOTH_WIRE) == 0;
    const bool is_wire_shading_mode = (state.v3d->shading.type == OB_WIRE);

    uint4 data_mask = data_mask_get(edit_flag);

    float backwire_opacity = (state.xray_enabled) ? 0.5f : 1.0f;
    float face_alpha = (show_face_) ? 1.0f : 0.0f;
    float retopology_offset = RETOPOLOGY_OFFSET(state.v3d);
    /* Cull back-faces for retopology face pass. This makes it so back-faces are not drawn.
     * Doing so lets us distinguish back-faces from front-faces. */
    DRWState face_culling = (show_retopology_) ? DRW_STATE_CULL_BACK : DRWState(0);

    GPUTexture **depth_tex = (state.xray_enabled) ? &res.depth_tx : &res.dummy_depth_tx;

    {
      auto &pass = edit_mesh_prepass_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | face_culling,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders.mesh_edit_depth.get());
      pass.push_constant("retopologyOffset", retopology_offset);
    }
    {
      /* Normals */
      const bool use_screen_size = (edit_flag & V3D_OVERLAY_EDIT_CONSTANT_SCREEN_SIZE_NORMALS);
      const bool use_hq_normals = (state.scene->r.perf_flag & SCE_PERF_HQ_NORMALS) ||
                                  GPU_use_hq_normals_workaround();

      DRWState pass_state = DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR |
                            DRW_STATE_DEPTH_LESS_EQUAL;
      if (state.xray_enabled) {
        pass_state |= DRW_STATE_BLEND_ALPHA;
      }

      auto &pass = edit_mesh_normals_ps_;
      pass.init();
      pass.state_set(pass_state, state.clipping_plane_count);

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
      /* Support masked transparency in Workbench.
       * EEVEE can't be supported since depth won't match. */
      const bool shadeless = eDrawType(state.v3d->shading.type) == OB_WIRE;

      auto &pass = edit_mesh_weight_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WRITE_DEPTH,
                     state.clipping_plane_count);
      pass.shader_set(shadeless ? res.shaders.paint_weight.get() :
                                  res.shaders.paint_weight_fake_shading.get());
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      pass.bind_texture("colorramp", &res.weight_ramp_tx);
      pass.push_constant("drawContours", false);
      pass.push_constant("opacity", state.overlay.weight_paint_mode_opacity);
      if (!shadeless) {
        /* Arbitrary light to give a hint of the geometry behind the weights. */
        pass.push_constant("light_dir", math::normalize(float3(0.0f, 0.5f, 0.86602f)));
      }
    }
    {
      auto &pass = edit_mesh_analysis_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders.mesh_analysis.get());
      pass.bind_texture("weightTex", res.weight_ramp_tx);
    }

    auto mesh_edit_common_resource_bind = [&](PassSimple &pass, float alpha) {
      pass.bind_texture("depthTex", depth_tex);
      /* TODO(fclem): UBO. */
      pass.push_constant("wireShading", is_wire_shading_mode);
      pass.push_constant("selectFace", select_face_);
      pass.push_constant("selectEdge", select_edge_);
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
                         DRW_STATE_FIRST_VERTEX_CONVENTION,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders.mesh_edit_edge.get());
      pass.push_constant("do_smooth_wire", do_smooth_wire);
      pass.push_constant("use_vertex_selection", select_vert_);
      mesh_edit_common_resource_bind(pass, backwire_opacity);
    }
    {
      auto &pass = edit_mesh_faces_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                         face_culling,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders.mesh_edit_face.get());
      mesh_edit_common_resource_bind(pass, face_alpha);
    }
    {
      auto &pass = edit_mesh_cages_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders.mesh_edit_face.get());
      mesh_edit_common_resource_bind(pass, face_alpha);
    }
    {
      auto &pass = edit_mesh_verts_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                         DRW_STATE_WRITE_DEPTH,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders.mesh_edit_vert.get());
      mesh_edit_common_resource_bind(pass, backwire_opacity);
    }
    {
      auto &pass = edit_mesh_facedots_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                         DRW_STATE_WRITE_DEPTH,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders.mesh_edit_facedot.get());
      mesh_edit_common_resource_bind(pass, backwire_opacity);
    }
    {
      auto &pass = edit_mesh_skin_roots_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                         DRW_STATE_WRITE_DEPTH,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders.mesh_edit_skin_root.get());
      pass.push_constant("retopologyOffset", retopology_offset);
      pass.bind_ubo("globalsBlock", &res.globals_buf);
    }
  }

  void edit_object_sync(Manager &manager,
                        const ObjectRef &ob_ref,
                        const State &state,
                        Resources & /*res*/)
  {
    if (!enabled_) {
      return;
    }

    ResourceHandle res_handle = manager.unique_handle(ob_ref);

    Object *ob = ob_ref.object;
    Mesh &mesh = *static_cast<Mesh *>(ob->data);
    /* WORKAROUND: GPU subdiv uses a different normal format. Remove this once GPU subdiv is
     * refactored. */
    const bool use_gpu_subdiv = BKE_subsurf_modifier_has_gpu_subdiv(static_cast<Mesh *>(ob->data));
    const bool draw_as_solid = (ob->dt > OB_WIRE);

    if (show_retopology_) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_triangles(mesh);
      edit_mesh_prepass_ps_.draw(geom, res_handle);
    }
    if (draw_as_solid) {
      gpu::Batch *geom = DRW_cache_mesh_surface_get(ob);
      edit_mesh_prepass_ps_.draw(geom, res_handle);
    }

    if (show_mesh_analysis_) {
      gpu::Batch *geom = DRW_cache_mesh_surface_mesh_analysis_get(ob);
      edit_mesh_analysis_ps_.draw(geom, res_handle);
    }

    if (show_weight_) {
      gpu::Batch *geom = DRW_cache_mesh_surface_weights_get(ob);
      edit_mesh_weight_ps_.draw(geom, res_handle);
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
    if (select_vert_) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_vertices(mesh);
      edit_mesh_verts_ps_.draw(geom, res_handle);
    }
    if (show_face_dots_) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_facedots(mesh);
      edit_mesh_facedots_ps_.draw(geom, res_handle);
    }

    if (mesh_has_skin_roots(ob)) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edit_skin_roots(mesh);
      edit_mesh_skin_roots_ps_.draw_expand(geom, GPU_PRIM_LINES, 32, 1, res_handle);
    }
    if (DRW_state_show_text() && (state.overlay.edit_flag & overlay_edit_text)) {
      DRW_text_edit_mesh_measure_stats(state.region, state.v3d, ob, &state.scene->unit, state.dt);
    }
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    GPU_debug_group_begin("Mesh Edit");

    GPU_framebuffer_bind(framebuffer);
    manager.submit(edit_mesh_prepass_ps_, view);
    manager.submit(edit_mesh_analysis_ps_, view);
    manager.submit(edit_mesh_weight_ps_, view);

    if (xray_enabled_) {
      GPU_debug_group_end();
      return;
    }

    view_edit_cage_.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist_, 0.5f));
    view_edit_edge_.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist_, 1.0f));
    view_edit_vert_.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist_, 1.5f));

    manager.submit(edit_mesh_normals_ps_, view);
    manager.submit(edit_mesh_faces_ps_, view);
    manager.submit(edit_mesh_cages_ps_, view_edit_cage_);
    manager.submit(edit_mesh_edges_ps_, view_edit_edge_);
    manager.submit(edit_mesh_verts_ps_, view_edit_vert_);
    manager.submit(edit_mesh_skin_roots_ps_, view_edit_vert_);
    manager.submit(edit_mesh_facedots_ps_, view_edit_vert_);

    GPU_debug_group_end();
  }

  void draw_color_only(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    if (!xray_enabled_) {
      return;
    }

    GPU_debug_group_begin("Mesh Edit Color Only");

    view_edit_cage_.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist_, 0.5f));
    view_edit_edge_.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist_, 1.0f));
    view_edit_vert_.sync(view.viewmat(), winmat_polygon_offset(view.winmat(), view_dist_, 1.5f));

    GPU_framebuffer_bind(framebuffer);
    manager.submit(edit_mesh_normals_ps_, view);
    manager.submit(edit_mesh_faces_ps_, view);
    manager.submit(edit_mesh_cages_ps_, view_edit_cage_);
    manager.submit(edit_mesh_edges_ps_, view_edit_edge_);
    manager.submit(edit_mesh_verts_ps_, view_edit_vert_);
    manager.submit(edit_mesh_skin_roots_ps_, view_edit_vert_);
    manager.submit(edit_mesh_facedots_ps_, view_edit_vert_);

    GPU_debug_group_end();
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
};

class MeshUVs {
 private:
  PassSimple analysis_ps_ = {"Mesh Analysis"};

  /* TODO(fclem): Should be its own Overlay?. */
  PassSimple wireframe_ps_ = {"Wireframe"};

  PassSimple edges_ps_ = {"Edges"};
  PassSimple faces_ps_ = {"Faces"};
  PassSimple verts_ps_ = {"Verts"};
  PassSimple facedots_ps_ = {"FaceDots"};

  /* TODO(fclem): Should be its own Overlay?. */
  PassSimple image_border_ps_ = {"ImageBorder"};

  /* TODO(fclem): Should be its own Overlay?. */
  PassSimple brush_stencil_ps_ = {"BrushStencil"};

  /* TODO(fclem): Should be its own Overlay?. */
  PassSimple paint_mask_ps_ = {"PaintMask"};

  bool show_vert_ = false;
  bool show_face_ = false;
  bool show_face_dots_ = false;
  bool show_uv_edit = false;

  /** Wireframe Overlay */
  /* Draw final evaluated UVs (modifier stack applied) as grayed out wire-frame. */
  /* TODO(fclem): Maybe should be its own Overlay?. */
  bool show_wireframe_ = false;

  /** Brush stencil. */
  /* TODO(fclem): Maybe should be its own Overlay?. */
  bool show_stencil_ = false;

  /** Paint Mask overlay. */
  /* TODO(fclem): Maybe should be its own Overlay?. */
  bool show_mask_ = false;
  eMaskOverlayMode mask_mode_ = MASK_OVERLAY_ALPHACHANNEL;
  Mask *mask_id_ = nullptr;
  Texture mask_texture_ = {"mask_texture_"};

  /** Stretching Overlay. */
  bool show_mesh_analysis_ = false;
  eSpaceImage_UVDT_Stretch mesh_analysis_type_;
  /**
   * In order to display the stretching relative to all objects in edit mode, we have to sum the
   * area ***AFTER*** extraction and before drawing. To that end, we get a pointer to the resulting
   * total per mesh area location to dereference after extraction.
   */
  Vector<float *> per_mesh_area_3d_;
  Vector<float *> per_mesh_area_2d_;
  float total_area_ratio_;

  /** UDIM border overlay. */
  bool show_tiled_image_active_ = false;
  bool show_tiled_image_border_ = false;
  bool show_tiled_image_label_ = false;

  /* Set of original objects that have been drawn. */
  Set<const Object *> drawn_object_set_;

  bool enabled_ = false;

 public:
  void begin_sync(Resources &res, const State &state)
  {
    enabled_ = state.space_type == SPACE_IMAGE;

    if (!enabled_) {
      return;
    }

    const ToolSettings *tool_setting = state.scene->toolsettings;
    const SpaceImage *space_image = reinterpret_cast<const SpaceImage *>(state.space_data);
    ::Image *image = space_image->image;
    const bool is_tiled_image = image && (image->source == IMA_SRC_TILED);
    const bool is_viewer = image && ELEM(image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE);
    /* Only disable UV drawing on top of render results.
     * Otherwise, show UVs even in the absence of active image. */
    enabled_ = !is_viewer;

    if (!enabled_) {
      return;
    }

    const bool space_mode_is_paint = space_image->mode == SI_MODE_PAINT;
    const bool space_mode_is_view = space_image->mode == SI_MODE_VIEW;
    const bool space_mode_is_mask = space_image->mode == SI_MODE_MASK;
    const bool space_mode_is_uv = space_image->mode == SI_MODE_UV;

    const bool object_mode_is_edit = state.object_mode & OB_MODE_EDIT;
    const bool object_mode_is_paint = state.object_mode & OB_MODE_TEXTURE_PAINT;

    {
      /* Edit UV Overlay. */
      show_uv_edit = space_mode_is_uv && object_mode_is_edit;
      show_mesh_analysis_ = show_uv_edit && (space_image->flag & SI_DRAW_STRETCH);

      if (!show_uv_edit) {
        show_vert_ = false;
        show_face_ = false;
        show_face_dots_ = false;
      }
      else {
        const bool hide_faces = space_image->flag & SI_NO_DRAWFACES;

        int sel_mode_2d = tool_setting->uv_selectmode;
        show_vert_ = (sel_mode_2d != UV_SELECT_EDGE);
        show_face_ = !show_mesh_analysis_ && !hide_faces;
        show_face_dots_ = (sel_mode_2d & UV_SELECT_FACE) && !hide_faces;

        if (tool_setting->uv_flag & UV_SYNC_SELECTION) {
          int sel_mode_3d = tool_setting->selectmode;
          /* NOTE: Ignore #SCE_SELECT_VERTEX because a single selected edge
           * on the mesh may cause single UV vertices to be selected. */
          show_vert_ = true /* (sel_mode_3d & SCE_SELECT_VERTEX) */;
          show_face_dots_ = (sel_mode_3d & SCE_SELECT_FACE) && !hide_faces;
        }
      }

      if (show_mesh_analysis_) {
        mesh_analysis_type_ = eSpaceImage_UVDT_Stretch(space_image->dt_uvstretch);
      }
    }
    {
      /* Wireframe UV Overlay. */
      const bool show_wireframe_uv_edit = space_image->flag & SI_DRAWSHADOW;
      const bool show_wireframe_tex_paint = !(space_image->flag & SI_NO_DRAW_TEXPAINT);

      if (space_mode_is_uv && object_mode_is_edit) {
        show_wireframe_ = show_wireframe_uv_edit;
      }
      else if (space_mode_is_uv && object_mode_is_paint) {
        show_wireframe_ = show_wireframe_tex_paint;
      }
      else if (space_mode_is_paint && (object_mode_is_paint || object_mode_is_edit)) {
        show_wireframe_ = show_wireframe_tex_paint;
      }
      else if (space_mode_is_view && object_mode_is_paint) {
        show_wireframe_ = show_wireframe_tex_paint;
      }
      else {
        show_wireframe_ = false;
      }
    }
    {
      /* Brush Stencil Overlay. */
      const Brush *brush = BKE_paint_brush_for_read(&tool_setting->imapaint.paint);
      show_stencil_ = space_mode_is_paint && brush &&
                      (brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_CLONE) &&
                      brush->clone.image;
    }
    {
      /* Mask Overlay. */
      show_mask_ = space_mode_is_mask && space_image->mask_info.mask &&
                   space_image->mask_info.draw_flag & MASK_DRAWFLAG_OVERLAY;
      if (show_mask_) {
        mask_mode_ = eMaskOverlayMode(space_image->mask_info.overlay_mode);
        mask_id_ = (Mask *)DEG_get_evaluated_id(state.depsgraph, &space_image->mask_info.mask->id);
      }
      else {
        mask_id_ = nullptr;
      }
    }
    {
      /* UDIM Overlay. */
      /* TODO: Always enable this overlay even if overlays are disabled. */
      show_tiled_image_border_ = is_tiled_image;
      show_tiled_image_active_ = is_tiled_image; /* TODO: Only disable this if overlays are off. */
      show_tiled_image_label_ = is_tiled_image;  /* TODO: Only disable this if overlays are off. */
    }

    const bool do_smooth_wire = (U.gpu_flag & USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE) != 0;
    const float dash_length = 4.0f * UI_SCALE_FAC;

    if (show_wireframe_) {
      auto &pass = wireframe_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                     DRW_STATE_BLEND_ALPHA);
      pass.shader_set(res.shaders.uv_wireframe.get());
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      pass.push_constant("alpha", space_image->uv_opacity);
      pass.push_constant("doSmoothWire", do_smooth_wire);
    }

    if (show_uv_edit) {
      auto &pass = edges_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                     DRW_STATE_BLEND_ALPHA);

      GPUShader *sh = res.shaders.uv_edit_edge.get();
      pass.specialize_constant(sh, "use_edge_select", !show_vert_);
      pass.shader_set(sh);
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      pass.push_constant("lineStyle", int(edit_uv_line_style_from_space_image(space_image)));
      pass.push_constant("alpha", space_image->uv_opacity);
      pass.push_constant("dashLength", dash_length);
      pass.push_constant("doSmoothWire", do_smooth_wire);
    }

    if (show_vert_) {
      const float point_size = UI_GetThemeValuef(TH_VERTEX_SIZE) * UI_SCALE_FAC;
      float4 theme_color;
      UI_GetThemeColor4fv(TH_VERTEX, theme_color);
      srgb_to_linearrgb_v4(theme_color, theme_color);

      auto &pass = verts_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                     DRW_STATE_BLEND_ALPHA);
      pass.shader_set(res.shaders.uv_edit_vert.get());
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      pass.push_constant("pointSize", (point_size + 1.5f) * float(M_SQRT2));
      pass.push_constant("outlineWidth", 0.75f);
      pass.push_constant("color", theme_color);
    }

    if (show_face_dots_) {
      const float point_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) * UI_SCALE_FAC;

      auto &pass = facedots_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                     DRW_STATE_BLEND_ALPHA);
      pass.shader_set(res.shaders.uv_edit_facedot.get());
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      pass.push_constant("pointSize", (point_size + 1.5f) * float(M_SQRT2));
    }

    if (show_face_) {
      auto &pass = faces_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS | DRW_STATE_BLEND_ALPHA);
      pass.shader_set(res.shaders.uv_edit_face.get());
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      pass.push_constant("uvOpacity", space_image->uv_opacity);
    }

    if (show_mesh_analysis_) {
      auto &pass = analysis_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS | DRW_STATE_BLEND_ALPHA);
      pass.shader_set(mesh_analysis_type_ == SI_UVDT_STRETCH_ANGLE ?
                          res.shaders.uv_analysis_stretch_angle.get() :
                          res.shaders.uv_analysis_stretch_area.get());
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      pass.push_constant("aspect", state.image_uv_aspect);
      pass.push_constant("stretch_opacity", space_image->stretch_opacity);
      pass.push_constant("totalAreaRatio", &total_area_ratio_);
    }

    per_mesh_area_3d_.clear();
    per_mesh_area_2d_.clear();

    drawn_object_set_.clear();
  }

  void edit_object_sync(Manager &manager, const ObjectRef &ob_ref, const State &state)
  {
    if (!enabled_ || ob_ref.object->type != OB_MESH) {
      return;
    }

    /* When editing objects that share the same mesh we should only draw the
     * first object to avoid overlapping UVs. Moreover, only the first evaluated object has the
     * correct batches with the correct selection state.
     * To this end, we skip duplicates and use the evaluated object returned by the depsgraph.
     * See #83187. */
    Object *object_orig = DEG_get_original_object(ob_ref.object);
    Object *object_eval = DEG_get_evaluated_object(state.depsgraph, object_orig);

    if (!drawn_object_set_.add(object_orig)) {
      return;
    }

    ResourceHandle res_handle = manager.unique_handle(ob_ref);

    Object &ob = *object_eval;
    Mesh &mesh = *static_cast<Mesh *>(ob.data);

    if (object_eval != ob_ref.object) {
      /* We are requesting batches on an evaluated ID that is potentially not iterated over.
       * So we have to manually call these cache validation and extraction method. */
      DRW_mesh_batch_cache_validate(ob, mesh);
    }

    if (show_uv_edit) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edituv_edges(ob, mesh);
      edges_ps_.draw_expand(geom, GPU_PRIM_TRIS, 2, 1, res_handle);
    }
    if (show_vert_) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edituv_verts(ob, mesh);
      verts_ps_.draw(geom, res_handle);
    }
    if (show_face_dots_) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edituv_facedots(ob, mesh);
      facedots_ps_.draw(geom, res_handle);
    }
    if (show_face_) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_edituv_faces(ob, mesh);
      faces_ps_.draw(geom, res_handle);
    }

    if (show_mesh_analysis_) {
      int index_3d, index_2d;
      if (mesh_analysis_type_ == SI_UVDT_STRETCH_AREA) {
        index_3d = per_mesh_area_3d_.append_and_get_index(nullptr);
        index_2d = per_mesh_area_2d_.append_and_get_index(nullptr);
      }

      gpu::Batch *geom =
          mesh_analysis_type_ == SI_UVDT_STRETCH_ANGLE ?
              DRW_mesh_batch_cache_get_edituv_faces_stretch_angle(ob, mesh) :
              DRW_mesh_batch_cache_get_edituv_faces_stretch_area(
                  ob, mesh, &per_mesh_area_3d_[index_3d], &per_mesh_area_2d_[index_2d]);

      analysis_ps_.draw(geom, res_handle);
    }

    if (show_wireframe_) {
      gpu::Batch *geom = DRW_mesh_batch_cache_get_uv_edges(ob, mesh);
      wireframe_ps_.draw_expand(geom, GPU_PRIM_TRIS, 2, 1, res_handle);
    }

    if (object_eval != ob_ref.object) {
      /* TODO(fclem): Refactor. Global access. But as explained above it is a bit complicated. */
      drw_batch_cache_generate_requested_delayed(&ob);
    }
  }

  void end_sync(Resources &res, ShapeCache &shapes, const State &state)
  {
    if (!enabled_) {
      return;
    }

    {
      float total_3d = 0.0f;
      float total_2d = 0.0f;
      for (const float *mesh_area_2d : per_mesh_area_2d_) {
        total_2d += *mesh_area_2d;
      }
      for (const float *mesh_area_3d : per_mesh_area_3d_) {
        total_3d += *mesh_area_3d;
      }
      total_area_ratio_ = total_3d * math::safe_rcp(total_2d);
    }

    const ToolSettings *tool_setting = state.scene->toolsettings;
    const SpaceImage *space_image = reinterpret_cast<const SpaceImage *>(state.space_data);
    ::Image *image = space_image->image;

    if (show_tiled_image_border_) {
      float4 theme_color;
      float4 selected_color;
      uchar4 text_color;
      /* Color Management: Exception here as texts are drawn in sRGB space directly. No conversion
       * required. */
      UI_GetThemeColorShade4ubv(TH_BACK, 60, text_color);
      UI_GetThemeColorShade4fv(TH_BACK, 60, theme_color);
      UI_GetThemeColor4fv(TH_FACE_SELECT, selected_color);
      srgb_to_linearrgb_v4(theme_color, theme_color);
      srgb_to_linearrgb_v4(selected_color, selected_color);

      auto &pass = image_border_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS);
      pass.shader_set(res.shaders.uv_image_borders.get());

      auto draw_tile = [&](const ImageTile *tile, const bool is_active) {
        const int tile_x = ((tile->tile_number - 1001) % 10);
        const int tile_y = ((tile->tile_number - 1001) / 10);
        const float3 tile_location(tile_x, tile_y, 0.0f);
        pass.push_constant("tile_pos", tile_location);
        pass.push_constant("ucolor", is_active ? selected_color : theme_color);
        pass.draw(shapes.quad_wire.get());

        /* Note: don't draw label twice for active tile. */
        if (show_tiled_image_label_ && !is_active) {
          std::string text = std::to_string(tile->tile_number);
          DRW_text_cache_add(state.dt,
                             tile_location,
                             text.c_str(),
                             text.size(),
                             10,
                             10,
                             DRW_TEXT_CACHE_GLOBALSPACE,
                             text_color);
        }
      };

      ListBaseWrapper<ImageTile> tiles(image->tiles);

      for (const ImageTile *tile : tiles) {
        draw_tile(tile, false);
      }
      /* Draw active tile on top. */
      if (show_tiled_image_active_) {
        draw_tile(tiles.get(image->active_tile_index), true);
      }
    }

    if (show_stencil_) {
      auto &pass = brush_stencil_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS |
                     DRW_STATE_BLEND_ALPHA_PREMUL);

      const Brush *brush = BKE_paint_brush_for_read(&tool_setting->imapaint.paint);
      ::Image *stencil_image = brush->clone.image;
      TextureRef stencil_texture;
      stencil_texture.wrap(BKE_image_get_gpu_texture(stencil_image, nullptr));

      if (stencil_texture.is_valid()) {
        float2 size_image;
        BKE_image_get_size_fl(image, nullptr, &size_image[0]);

        pass.shader_set(res.shaders.uv_brush_stencil.get());
        pass.bind_texture("imgTexture", stencil_texture);
        pass.push_constant("imgPremultiplied", true);
        pass.push_constant("imgAlphaBlend", true);
        pass.push_constant("ucolor", float4(1.0f, 1.0f, 1.0f, brush->clone.alpha));
        pass.push_constant("brush_offset", float2(brush->clone.offset));
        pass.push_constant("brush_scale", float2(stencil_texture.size().xy()) / size_image);
        pass.draw(shapes.quad_solid.get());
      }
    }

    if (show_mask_) {
      paint_mask_texture_ensure(mask_id_, state.image_size, state.image_aspect);

      const bool is_combined = mask_mode_ == MASK_OVERLAY_COMBINED;
      const float opacity = is_combined ? space_image->mask_info.blend_factor : 1.0f;

      auto &pass = paint_mask_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS |
                     (is_combined ? DRW_STATE_BLEND_MUL : DRW_STATE_BLEND_ALPHA));
      pass.shader_set(res.shaders.uv_paint_mask.get());
      pass.bind_texture("imgTexture", mask_texture_);
      pass.push_constant("color", float4(1.0f, 1.0f, 1.0f, 1.0f));
      pass.push_constant("opacity", opacity);
      pass.push_constant("brush_offset", float2(0.0f));
      pass.push_constant("brush_scale", float2(1.0f));
      pass.draw(shapes.quad_solid.get());
    }
  }

  void draw(GPUFrameBuffer *framebuffer, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    GPU_debug_group_begin("Mesh Edit UVs");

    GPU_framebuffer_bind(framebuffer);
    if (show_mask_ && (mask_mode_ != MASK_OVERLAY_COMBINED)) {
      manager.submit(paint_mask_ps_, view);
    }
    if (show_tiled_image_border_) {
      manager.submit(image_border_ps_, view);
    }
    if (show_wireframe_) {
      manager.submit(wireframe_ps_, view);
    }
    if (show_mesh_analysis_) {
      manager.submit(analysis_ps_, view);
    }
    if (show_face_) {
      manager.submit(faces_ps_, view);
    }
    if (show_uv_edit) {
      manager.submit(edges_ps_, view);
    }
    if (show_face_dots_) {
      manager.submit(facedots_ps_, view);
    }
    if (show_vert_) {
      manager.submit(verts_ps_, view);
    }
    if (show_stencil_) {
      manager.submit(brush_stencil_ps_, view);
    }

    GPU_debug_group_end();
  }

  void draw_on_render(GPUFrameBuffer *framebuffer, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    /* Mask in #MASK_OVERLAY_COMBINED mode renders onto the render framebuffer and modifies the
     * image in scene referred color space. The #MASK_OVERLAY_ALPHACHANNEL renders onto the overlay
     * framebuffer. */
    if (show_mask_ && (mask_mode_ == MASK_OVERLAY_COMBINED)) {
      manager.submit(paint_mask_ps_, view);
    }
  }

 private:
  static OVERLAY_UVLineStyle edit_uv_line_style_from_space_image(const SpaceImage *sima)
  {
    const bool is_uv_editor = sima->mode == SI_MODE_UV;
    if (is_uv_editor) {
      switch (sima->dt_uv) {
        case SI_UVDT_OUTLINE:
          return OVERLAY_UV_LINE_STYLE_OUTLINE;
        case SI_UVDT_BLACK:
          return OVERLAY_UV_LINE_STYLE_BLACK;
        case SI_UVDT_WHITE:
          return OVERLAY_UV_LINE_STYLE_WHITE;
        case SI_UVDT_DASH:
          return OVERLAY_UV_LINE_STYLE_DASH;
        default:
          return OVERLAY_UV_LINE_STYLE_BLACK;
      }
    }
    else {
      return OVERLAY_UV_LINE_STYLE_SHADOW;
    }
  }

  /* TODO(jbakker): the GPU texture should be cached with the mask. */
  void paint_mask_texture_ensure(Mask *mask, const int2 &resolution, const float2 &aspect)
  {
    const int width = resolution.x;
    const int height = floor(float(resolution.y) * (aspect.y / aspect.x));
    float *buffer = static_cast<float *>(MEM_mallocN(sizeof(float) * height * width, __func__));

    MaskRasterHandle *handle = BKE_maskrasterize_handle_new();
    BKE_maskrasterize_handle_init(handle, mask, width, height, true, true, true);
    BKE_maskrasterize_buffer(handle, width, height, buffer);
    BKE_maskrasterize_handle_free(handle);

    mask_texture_.free();
    mask_texture_.ensure_2d(GPU_R16F, int2(width, height), GPU_TEXTURE_USAGE_SHADER_READ, buffer);

    MEM_freeN(buffer);
  }
};

}  // namespace blender::draw::overlay
