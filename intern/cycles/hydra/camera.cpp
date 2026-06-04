/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/camera.h"
#include "hydra/session.h"
#include "hydra/util.h"
#include "scene/camera.h"

#include <pxr/base/gf/frustum.h>
#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/usd/usdGeom/tokens.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

extern Transform convert_transform(const GfMatrix4d &matrix);
static Transform convert_camera_transform(const GfMatrix4d &matrix, const float metersPerUnit)
{
  Transform t = convert_transform(matrix);
  // Flip Z axis
  t.x.z *= -1.0f;
  t.y.z *= -1.0f;
  t.z.z *= -1.0f;
  // Scale translation
  t.x.w *= metersPerUnit;
  t.y.w *= metersPerUnit;
  t.z.w *= metersPerUnit;
  return t;
}

HdCyclesCamera::HdCyclesCamera(const SdfPath &sprimId) : HdCamera(sprimId)
{
  // Synchronize default values
  _horizontalAperture = _data.GetHorizontalAperture() * GfCamera::APERTURE_UNIT;
  _verticalAperture = _data.GetVerticalAperture() * GfCamera::APERTURE_UNIT;
  _horizontalApertureOffset = _data.GetHorizontalApertureOffset() * GfCamera::APERTURE_UNIT;
  _verticalApertureOffset = _data.GetVerticalApertureOffset() * GfCamera::APERTURE_UNIT;
  _focalLength = _data.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT;
  _clippingRange = _data.GetClippingRange();
  _fStop = _data.GetFStop();
  _focusDistance = _data.GetFocusDistance();
}

HdCyclesCamera::~HdCyclesCamera() = default;

HdDirtyBits HdCyclesCamera::GetInitialDirtyBitsMask() const
{
  return DirtyBits::AllDirty;
}

void HdCyclesCamera::Sync(HdSceneDelegate *sceneDelegate,
                          HdRenderParam * /*renderParam*/,
                          HdDirtyBits *dirtyBits)
{
  if (*dirtyBits == DirtyBits::Clean) {
    return;
  }

  const SdfPath &id = GetId();
  const HdSceneIndexPrim prim = GetPrim(sceneDelegate, id);
  const HdContainerDataSourceHandle &primDs = prim.dataSource;

  HdCameraSchema cameraSchema = HdCameraSchema::GetFromParent(primDs);
  HdContainerDataSourceHandle cameraDs = cameraSchema.GetContainer();

  /* Read the shutter window from the camera prim so animated transforms can
   * be sampled across it for motion blur. */
  float shutterOpen = 0.0f;
  float shutterClose = 0.0f;
  if (auto ds = cameraSchema.GetShutterOpen()) {
    shutterOpen = float(ds->GetTypedValue(0.0f));
  }
  if (auto ds = cameraSchema.GetShutterClose()) {
    shutterClose = float(ds->GetTypedValue(0.0f));
  }

  if (*dirtyBits & DirtyBits::DirtyTransform) {
    HdXformSchema xformSchema = HdXformSchema::GetFromParent(primDs);
    if (auto matrixDs = xformSchema.GetMatrix()) {
      SampleTyped<GfMatrix4d, 2>(matrixDs, shutterOpen, shutterClose, &_transformSamples);
      bool transform_found = false;
      for (size_t i = 0; i < _transformSamples.count; ++i) {
        if (_transformSamples.times[i] == 0.0f) {
          _transform = _transformSamples.values[i];
          _data.SetTransform(_transform);
          transform_found = true;
          break;
        }
      }
      if (!transform_found && _transformSamples.count) {
        _transform = _transformSamples.values[0];
        _data.SetTransform(_transform);
      }
    }
  }

  if (*dirtyBits & DirtyBits::DirtyWindowPolicy) {
    /* The window policy is not part of the camera schema; it lives on the
     * legacy data source. Leave the default of `CameraUtilFit`. */
  }

  if (*dirtyBits & DirtyBits::DirtyClipPlanes) {
    if (auto ds = cameraSchema.GetClippingPlanes()) {
      const VtArray<GfVec4d> clipPlanes = ds->GetTypedValue(0.0f);
      _clipPlanes.assign(clipPlanes.cbegin(), clipPlanes.cend());
    }
  }

  if (*dirtyBits & DirtyBits::DirtyParams) {
    if (auto ds = cameraSchema.GetProjection()) {
      const TfToken projection = ds->GetTypedValue(0.0f);
      _projection = (projection == HdCameraSchemaTokens->orthographic) ? Orthographic :
                                                                         Perspective;
      _data.SetProjection(_projection != Orthographic ? GfCamera::Perspective :
                                                        GfCamera::Orthographic);
    }

    if (auto ds = cameraSchema.GetHorizontalAperture()) {
      const float horizontalAperture = ds->GetTypedValue(0.0f);
      _horizontalAperture = horizontalAperture;
      _data.SetHorizontalAperture(horizontalAperture / GfCamera::APERTURE_UNIT);
    }

    if (auto ds = cameraSchema.GetVerticalAperture()) {
      const float verticalAperture = ds->GetTypedValue(0.0f);
      _verticalAperture = verticalAperture;
      _data.SetVerticalAperture(verticalAperture / GfCamera::APERTURE_UNIT);
    }

    if (auto ds = cameraSchema.GetHorizontalApertureOffset()) {
      const float horizontalApertureOffset = ds->GetTypedValue(0.0f);
      _horizontalApertureOffset = horizontalApertureOffset;
      _data.SetHorizontalApertureOffset(horizontalApertureOffset / GfCamera::APERTURE_UNIT);
    }

    if (auto ds = cameraSchema.GetVerticalApertureOffset()) {
      const float verticalApertureOffset = ds->GetTypedValue(0.0f);
      _verticalApertureOffset = verticalApertureOffset;
      _data.SetVerticalApertureOffset(verticalApertureOffset / GfCamera::APERTURE_UNIT);
    }

    if (auto ds = cameraSchema.GetFocalLength()) {
      const float focalLength = ds->GetTypedValue(0.0f);
      _focalLength = focalLength;
      _data.SetFocalLength(focalLength / GfCamera::FOCAL_LENGTH_UNIT);
    }

    if (auto ds = cameraSchema.GetClippingRange()) {
      const GfVec2f range = ds->GetTypedValue(0.0f);
      const GfRange1f clippingRange(range[0], range[1]);
      _clippingRange = clippingRange;
      _data.SetClippingRange(clippingRange);
    }

    if (auto ds = cameraSchema.GetFStop()) {
      const float fStop = ds->GetTypedValue(0.0f);
      _fStop = fStop;
      _data.SetFStop(fStop);
    }

    if (auto ds = cameraSchema.GetFocusDistance()) {
      const float focusDistance = ds->GetTypedValue(0.0f);
      _focusDistance = focusDistance;
      _data.SetFocusDistance(focusDistance);
    }
  }

  *dirtyBits = DirtyBits::Clean;
}

