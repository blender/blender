/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to build a shape node. It contains a Rep, which is the shape geometry
 */

#include <vector>

#include "../geometry/BBox.h"
#include "../geometry/Geom.h"

#include "../system/FreestyleConfig.h"

#include "FrsMaterial.h"
#include "Node.h"
#include "Rep.h"

using namespace std;

namespace Freestyle {

using namespace Geometry;

class NodeShape : public Node {
 public:
  inline NodeShape() : Node() {}

  virtual ~NodeShape();

  /** Adds a Rep to the _Shapes list
   *  The delete of the rep is done when it is not used any more by the Scene Manager.
   *  So, it must not be deleted by the caller
   */
  virtual void AddRep(Rep *iRep)
  {
    if (nullptr == iRep) {
      return;
    }
    _Shapes.push_back(iRep);
    iRep->addRef();

    // updates bbox:
    AddBBox(iRep->bbox());
  }

  /** Accept the corresponding visitor */
  virtual void accept(SceneVisitor &v);

  /** Sets the shape material */
  inline void setFrsMaterial(const FrsMaterial &iMaterial)
  {
    _FrsMaterial = iMaterial;
  }

  /** accessors */
  /** returns the shape's material */
  inline FrsMaterial &frs_material()
  {
    return _FrsMaterial;
  }

  inline const vector<Rep *> &shapes()
  {
    return _Shapes;
  }

 private:
  /** list of shapes */
  vector<Rep *> _Shapes;

  /** Shape Material */
  FrsMaterial _FrsMaterial;
};

} /* namespace Freestyle */
