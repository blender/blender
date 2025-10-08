/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Engine for drawing a selection map where the pixels indicate the selection indices.
 */

#include "BKE_editmesh.hh"
#include "BKE_mesh_types.hh"
#include "BLI_math_matrix.h"

#include "BLT_translation.hh"

#include "DEG_depsgraph_query.hh"
#include "DRW_render.hh"
#include "ED_view3d.hh"

#include "RE_engine.h"

#include "DRW_engine.hh"
#include "DRW_select_buffer.hh"

#include "draw_cache_impl.hh"
#include "draw_common_c.hh"
#include "draw_context_private.hh"
#include "draw_manager.hh"
#include "draw_pass.hh"
#include "draw_view_data.hh"

#include "../overlay/overlay_private.hh"

#include "select_engine.hh"

namespace blender::draw::edit_select {

#define USE_CAGE_OCCLUSION

struct Instance : public DrawEngine {
 private:
  PassSimple depth_only_ps = {"depth_only_ps"};
  PassSimple::Sub *depth_only = nullptr;
  PassSimple::Sub *depth_occlude = nullptr;

  PassSimple select_edge_ps = {"select_id_edge_ps"};
  PassSimple::Sub *select_edge = nullptr;

  PassSimple select_id_vert_ps = {"select_id_vert_ps"};
  PassSimple::Sub *select_vert = nullptr;

  PassSimple select_face_ps = {"select_id_face_ps"};
  PassSimple::Sub *select_face_uniform = nullptr;
  PassSimple::Sub *select_face_flat = nullptr;

  View view_faces = {"view_faces"};
  View view_edges = {"view_edges"};
  View view_verts = {"view_verts"};

  UniformArrayBuffer<float4, 6> clip_planes_buf;

  const DRWContext *draw_ctx = nullptr;

 public:
  struct StaticData {
    gpu::FrameBuffer *framebuffer_select_id;
    blender::gpu::Texture *texture_u32;

    struct Shaders {
      /* Depth Pre Pass */
      gpu::Shader *select_id_flat;
      gpu::Shader *select_id_uniform;
    } sh_data[GPU_SHADER_CFG_LEN];

    SELECTID_Context context;

    static StaticData &get()
    {
      static StaticData data = {};
      return data;
    }
  };

  blender::StringRefNull name_get() final
  {
    return "SelectID";
  }

  void init() final
  {
    this->draw_ctx = DRW_context_get();
    StaticData &e_data = StaticData::get();
    GPUShaderConfig sh_cfg = RV3D_CLIPPING_ENABLED(draw_ctx->v3d, draw_ctx->rv3d) ?
                                 GPU_SHADER_CFG_CLIPPED :
                                 GPU_SHADER_CFG_DEFAULT;

    StaticData::Shaders *sh_data = &e_data.sh_data[sh_cfg];

    /* Prepass */
    if (!sh_data->select_id_flat) {
      sh_data->select_id_flat = GPU_shader_create_from_info_name(
          sh_cfg == GPU_SHADER_CFG_CLIPPED ? "select_id_flat_clipped" : "select_id_flat");
    }
    if (!sh_data->select_id_uniform) {
      sh_data->select_id_uniform = GPU_shader_create_from_info_name(
          sh_cfg == GPU_SHADER_CFG_CLIPPED ? "select_id_uniform_clipped" : "select_id_uniform");
    }
  }