void HdCyclesCamera::Finalize(HdRenderParam *renderParam)
{
  HdCamera::Finalize(renderParam);
}

void HdCyclesCamera::ApplyCameraSettings(HdRenderParam *renderParam, Camera *cam) const
{
  ApplyCameraSettings(renderParam, _data, _windowPolicy, cam);

  const float metersPerUnit = static_cast<HdCyclesSession *>(renderParam)->GetStageMetersPerUnit();

  array<Transform> motion(_transformSamples.count);
  for (size_t i = 0; i < _transformSamples.count; ++i) {
    motion[i] = convert_camera_transform(_transformSamples.values[i], metersPerUnit);
  }
  cam->set_motion(motion);
}

void HdCyclesCamera::ApplyCameraSettings(HdRenderParam *renderParam,
                                         const GfCamera &dataUnconformedWindow,
                                         CameraUtilConformWindowPolicy windowPolicy,
                                         Camera *cam)
{
  const float width = cam->get_full_width();
  const float height = cam->get_full_height();

  auto data = dataUnconformedWindow;
  CameraUtilConformWindow(&data, windowPolicy, width / height);

  if (data.GetProjection() == GfCamera::Orthographic) {
    cam->set_camera_type(CAMERA_ORTHOGRAPHIC);
  }
  else {
    cam->set_camera_type(CAMERA_PERSPECTIVE);
  }

  const float metersPerUnit = static_cast<HdCyclesSession *>(renderParam)->GetStageMetersPerUnit();

  auto viewplane = data.GetFrustum().GetWindow();
  auto focalLength = 1.0f;
  if (data.GetProjection() == GfCamera::Perspective) {
    viewplane *= 2.0 / viewplane.GetSize()[1];  // Normalize viewplane
    focalLength = data.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT * metersPerUnit;

    cam->set_fov(GfDegreesToRadians(data.GetFieldOfView(GfCamera::FOVVertical)));
  }

  cam->set_sensorwidth(data.GetHorizontalAperture() * GfCamera::APERTURE_UNIT * metersPerUnit);
  cam->set_sensorheight(data.GetVerticalAperture() * GfCamera::APERTURE_UNIT * metersPerUnit);

  cam->set_nearclip(data.GetClippingRange().GetMin() * metersPerUnit);
  cam->set_farclip(data.GetClippingRange().GetMax() * metersPerUnit);

  cam->set_viewplane_left(viewplane.GetMin()[0]);
  cam->set_viewplane_right(viewplane.GetMax()[0]);
  cam->set_viewplane_bottom(viewplane.GetMin()[1]);
  cam->set_viewplane_top(viewplane.GetMax()[1]);

  if (data.GetFStop() != 0.0f) {
    cam->set_focaldistance(data.GetFocusDistance() * metersPerUnit);
    cam->set_aperturesize(focalLength / (2.0f * data.GetFStop()));
  }

  cam->set_matrix(convert_camera_transform(data.GetTransform(), metersPerUnit));
}

void HdCyclesCamera::ApplyCameraSettings(HdRenderParam *renderParam,
                                         const GfMatrix4d &worldToViewMatrix,
                                         const GfMatrix4d &projectionMatrix,
                                         const std::vector<GfVec4d> & /*clipPlanes*/,
                                         Camera *cam)
{
  GfCamera data;
  data.SetFromViewAndProjectionMatrix(worldToViewMatrix, projectionMatrix);

  ApplyCameraSettings(renderParam, data, CameraUtilFit, cam);
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
