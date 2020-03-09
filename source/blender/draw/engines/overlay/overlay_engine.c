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
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 *
 * Engine for drawing a selection map where the pixels indicate the selection indices.
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DEG_depsgraph_query.h"

#include "ED_view3d.h"

#include "BKE_object.h"

#include "overlay_engine.h"
#include "overlay_private.h"

/* -------------------------------------------------------------------- */
/** \name Engine Callbacks
 * \{ */

static void OVERLAY_engine_init(void *vedata)
{
  OVERLAY_Data *data = vedata;
  OVERLAY_StorageList *stl = data->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const RegionView3D *rv3d = draw_ctx->rv3d;
  const View3D *v3d = draw_ctx->v3d;

  if (!stl->pd) {
    /* Alloc transient pointers */
    stl->pd = MEM_callocN(sizeof(*stl->pd), __func__);
  }

  OVERLAY_PrivateData *pd = stl->pd;

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
  }

  if (v3d->shading.type == OB_WIRE) {
    pd->overlay.flag |= V3D_OVERLAY_WIREFRAMES;
  }

  pd->wireframe_mode = (v3d->shading.type == OB_WIRE);
  pd->clipping_state = RV3D_CLIPPING_ENABLED(v3d, rv3d) ? DRW_STATE_CLIP_PLANES : 0;
  pd->xray_opacity = XRAY_ALPHA(v3d);
  pd->xray_enabled = XRAY_ACTIVE(v3d);
  pd->xray_enabled_and_not_wire = pd->xray_enabled && v3d->shading.type > OB_WIRE;
  pd->clear_in_front = (v3d->shading.type != OB_SOLID);
  pd->cfra = DEG_get_ctime(draw_ctx->depsgraph);

  OVERLAY_antialiasing_init(vedata);

  switch (stl->pd->ctx_mode) {
    case CTX_MODE_EDIT_MESH:
      OVERLAY_edit_mesh_init(vedata);
      break;
    default:
      /* Nothing to do. */
      break;
  }
  OVERLAY_facing_init(vedata);
  OVERLAY_grid_init(vedata);
  OVERLAY_image_init(vedata);
  OVERLAY_outline_init(vedata);
  OVERLAY_wireframe_init(vedata);
}

static void OVERLAY_cache_init(void *vedata)
{
  OVERLAY_Data *data = vedata;
  OVERLAY_StorageList *stl = data->stl;
  OVERLAY_PrivateData *pd = stl->pd;

  switch (pd->ctx_mode) {
    case CTX_MODE_EDIT_MESH:
      OVERLAY_edit_mesh_cache_init(vedata);
      break;
    case CTX_MODE_EDIT_SURFACE:
    case CTX_MODE_EDIT_CURVE:
      OVERLAY_edit_curve_cache_init(vedata);
      break;
    case CTX_MODE_EDIT_TEXT:
      OVERLAY_edit_text_cache_init(vedata);
      break;
    case CTX_MODE_EDIT_ARMATURE:
      break;
    case CTX_MODE_EDIT_METABALL:
      break;
    case CTX_MODE_EDIT_LATTICE:
      OVERLAY_edit_lattice_cache_init(vedata);
      break;
    case CTX_MODE_PARTICLE:
      OVERLAY_edit_particle_cache_init(vedata);
      break;
    case CTX_MODE_POSE:
    case CTX_MODE_PAINT_WEIGHT:
    case CTX_MODE_PAINT_VERTEX:
    case CTX_MODE_PAINT_TEXTURE:
      OVERLAY_paint_cache_init(vedata);
      break;
    case CTX_MODE_SCULPT:
      OVERLAY_sculpt_cache_init(vedata);
      break;
    case CTX_MODE_EDIT_GPENCIL:
    case CTX_MODE_PAINT_GPENCIL:
    case CTX_MODE_SCULPT_GPENCIL:
    case CTX_MODE_VERTEX_GPENCIL:
    case CTX_MODE_WEIGHT_GPENCIL:
      OVERLAY_edit_gpencil_cache_init(vedata);
      break;
    case CTX_MODE_OBJECT:
      break;
    default:
      BLI_assert(!"Draw mode invalid");
      break;
  }
  OVERLAY_antialiasing_cache_init(vedata);
  OVERLAY_armature_cache_init(vedata);
  OVERLAY_background_cache_init(vedata);
  OVERLAY_extra_cache_init(vedata);
  OVERLAY_facing_cache_init(vedata);
  OVERLAY_gpencil_cache_init(vedata);
  OVERLAY_grid_cache_init(vedata);
  OVERLAY_image_cache_init(vedata);
  OVERLAY_metaball_cache_init(vedata);
  OVERLAY_motion_path_cache_init(vedata);
  OVERLAY_outline_cache_init(vedata);
  OVERLAY_particle_cache_init(vedata);
  OVERLAY_wireframe_cache_init(vedata);
}

