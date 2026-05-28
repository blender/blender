/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "camera_delegate.hh"

#include "DNA_ID.h"
#include "DNA_camera_types.h"
#include "DNA_scene_types.h"

#include "BKE_idprop.hh"

#include "BLI_listbase.h"

#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/imaging/hd/tokens.h>

namespace blender::io::hydra {

class BlenderCameraIDPropertiesDataSource : public pxr::HdContainerDataSource {
 public:
  HD_DECLARE_DATASOURCE(BlenderCameraIDPropertiesDataSource);

  void set_camera(const Camera *camera)
  {
    camera_ = camera;
  }

  pxr::TfTokenVector GetNames() override
  {
    pxr::TfTokenVector result;
    if (camera_ && camera_->id.properties) {
      for (const IDProperty &prop : camera_->id.properties->data.group) {
        result.emplace_back(prop.name);
      }
    }
    return result;
  }

  pxr::HdDataSourceBaseHandle Get(const pxr::TfToken &name) override
  {
    if (!camera_ || !camera_->id.properties) {
      return nullptr;
    }
    const IDProperty *prop = IDP_GetPropertyFromGroup(camera_->id.properties, name.GetText());
    if (!prop) {
      return nullptr;
    }
    switch (prop->type) {
      case IDP_INT:
        return pxr::HdRetainedTypedSampledDataSource<int>::New(IDP_int_get(prop));
      case IDP_FLOAT:
        return pxr::HdRetainedTypedSampledDataSource<float>::New(IDP_float_get(prop));
      case IDP_DOUBLE:
        return pxr::HdRetainedTypedSampledDataSource<double>::New(IDP_double_get(prop));
      case IDP_BOOLEAN:
        return pxr::HdRetainedTypedSampledDataSource<bool>::New(bool(IDP_bool_get(prop)));
      default:
        return nullptr;
    }
  }

 private:
  const Camera *camera_ = nullptr;
};

CameraDelegate::CameraDelegate(pxr::HdRenderIndex *render_index, pxr::SdfPath const &camera_id)
    : camera_scene_index_(pxr::HdRetainedSceneIndex::New()),
      camera_id_(camera_id),
      free_camera_ds_(pxr::HdxFreeCameraPrimDataSource::New()),
      id_properties_ds_(BlenderCameraIDPropertiesDataSource::New())
{
  pxr::HdContainerDataSourceHandle camera_ds = pxr::HdOverlayContainerDataSource::New(
      id_properties_ds_, free_camera_ds_);

  camera_scene_index_->AddPrims({{camera_id_, pxr::HdPrimTypeTokens->camera, camera_ds}});

  render_index->InsertSceneIndex(
      camera_scene_index_, pxr::SdfPath::AbsoluteRootPath(), /*needsPrefixing=*/false);
}

void CameraDelegate::sync(const Scene *scene)
{
  const Camera *camera = (scene && scene->camera) ? id_cast<const Camera *>(scene->camera->data) :
                                                    nullptr;
  id_properties_ds_->set_camera(camera);
  camera_scene_index_->DirtyPrims({{camera_id_, pxr::HdDataSourceLocator::EmptyLocator()}});
}

void CameraDelegate::SetCamera(pxr::GfCamera const &camera)
{
  pxr::HdDataSourceLocatorSet dirty_locators;
  free_camera_ds_->SetCamera(camera, &dirty_locators);
  if (!dirty_locators.IsEmpty()) {
    camera_scene_index_->DirtyPrims({{camera_id_, dirty_locators}});
  }
}

}  // namespace blender::io::hydra