  void begin_sync() final
  {
    StaticData &e_data = StaticData::get();
    GPUShaderConfig sh_cfg = RV3D_CLIPPING_ENABLED(draw_ctx->v3d, draw_ctx->rv3d) ?
                                 GPU_SHADER_CFG_CLIPPED :
                                 GPU_SHADER_CFG_DEFAULT;

    StaticData::Shaders *sh = &e_data.sh_data[sh_cfg];

    if (e_data.context.select_mode == -1) {
      e_data.context.select_mode = get_object_select_mode(draw_ctx->scene, draw_ctx->obact);
      BLI_assert(e_data.context.select_mode != 0);
    }

    DRWState state = DRW_STATE_DEFAULT;
    if (RV3D_CLIPPING_ENABLED(draw_ctx->v3d, draw_ctx->rv3d)) {
      state |= DRW_STATE_CLIP_PLANES;
    }

    bool retopology_occlusion = RETOPOLOGY_ENABLED(draw_ctx->v3d) && !XRAY_ENABLED(draw_ctx->v3d);
    float retopology_offset = RETOPOLOGY_OFFSET(draw_ctx->v3d);

    for (int i : IndexRange(6)) {
      clip_planes_buf[i] = float4(0);
    }

    /* Note there might be less than 6 planes, but we always compute the 6 of them for simplicity.
     */
    int clipping_plane_count = RV3D_CLIPPING_ENABLED(draw_ctx->v3d, draw_ctx->rv3d) ? 6 : 0;
    int plane_len = min((RV3D_LOCK_FLAGS(draw_ctx->rv3d) & RV3D_BOXCLIP) ? 4 : 6,
                        clipping_plane_count);

    for (auto i : IndexRange(plane_len)) {
      clip_planes_buf[i] = draw_ctx->rv3d->clip[i];
    }

    clip_planes_buf.push_update();

    {
      depth_only_ps.init();
      depth_only_ps.state_set(state, clipping_plane_count);
      depth_only_ps.bind_ubo(DRW_CLIPPING_UBO_SLOT, clip_planes_buf);
      depth_only = nullptr;
      depth_occlude = nullptr;
      {
        auto &sub = depth_only_ps.sub("DepthOnly");
        sub.shader_set(sh->select_id_uniform);
        sub.push_constant("retopology_offset", retopology_offset);
        sub.push_constant("select_id", 0);
        depth_only = &sub;
      }
      if (retopology_occlusion) {
        auto &sub = depth_only_ps.sub("Occlusion");
        sub.shader_set(sh->select_id_uniform);
        sub.push_constant("retopology_offset", 0.0f);
        sub.push_constant("select_id", 0);
        depth_occlude = &sub;
      }

      select_face_ps.init();
      select_face_ps.state_set(state, clipping_plane_count);
      select_face_ps.bind_ubo(DRW_CLIPPING_UBO_SLOT, clip_planes_buf);
      select_face_uniform = nullptr;
      select_face_flat = nullptr;
      if (e_data.context.select_mode & SCE_SELECT_FACE) {
        auto &sub = select_face_ps.sub("Face");
        sub.shader_set(sh->select_id_flat);
        sub.push_constant("retopology_offset", retopology_offset);
        select_face_flat = &sub;
      }
      else {
        auto &sub = select_face_ps.sub("FaceNoSelect");
        sub.shader_set(sh->select_id_uniform);
        sub.push_constant("select_id", 0);
        sub.push_constant("retopology_offset", retopology_offset);
        select_face_uniform = &sub;
      }

      select_edge_ps.init();
      select_edge_ps.bind_ubo(DRW_CLIPPING_UBO_SLOT, clip_planes_buf);
      select_edge = nullptr;
      if (e_data.context.select_mode & SCE_SELECT_EDGE) {
        auto &sub = select_edge_ps.sub("Sub");
        sub.state_set(state | DRW_STATE_FIRST_VERTEX_CONVENTION, clipping_plane_count);
        sub.shader_set(sh->select_id_flat);
        sub.push_constant("retopology_offset", retopology_offset);
        select_edge = &sub;
      }

      select_id_vert_ps.init();
      select_id_vert_ps.bind_ubo(DRW_CLIPPING_UBO_SLOT, clip_planes_buf);
      select_vert = nullptr;
      if (e_data.context.select_mode & SCE_SELECT_VERTEX) {
        const float vertex_size = U.pixelsize *
                                  blender::draw::overlay::Resources::vertex_size_get();
        auto &sub = select_id_vert_ps.sub("Sub");
        sub.state_set(state, clipping_plane_count);
        sub.shader_set(sh->select_id_flat);
        sub.push_constant("vertex_size", float(2 * vertex_size));
        sub.push_constant("retopology_offset", retopology_offset);
        select_vert = &sub;
      }
    }

    e_data.context.elem_ranges.clear();

    e_data.context.persmat = float4x4(draw_ctx->rv3d->persmat);
    e_data.context.max_index_drawn_len = 1;
    framebuffer_setup();
    GPU_framebuffer_bind(e_data.framebuffer_select_id);
    GPU_framebuffer_clear_color_depth(e_data.framebuffer_select_id, blender::float4{0.0f}, 1.0f);
  }

