/* SPDX-FileCopyrightText: 2008-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define the representation of 3D Line.
 */

#include "LineRep.h"

namespace Freestyle {

void LineRep::ComputeBBox()
{
  real XMax = _vertices.front()[0];
  real YMax = _vertices.front()[1];
  real ZMax = _vertices.front()[2];

  real XMin = _vertices.front()[0];
  real YMin = _vertices.front()[1];
  real ZMin = _vertices.front()[2];

  // parse all the coordinates to find
  // the XMax, YMax, ZMax
  vector<Vec3r>::iterator v;
  for (v = _vertices.begin(); v != _vertices.end(); ++v) {
    // X
    if ((*v)[0] > XMax) {
      XMax = (*v)[0];
    }
    if ((*v)[0] < XMin) {
      XMin = (*v)[0];
    }

    // Y
    if ((*v)[1] > YMax) {
      YMax = (*v)[1];
    }
    if ((*v)[1] < YMin) {
      YMin = (*v)[1];
    }

    // Z
    if ((*v)[2] > ZMax) {
      ZMax = (*v)[2];
    }
    if ((*v)[2] < ZMin) {
      ZMin = (*v)[2];
    }
  }

  setBBox(BBox<Vec3r>(Vec3r(XMin, YMin, ZMin), Vec3r(XMax, YMax, ZMax)));
}

} /* namespace Freestyle */
