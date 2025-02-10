/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Engine for drawing a selection map where the pixels indicate the selection indices.
 */

#include "BLI_math_matrix.h"

#include "BLT_translation.hh"

#include "ED_view3d.hh"

#include "RE_engine.h"

#include "DRW_engine.hh"
#include "DRW_select_buffer.hh"

#include "draw_cache_impl.hh"
#include "draw_common_c.hh"
#include "draw_manager_c.hh"

#include "select_engine.hh"
#include "select_private.hh"

#define SELECT_ENGINE "SELECT_ENGINE"

/* *********** STATIC *********** */

struct SelectEngineData {
  GPUFrameBuffer *framebuffer_select_id;
  GPUTexture *texture_u32;

  SELECTID_Shaders sh_data[GPU_SHADER_CFG_LEN];
  SELECTID_Context context;
};

static SelectEngineData &get_engine_data()
{
  static SelectEngineData data = {};
  return data;
}

/* -------------------------------------------------------------------- */
/** \name Utils
 * \{ */

static void select_engine_framebuffer_setup()
{
  SelectEngineData &e_data = get_engine_data();
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
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
        "select_buf_ids", size[0], size[1], 1, GPU_R32UI, usage, nullptr);
    GPU_framebuffer_texture_attach(e_data.framebuffer_select_id, e_data.texture_u32, 0, 0);

    GPU_framebuffer_check_valid(e_data.framebuffer_select_id, nullptr);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Functions
 * \{ */

static void select_engine_init(void *vedata)
{
  SelectEngineData &e_data = get_engine_data();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  eGPUShaderConfig sh_cfg = draw_ctx->sh_cfg;

  SELECTID_Data *ved = reinterpret_cast<SELECTID_Data *>(vedata);
  SELECTID_Shaders *sh_data = &e_data.sh_data[sh_cfg];

  if (ved->instance == nullptr) {
    ved->instance = new SELECTID_Instance();
  }

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

static void select_cache_init(void *vedata)
{
  SELECTID_Instance &inst = *reinterpret_cast<SELECTID_Data *>(vedata)->instance;
  SelectEngineData &e_data = get_engine_data();

  const DRWContextState *draw_ctx = DRW_context_state_get();
  SELECTID_Shaders *sh = &e_data.sh_data[draw_ctx->sh_cfg];

  if (e_data.context.select_mode == -1) {
    e_data.context.select_mode = select_id_get_object_select_mode(draw_ctx->scene,
                                                                  draw_ctx->obact);
    BLI_assert(e_data.context.select_mode != 0);
  }

  DRWState state = DRW_STATE_DEFAULT;
  if (RV3D_CLIPPING_ENABLED(draw_ctx->v3d, draw_ctx->rv3d)) {
    state |= DRW_STATE_CLIP_PLANES;
  }

  bool retopology_occlusion = RETOPOLOGY_ENABLED(draw_ctx->v3d) && !XRAY_ENABLED(draw_ctx->v3d);
  float retopology_offset = RETOPOLOGY_OFFSET(draw_ctx->v3d);

  /* Note there might be less than 6 planes, but we always compute the 6 of them for simplicity. */
  int clipping_plane_count = RV3D_CLIPPING_ENABLED(draw_ctx->v3d, draw_ctx->rv3d) ? 6 : 0;

  {
    inst.depth_only_ps.init();
    inst.depth_only_ps.state_set(state, clipping_plane_count);
    inst.depth_only = nullptr;
    inst.depth_occlude = nullptr;
    {
      auto &sub = inst.depth_only_ps.sub("DepthOnly");
      sub.shader_set(sh->select_id_uniform);
      sub.push_constant("retopologyOffset", retopology_offset);
      sub.push_constant("select_id", 0);
      inst.depth_only = &sub;
    }
    if (retopology_occlusion) {
      auto &sub = inst.depth_only_ps.sub("Occlusion");
      sub.shader_set(sh->select_id_uniform);
      sub.push_constant("retopologyOffset", 0.0f);
      sub.push_constant("select_id", 0);
      inst.depth_occlude = &sub;
    }

    inst.select_face_ps.init();
    inst.select_face_ps.state_set(state, clipping_plane_count);
    inst.select_face_uniform = nullptr;
    inst.select_face_flat = nullptr;
    if (e_data.context.select_mode & SCE_SELECT_FACE) {
      auto &sub = inst.select_face_ps.sub("Face");
      sub.shader_set(sh->select_id_flat);
      sub.push_constant("retopologyOffset", retopology_offset);
      inst.select_face_flat = &sub;
    }
    else {
      auto &sub = inst.select_face_ps.sub("FaceNoSelect");
      sub.shader_set(sh->select_id_uniform);
      sub.push_constant("select_id", 0);
      sub.push_constant("retopologyOffset", retopology_offset);
      inst.select_face_uniform = &sub;
    }

    inst.select_edge_ps.init();
    inst.select_edge = nullptr;
    if (e_data.context.select_mode & SCE_SELECT_EDGE) {
      auto &sub = inst.select_edge_ps.sub("Sub");
      sub.state_set(state | DRW_STATE_FIRST_VERTEX_CONVENTION, clipping_plane_count);
      sub.shader_set(sh->select_id_flat);
      sub.push_constant("retopologyOffset", retopology_offset);
      inst.select_edge = &sub;
    }

    inst.select_id_vert_ps.init();
    inst.select_vert = nullptr;
    if (e_data.context.select_mode & SCE_SELECT_VERTEX) {
      auto &sub = inst.select_id_vert_ps.sub("Sub");
      sub.state_set(state, clipping_plane_count);
      sub.shader_set(sh->select_id_flat);
      sub.push_constant("sizeVertex", float(2 * G_draw.block.size_vertex));
      sub.push_constant("retopologyOffset", retopology_offset);
      inst.select_vert = &sub;
    }
  }

  /* Create selection data. */
  for (uint sel_id : e_data.context.objects.index_range()) {
    Object *obj_eval = e_data.context.objects[sel_id];
    DrawData *data = DRW_drawdata_ensure(
        &obj_eval->id, &draw_engine_select_type, sizeof(SELECTID_ObjectData), nullptr, nullptr);
    SELECTID_ObjectData *sel_data = reinterpret_cast<SELECTID_ObjectData *>(data);

    data->recalc = 0;
    sel_data->drawn_index = sel_id;
    sel_data->in_pass = false;
    sel_data->is_drawn = false;
  }

  copy_m4_m4(e_data.context.persmat, draw_ctx->rv3d->persmat);
  e_data.context.index_drawn_len = 1;
  select_engine_framebuffer_setup();
  GPU_framebuffer_bind(e_data.framebuffer_select_id);
  GPU_framebuffer_clear_color_depth(e_data.framebuffer_select_id, blender::float4{0.0f}, 1.0f);
}

static void select_cache_populate(void *vedata, Object *ob)
{
  Manager &manager = *DRW_manager_get();
  ObjectRef ob_ref = DRW_object_ref_get(ob);
  SelectEngineData &e_data = get_engine_data();
  SELECTID_Instance &inst = *reinterpret_cast<SELECTID_Data *>(vedata)->instance;
  SELECTID_ObjectData *sel_data = (SELECTID_ObjectData *)DRW_drawdata_get(
      &ob->id, &draw_engine_select_type);

  if (!sel_data || sel_data->is_drawn) {
    if (sel_data) {
      /* Remove data, object is not in array. */
      DrawDataList *drawdata = DRW_drawdatalist_from_id(&ob->id);
      BLI_freelinkN((ListBase *)drawdata, sel_data);
    }

    /* This object is not in the array. It is here to participate in the depth buffer. */
    if (ob->dt >= OB_SOLID) {
      blender::gpu::Batch *geom_faces = DRW_mesh_batch_cache_get_surface(
          *static_cast<Mesh *>(ob->data));

      inst.depth_occlude->draw(geom_faces, manager.resource_handle(ob_ref));
    }
  }
  else if (!sel_data->in_pass) {
    ResourceHandle res_handle = manager.resource_handle(ob_ref);
    const DRWContextState *draw_ctx = DRW_context_state_get();
    ObjectOffsets *ob_offsets = &e_data.context.index_offsets[sel_data->drawn_index];
    uint offset = e_data.context.index_drawn_len;
    select_id_draw_object(inst,
                          draw_ctx->v3d,
                          ob,
                          res_handle,
                          e_data.context.select_mode,
                          offset,
                          &ob_offsets->vert,
                          &ob_offsets->edge,
                          &ob_offsets->face);

    ob_offsets->offset = offset;
    sel_data->in_pass = true;
    e_data.context.index_drawn_len = ob_offsets->vert;
  }
}

static void select_draw_scene(void *vedata)
{
  Manager &manager = *DRW_manager_get();
  SelectEngineData &e_data = get_engine_data();
  SELECTID_Instance &inst = *reinterpret_cast<SELECTID_Data *>(vedata)->instance;

  {
    const DRWContextState *draw_ctx = DRW_context_state_get();
    View::OffsetData offset_data(*draw_ctx->rv3d);
    /* Create view with depth offset */
    const View &view = View::default_get();
    inst.view_faces.sync(view.viewmat(), view.winmat());
    inst.view_edges.sync(view.viewmat(), offset_data.winmat_polygon_offset(view.winmat(), 1.0f));
    inst.view_verts.sync(view.viewmat(), offset_data.winmat_polygon_offset(view.winmat(), 1.1f));
  }

  {
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    GPU_framebuffer_bind(dfbl->depth_only_fb);
    GPU_framebuffer_clear_depth(dfbl->depth_only_fb, 1.0f);
    manager.submit(inst.depth_only_ps, inst.view_faces);
  }

  /* Setup framebuffer */
  GPU_framebuffer_bind(e_data.framebuffer_select_id);

  manager.submit(inst.select_face_ps, inst.view_faces);

  if (e_data.context.select_mode & SCE_SELECT_EDGE) {
    manager.submit(inst.select_edge_ps, inst.view_edges);
  }

  if (e_data.context.select_mode & SCE_SELECT_VERTEX) {
    manager.submit(inst.select_id_vert_ps, inst.view_verts);
  }

  /* Mark objects from the array to later identify which ones are not in the array. */
  for (Object *obj_eval : e_data.context.objects) {
    DrawData *data = DRW_drawdata_ensure(
        &obj_eval->id, &draw_engine_select_type, sizeof(SELECTID_ObjectData), nullptr, nullptr);
    SELECTID_ObjectData *sel_data = reinterpret_cast<SELECTID_ObjectData *>(data);
    sel_data->is_drawn = true;
  }
}

static void select_engine_free()
{
  SelectEngineData &e_data = get_engine_data();
  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    SELECTID_Shaders *sh_data = &e_data.sh_data[sh_data_index];
    GPU_SHADER_FREE_SAFE(sh_data->select_id_flat);
    GPU_SHADER_FREE_SAFE(sh_data->select_id_uniform);
  }

  GPU_TEXTURE_FREE_SAFE(e_data.texture_u32);
  GPU_FRAMEBUFFER_FREE_SAFE(e_data.framebuffer_select_id);
}

static void select_instance_free(void *instance)
{
  delete reinterpret_cast<SELECTID_Instance *>(instance);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Type
 * \{ */

static const DrawEngineDataSize select_data_size = DRW_VIEWPORT_DATA_SIZE(SELECTID_Data);

DrawEngineType draw_engine_select_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("Select ID"),
    /*vedata_size*/ &select_data_size,
    /*engine_init*/ &select_engine_init,
    /*engine_free*/ &select_engine_free,
    /*instance_free*/ select_instance_free,
    /*cache_init*/ &select_cache_init,
    /*cache_populate*/ &select_cache_populate,
    /*cache_finish*/ nullptr,
    /*draw_scene*/ &select_draw_scene,
    /*view_update*/ nullptr,
    /*id_update*/ nullptr,
    /*render_to_image*/ nullptr,
    /*store_metadata*/ nullptr,
};

/* NOTE: currently unused, we may want to register so we can see this when debugging the view. */

RenderEngineType DRW_engine_viewport_select_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ SELECT_ENGINE,
    /*name*/ N_("Select ID"),
    /*flag*/ RE_INTERNAL | RE_USE_STEREO_VIEWPORT | RE_USE_GPU_CONTEXT,
    /*update*/ nullptr,
    /*render*/ nullptr,
    /*render_frame_finish*/ nullptr,
    /*draw*/ nullptr,
    /*bake*/ nullptr,
    /*view_update*/ nullptr,
    /*view_draw*/ nullptr,
    /*update_script_node*/ nullptr,
    /*update_render_passes*/ nullptr,
    /*draw_engine*/ &draw_engine_select_type,
    /*rna_ext*/
    {
        /*data*/ nullptr,
        /*srna*/ nullptr,
        /*call*/ nullptr,
    },
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exposed `select_private.h` functions
 * \{ */

SELECTID_Context *DRW_select_engine_context_get()
{
  SelectEngineData &e_data = get_engine_data();
  return &e_data.context;
}

GPUFrameBuffer *DRW_engine_select_framebuffer_get()
{
  SelectEngineData &e_data = get_engine_data();
  return e_data.framebuffer_select_id;
}

GPUTexture *DRW_engine_select_texture_get()
{
  SelectEngineData &e_data = get_engine_data();
  return e_data.texture_u32;
}

/** \} */

#undef SELECT_ENGINE