BLI_INLINE OVERLAY_DupliData *OVERLAY_duplidata_get(Object *ob, void *vedata, bool *do_init)
{
  OVERLAY_DupliData **dupli_data = (OVERLAY_DupliData **)DRW_duplidata_get(vedata);
  *do_init = false;
  if (!ELEM(ob->type, OB_MESH, OB_SURF, OB_LATTICE, OB_CURVE, OB_FONT)) {
    return NULL;
  }

  if (dupli_data) {
    if (*dupli_data == NULL) {
      *dupli_data = MEM_callocN(sizeof(OVERLAY_DupliData), __func__);
      *do_init = true;
    }
    else if ((*dupli_data)->base_flag != ob->base_flag) {
      /* Select state might have change, reinit. */
      *do_init = true;
    }
    return *dupli_data;
  }
  return NULL;
}

static bool overlay_object_is_edit_mode(const OVERLAY_PrivateData *pd, const Object *ob)
{
  if (DRW_object_is_in_edit_mode(ob)) {
    /* Also check for context mode as the object mode is not 100% reliable. (see T72490) */
    switch (ob->type) {
      case OB_MESH:
        return pd->ctx_mode == CTX_MODE_EDIT_MESH;
      case OB_ARMATURE:
        return pd->ctx_mode == CTX_MODE_EDIT_ARMATURE;
      case OB_CURVE:
        return pd->ctx_mode == CTX_MODE_EDIT_CURVE;
      case OB_SURF:
        return pd->ctx_mode == CTX_MODE_EDIT_SURFACE;
      case OB_LATTICE:
        return pd->ctx_mode == CTX_MODE_EDIT_LATTICE;
      case OB_MBALL:
        return pd->ctx_mode == CTX_MODE_EDIT_METABALL;
      case OB_FONT:
        return pd->ctx_mode == CTX_MODE_EDIT_TEXT;
    }
  }
  return false;
}

