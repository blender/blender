/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "world.hh"
#include "usd_private.hh"

#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdLux/tokens.h>

#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "BLI_math_constants.h"

#include "BKE_studiolight.h"

#include "hydra_scene_delegate.hh"
#include "image.hh"

/* TODO: add custom `tftoken` "transparency"? */

/* NOTE: opacity and blur aren't supported by USD */

namespace blender::io::hydra {

WorldData::WorldData(HydraSceneDelegate *scene_delegate, pxr::SdfPath const &prim_id)
    : LightData(scene_delegate, nullptr, prim_id)
{
  prim_type_ = pxr::HdPrimTypeTokens->domeLight;
}

void WorldData::init()
{
  data_.clear();

  pxr::GfVec3f color(1.0f, 1.0f, 1.0f);
  float intensity = 1.0f;
  pxr::SdfAssetPath texture_file;

  if (scene_delegate_->shading_settings.use_scene_world) {
    const World *world = scene_delegate_->scene->world;
    ID_LOG("%s", world->id.name);

    usd::WorldToDomeLight res;
    usd::world_material_to_dome_light(scene_delegate_->scene, res);

    if (res.image) {
      const std::string file_path = cache_or_get_image_file(
          scene_delegate_->bmain, scene_delegate_->scene, res.image, res.iuser);
      if (!file_path.empty()) {
        texture_file = pxr::SdfAssetPath(file_path, file_path);
      }

      if (res.mult_found) {
        color = pxr::GfVec3f(res.color_mult);
      }
    }
    else if (res.color_found) {
      const std::string File_path = blender::io::usd::cache_image_color(res.color);
      texture_file = pxr::SdfAssetPath(File_path, File_path);
      intensity = res.intensity;
    }
    else {
      intensity = 0.0f;
      color = pxr::GfVec3f(0.0f, 0.0f, 0.0f);
    }

    transform = res.transform;
  }
  else {
    ID_LOG("studiolight: %s", scene_delegate_->shading_settings.studiolight_name.c_str());

    StudioLight *sl = BKE_studiolight_find(
        scene_delegate_->shading_settings.studiolight_name.c_str(),
        STUDIOLIGHT_ORIENTATIONS_MATERIAL_MODE);
    if (sl != nullptr && sl->flag & STUDIOLIGHT_TYPE_WORLD) {
      texture_file = pxr::SdfAssetPath(sl->filepath, sl->filepath);
      /* Coefficient to follow Cycles result */
      intensity = scene_delegate_->shading_settings.studiolight_intensity / 2;
    }

    transform = pxr::GfMatrix4d().SetRotate(
        pxr::GfRotation(pxr::GfVec3d(0.0, 0.0, -1.0),
                        RAD2DEGF(scene_delegate_->shading_settings.studiolight_rotation)));
  }

  data_[pxr::UsdLuxTokens->orientToStageUpAxis] = true;
  data_[pxr::HdLightTokens->intensity] = intensity;
  data_[pxr::HdLightTokens->exposure] = 0.0f;
  data_[pxr::HdLightTokens->color] = color;
  data_[pxr::HdLightTokens->textureFile] = texture_file;
}

void WorldData::update()
{
  ID_LOG("");

  if (!scene_delegate_->shading_settings.use_scene_world ||
      (scene_delegate_->shading_settings.use_scene_world && scene_delegate_->scene->world))
  {
    init();
    if (data_.empty()) {
      remove();
      return;
    }
    insert();
    scene_delegate_->GetRenderIndex().GetChangeTracker().MarkSprimDirty(prim_id,
                                                                        pxr::HdLight::AllDirty);
  }
  else {
    remove();
  }
}

}  // namespace blender::io::hydra
