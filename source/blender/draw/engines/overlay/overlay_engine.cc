/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Engine for drawing a selection map where the pixels indicate the selection indices.
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DEG_depsgraph_query.h"

#include "ED_view3d.h"

#include "UI_interface.h"

#include "BKE_duplilist.h"
#include "BKE_object.h"
#include "BKE_paint.h"

#include "GPU_capabilities.h"

#include "DNA_space_types.h"

#include "draw_manager.hh"
#include "overlay_next_instance.hh"

#include "overlay_engine.h"
#include "overlay_private.hh"

using namespace blender::draw;

using Instance = blender::draw::overlay::Instance;

/* -------------------------------------------------------------------- */
/** \name Engine Callbacks
 * \{ */

static void OVERLAY_engine_init(void *vedata)
{
  OVERLAY_Data *data = static_cast<OVERLAY_Data *>(vedata);
  OVERLAY_StorageList *stl = data->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const RegionView3D *rv3d = draw_ctx->rv3d;
  const View3D *v3d = draw_ctx->v3d;
  const Scene *scene = draw_ctx->scene;
  const ToolSettings *ts = scene->toolsettings;

  if (!stl->pd) {
    /* Allocate transient pointers. */
    stl->pd = static_cast<OVERLAY_PrivateData *>(MEM_callocN(sizeof(*stl->pd), __func__));
  }

  /* Allocate instance. */
  if (data->instance == nullptr) {
    data->instance = new Instance(select::SelectionType::DISABLED);
  }

  OVERLAY_PrivateData *pd = stl->pd;
  pd->space_type = v3d != nullptr ? int(SPACE_VIEW3D) : draw_ctx->space_data->spacetype;

  if (pd->space_type == SPACE_IMAGE) {
    const SpaceImage *sima = (SpaceImage *)draw_ctx->space_data;
    pd->hide_overlays = (sima->overlay.flag & SI_OVERLAY_SHOW_OVERLAYS) == 0;
    pd->clipping_state = DRWState(0);
    OVERLAY_grid_init(data);
    OVERLAY_edit_uv_init(data);
    return;
  }
  if (pd->space_type == SPACE_NODE) {
    pd->hide_overlays = true;
    pd->clipping_state = DRWState(0);
    return;
  }

  pd->hide_overlays = (v3d->flag2 & V3D_HIDE_OVERLAYS) != 0;
  pd->ctx_mode = CTX_data_mode_enum_ex(
      draw_ctx->object_edit, draw_ctx->obact, draw_ctx->object_mode);

  if (!pd->hide_overlays) {
    pd->overlay = v3d->overlay;
    pd->v3d_flag = v3d->flag;
    pd->v3d_gridflag = v3d->gridflag;
  }
  else {
    memset(&pd->overlay, 0, sizeof(pd->overlay));
    pd->v3d_flag = 0;
    pd->v3d_gridflag = 0;
    pd->overlay.flag = V3D_OVERLAY_HIDE_TEXT | V3D_OVERLAY_HIDE_MOTION_PATHS |
                       V3D_OVERLAY_HIDE_BONES | V3D_OVERLAY_HIDE_OBJECT_XTRAS |
                       V3D_OVERLAY_HIDE_OBJECT_ORIGINS;
    pd->overlay.wireframe_threshold = v3d->overlay.wireframe_threshold;
    pd->overlay.wireframe_opacity = v3d->overlay.wireframe_opacity;
  }

  if (v3d->shading.type == OB_WIRE) {
    pd->overlay.flag |= V3D_OVERLAY_WIREFRAMES;
  }

  if (ts->sculpt) {
    if (!(v3d->overlay.flag & int(V3D_OVERLAY_SCULPT_SHOW_FACE_SETS))) {
      pd->overlay.sculpt_mode_face_sets_opacity = 0.0f;
    }
    if (!(v3d->overlay.flag & int(V3D_OVERLAY_SCULPT_SHOW_MASK))) {
      pd->overlay.sculpt_mode_mask_opacity = 0.0f;
    }
  }

  pd->use_in_front = (v3d->shading.type <= OB_SOLID) ||
                     BKE_scene_uses_blender_workbench(draw_ctx->scene);
  pd->wireframe_mode = (v3d->shading.type == OB_WIRE);
  pd->clipping_state = RV3D_CLIPPING_ENABLED(v3d, rv3d) ? DRW_STATE_CLIP_PLANES : DRWState(0);
  pd->xray_opacity = XRAY_ALPHA(v3d);
  pd->xray_enabled = XRAY_ACTIVE(v3d);
  pd->xray_enabled_and_not_wire = pd->xray_enabled && v3d->shading.type > OB_WIRE;
  pd->clear_in_front = (v3d->shading.type != OB_SOLID);
  pd->cfra = DEG_get_ctime(draw_ctx->depsgraph);

  OVERLAY_antialiasing_init(data);

  switch (stl->pd->ctx_mode) {
    case CTX_MODE_EDIT_MESH:
      OVERLAY_edit_mesh_init(data);
      break;
    case CTX_MODE_EDIT_CURVES:
      OVERLAY_edit_curves_init(data);
      break;
    default:
      /* Nothing to do. */
      break;
  }
  OVERLAY_facing_init(data);
  OVERLAY_grid_init(data);
  OVERLAY_image_init(data);
  OVERLAY_outline_init(data);
  OVERLAY_wireframe_init(data);
  OVERLAY_paint_init(data);
}