  ElemIndexRanges edit_mesh_sync(Object *ob,
                                 BMEditMesh *em,
                                 ResourceHandleRange res_handle,
                                 short select_mode,
                                 bool draw_facedot,
                                 const uint initial_index)
  {
    using namespace blender::draw;
    using namespace blender;
    Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(*ob);

    ElemIndexRanges ranges{};
    ranges.total = IndexRange::from_begin_size(initial_index, 0);

    BM_mesh_elem_table_ensure(em->bm, BM_VERT | BM_EDGE | BM_FACE);

    if (select_mode & SCE_SELECT_FACE) {
      ranges.face = alloc_range(ranges.total, em->bm->totface);

      gpu::Batch *geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(mesh);
      PassSimple::Sub *face_sub = select_face_flat;
      face_sub->push_constant("offset", int(ranges.face.start()));
      face_sub->draw(geom_faces, res_handle);

      if (draw_facedot) {
        gpu::Batch *geom_facedots = DRW_mesh_batch_cache_get_facedots_with_select_id(mesh);
        face_sub->draw(geom_facedots, res_handle);
      }
    }
    else {
      if (ob->dt >= OB_SOLID) {
#ifdef USE_CAGE_OCCLUSION
        gpu::Batch *geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(mesh);
#else
        gpu::Batch *geom_faces = DRW_mesh_batch_cache_get_surface(mesh);
#endif
        select_face_uniform->draw(geom_faces, res_handle);
      }
    }

    /* Unlike faces, only draw edges if edge select mode. */
    if (select_mode & SCE_SELECT_EDGE) {
      ranges.edge = alloc_range(ranges.total, em->bm->totedge);

      gpu::Batch *geom_edges = DRW_mesh_batch_cache_get_edges_with_select_id(mesh);
      select_edge->push_constant("offset", int(ranges.edge.start()));
      select_edge->draw(geom_edges, res_handle);
    }

    /* Unlike faces, only verts if vert select mode. */
    if (select_mode & SCE_SELECT_VERTEX) {
      ranges.vert = alloc_range(ranges.total, em->bm->totvert);

      gpu::Batch *geom_verts = DRW_mesh_batch_cache_get_verts_with_select_id(mesh);
      select_vert->push_constant("offset", int(ranges.vert.start()));
      select_vert->draw(geom_verts, res_handle);
    }
    return ranges;
  }

  ElemIndexRanges mesh_sync(Object *ob,
                            ResourceHandleRange res_handle,
                            short select_mode,
                            const uint initial_index)
  {
    using namespace blender::draw;
    using namespace blender;
    Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(*ob);

    ElemIndexRanges ranges{};
    ranges.total = IndexRange::from_begin_size(initial_index, 0);

    gpu::Batch *geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(mesh);
    if (select_mode & SCE_SELECT_FACE) {
      ranges.face = alloc_range(ranges.total, mesh.faces_num);

      select_face_flat->push_constant("offset", int(ranges.face.start()));
      select_face_flat->draw(geom_faces, res_handle);
    }
    else {
      /* Only draw faces to mask out verts, we don't want their selection ID's. */
      select_face_uniform->draw(geom_faces, res_handle);
    }

    if (select_mode & SCE_SELECT_EDGE) {
      ranges.edge = alloc_range(ranges.total, mesh.edges_num);

      gpu::Batch *geom_edges = DRW_mesh_batch_cache_get_edges_with_select_id(mesh);
      select_edge->push_constant("offset", int(ranges.edge.start()));
      select_edge->draw(geom_edges, res_handle);
    }

    if (select_mode & SCE_SELECT_VERTEX) {
      ranges.vert = alloc_range(ranges.total, mesh.verts_num);

      gpu::Batch *geom_verts = DRW_mesh_batch_cache_get_verts_with_select_id(mesh);
      select_vert->push_constant("offset", int(ranges.vert.start()));
      select_vert->draw(geom_verts, res_handle);
    }

    return ranges;
  }

