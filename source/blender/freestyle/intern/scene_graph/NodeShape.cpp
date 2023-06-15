/* SPDX-FileCopyrightText: 2008-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to build a shape node. It contains a Rep, which is the shape geometry
 */

#include "NodeShape.h"

namespace Freestyle {

NodeShape::~NodeShape()
{
  vector<Rep *>::iterator rep;

  if (!_Shapes.empty()) {
    for (rep = _Shapes.begin(); rep != _Shapes.end(); ++rep) {
      int refCount = (*rep)->destroy();
      if (0 == refCount) {
        delete (*rep);
      }
    }

    _Shapes.clear();
  }
}

void NodeShape::accept(SceneVisitor &v)
{
  v.visitNodeShape(*this);

  v.visitFrsMaterial(_FrsMaterial);

  v.visitNodeShapeBefore(*this);
  vector<Rep *>::iterator rep;
  for (rep = _Shapes.begin(); rep != _Shapes.end(); ++rep) {
    (*rep)->accept(v);
  }
  v.visitNodeShapeAfter(*this);
}

} /* namespace Freestyle */
