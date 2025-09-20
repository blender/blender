/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "GPU_viewport.hh"

#include "BLI_string_utf8.h"

#include "DRW_render.hh"

#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_colortools.hh"
#include "BKE_image.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_node_c.hh"

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
  const Image *image = sima.image;

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

static eDRWColorManagementType drw_color_management_type_for_space_node(Main &bmain,
                                                                        const SpaceNode &snode)
{
  if ((snode.flag & SNODE_BACKDRAW) && ED_node_is_compositor(&snode)) {
    const Image *image = BKE_image_ensure_viewer(&bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
    if ((image->flag & IMA_VIEW_AS_RENDER) == 0) {
      return eDRWColorManagementType::ViewTransform;
    }
  }

  const eSpaceNode_Flag display_channels_mode = static_cast<eSpaceNode_Flag>(snode.flag);
  const bool display_color_channel = (display_channels_mode & SNODE_SHOW_ALPHA) == 0;
  if (display_color_channel) {
    return eDRWColorManagementType::UseRenderSettings;
  }
  return eDRWColorManagementType::ViewTransform;
}

static eDRWColorManagementType drw_color_management_type_get(Main *bmain,
                                                             const Scene &scene,
                                                             const View3D *v3d,
                                                             const SpaceLink *space_data)
{
  if (v3d) {
    return drw_color_management_type_for_v3d(scene, *v3d);
  }
  if (space_data) {
    switch (space_data->spacetype) {
      case SPACE_IMAGE: {
        const SpaceImage *sima = reinterpret_cast<const SpaceImage *>(space_data);
        return drw_color_management_type_for_space_image(*sima);
      }
      case SPACE_NODE: {
        const SpaceNode *snode = reinterpret_cast<const SpaceNode *>(space_data);
        return drw_color_management_type_for_space_node(*bmain, *snode);
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
      BKE_color_managed_view_settings_init(&view_settings, display_settings, nullptr);
      break;
    }
    case eDRWColorManagementType::ViewTransformAndLook: {
      /* Use only view transform + look and nothing else for lookdev without
       * scene lighting, as exposure depends on scene light intensity. */
      BKE_color_managed_view_settings_init(&view_settings, display_settings, nullptr);
      STRNCPY_UTF8(view_settings.view_transform, scene.view_settings.view_transform);
      STRNCPY_UTF8(view_settings.look, scene.view_settings.look);
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

void viewport_color_management_set(GPUViewport &viewport, DRWContext &draw_ctx)
{
  const Depsgraph *depsgraph = draw_ctx.depsgraph;
  Main *bmain = DEG_get_bmain(depsgraph);

  const eDRWColorManagementType color_management_type = drw_color_management_type_get(
      bmain, *draw_ctx.scene, draw_ctx.v3d, draw_ctx.space_data);
  viewport_settings_apply(viewport, *draw_ctx.scene, color_management_type);
}

}  // namespace blender::draw::color_management
