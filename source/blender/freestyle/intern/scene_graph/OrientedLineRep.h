/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to display an oriented line representation.
 */

#include "LineRep.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

class OrientedLineRep : public LineRep {
 public:
  OrientedLineRep() : LineRep() {}
  /** Builds a single line from 2 vertices
   *  v1
   *    first vertex
   *  v2
   *    second vertex
   */
  inline OrientedLineRep(const Vec3r &v1, const Vec3r &v2) : LineRep(v1, v2) {}

  /** Builds a line rep from a vertex chain */
  inline OrientedLineRep(const vector<Vec3r> &vertices) : LineRep(vertices) {}

  /** Builds a line rep from a vertex chain */
  inline OrientedLineRep(const list<Vec3r> &vertices) : LineRep(vertices) {}

  virtual ~OrientedLineRep() {}

  /** Accept the corresponding visitor */
  virtual void accept(SceneVisitor &v);
};

} /* namespace Freestyle */