static void OVERLAY_cache_init(void *vedata)
{
  OVERLAY_Data *data = static_cast<OVERLAY_Data *>(vedata);
  OVERLAY_StorageList *stl = data->stl;
  OVERLAY_PrivateData *pd = stl->pd;

  if (pd->space_type == SPACE_IMAGE) {
    OVERLAY_background_cache_init(data);
    OVERLAY_grid_cache_init(data);
    OVERLAY_edit_uv_cache_init(data);
    return;
  }
  if (pd->space_type == SPACE_NODE) {
    OVERLAY_background_cache_init(data);
    return;
  }

  switch (pd->ctx_mode) {
    case CTX_MODE_EDIT_MESH: {
      OVERLAY_edit_mesh_cache_init(data);
      /* `pd->edit_mesh.flag` is valid after calling `OVERLAY_edit_mesh_cache_init`. */
      const bool draw_edit_weights = (pd->edit_mesh.flag & V3D_OVERLAY_EDIT_WEIGHT);
      if (draw_edit_weights) {
        OVERLAY_paint_cache_init(data);
      }
      break;
    }
    case CTX_MODE_EDIT_SURFACE:
    case CTX_MODE_EDIT_CURVE:
      OVERLAY_edit_curve_cache_init(data);
      break;
    case CTX_MODE_EDIT_TEXT:
      OVERLAY_edit_text_cache_init(data);
      break;
    case CTX_MODE_EDIT_ARMATURE:
      break;
    case CTX_MODE_EDIT_METABALL:
      break;
    case CTX_MODE_EDIT_LATTICE:
      OVERLAY_edit_lattice_cache_init(data);
      break;
    case CTX_MODE_PAINT_GREASE_PENCIL:
    case CTX_MODE_EDIT_GREASE_PENCIL:
      OVERLAY_edit_grease_pencil_cache_init(data);
      break;
    case CTX_MODE_PARTICLE:
      OVERLAY_edit_particle_cache_init(data);
      break;
    case CTX_MODE_POSE:
    case CTX_MODE_PAINT_WEIGHT:
    case CTX_MODE_PAINT_VERTEX:
    case CTX_MODE_PAINT_TEXTURE:
      OVERLAY_paint_cache_init(data);
      break;
    case CTX_MODE_SCULPT:
      OVERLAY_sculpt_cache_init(data);
      break;
    case CTX_MODE_EDIT_GPENCIL_LEGACY:
      OVERLAY_edit_gpencil_legacy_cache_init(data);
      break;
    case CTX_MODE_PAINT_GPENCIL_LEGACY:
    case CTX_MODE_SCULPT_GPENCIL_LEGACY:
    case CTX_MODE_VERTEX_GPENCIL_LEGACY:
    case CTX_MODE_WEIGHT_GPENCIL_LEGACY:
      OVERLAY_edit_gpencil_legacy_cache_init(data);
      break;
    case CTX_MODE_EDIT_CURVES:
      OVERLAY_edit_curves_cache_init(data);
      break;
    case CTX_MODE_SCULPT_CURVES:
      OVERLAY_sculpt_curves_cache_init(data);
      break;
    case CTX_MODE_EDIT_POINT_CLOUD:
    case CTX_MODE_OBJECT:
      break;
    default:
      BLI_assert_msg(0, "Draw mode invalid");
      break;
  }
  OVERLAY_antialiasing_cache_init(data);
  OVERLAY_armature_cache_init(data);
  OVERLAY_viewer_attribute_cache_init(data);
  OVERLAY_background_cache_init(data);
  OVERLAY_fade_cache_init(data);
  OVERLAY_mode_transfer_cache_init(data);
  OVERLAY_extra_cache_init(data);
  OVERLAY_facing_cache_init(data);
  OVERLAY_gpencil_legacy_cache_init(data);
  OVERLAY_grid_cache_init(data);
  OVERLAY_image_cache_init(data);
  OVERLAY_metaball_cache_init(data);
  OVERLAY_motion_path_cache_init(data);
  OVERLAY_outline_cache_init(data);
  OVERLAY_particle_cache_init(data);
  OVERLAY_wireframe_cache_init(data);
  OVERLAY_volume_cache_init(data);
}

