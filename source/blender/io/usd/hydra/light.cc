/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#include "light.h"

#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/usdLux/tokens.h>

#include "DNA_light_types.h"

#include "BLI_math_rotation.h"

#include "hydra_scene_delegate.h"

namespace blender::io::hydra {

LightData::LightData(HydraSceneDelegate *scene_delegate,
                     Object *object,
                     pxr::SdfPath const &prim_id)
    : ObjectData(scene_delegate, object, prim_id)
{
}

void LightData::init()
{
  ID_LOGN(1, "");

  Light *light = (Light *)((Object *)id)->data;
  data_.clear();

  switch (light->type) {
    case LA_AREA: {
      switch (light->area_shape) {
        case LA_AREA_SQUARE:
          data_[pxr::HdLightTokens->width] = light->area_size;
          data_[pxr::HdLightTokens->height] = light->area_size;
          break;
        case LA_AREA_RECT:
          data_[pxr::HdLightTokens->width] = light->area_size;
          data_[pxr::HdLightTokens->height] = light->area_sizey;
          break;
        case LA_AREA_DISK:
          data_[pxr::HdLightTokens->radius] = light->area_size / 2.0f;
          break;
        case LA_AREA_ELLIPSE:
          /* An ellipse light deteriorates into a disk light. */
          data_[pxr::HdLightTokens->radius] = (light->area_size + light->area_sizey) / 4.0f;
          break;
      }
      break;
    }
    case LA_LOCAL:
    case LA_SPOT: {
      data_[pxr::HdLightTokens->radius] = light->radius;
      if (light->radius == 0.0f) {
        data_[pxr::UsdLuxTokens->treatAsPoint] = true;
      }

      if (light->type == LA_SPOT) {
        data_[pxr::UsdLuxTokens->inputsShapingConeAngle] = RAD2DEGF(light->spotsize * 0.5f);
        data_[pxr::UsdLuxTokens->inputsShapingConeSoftness] = light->spotblend;
      }
      break;
    }
    case LA_SUN: {
      data_[pxr::HdLightTokens->angle] = RAD2DEGF(light->sun_angle * 0.5f);
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }

  float intensity;
  if (light->type == LA_SUN) {
    /* Unclear why, but approximately matches Karma. */
    intensity = light->energy / 4.0f;
  }
  else {
    /* Convert from radiant flux to intensity. */
    intensity = light->energy / M_PI;
  }

  data_[pxr::HdLightTokens->intensity] = intensity;
  data_[pxr::HdLightTokens->exposure] = 0.0f;
  data_[pxr::HdLightTokens->color] = pxr::GfVec3f(light->r, light->g, light->b);
  data_[pxr::HdLightTokens->diffuse] = light->diff_fac;
  data_[pxr::HdLightTokens->specular] = light->spec_fac;
  data_[pxr::HdLightTokens->normalize] = true;

  prim_type_ = prim_type(light);

  write_transform();
}

void LightData::insert()
{
  ID_LOGN(1, "");
  scene_delegate_->GetRenderIndex().InsertSprim(prim_type_, scene_delegate_, prim_id);
}

void LightData::remove()
{
  ID_LOG(1, "");
  scene_delegate_->GetRenderIndex().RemoveSprim(prim_type_, prim_id);
}

void LightData::update()
{
  Object *object = (Object *)id;
  Light *light = (Light *)object->data;
  pxr::HdDirtyBits bits = pxr::HdLight::Clean;
  if (id->recalc & ID_RECALC_GEOMETRY || light->id.recalc & ID_RECALC_GEOMETRY) {
    if (prim_type(light) != prim_type_) {
      remove();
      init();
      insert();
      return;
    }
    init();
    bits = pxr::HdLight::AllDirty;
  }
  else if (id->recalc & ID_RECALC_TRANSFORM) {
    write_transform();
    bits = pxr::HdLight::DirtyTransform;
  }
  if (bits != pxr::HdChangeTracker::Clean) {
    scene_delegate_->GetRenderIndex().GetChangeTracker().MarkSprimDirty(prim_id, bits);
    ID_LOGN(1, "");
  }
}

pxr::VtValue LightData::get_data(pxr::TfToken const &key) const
{
  ID_LOGN(3, "%s", key.GetText());
  auto it = data_.find(key);
  if (it != data_.end()) {
    return pxr::VtValue(it->second);
  }

  return pxr::VtValue();
}

pxr::TfToken LightData::prim_type(Light *light)
{
  switch (light->type) {
    case LA_AREA:
      switch (light->area_shape) {
        case LA_AREA_SQUARE:
        case LA_AREA_RECT:
          return pxr::HdPrimTypeTokens->rectLight;

        case LA_AREA_DISK:
        case LA_AREA_ELLIPSE:
          return pxr::HdPrimTypeTokens->diskLight;

        default:
          return pxr::HdPrimTypeTokens->rectLight;
      }
      break;

    case LA_LOCAL:
    case LA_SPOT:
      return pxr::HdPrimTypeTokens->sphereLight;

    case LA_SUN:
      return pxr::HdPrimTypeTokens->distantLight;

    default:
      BLI_assert_unreachable();
  }
  return pxr::TfToken();
}

}  // namespace blender::io::hydra