static void OVERLAY_cache_populate(void *vedata, Object *ob)
{
  OVERLAY_Data *data = vedata;
  OVERLAY_PrivateData *pd = data->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool is_select = DRW_state_is_select();
  const bool renderable = DRW_object_is_renderable(ob);
  const bool in_pose_mode = ob->type == OB_ARMATURE && OVERLAY_armature_is_pose_mode(ob, draw_ctx);
  const bool in_edit_mode = overlay_object_is_edit_mode(pd, ob);
  const bool in_particle_edit_mode = ob->mode == OB_MODE_PARTICLE_EDIT;
  const bool in_paint_mode = (ob == draw_ctx->obact) &&
                             (draw_ctx->object_mode & OB_MODE_ALL_PAINT);
  const bool in_sculpt_mode = (ob == draw_ctx->obact) && (ob->sculpt != NULL);
  const bool has_surface = ELEM(
      ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_MBALL, OB_FONT, OB_GPENCIL);
  const bool draw_surface = (ob->dt >= OB_WIRE) && (renderable || (ob->dt == OB_WIRE));
  const bool draw_facing = draw_surface && (pd->overlay.flag & V3D_OVERLAY_FACE_ORIENTATION);
  const bool draw_bones = (pd->overlay.flag & V3D_OVERLAY_HIDE_BONES) == 0;
  const bool draw_wires = draw_surface && has_surface &&
                          (pd->wireframe_mode || !pd->hide_overlays);
  const bool draw_outlines = !in_edit_mode && !in_paint_mode && renderable && has_surface &&
                             (pd->v3d_flag & V3D_SELECT_OUTLINE) &&
                             (ob->base_flag & BASE_SELECTED);
  const bool draw_bone_selection = (ob->type == OB_MESH) && pd->armature.do_pose_fade_geom &&
                                   !is_select;
  const bool draw_extras =
      (!pd->hide_overlays) &&
      (((pd->overlay.flag & V3D_OVERLAY_HIDE_OBJECT_XTRAS) == 0) ||
       /* Show if this is the camera we're looking through since it's useful for selecting. */
       ((draw_ctx->rv3d->persp == RV3D_CAMOB) && ((ID *)draw_ctx->v3d->camera == ob->id.orig_id)));

  const bool draw_motion_paths = (pd->overlay.flag & V3D_OVERLAY_HIDE_MOTION_PATHS) == 0;

  bool do_init;
  OVERLAY_DupliData *dupli = OVERLAY_duplidata_get(ob, vedata, &do_init);

  if (draw_facing) {
    OVERLAY_facing_cache_populate(vedata, ob);
  }
  if (draw_wires) {
    OVERLAY_wireframe_cache_populate(vedata, ob, dupli, do_init);
  }
  if (draw_outlines) {
    OVERLAY_outline_cache_populate(vedata, ob, dupli, do_init);
  }
  if (draw_bone_selection) {
    OVERLAY_pose_cache_populate(vedata, ob);
  }

  if (in_edit_mode && !pd->hide_overlays) {
    switch (ob->type) {
      case OB_MESH:
        OVERLAY_edit_mesh_cache_populate(vedata, ob);
        break;
      case OB_ARMATURE:
        if (draw_bones) {
          OVERLAY_edit_armature_cache_populate(vedata, ob);
        }
        break;
      case OB_CURVE:
        OVERLAY_edit_curve_cache_populate(vedata, ob);
        break;
      case OB_SURF:
        OVERLAY_edit_surf_cache_populate(vedata, ob);
        break;
      case OB_LATTICE:
        OVERLAY_edit_lattice_cache_populate(vedata, ob);
        break;
      case OB_MBALL:
        OVERLAY_edit_metaball_cache_populate(vedata, ob);
        break;
      case OB_FONT:
        OVERLAY_edit_text_cache_populate(vedata, ob);
        break;
    }
  }
  else if (in_pose_mode && draw_bones) {
    OVERLAY_pose_armature_cache_populate(vedata, ob);
  }
  else if (in_paint_mode) {
    switch (draw_ctx->object_mode) {
      case OB_MODE_VERTEX_PAINT:
        OVERLAY_paint_vertex_cache_populate(vedata, ob);
        break;
      case OB_MODE_WEIGHT_PAINT:
        OVERLAY_paint_weight_cache_populate(vedata, ob);
        break;
      case OB_MODE_TEXTURE_PAINT:
        OVERLAY_paint_texture_cache_populate(vedata, ob);
        break;
      default:
        break;
    }
  }
  else if (in_particle_edit_mode) {
    OVERLAY_edit_particle_cache_populate(vedata, ob);
  }

  if (in_sculpt_mode) {
    OVERLAY_sculpt_cache_populate(vedata, ob);
  }

  if (draw_motion_paths) {
    OVERLAY_motion_path_cache_populate(vedata, ob);
  }

  if (!pd->hide_overlays) {
    switch (ob->type) {
      case OB_ARMATURE:
        if (draw_bones && (is_select || (!in_edit_mode && !in_pose_mode))) {
          OVERLAY_armature_cache_populate(vedata, ob);
        }
        break;
      case OB_MBALL:
        if (!in_edit_mode) {
          OVERLAY_metaball_cache_populate(vedata, ob);
        }
        break;
      case OB_GPENCIL:
        OVERLAY_gpencil_cache_populate(vedata, ob);
        break;
    }
  }
  /* Non-Meshes */
  if (draw_extras) {
    switch (ob->type) {
      case OB_EMPTY:
        OVERLAY_empty_cache_populate(vedata, ob);
        break;
      case OB_LAMP:
        OVERLAY_light_cache_populate(vedata, ob);
        break;
      case OB_CAMERA:
        OVERLAY_camera_cache_populate(vedata, ob);
        break;
      case OB_SPEAKER:
        OVERLAY_speaker_cache_populate(vedata, ob);
        break;
      case OB_LIGHTPROBE:
        OVERLAY_lightprobe_cache_populate(vedata, ob);
        break;
      case OB_LATTICE:
        OVERLAY_lattice_cache_populate(vedata, ob);
        break;
    }
  }

  if (!BLI_listbase_is_empty(&ob->particlesystem)) {
    OVERLAY_particle_cache_populate(vedata, ob);
  }

  /* Relationship, object center, bounbox ... */
  if (!pd->hide_overlays) {
    OVERLAY_extra_cache_populate(vedata, ob);
  }

  if (dupli) {
    dupli->base_flag = ob->base_flag;
  }
}

static void OVERLAY_cache_finish(void *vedata)
{
  /* TODO(fclem) Only do this when really needed. */
  {
    /* HACK we allocate the infront depth here to avoid the overhead when if is not needed. */
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

    DRW_texture_ensure_fullscreen_2d(&dtxl->depth_in_front, GPU_DEPTH24_STENCIL8, 0);

    GPU_framebuffer_ensure_config(
        &dfbl->default_fb,
        {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_TEXTURE(dtxl->color)});
    GPU_framebuffer_ensure_config(
        &dfbl->in_front_fb,
        {GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front), GPU_ATTACHMENT_TEXTURE(dtxl->color)});
  }

  OVERLAY_antialiasing_cache_finish(vedata);
  OVERLAY_armature_cache_finish(vedata);
  OVERLAY_image_cache_finish(vedata);
}

