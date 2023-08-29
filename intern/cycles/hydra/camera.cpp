/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/camera.h"
#include "hydra/session.h"
#include "scene/camera.h"

#include <pxr/base/gf/frustum.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/usd/usdGeom/tokens.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

extern Transform convert_transform(const GfMatrix4d &matrix);
Transform convert_camera_transform(const GfMatrix4d &matrix, float metersPerUnit)
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

#if PXR_VERSION < 2102
// clang-format off
TF_DEFINE_PRIVATE_TOKENS(_tokens,
    (projection)
    (orthographic)
);
// clang-format on
#endif

HdCyclesCamera::HdCyclesCamera(const SdfPath &sprimId) : HdCamera(sprimId)
{
#if PXR_VERSION >= 2102
  // Synchronize default values
  _horizontalAperture = _data.GetHorizontalAperture() * GfCamera::APERTURE_UNIT;
  _verticalAperture = _data.GetVerticalAperture() * GfCamera::APERTURE_UNIT;
  _horizontalApertureOffset = _data.GetHorizontalApertureOffset() * GfCamera::APERTURE_UNIT;
  _verticalApertureOffset = _data.GetVerticalApertureOffset() * GfCamera::APERTURE_UNIT;
  _focalLength = _data.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT;
  _clippingRange = _data.GetClippingRange();
  _fStop = _data.GetFStop();
  _focusDistance = _data.GetFocusDistance();
#endif
}

HdCyclesCamera::~HdCyclesCamera() {}

HdDirtyBits HdCyclesCamera::GetInitialDirtyBitsMask() const
{
  return DirtyBits::AllDirty;
}

void HdCyclesCamera::Sync(HdSceneDelegate *sceneDelegate,
                          HdRenderParam *renderParam,
                          HdDirtyBits *dirtyBits)
{
  if (*dirtyBits == DirtyBits::Clean) {
    return;
  }

  VtValue value;
  const SdfPath &id = GetId();

#if PXR_VERSION >= 2102
  if (*dirtyBits & DirtyBits::DirtyTransform) {
    sceneDelegate->SampleTransform(id, &_transformSamples);

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
#else
  if (*dirtyBits & DirtyBits::DirtyViewMatrix) {
    sceneDelegate->SampleTransform(id, &_transformSamples);

    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->worldToViewMatrix);
    if (!value.IsEmpty()) {
      _worldToViewMatrix = value.Get<GfMatrix4d>();
      _worldToViewInverseMatrix = _worldToViewMatrix.GetInverse();
      _data.SetTransform(_worldToViewInverseMatrix);
    }
  }
#endif

#if PXR_VERSION < 2111
  if (*dirtyBits & DirtyBits::DirtyProjMatrix) {
    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->projectionMatrix);
    if (!value.IsEmpty()) {
      _projectionMatrix = value.Get<GfMatrix4d>();
      const float focalLength = _data.GetFocalLength();  // Get default focal length
#  if PXR_VERSION >= 2102
      _data.SetFromViewAndProjectionMatrix(GetViewMatrix(), _projectionMatrix, focalLength);
#  else
      if (_projectionMatrix[2][3] < -0.5) {
        _data.SetProjection(GfCamera::Perspective);

        const float horizontalAperture = (2.0 * focalLength) / _projectionMatrix[0][0];
        _data.SetHorizontalAperture(horizontalAperture);
        _data.SetHorizontalApertureOffset(0.5 * horizontalAperture * _projectionMatrix[2][0]);
        const float verticalAperture = (2.0 * focalLength) / _projectionMatrix[1][1];
        _data.SetVerticalAperture(verticalAperture);
        _data.SetVerticalApertureOffset(0.5 * verticalAperture * _projectionMatrix[2][1]);

        _data.SetClippingRange(
            GfRange1f(_projectionMatrix[3][2] / (_projectionMatrix[2][2] - 1.0),
                      _projectionMatrix[3][2] / (_projectionMatrix[2][2] + 1.0)));
      }
      else {
        _data.SetProjection(GfCamera::Orthographic);

        const float horizontalAperture = (2.0 / GfCamera::APERTURE_UNIT) / _projectionMatrix[0][0];
        _data.SetHorizontalAperture(horizontalAperture);
        _data.SetHorizontalApertureOffset(-0.5 * horizontalAperture * _projectionMatrix[3][0]);
        const float verticalAperture = (2.0 / GfCamera::APERTURE_UNIT) / _projectionMatrix[1][1];
        _data.SetVerticalAperture(verticalAperture);
        _data.SetVerticalApertureOffset(-0.5 * verticalAperture * _projectionMatrix[3][1]);

        const double nearMinusFarHalf = 1.0 / _projectionMatrix[2][2];
        const double nearPlusFarHalf = nearMinusFarHalf * _projectionMatrix[3][2];
        _data.SetClippingRange(
            GfRange1f(nearPlusFarHalf + nearMinusFarHalf, nearPlusFarHalf - nearMinusFarHalf));
      }
#  endif
    }
  }
