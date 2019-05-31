/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup freestyle
 * \brief Class to represent a transform node. A Transform node contains one or several children,
 * \brief all affected by the transformation.
 */

#include "NodeTransform.h"

#include "BLI_math.h"

namespace Freestyle {

void NodeTransform::Translate(real x, real y, real z)
{
  _Matrix(0, 3) += x;
  _Matrix(1, 3) += y;
  _Matrix(2, 3) += z;
}

void NodeTransform::Rotate(real iAngle, real x, real y, real z)
{
  // Normalize the x,y,z vector;
  real norm = (real)sqrt(x * x + y * y + z * z);
  if (0 == norm) {
    return;
  }

  x /= norm;
  y /= norm;
  z /= norm;

  /* find the corresponding matrix with the Rodrigues formula:
   * R = I + sin(iAngle)*Ntilda + (1-cos(iAngle))*Ntilda*Ntilda
   */
  Matrix33r Ntilda;
  Ntilda(0, 0) = Ntilda(1, 1) = Ntilda(2, 2) = 0.0f;
  Ntilda(0, 1) = -z;
  Ntilda(0, 2) = y;
  Ntilda(1, 0) = z;
  Ntilda(1, 2) = -x;
  Ntilda(2, 0) = -y;
  Ntilda(2, 1) = x;

  const Matrix33r Ntilda2(Ntilda * Ntilda);

  const real sinAngle = (real)sin((iAngle / 180.0f) * M_PI);
  const real cosAngle = (real)cos((iAngle / 180.0f) * M_PI);

  Matrix33r NS(Ntilda * sinAngle);
  Matrix33r NC(Ntilda2 * (1.0f - cosAngle));
  Matrix33r R;
  R = Matrix33r::identity();
  R += NS + NC;

  // R4 is the corresponding 4x4 matrix
  Matrix44r R4;
  R4 = Matrix44r::identity();

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      R4(i, j) = R(i, j);
    }
  }

  // Finally, we multiply our current matrix by R4:
  Matrix44r mat_tmp(_Matrix);
  _Matrix = mat_tmp * R4;
}

void NodeTransform::Scale(real x, real y, real z)
{
  _Matrix(0, 0) *= x;
  _Matrix(1, 1) *= y;
  _Matrix(2, 2) *= z;

  _Scaled = true;
}

void NodeTransform::MultiplyMatrix(const Matrix44r &iMatrix)
{
  Matrix44r mat_tmp(_Matrix);
  _Matrix = mat_tmp * iMatrix;
}

void NodeTransform::setMatrix(const Matrix44r &iMatrix)
{
  _Matrix = iMatrix;
  if (isScaled(iMatrix)) {
    _Scaled = true;
  }
}

void NodeTransform::accept(SceneVisitor &v)
{
  v.visitNodeTransform(*this);

  v.visitNodeTransformBefore(*this);
  for (vector<Node *>::iterator node = _Children.begin(), end = _Children.end(); node != end;
       ++node) {
    (*node)->accept(v);
  }
  v.visitNodeTransformAfter(*this);
}

void NodeTransform::AddBBox(const BBox<Vec3r> &iBBox)
{
  Vec3r oldMin(iBBox.getMin());
  Vec3r oldMax(iBBox.getMax());

  // compute the 8 corners of the bbox
  HVec3r box[8];
  box[0] = HVec3r(iBBox.getMin());
  box[1] = HVec3r(oldMax[0], oldMin[1], oldMin[2]);
  box[2] = HVec3r(oldMax[0], oldMax[1], oldMin[2]);
  box[3] = HVec3r(oldMin[0], oldMax[1], oldMin[2]);
  box[4] = HVec3r(oldMin[0], oldMin[1], oldMax[2]);
  box[5] = HVec3r(oldMax[0], oldMin[1], oldMax[2]);
  box[6] = HVec3r(oldMax[0], oldMax[1], oldMax[2]);
  box[7] = HVec3r(oldMin[0], oldMax[1], oldMax[2]);

  // Computes the transform iBBox
  HVec3r tbox[8];
  unsigned int i;
  for (i = 0; i < 8; i++) {
    tbox[i] = _Matrix * box[i];
  }

  Vec3r newMin(tbox[0]);
  Vec3r newMax(tbox[0]);
  for (i = 0; i < 8; i++) {
    for (unsigned int j = 0; j < 3; j++) {
      if (newMin[j] > tbox[i][j]) {
        newMin[j] = tbox[i][j];
      }
      if (newMax[j] < tbox[i][j]) {
        newMax[j] = tbox[i][j];
      }
    }
  }

  BBox<Vec3r> transformBox(newMin, newMax);

  Node::AddBBox(transformBox);
}

bool NodeTransform::isScaled(const Matrix44r &M)
{
  for (unsigned int j = 0; j < 3; j++) {
    real norm = 0;
    for (unsigned int i = 0; i < 3; i++) {
      norm += M(i, j) * M(i, j);
    }
    if ((norm > 1.01) || (norm < 0.99)) {
      return true;
    }
  }

  return false;
}

} /* namespace Freestyle */
