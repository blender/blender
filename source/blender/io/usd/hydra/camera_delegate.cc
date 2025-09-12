/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "camera_delegate.hh"

#include "DNA_ID.h"
#include "DNA_camera_types.h"
#include "DNA_scene_types.h"

#include "BKE_idprop.hh"

#include <pxr/imaging/hd/camera.h>

namespace blender::io::hydra {

static pxr::VtValue vt_value(const IDProperty *prop)
{
  switch (prop->type) {
    case IDP_INT:
      return pxr::VtValue{IDP_int_get(prop)};
    case IDP_FLOAT:
      return pxr::VtValue{IDP_float_get(prop)};
    case IDP_DOUBLE:
      return pxr::VtValue{IDP_double_get(prop)};
    case IDP_BOOLEAN:
      return pxr::VtValue{bool(IDP_bool_get(prop))};
  }
  return pxr::VtValue{};
}

CameraDelegate::CameraDelegate(pxr::HdRenderIndex *render_index, pxr::SdfPath const &delegate_id)
    : pxr::HdxFreeCameraSceneDelegate{render_index, delegate_id}
{
}

void CameraDelegate::sync(const Scene *scene)
{
  if (!scene || !scene->camera) {
    return;
  }

  const Camera *camera = static_cast<const Camera *>(scene->camera->data);
  if (camera_ == camera) {
    return;
  }

  camera_ = camera;
  GetRenderIndex().GetChangeTracker().MarkSprimDirty(GetCameraId(), pxr::HdCamera::DirtyParams);
}

void CameraDelegate::update(const ID *camera)
{
  if (&camera_->id == camera) {
    GetRenderIndex().GetChangeTracker().MarkSprimDirty(GetCameraId(), pxr::HdCamera::DirtyParams);
  }
}

pxr::VtValue CameraDelegate::GetCameraParamValue(pxr::SdfPath const &id, pxr::TfToken const &key)
{
  if (camera_ && camera_->id.properties) {
    const IDProperty *prop = IDP_GetPropertyFromGroup(camera_->id.properties, key.GetText());
    if (prop) {
      return vt_value(prop);
    }
  }
  return pxr::HdxFreeCameraSceneDelegate::GetCameraParamValue(id, key);
}

}  // namespace blender::io::hydra
