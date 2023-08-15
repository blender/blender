/* SPDX-FileCopyrightText: 2008-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define the representation of a triangle
 */

#include "TriangleRep.h"

namespace Freestyle {

void TriangleRep::ComputeBBox()
{
  real XMax = _vertices[0][0];
  real YMax = _vertices[0][1];
  real ZMax = _vertices[0][2];

  real XMin = _vertices[0][0];
  real YMin = _vertices[0][1];
  real ZMin = _vertices[0][2];

  // parse all the coordinates to find the XMax, YMax, ZMax
  for (int i = 0; i < 3; ++i) {
    // X
    if (_vertices[i][0] > XMax) {
      XMax = _vertices[i][0];
    }
    if (_vertices[i][0] < XMin) {
      XMin = _vertices[i][0];
    }

    // Y
    if (_vertices[i][1] > YMax) {
      YMax = _vertices[i][1];
    }
    if (_vertices[i][1] < YMin) {
      YMin = _vertices[i][1];
    }

    // Z
    if (_vertices[i][2] > ZMax) {
      ZMax = _vertices[i][2];
    }
    if (_vertices[i][2] < ZMin) {
      ZMin = _vertices[i][2];
    }
  }

  setBBox(BBox<Vec3r>(Vec3r(XMin, YMin, ZMin), Vec3r(XMax, YMax, ZMax)));
}

} /* namespace Freestyle */