static void OVERLAY_draw_scene(void *vedata)
{
  OVERLAY_Data *data = vedata;
  OVERLAY_PrivateData *pd = data->stl->pd;
  OVERLAY_FramebufferList *fbl = data->fbl;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  if (DRW_state_is_fbo()) {
    float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_framebuffer_bind(dfbl->overlay_only_fb);
    GPU_framebuffer_clear_color(dfbl->overlay_only_fb, clear_col);
  }

  OVERLAY_image_background_draw(vedata);
  OVERLAY_background_draw(vedata);

  OVERLAY_antialiasing_start(vedata);

  DRW_view_set_active(NULL);

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_color_only_fb);
  }

  OVERLAY_outline_draw(vedata);
  OVERLAY_xray_depth_copy(vedata);

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_default_fb);
  }

  OVERLAY_image_draw(vedata);
  OVERLAY_facing_draw(vedata);
  OVERLAY_extra_blend_draw(vedata);

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_line_fb);
  }

  OVERLAY_wireframe_draw(vedata);
  OVERLAY_armature_draw(vedata);
  OVERLAY_particle_draw(vedata);
  OVERLAY_metaball_draw(vedata);
  OVERLAY_gpencil_draw(vedata);
  OVERLAY_extra_draw(vedata);

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_color_only_fb);
  }

  OVERLAY_xray_fade_draw(vedata);
  OVERLAY_grid_draw(vedata);

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_line_in_front_fb);
  }

  OVERLAY_wireframe_in_front_draw(vedata);
  OVERLAY_armature_in_front_draw(vedata);
  OVERLAY_extra_in_front_draw(vedata);
  OVERLAY_metaball_in_front_draw(vedata);

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_color_only_fb);
  }

  OVERLAY_image_in_front_draw(vedata);
  OVERLAY_motion_path_draw(vedata);
  OVERLAY_extra_centers_draw(vedata);

  if (DRW_state_is_select()) {
    /* Edit modes have their own selection code. */
    return;
  }

  /* Functions after this point can change FBO freely. */

  switch (pd->ctx_mode) {
    case CTX_MODE_EDIT_MESH:
      OVERLAY_edit_mesh_draw(vedata);
      break;
    case CTX_MODE_EDIT_SURFACE:
    case CTX_MODE_EDIT_CURVE:
      OVERLAY_edit_curve_draw(vedata);
      break;
    case CTX_MODE_EDIT_TEXT:
      OVERLAY_edit_text_draw(vedata);
      break;
    case CTX_MODE_EDIT_LATTICE:
      OVERLAY_edit_lattice_draw(vedata);
      break;
    case CTX_MODE_POSE:
      OVERLAY_paint_draw(vedata);
      OVERLAY_pose_draw(vedata);
      break;
    case CTX_MODE_PAINT_WEIGHT:
    case CTX_MODE_PAINT_VERTEX:
    case CTX_MODE_PAINT_TEXTURE:
      OVERLAY_paint_draw(vedata);
      break;
    case CTX_MODE_PARTICLE:
      OVERLAY_edit_particle_draw(vedata);
      break;
    case CTX_MODE_SCULPT:
      OVERLAY_sculpt_draw(vedata);
      break;
    case CTX_MODE_EDIT_GPENCIL:
    case CTX_MODE_PAINT_GPENCIL:
    case CTX_MODE_SCULPT_GPENCIL:
    case CTX_MODE_VERTEX_GPENCIL:
    case CTX_MODE_WEIGHT_GPENCIL:
      OVERLAY_edit_gpencil_draw(vedata);
      break;
    default:
      break;
  }

  OVERLAY_antialiasing_end(vedata);
}

static void OVERLAY_engine_free(void)
{
  OVERLAY_shader_free();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Type
 * \{ */

static const DrawEngineDataSize overlay_data_size = DRW_VIEWPORT_DATA_SIZE(OVERLAY_Data);

DrawEngineType draw_engine_overlay_type = {
    NULL,
    NULL,
    N_("Overlay"),
    &overlay_data_size,
    &OVERLAY_engine_init,
    &OVERLAY_engine_free,
    &OVERLAY_cache_init,
    &OVERLAY_cache_populate,
    &OVERLAY_cache_finish,
    &OVERLAY_draw_scene,
    NULL,
    NULL,
    NULL,
};

/** \} */

#undef SELECT_ENGINE
