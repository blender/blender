/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to represent a light node
 */

#include "Node.h"

#include "../geometry/Geom.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

using namespace Geometry;

class NodeOrthographicCamera;

class NodePerspectiveCamera;

class NodeCamera : public Node {
 public:
  typedef enum {
    PERSPECTIVE,
    ORTHOGRAPHIC,
    GENERIC,
  } CameraType;

  /** Default matrices: Identity for both projection and modelview. */
  NodeCamera(CameraType camera_type = GENERIC);
#if 0 /* UNUSED, gives warning in gcc */
  NodeCamera(const NodeCamera &iBrother);
#endif

  virtual ~NodeCamera() {}

  /** Accept the corresponding visitor */
  virtual void accept(SceneVisitor &v);

  /** Matrix is copied */
  void setModelViewMatrix(double modelview_matrix[16]);

  /** Matrix is copied */
  void setProjectionMatrix(double projection_matrix[16]);

  double *modelViewMatrix()
  {
    return modelview_matrix_;
  }

  double *projectionMatrix()
  {
    return projection_matrix_;
  }

 protected:
  // row major right handed matrix
  double modelview_matrix_[16];
  // row major right handed matrix
  double projection_matrix_[16];

  CameraType camera_type_;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:NodeCamera")
#endif
};

class NodeOrthographicCamera : public NodeCamera {
 public:
  NodeOrthographicCamera();

  /** Builds a parallel projection matrix a la glOrtho.
   *    A   0   0   tx
   *    0   B   0   ty
   *    0   0   C   tz
   *    0   0   0   1
   *
   *  where
   *    A =   2              / (right - left)
   *    B =   2              / (top   - bottom)
   *    C =  -2              / (far   - near)
   *    tx = -(right + left) / (right - left)
   *    ty = -(top + bottom) / (top   - bottom)
   *    tz = -(zFar + zNear) / (zFar  - zNear)
   */
  NodeOrthographicCamera(
      double left, double right, double bottom, double top, double zNear, double zFar);

  double left() const
  {
    return left_;
  }

  double right() const
  {
    return right_;
  }

  double bottom() const
  {
    return bottom_;
  }

  double top() const
  {
    return top_;
  }

  double zNear() const
  {
    return zNear_;
  }

  double zFar() const
  {
    return zFar_;
  }

  NodeOrthographicCamera(const NodeOrthographicCamera &iBrother) = default;

 private:
  double left_;
  double right_;
  double bottom_;
  double top_;
  double zNear_;
  double zFar_;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:NodeOrthographicCamera")
#endif
};

class NodePerspectiveCamera : public NodeCamera {
 public:
  NodePerspectiveCamera();

  /** Builds a perspective projection matrix a la gluPerspective.
   *  Given f defined as follows:
   *    fovy
   *    f = cotangent(____)
   *    2
   *  The generated matrix is
   *    (   f                                                         )
   *    | ______            0             0                0          |
   *    | aspect                                                      |
   *    |                                                             |
   *    |   0               f             0                0          |
   *    |                                                             |
   *    |                             zNear+zFar      2*zNear*zFar    |
   *    |   0               0         __________      ____________    |
   *    |                             zNear-zFar       zNear-zFar     |
   *    |                                                             |
   *    (   0               0             -1               0          )
   *  \param fovy:
   *    Field of View specified in radians.
   */
  NodePerspectiveCamera(double fovy, double aspect, double zNear, double zFar);

  /** Builds a perspective projection matrix a la glFrustum.
   *    (  2*zNear                                                         )
   *    | __________           0               A               0           |
   *    | right-left                                                       |
   *    |                                                                  |
   *    |                   2*zNear                                        |
   *    |     0            __________          B               0           |
   *    |                  top-bottom                                      |
   *    |                                                                  |
   *    |     0                0               C               D           |
   *    |                                                                  |
   *    |     0                0               -1              0           |
   *    (                                                                  )
   *
   *        right+left
   *    A = __________
   *        right-left
   *
   *        top+bottom
   *    B = __________
   *        top-bottom
   *
   *          zFar+zNear
   *    C = - __________
   *          zFar-zNear
   *
   *          2*zFar*zNear
   *    D = - ____________
   *           zFar-zNear
   */
  NodePerspectiveCamera(
      double left, double right, double bottom, double top, double zNear, double zFar);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:NodePerspectiveCamera")
#endif
};

} /* namespace Freestyle */