  ElemIndexRanges object_sync(
      View3D *v3d, Object *ob, ResourceHandleRange res_handle, short select_mode, uint index_start)
  {
    BLI_assert_msg(index_start > 0, "Index 0 is reserved for no selection");

    switch (ob->type) {
      case OB_MESH: {
        const bool is_editmode = ob->mode == OB_MODE_EDIT;
        /* NOTE: it's important to get the edit-mesh before modifiers have been applied
         * because the evaluated mesh may not have an edit-mesh, see #138715.
         * Match edit-mesh access from #mesh_render_data_create. */
        const Mesh *orig_edit_mesh = is_editmode ? BKE_object_get_pre_modified_mesh(ob) : nullptr;
        BMEditMesh *em = (orig_edit_mesh) ? orig_edit_mesh->runtime->edit_mesh.get() : nullptr;

        if (em) {
          bool draw_facedot = check_ob_drawface_dot(select_mode, v3d, eDrawType(ob->dt));
          return edit_mesh_sync(ob, em, res_handle, select_mode, draw_facedot, index_start);
        }
        return mesh_sync(ob, res_handle, select_mode, index_start);
      }
      case OB_CURVES_LEGACY:
      case OB_SURF:
        break;
    }
    BLI_assert_unreachable();
    return ElemIndexRanges{};
  }

  void object_sync(ObjectRef &ob_ref, Manager &manager) final
  {
    Object *ob = ob_ref.object;
    StaticData &e_data = StaticData::get();
    SELECTID_Context &sel_ctx = e_data.context;

    if (!sel_ctx.objects.contains(ob)) {
      if (ob->dt >= OB_SOLID) {
        /* This object is not selectable. It is here to participate in occlusion.
         * This is the case in retopology mode. */
        blender::gpu::Batch *geom_faces = DRW_mesh_batch_cache_get_surface(
            DRW_object_get_data_for_drawing<Mesh>(*ob));

        depth_occlude->draw(geom_faces, manager.unique_handle(ob_ref));
      }
      return;
    }

    /* Only sync selectable object once.
     * This can happen in retopology mode where there is two sync loop. */
    sel_ctx.elem_ranges.lookup_or_add_cb(ob, [&]() {
      ResourceHandleRange res_handle = manager.unique_handle(ob_ref);
      ElemIndexRanges elem_ranges = object_sync(
          draw_ctx->v3d, ob, res_handle, sel_ctx.select_mode, sel_ctx.max_index_drawn_len);
      sel_ctx.max_index_drawn_len = elem_ranges.total.one_after_last();
      return elem_ranges;
    });
  }

  void end_sync() final {}

  void draw(Manager &manager) final
  {
    StaticData &e_data = StaticData::get();

    DRW_submission_start();
    {
      View::OffsetData offset_data(*draw_ctx->rv3d);
      /* Create view with depth offset */
      const View &view = View::default_get();
      view_faces.sync(view.viewmat(), view.winmat());
      view_edges.sync(view.viewmat(), offset_data.winmat_polygon_offset(view.winmat(), 1.0f));
      view_verts.sync(view.viewmat(), offset_data.winmat_polygon_offset(view.winmat(), 1.1f));
    }

    {
      DefaultFramebufferList *dfbl = draw_ctx->viewport_framebuffer_list_get();
      GPU_framebuffer_bind(dfbl->depth_only_fb);
      GPU_framebuffer_clear_depth(dfbl->depth_only_fb, 1.0f);
      manager.submit(depth_only_ps, view_faces);
    }

    /* Setup framebuffer */
    GPU_framebuffer_bind(e_data.framebuffer_select_id);

    manager.submit(select_face_ps, view_faces);

    if (e_data.context.select_mode & SCE_SELECT_EDGE) {
      manager.submit(select_edge_ps, view_edges);
    }

    if (e_data.context.select_mode & SCE_SELECT_VERTEX) {
      manager.submit(select_id_vert_ps, view_verts);
    }
    DRW_submission_end();
  }