#endif

  if (*dirtyBits & DirtyBits::DirtyWindowPolicy) {
    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->windowPolicy);
    if (!value.IsEmpty()) {
      _windowPolicy = value.Get<CameraUtilConformWindowPolicy>();
    }
  }

  if (*dirtyBits & DirtyBits::DirtyClipPlanes) {
    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->clipPlanes);
    if (!value.IsEmpty()) {
      _clipPlanes = value.Get<std::vector<GfVec4d>>();
    }
  }

  if (*dirtyBits & DirtyBits::DirtyParams) {
#if PXR_VERSION >= 2102
    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->projection);
    if (!value.IsEmpty()) {
      _projection = value.Get<Projection>();
      _data.SetProjection(_projection != Orthographic ? GfCamera::Perspective :
                                                        GfCamera::Orthographic);
    }
#else
    value = sceneDelegate->GetCameraParamValue(id, _tokens->projection);
    if (!value.IsEmpty()) {
      _data.SetProjection(value.Get<TfToken>() != _tokens->orthographic ? GfCamera::Perspective :
                                                                          GfCamera::Orthographic);
    }
#endif

    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->horizontalAperture);
    if (!value.IsEmpty()) {
      const auto horizontalAperture = value.Get<float>();
#if PXR_VERSION >= 2102
      _horizontalAperture = horizontalAperture;
#endif
      _data.SetHorizontalAperture(horizontalAperture / GfCamera::APERTURE_UNIT);
    }

    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->verticalAperture);
    if (!value.IsEmpty()) {
      const auto verticalAperture = value.Get<float>();
#if PXR_VERSION >= 2102
      _verticalAperture = verticalAperture;
#endif
      _data.SetVerticalAperture(verticalAperture / GfCamera::APERTURE_UNIT);
    }

    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->horizontalApertureOffset);
    if (!value.IsEmpty()) {
      const auto horizontalApertureOffset = value.Get<float>();
#if PXR_VERSION >= 2102
      _horizontalApertureOffset = horizontalApertureOffset;
#endif
      _data.SetHorizontalApertureOffset(horizontalApertureOffset / GfCamera::APERTURE_UNIT);
    }

    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->verticalApertureOffset);
    if (!value.IsEmpty()) {
      const auto verticalApertureOffset = value.Get<float>();
#if PXR_VERSION >= 2102
      _verticalApertureOffset = verticalApertureOffset;
#endif
      _data.SetVerticalApertureOffset(verticalApertureOffset / GfCamera::APERTURE_UNIT);
    }

    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->focalLength);
    if (!value.IsEmpty()) {
      const auto focalLength = value.Get<float>();
#if PXR_VERSION >= 2102
      _focalLength = focalLength;
#endif
      _data.SetFocalLength(focalLength / GfCamera::FOCAL_LENGTH_UNIT);
    }

    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->clippingRange);
    if (!value.IsEmpty()) {
      const auto clippingRange = value.Get<GfRange1f>();
#if PXR_VERSION >= 2102
      _clippingRange = clippingRange;
#endif
      _data.SetClippingRange(clippingRange);
    }

    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->fStop);
    if (!value.IsEmpty()) {
      const auto fStop = value.Get<float>();
#if PXR_VERSION >= 2102
      _fStop = fStop;
#endif
      _data.SetFStop(fStop);
    }

    value = sceneDelegate->GetCameraParamValue(id, HdCameraTokens->focusDistance);
    if (!value.IsEmpty()) {
      const auto focusDistance = value.Get<float>();
#if PXR_VERSION >= 2102
      _focusDistance = focusDistance;
#endif
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
                                         const std::vector<GfVec4d> &clipPlanes,
                                         Camera *cam)
{
#if PXR_VERSION >= 2102
  GfCamera data;
  data.SetFromViewAndProjectionMatrix(worldToViewMatrix, projectionMatrix);

  ApplyCameraSettings(renderParam, data, CameraUtilFit, cam);
#else
  TF_CODING_ERROR("Not implemented");
#endif
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