BLI_INLINE OVERLAY_DupliData *OVERLAY_duplidata_get(Object *ob, void *vedata, bool *do_init)
{
  OVERLAY_DupliData **dupli_data = (OVERLAY_DupliData **)DRW_duplidata_get(vedata);
  *do_init = false;
  if (!ELEM(ob->type, OB_MESH, OB_SURF, OB_LATTICE, OB_CURVES_LEGACY, OB_FONT)) {
    return nullptr;
  }

  if (dupli_data) {
    if (*dupli_data == nullptr) {
      *dupli_data = static_cast<OVERLAY_DupliData *>(
          MEM_callocN(sizeof(OVERLAY_DupliData), __func__));
      *do_init = true;
    }
    else if ((*dupli_data)->base_flag != ob->base_flag) {
      /* Select state might have change, reinitialize. */
      *do_init = true;
    }
    return *dupli_data;
  }
  return nullptr;
}

static bool overlay_object_is_edit_mode(const OVERLAY_PrivateData *pd, const Object *ob)
{
  if (DRW_object_is_in_edit_mode(ob)) {
    /* Also check for context mode as the object mode is not 100% reliable. (see #72490) */
    switch (ob->type) {
      case OB_MESH:
        return pd->ctx_mode == CTX_MODE_EDIT_MESH;
      case OB_ARMATURE:
        return pd->ctx_mode == CTX_MODE_EDIT_ARMATURE;
      case OB_CURVES_LEGACY:
        return pd->ctx_mode == CTX_MODE_EDIT_CURVE;
      case OB_SURF:
        return pd->ctx_mode == CTX_MODE_EDIT_SURFACE;
      case OB_LATTICE:
        return pd->ctx_mode == CTX_MODE_EDIT_LATTICE;
      case OB_MBALL:
        return pd->ctx_mode == CTX_MODE_EDIT_METABALL;
      case OB_FONT:
        return pd->ctx_mode == CTX_MODE_EDIT_TEXT;
      case OB_CURVES:
        return pd->ctx_mode == CTX_MODE_EDIT_CURVES;
      case OB_POINTCLOUD:
      case OB_VOLUME:
        /* No edit mode yet. */
        return false;
      case OB_GREASE_PENCIL:
        return pd->ctx_mode == CTX_MODE_EDIT_GREASE_PENCIL;
    }
  }
  return false;
}

static bool overlay_should_fade_object(Object *ob, Object *active_object)
{
  if (!active_object || !ob) {
    return false;
  }

  if (ELEM(active_object->mode, OB_MODE_OBJECT, OB_MODE_POSE)) {
    return false;
  }

  if ((active_object->mode & ob->mode) != 0) {
    return false;
  }

  return true;
}

