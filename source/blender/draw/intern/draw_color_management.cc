/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "draw_manager_c.hh"

#include "DRW_render.hh"

#include "GPU_batch.hh"
#include "GPU_framebuffer.hh"
#include "GPU_matrix.hh"
#include "GPU_texture.hh"

#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_colortools.hh"

#include "draw_color_management.hh"

namespace blender::draw::color_management {

enum class eDRWColorManagementType {
  ViewTransform = 0,
  ViewTransformAndLook,
  UseRenderSettings,
};

static float dither_get(eDRWColorManagementType color_management_type, const Scene &scene)
{
  if (ELEM(color_management_type,
           eDRWColorManagementType::ViewTransformAndLook,
           eDRWColorManagementType::UseRenderSettings))
  {
    return scene.r.dither_intensity;
  }
  return 0.0f;
}

static eDRWColorManagementType drw_color_management_type_for_v3d(const Scene &scene,
                                                                 const View3D &v3d)
{

  const bool use_workbench = BKE_scene_uses_blender_workbench(&scene);
  const bool use_scene_lights = V3D_USES_SCENE_LIGHTS(&v3d);
  const bool use_scene_world = V3D_USES_SCENE_WORLD(&v3d);

  if ((use_workbench && v3d.shading.type == OB_RENDER) || use_scene_lights || use_scene_world) {
    return eDRWColorManagementType::UseRenderSettings;
  }
  if (v3d.shading.type >= OB_MATERIAL) {
    return eDRWColorManagementType::ViewTransformAndLook;
  }
  return eDRWColorManagementType::ViewTransform;
}

static eDRWColorManagementType drw_color_management_type_for_space_image(const SpaceImage &sima)
{
  Image *image = sima.image;

  /* Use inverse logic as there isn't a setting for `Color & Alpha`. */
  const eSpaceImage_Flag display_channels_mode = static_cast<eSpaceImage_Flag>(sima.flag);
  const bool display_color_channel = (display_channels_mode & (SI_SHOW_ALPHA | SI_SHOW_ZBUF)) == 0;

  if (display_color_channel && image && (image->source != IMA_SRC_GENERATED) &&
      ((image->flag & IMA_VIEW_AS_RENDER) != 0))
  {
    return eDRWColorManagementType::UseRenderSettings;
  }
  return eDRWColorManagementType::ViewTransform;
}

static eDRWColorManagementType drw_color_management_type_for_space_node(const SpaceNode &snode)
{
  const eSpaceNode_Flag display_channels_mode = static_cast<eSpaceNode_Flag>(snode.flag);
  const bool display_color_channel = (display_channels_mode & SNODE_SHOW_ALPHA) == 0;
  if (display_color_channel) {
    return eDRWColorManagementType::UseRenderSettings;
  }
  return eDRWColorManagementType::ViewTransform;
}

static eDRWColorManagementType drw_color_management_type_get(const Scene &scene,
                                                             const View3D *v3d,
                                                             const SpaceLink *space_data)
{
  if (v3d) {
    return drw_color_management_type_for_v3d(scene, *v3d);
  }
  if (space_data) {
    switch (space_data->spacetype) {
      case SPACE_IMAGE: {
        const SpaceImage *sima = static_cast<const SpaceImage *>(
            static_cast<const void *>(space_data));
        return drw_color_management_type_for_space_image(*sima);
      }
      case SPACE_NODE: {
        const SpaceNode *snode = static_cast<const SpaceNode *>(
            static_cast<const void *>(space_data));
        return drw_color_management_type_for_space_node(*snode);
      }
    }
  }
  return eDRWColorManagementType::UseRenderSettings;
}

static void viewport_settings_apply(GPUViewport &viewport,
                                    const Scene &scene,
                                    const eDRWColorManagementType color_management_type)
{
  const ColorManagedDisplaySettings *display_settings = &scene.display_settings;
  ColorManagedViewSettings view_settings;

  switch (color_management_type) {
    case eDRWColorManagementType::ViewTransform: {
      /* For workbench use only default view transform in configuration,
       * using no scene settings. */
      BKE_color_managed_view_settings_init_render(&view_settings, display_settings, nullptr);
      break;
    }
    case eDRWColorManagementType::ViewTransformAndLook: {
      /* Use only view transform + look and nothing else for lookdev without
       * scene lighting, as exposure depends on scene light intensity. */
      BKE_color_managed_view_settings_init_render(&view_settings, display_settings, nullptr);
      STRNCPY(view_settings.view_transform, scene.view_settings.view_transform);
      STRNCPY(view_settings.look, scene.view_settings.look);
      break;
    }
    case eDRWColorManagementType::UseRenderSettings: {
      /* Use full render settings, for renders with scene lighting. */
      view_settings = scene.view_settings;
      break;
    }
  }

  const float dither = dither_get(color_management_type, scene);
  GPU_viewport_colorspace_set(&viewport, &view_settings, display_settings, dither);
}

static void viewport_color_management_set(GPUViewport &viewport)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();

  const eDRWColorManagementType color_management_type = drw_color_management_type_get(
      *draw_ctx->scene, draw_ctx->v3d, draw_ctx->space_data);
  viewport_settings_apply(viewport, *draw_ctx->scene, color_management_type);
}

}  // namespace blender::draw::color_management

/* -------------------------------------------------------------------- */
/** \name Color Management
 * \{ */

void DRW_viewport_colormanagement_set(GPUViewport *viewport)
{
  blender::draw::color_management::viewport_color_management_set(*viewport);
}

void DRW_transform_none(GPUTexture *tex)
{
  drw_state_set(DRW_STATE_WRITE_COLOR);

  GPU_matrix_identity_set();
  GPU_matrix_identity_projection_set();

  /* Draw as texture for final render (without immediate mode). */
  blender::gpu::Batch *geom = DRW_cache_fullscreen_quad_get();
  GPU_batch_program_set_builtin(geom, GPU_SHADER_3D_IMAGE_COLOR);
  GPU_batch_uniform_4f(geom, "color", 1.0f, 1.0f, 1.0f, 1.0f);
  GPU_batch_texture_bind(geom, "image", tex);

  GPU_batch_draw(geom);

  GPU_texture_unbind(tex);
}

/** \} */
