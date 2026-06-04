/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "world.hh"

#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdLux/tokens.h>

#include "BLI_math_constants.h"

#include "BKE_studiolight.h"

#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "image.hh"
#include "light.hh"
#include "populate_context.hh"
#include "usd_private.hh"

/* TODO: add custom #TfToken "transparency"? */

/* NOTE: opacity and blur aren't supported by USD */

namespace blender::io::hydra {

pxr::HdContainerDataSourceHandle build_world_data_source(Main *bmain,
                                                         Scene *scene,
                                                         const View3D *view3d,
                                                         pxr::GfMatrix4d *r_transform)
{
  pxr::GfVec3f color(1.0f, 1.0f, 1.0f);
  float intensity = 1.0f;
  pxr::SdfAssetPath texture_file;
  pxr::GfMatrix4d transform(1.0);

  const bool use_scene_world = !view3d || V3D_USES_SCENE_WORLD(view3d);

  if (use_scene_world) {
    if (!scene->world) {
      return nullptr;
    }

    usd::WorldToDomeLight res;
    usd::world_material_to_dome_light(scene, res);

    if (res.image) {
      const std::string file_path = cache_or_get_image_file(bmain, scene, res.image, res.iuser);
      if (!file_path.empty()) {
        texture_file = pxr::SdfAssetPath(file_path, file_path);
      }

      if (res.mult_found) {
        color = pxr::GfVec3f(res.color_mult);
      }
    }
    else if (res.color_found) {
      const std::string file_path = io::usd::cache_image_color(res.color);
      texture_file = pxr::SdfAssetPath(file_path, file_path);
      intensity = res.intensity;
    }
    else {
      intensity = 0.0f;
      color = pxr::GfVec3f(0.0f, 0.0f, 0.0f);
    }

    transform = res.transform;
  }
  else {
    StudioLight *sl = BKE_studiolight_find(view3d->shading.lookdev_light,
                                           STUDIOLIGHT_ORIENTATIONS_MATERIAL_MODE);
    if (sl != nullptr && sl->flag & STUDIOLIGHT_TYPE_WORLD) {
      texture_file = pxr::SdfAssetPath(sl->filepath, sl->filepath);
      /* Coefficient to follow Cycles result */
      intensity = view3d->shading.studiolight_intensity / 2.0f;
    }
    else {
      return nullptr;
    }

    transform = pxr::GfMatrix4d().SetRotate(pxr::GfRotation(
        pxr::GfVec3d(0.0, 0.0, -1.0), RAD2DEGF(view3d->shading.studiolight_rot_z)));
  }

  *r_transform = transform;

  HdContainerBuilder b;
  b.add(pxr::UsdLuxTokens->orientToStageUpAxis, true);
  b.add(pxr::HdLightTokens->intensity, intensity);
  b.add(pxr::HdLightTokens->exposure, 0.0f);
  b.add(pxr::HdLightTokens->color, color);
  b.add(pxr::HdLightTokens->textureFile, texture_file);
  return b.build();
}

void EmittedWorld::emit(PopulateContext &ctx,
                        Main *bmain,
                        Scene *scene,
                        const View3D *view3d,
                        const pxr::SdfPath &world_path,
                        const bool world_shading_changed)
{
  Inputs inputs;
  inputs.world = scene->world;
  inputs.use_scene_world = !view3d || V3D_USES_SCENE_WORLD(view3d);
  if (view3d) {
    inputs.shading_type = view3d->shading.type;
    BLI_strncpy(inputs.studiolight, view3d->shading.lookdev_light, sizeof(inputs.studiolight));
    inputs.studiolight_intensity = view3d->shading.studiolight_intensity;
    inputs.studiolight_rot_z = view3d->shading.studiolight_rot_z;
  }

  if (used_ && inputs == cached_inputs_ && !world_shading_changed) {
    ctx.new_paths.add(world_path);
    return;
  }

  pxr::GfMatrix4d world_transform(1.0);
  pxr::HdContainerDataSourceHandle world_params = build_world_data_source(
      bmain, scene, view3d, &world_transform);
  if (world_params) {
    pxr::HdContainerDataSourceHandle world_prim = build_light_prim_data_source(
        world_params, world_transform, true);
    ctx.emit_prim(world_path, pxr::HdPrimTypeTokens->domeLight, world_prim);
  }
  used_ = (world_params != nullptr);
  cached_inputs_ = inputs;
}

void EmittedWorld::clear()
{
  used_ = false;
  cached_inputs_ = Inputs();
}

}  // namespace blender::io::hydra