static void OVERLAY_cache_populate(void *vedata, Object *ob)
{
  OVERLAY_Data *data = static_cast<OVERLAY_Data *>(vedata);
  OVERLAY_PrivateData *pd = data->stl->pd;

  if (pd->space_type == SPACE_IMAGE) {
    return;
  }

  const DRWContextState *draw_ctx = DRW_context_state_get();
  DupliObject *dupli_object = DRW_object_get_dupli(ob);
  Object *dupli_parent = DRW_object_get_dupli_parent(ob);
  const bool is_select = DRW_state_is_select();
  const bool renderable = DRW_object_is_renderable(ob);
  const bool is_preview = dupli_object != nullptr &&
                          dupli_object->preview_base_geometry != nullptr;
  const bool in_pose_mode = ob->type == OB_ARMATURE && OVERLAY_armature_is_pose_mode(ob, draw_ctx);
  const bool in_edit_mode = overlay_object_is_edit_mode(pd, ob);
  const bool is_instance = (ob->base_flag & BASE_FROM_DUPLI);
  const bool instance_parent_in_edit_mode = is_instance ?
                                                overlay_object_is_edit_mode(
                                                    pd, DRW_object_get_dupli_parent(ob)) :
                                                false;
  const bool in_particle_edit_mode = (ob->mode == OB_MODE_PARTICLE_EDIT) &&
                                     (pd->ctx_mode == CTX_MODE_PARTICLE);
  const bool in_paint_mode = (ob == draw_ctx->obact) &&
                             (draw_ctx->object_mode & OB_MODE_ALL_PAINT);
  const bool in_sculpt_curve_mode = (ob == draw_ctx->obact ||
                                     (is_preview && dupli_parent == draw_ctx->obact)) &&
                                    (draw_ctx->object_mode & OB_MODE_SCULPT_CURVES);
  const bool in_sculpt_mode = (ob == draw_ctx->obact) && (ob->sculpt != nullptr) &&
                              (ob->sculpt->mode_type == OB_MODE_SCULPT);
  const bool has_surface = ELEM(ob->type,
                                OB_MESH,
                                OB_CURVES_LEGACY,
                                OB_SURF,
                                OB_FONT,
                                OB_GPENCIL_LEGACY,
                                OB_CURVES,
                                OB_POINTCLOUD,
                                OB_VOLUME,
                                OB_GREASE_PENCIL);
  const bool draw_surface = (ob->dt >= OB_WIRE) && (renderable || (ob->dt == OB_WIRE));
  const bool draw_facing = draw_surface && (pd->overlay.flag & V3D_OVERLAY_FACE_ORIENTATION) &&
                           !is_select;
  const bool draw_fade = draw_surface && (pd->overlay.flag & V3D_OVERLAY_FADE_INACTIVE) &&
                         overlay_should_fade_object(ob, draw_ctx->obact);
  const bool draw_mode_transfer = draw_surface;
  const bool draw_bones = (pd->overlay.flag & V3D_OVERLAY_HIDE_BONES) == 0;
  const bool draw_wires = draw_surface && has_surface &&
                          (pd->wireframe_mode || !pd->hide_overlays);
  const bool draw_outlines = !in_edit_mode && !in_paint_mode && !in_sculpt_curve_mode &&
                             renderable && has_surface && !instance_parent_in_edit_mode &&
                             (pd->v3d_flag & V3D_SELECT_OUTLINE) &&
                             (ob->base_flag & BASE_SELECTED);
  const bool draw_bone_selection = (ob->type == OB_MESH) && pd->armature.do_pose_fade_geom &&
                                   !is_select;
  const bool draw_edit_weights = in_edit_mode && (pd->edit_mesh.flag & V3D_OVERLAY_EDIT_WEIGHT);
  const bool draw_extras =
      (!pd->hide_overlays) &&
      (((pd->overlay.flag & V3D_OVERLAY_HIDE_OBJECT_XTRAS) == 0) ||
       /* Show if this is the camera we're looking through since it's useful for selecting. */
       ((draw_ctx->rv3d->persp == RV3D_CAMOB) && ((ID *)draw_ctx->v3d->camera == ob->id.orig_id)));

  const bool draw_motion_paths = (pd->overlay.flag & V3D_OVERLAY_HIDE_MOTION_PATHS) == 0;

  bool do_init;
  OVERLAY_DupliData *dupli = OVERLAY_duplidata_get(ob, vedata, &do_init);

  if (draw_fade) {
    OVERLAY_fade_cache_populate(data, ob);
  }
  if (draw_facing) {
    OVERLAY_facing_cache_populate(data, ob);
  }
  if (draw_mode_transfer) {
    OVERLAY_mode_transfer_cache_populate(data, ob);
  }
  if (draw_wires) {
    OVERLAY_wireframe_cache_populate(data, ob, dupli, do_init);
  }
  if (draw_outlines) {
    OVERLAY_outline_cache_populate(data, ob, dupli, do_init);
  }
  if (draw_bone_selection) {
    OVERLAY_pose_cache_populate(data, ob);
  }

  if (pd->overlay.flag & V3D_OVERLAY_VIEWER_ATTRIBUTE) {
    if (is_preview) {
      OVERLAY_viewer_attribute_cache_populate(data, ob);
    }
  }

  if (ob->type == OB_VOLUME) {
    OVERLAY_volume_cache_populate(data, ob);
  }

  if (in_edit_mode && !pd->hide_overlays) {
    switch (ob->type) {
      case OB_MESH:
        OVERLAY_edit_mesh_cache_populate(data, ob);
        if (draw_edit_weights) {
          OVERLAY_paint_weight_cache_populate(data, ob);
        }
        break;
      case OB_ARMATURE:
        if (draw_bones) {
          OVERLAY_edit_armature_cache_populate(data, ob);
        }
        break;
      case OB_CURVES_LEGACY:
        OVERLAY_edit_curve_cache_populate(data, ob);
        break;
      case OB_SURF:
        OVERLAY_edit_surf_cache_populate(data, ob);
        break;
      case OB_LATTICE:
        OVERLAY_edit_lattice_cache_populate(data, ob);
        break;
      case OB_MBALL:
        OVERLAY_edit_metaball_cache_populate(data, ob);
        break;
      case OB_FONT:
        OVERLAY_edit_text_cache_populate(data, ob);
        break;
      case OB_CURVES:
        OVERLAY_edit_curves_cache_populate(data, ob);
        break;
      case OB_GREASE_PENCIL:
        if (U.experimental.use_grease_pencil_version3) {
          OVERLAY_edit_grease_pencil_cache_populate(data, ob);
        }
        break;
    }
  }
  else if (in_pose_mode && draw_bones) {
    OVERLAY_pose_armature_cache_populate(data, ob);
  }
  else if (in_paint_mode && !pd->hide_overlays) {
    switch (draw_ctx->object_mode) {
      case OB_MODE_VERTEX_PAINT:
        OVERLAY_paint_vertex_cache_populate(data, ob);
        break;
      case OB_MODE_WEIGHT_PAINT:
        OVERLAY_paint_weight_cache_populate(data, ob);
        break;
      case OB_MODE_TEXTURE_PAINT:
        OVERLAY_paint_texture_cache_populate(data, ob);
        break;
      default:
        break;
    }
  }
  else if (in_particle_edit_mode) {
    OVERLAY_edit_particle_cache_populate(data, ob);
  }

  if (in_sculpt_mode) {
    OVERLAY_sculpt_cache_populate(data, ob);
  }
  else if (in_sculpt_curve_mode) {
    OVERLAY_sculpt_curves_cache_populate(data, ob);
  }

  if (draw_motion_paths) {
    OVERLAY_motion_path_cache_populate(data, ob);
  }

  if (!pd->hide_overlays) {
    switch (ob->type) {
      case OB_ARMATURE:
        if (draw_bones && (is_select || (!in_edit_mode && !in_pose_mode))) {
          OVERLAY_armature_cache_populate(data, ob);
        }
        break;
      case OB_MBALL:
        if (!in_edit_mode) {
          OVERLAY_metaball_cache_populate(data, ob);
        }
        break;
      case OB_GPENCIL_LEGACY:
        OVERLAY_gpencil_legacy_cache_populate(data, ob);
        break;
    }
  }
  /* Non-Meshes */
  if (draw_extras) {
    switch (ob->type) {
      case OB_EMPTY:
        OVERLAY_empty_cache_populate(data, ob);
        break;
      case OB_LAMP:
        OVERLAY_light_cache_populate(data, ob);
        break;
      case OB_CAMERA:
        OVERLAY_camera_cache_populate(data, ob);
        break;
      case OB_SPEAKER:
        OVERLAY_speaker_cache_populate(data, ob);
        break;
      case OB_LIGHTPROBE:
        OVERLAY_lightprobe_cache_populate(data, ob);
        break;
      case OB_LATTICE: {
        /* Unlike the other types above, lattices actually have a bounding box defined, so hide the
         * lattice wires if only the bounding-box is requested. */
        if (ob->dt > OB_BOUNDBOX) {
          OVERLAY_lattice_cache_populate(data, ob);
        }
        break;
      }
    }
  }

  if (!BLI_listbase_is_empty(&ob->particlesystem)) {
    OVERLAY_particle_cache_populate(data, ob);
  }

  /* Relationship, object center, bounding-box... etc. */
  if (!pd->hide_overlays) {
    OVERLAY_extra_cache_populate(data, ob);
  }

  if (dupli) {
    dupli->base_flag = ob->base_flag;
  }
}

