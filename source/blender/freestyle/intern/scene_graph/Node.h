/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Abstract class for scene graph nodes. Inherits from BaseObject which defines the addRef
 * release mechanism.
 */

#include "SceneVisitor.h"

#include "../system/BaseObject.h"
#include "../system/FreestyleConfig.h"
#include "../system/Precision.h"

#include "../geometry/BBox.h"
#include "../geometry/Geom.h"

using namespace std;

namespace Freestyle {

using namespace Geometry;

class Node : public BaseObject {
 public:
  inline Node() : BaseObject() {}

  inline Node(const Node &iBrother) : BaseObject()
  {
    _BBox = iBrother.bbox();
  }

  virtual ~Node() {}

  /** Accept the corresponding visitor
   *  Each inherited node must overload this method
   */
  virtual void accept(SceneVisitor &v)
  {
    v.visitNode(*this);
  }

  /** bounding box management */
  /** Returns the node bounding box
   *  If no bounding box exists, an empty bbox is returned
   */
  virtual const BBox<Vec3r> &bbox() const
  {
    return _BBox;
  }

  /** Sets the Node bounding box */
  virtual void setBBox(const BBox<Vec3r> &iBox)
  {
    _BBox = iBox;
  }

  /** Makes the union of _BBox and iBox */
  virtual void AddBBox(const BBox<Vec3r> &iBox)
  {
    if (iBox.empty()) {
      return;
    }

    if (_BBox.empty()) {
      _BBox = iBox;
    }
    else {
      _BBox += iBox;
    }
  }

  /** Updates the BBox */
  virtual const BBox<Vec3r> &UpdateBBox()
  {
    return _BBox;
  }

  /** Clears the bounding box */
  virtual void clearBBox()
  {
    _BBox.clear();
  }

 protected:
 private:
  BBox<Vec3r> _BBox;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Node")
#endif
};

} /* namespace Freestyle */
