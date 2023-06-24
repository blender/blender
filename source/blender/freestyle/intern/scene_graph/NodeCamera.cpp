/* SPDX-FileCopyrightText: 2008-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to represent a light node
 */

#include <cmath>
#include <cstring>  // for memcpy

#include "NodeCamera.h"

namespace Freestyle {

static void loadIdentity(double *matrix)
{
  int i;

  // Build Identity matrix
  for (i = 0; i < 16; ++i) {
    double value;
    if ((i % 5) == 0) {
      value = 1.0;
    }
    else {
      value = 0;
    }
    matrix[i] = value;
  }
}

NodeCamera::NodeCamera(CameraType camera_type) : camera_type_(camera_type)
{
  loadIdentity(modelview_matrix_);
  loadIdentity(projection_matrix_);
}

#if 0 /* UNUSED, gives warning in gcc */
NodeCamera::NodeCamera(const NodeCamera &iBrother) : camera_type_(iBrother.camera_type_)
{
  memcpy(modelview_matrix_, iBrother.modelview_matrix_, sizeof(double[16]));
  memcpy(projection_matrix_, iBrother.projection_matrix_, sizeof(double[16]));
}
#endif

void NodeCamera::accept(SceneVisitor &v)
{
  v.visitNodeCamera(*this);
}

void NodeCamera::setModelViewMatrix(double modelview_matrix[16])
{
  memcpy(modelview_matrix_, modelview_matrix, sizeof(double[16]));
}

void NodeCamera::setProjectionMatrix(double projection_matrix[16])
{
  memcpy(projection_matrix_, projection_matrix, sizeof(double[16]));
}

NodeOrthographicCamera::NodeOrthographicCamera()
    : NodeCamera(NodeCamera::ORTHOGRAPHIC),
      left_(0),
      right_(0),
      bottom_(0),
      top_(0),
      zNear_(0),
      zFar_(0)
{
  loadIdentity(projection_matrix_);
  loadIdentity(modelview_matrix_);
}

NodeOrthographicCamera::NodeOrthographicCamera(
    double left, double right, double bottom, double top, double zNear, double zFar)
    : NodeCamera(NodeCamera::ORTHOGRAPHIC),
      left_(left),
      right_(right),
      bottom_(bottom),
      top_(top),
      zNear_(zNear),
      zFar_(zFar)
{
  loadIdentity(projection_matrix_);

  projection_matrix_[0] = 2.0 / (right - left);
  projection_matrix_[3] = -(right + left) / (right - left);
  projection_matrix_[5] = 2.0 / (top - bottom);
  projection_matrix_[7] = -(top + bottom) / (top - bottom);
  projection_matrix_[10] = -2.0 / (zFar - zNear);
  projection_matrix_[11] = -(zFar + zNear) / (zFar - zNear);
}

NodePerspectiveCamera::NodePerspectiveCamera() : NodeCamera(NodeCamera::PERSPECTIVE) {}

NodePerspectiveCamera::NodePerspectiveCamera(double fovy, double aspect, double zNear, double zFar)
    : NodeCamera(NodeCamera::PERSPECTIVE)
{
  loadIdentity(projection_matrix_);

  double f = cos(fovy / 2.0) / sin(fovy / 2.0);  // cotangent

  projection_matrix_[0] = f / aspect;
  projection_matrix_[5] = f;
  projection_matrix_[10] = (zNear + zFar) / (zNear - zFar);
  projection_matrix_[11] = (2.0 * zNear * zFar) / (zNear - zFar);
  projection_matrix_[14] = -1.0;
  projection_matrix_[15] = 0;
}

NodePerspectiveCamera::NodePerspectiveCamera(
    double left, double right, double bottom, double top, double zNear, double zFar)
    : NodeCamera(NodeCamera::PERSPECTIVE)
{
  loadIdentity(projection_matrix_);

  projection_matrix_[0] = (2.0 * zNear) / (right - left);
  projection_matrix_[2] = (right + left) / (right - left);
  projection_matrix_[5] = (2.0 * zNear) / (top - bottom);
  projection_matrix_[6] = (top + bottom) / (top - bottom);
  projection_matrix_[10] = -(zFar + zNear) / (zFar - zNear);
  projection_matrix_[11] = -(2.0 * zFar * zNear) / (zFar - zNear);
  projection_matrix_[14] = -1.0;
  projection_matrix_[15] = 0;
}

} /* namespace Freestyle */