static void OVERLAY_cache_finish(void *vedata)
{
  OVERLAY_Data *data = static_cast<OVERLAY_Data *>(vedata);
  OVERLAY_PrivateData *pd = data->stl->pd;

  if (ELEM(pd->space_type, SPACE_IMAGE)) {
    OVERLAY_edit_uv_cache_finish(data);
    return;
  }
  if (ELEM(pd->space_type, SPACE_NODE)) {
    return;
  }

  /* TODO(fclem): Only do this when really needed. */
  {
    /* HACK we allocate the in front depth here to avoid the overhead when if is not needed. */
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

    DRW_texture_ensure_fullscreen_2d(
        &dtxl->depth_in_front, GPU_DEPTH24_STENCIL8, DRWTextureFlag(0));

    GPU_framebuffer_ensure_config(
        &dfbl->in_front_fb,
        {GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front), GPU_ATTACHMENT_TEXTURE(dtxl->color)});
  }

  OVERLAY_mode_transfer_cache_finish(data);
  OVERLAY_antialiasing_cache_finish(data);
  OVERLAY_armature_cache_finish(data);
  OVERLAY_image_cache_finish(data);
}

static void OVERLAY_draw_scene(void *vedata)
{
  OVERLAY_Data *data = static_cast<OVERLAY_Data *>(vedata);
  OVERLAY_PrivateData *pd = data->stl->pd;
  OVERLAY_FramebufferList *fbl = data->fbl;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  /* Needs to be done first as it modifies the scene color and depth buffer. */
  if (pd->space_type == SPACE_VIEW3D) {
    OVERLAY_image_scene_background_draw(data);
  }

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(dfbl->overlay_only_fb);
    /* Don't clear background for the node editor. The node editor draws the background and we
     * need to mask out the image from the already drawn overlay color buffer. */
    if (pd->space_type != SPACE_NODE) {
      const float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      GPU_framebuffer_clear_color(dfbl->overlay_only_fb, clear_col);
    }
  }

  if (pd->space_type == SPACE_IMAGE) {
    OVERLAY_background_draw(data);
    OVERLAY_grid_draw(data);
    if (DRW_state_is_fbo()) {
      GPU_framebuffer_bind(dfbl->overlay_fb);
    }
    OVERLAY_edit_uv_draw(data);
    return;
  }
  if (pd->space_type == SPACE_NODE) {
    OVERLAY_background_draw(data);
    return;
  }

  OVERLAY_image_background_draw(data);
  OVERLAY_background_draw(data);

  OVERLAY_antialiasing_start(data);

  DRW_view_set_active(nullptr);

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_color_only_fb);
  }

  OVERLAY_outline_draw(data);
  OVERLAY_xray_depth_copy(data);

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_default_fb);
  }

  OVERLAY_image_draw(data);
  OVERLAY_fade_draw(data);
  OVERLAY_facing_draw(data);
  OVERLAY_mode_transfer_draw(data);
  OVERLAY_extra_blend_draw(data);
  OVERLAY_volume_draw(data);

  /* These overlays are drawn here to avoid artifacts with wire-frame opacity. */
  switch (pd->ctx_mode) {
    case CTX_MODE_SCULPT:
      OVERLAY_sculpt_draw(data);
      break;
    case CTX_MODE_SCULPT_CURVES:
      OVERLAY_sculpt_curves_draw(data);
      break;
    case CTX_MODE_EDIT_MESH:
    case CTX_MODE_POSE:
    case CTX_MODE_PAINT_WEIGHT:
    case CTX_MODE_PAINT_VERTEX:
    case CTX_MODE_PAINT_TEXTURE:
      OVERLAY_paint_draw(data);
      break;
    default:
      break;
  }

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_line_fb);
  }

  if (pd->ctx_mode == CTX_MODE_SCULPT_CURVES) {
    OVERLAY_sculpt_curves_draw_wires(data);
  }

  OVERLAY_wireframe_draw(data);
  OVERLAY_armature_draw(data);
  OVERLAY_particle_draw(data);
  OVERLAY_metaball_draw(data);
  OVERLAY_gpencil_legacy_draw(data);
  OVERLAY_extra_draw(data);
  if (pd->overlay.flag & V3D_OVERLAY_VIEWER_ATTRIBUTE) {
    OVERLAY_viewer_attribute_draw(data);
  }

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_color_only_fb);
  }

  OVERLAY_xray_fade_draw(data);
  OVERLAY_grid_draw(data);

  OVERLAY_xray_depth_infront_copy(data);

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_in_front_fb);
  }

  OVERLAY_fade_infront_draw(data);
  OVERLAY_facing_infront_draw(data);
  OVERLAY_mode_transfer_infront_draw(data);

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_line_in_front_fb);
  }

  OVERLAY_wireframe_in_front_draw(data);
  OVERLAY_armature_in_front_draw(data);
  OVERLAY_extra_in_front_draw(data);
  OVERLAY_metaball_in_front_draw(data);

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_color_only_fb);
  }

  OVERLAY_image_in_front_draw(data);
  OVERLAY_motion_path_draw(data);
  OVERLAY_extra_centers_draw(data);

  if (DRW_state_is_select() || DRW_state_is_depth()) {
    /* Edit modes have their own selection code. */
    return;
  }

  /* Functions after this point can change FBO freely. */

  switch (pd->ctx_mode) {
    case CTX_MODE_EDIT_MESH:
      OVERLAY_edit_mesh_draw(data);
      break;
    case CTX_MODE_EDIT_SURFACE:
    case CTX_MODE_EDIT_CURVE:
      OVERLAY_edit_curve_draw(data);
      break;
    case CTX_MODE_EDIT_TEXT:
      OVERLAY_edit_text_draw(data);
      break;
    case CTX_MODE_EDIT_LATTICE:
      OVERLAY_edit_lattice_draw(data);
      break;
    case CTX_MODE_POSE:
      OVERLAY_pose_draw(data);
      break;
    case CTX_MODE_PARTICLE:
      OVERLAY_edit_particle_draw(data);
      break;
    case CTX_MODE_EDIT_GPENCIL_LEGACY:
      OVERLAY_edit_gpencil_legacy_draw(data);
      break;
    case CTX_MODE_PAINT_GPENCIL_LEGACY:
    case CTX_MODE_SCULPT_GPENCIL_LEGACY:
    case CTX_MODE_VERTEX_GPENCIL_LEGACY:
    case CTX_MODE_WEIGHT_GPENCIL_LEGACY:
      OVERLAY_edit_gpencil_legacy_draw(data);
      break;
    case CTX_MODE_SCULPT_CURVES:
      break;
    case CTX_MODE_EDIT_CURVES:
      OVERLAY_edit_curves_draw(data);
      break;
    case CTX_MODE_EDIT_GREASE_PENCIL:
      OVERLAY_edit_grease_pencil_draw(data);
      break;
    default:
      break;
  }

  OVERLAY_antialiasing_end(data);
}

static void OVERLAY_engine_free()
{
  OVERLAY_shader_free();
  overlay::ShaderModule::module_free();
}

static void OVERLAY_instance_free(void *instance_)
{
  auto *instance = (Instance *)instance_;
  if (instance != nullptr) {
    delete instance;
  }
}
/** \} */
/* -------------------------------------------------------------------- */
/** \name Engine Type
 * \{ */

static const DrawEngineDataSize overlay_data_size = DRW_VIEWPORT_DATA_SIZE(OVERLAY_Data);

DrawEngineType draw_engine_overlay_type = {
    nullptr,
    nullptr,
    N_("Overlay"),
    &overlay_data_size,
    &OVERLAY_engine_init,
    &OVERLAY_engine_free,
    &OVERLAY_instance_free,
    &OVERLAY_cache_init,
    &OVERLAY_cache_populate,
    &OVERLAY_cache_finish,
    &OVERLAY_draw_scene,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

/** \} */

#undef SELECT_ENGINE