 private:
  void framebuffer_setup()
  {
    StaticData &e_data = StaticData::get();
    DefaultTextureList *dtxl = draw_ctx->viewport_texture_list_get();
    int size[2];
    size[0] = GPU_texture_width(dtxl->depth);
    size[1] = GPU_texture_height(dtxl->depth);

    if (e_data.framebuffer_select_id == nullptr) {
      e_data.framebuffer_select_id = GPU_framebuffer_create("framebuffer_select_id");
    }

    if ((e_data.texture_u32 != nullptr) && ((GPU_texture_width(e_data.texture_u32) != size[0]) ||
                                            (GPU_texture_height(e_data.texture_u32) != size[1])))
    {
      GPU_texture_free(e_data.texture_u32);
      e_data.texture_u32 = nullptr;
    }

    /* Make sure the depth texture is attached.
     * It may disappear when loading another Blender session. */
    GPU_framebuffer_texture_attach(e_data.framebuffer_select_id, dtxl->depth, 0, 0);

    if (e_data.texture_u32 == nullptr) {
      eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
      e_data.texture_u32 = GPU_texture_create_2d(
          "select_buf_ids", size[0], size[1], 1, gpu::TextureFormat::UINT_32, usage, nullptr);
      GPU_framebuffer_texture_attach(e_data.framebuffer_select_id, e_data.texture_u32, 0, 0);

      GPU_framebuffer_check_valid(e_data.framebuffer_select_id, nullptr);
    }
  }

  short get_object_select_mode(Scene *scene, Object *ob)
  {
    short select_mode = 0;
    if (ob->mode & (OB_MODE_WEIGHT_PAINT | OB_MODE_VERTEX_PAINT | OB_MODE_TEXTURE_PAINT)) {
      /* In order to sample flat colors for vertex weights / texture-paint / vertex-paint
       * we need to be in SCE_SELECT_FACE mode so select_cache_init() correctly sets up
       * a shgroup with select_id_flat.
       * Note this is not working correctly for vertex-paint (yet), but has been discussed
       * in #66645 and there is a solution by @mano-wii in P1032.
       * So OB_MODE_VERTEX_PAINT is already included here [required for P1032 I guess]. */
      Mesh *me_orig = static_cast<Mesh *>(DEG_get_original(ob)->data);
      if (me_orig->editflag & ME_EDIT_PAINT_VERT_SEL) {
        select_mode = SCE_SELECT_VERTEX;
      }
      else {
        select_mode = SCE_SELECT_FACE;
      }
    }
    else {
      select_mode = scene->toolsettings->selectmode;
    }

    return select_mode;
  }

  bool check_ob_drawface_dot(short select_mode, const View3D *v3d, eDrawType dt)
  {
    if (select_mode & SCE_SELECT_FACE) {
      if ((dt < OB_SOLID) || XRAY_FLAG_ENABLED(v3d)) {
        return true;
      }
      if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_DOT) {
        return true;
      }
    }
    return false;
  }

  /* Return a new range if size `n` after `total_range` and grow `total_range` by the same amount.
   */
  IndexRange alloc_range(IndexRange &total_range, uint size)
  {
    const IndexRange indices = total_range.after(size);
    total_range = IndexRange::from_begin_size(total_range.start(), total_range.size() + size);
    return indices;
  }
};

DrawEngine *Engine::create_instance()
{
  return new Instance();
}

void Engine::free_static()
{
  Instance::StaticData &e_data = Instance::StaticData::get();
  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    Instance::StaticData::Shaders *sh_data = &e_data.sh_data[sh_data_index];
    GPU_SHADER_FREE_SAFE(sh_data->select_id_flat);
    GPU_SHADER_FREE_SAFE(sh_data->select_id_uniform);
  }

  GPU_TEXTURE_FREE_SAFE(e_data.texture_u32);
  GPU_FRAMEBUFFER_FREE_SAFE(e_data.framebuffer_select_id);
}

}  // namespace blender::draw::edit_select

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exposed `select_private.h` functions
 * \{ */

using namespace blender::draw::edit_select;

SELECTID_Context *DRW_select_engine_context_get()
{
  Instance::StaticData &e_data = Instance::StaticData::get();
  return &e_data.context;
}

blender::gpu::FrameBuffer *DRW_engine_select_framebuffer_get()
{
  Instance::StaticData &e_data = Instance::StaticData::get();
  return e_data.framebuffer_select_id;
}

blender::gpu::Texture *DRW_engine_select_texture_get()
{
  Instance::StaticData &e_data = Instance::StaticData::get();
  return e_data.texture_u32;
}

/** \} */
