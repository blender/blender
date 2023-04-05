/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to build a Node Tree designed to be displayed from a set of strokes structure.
 */

#include "Stroke.h"

#include "../scene_graph/LineRep.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class StrokeTesselator {
 public:
  inline StrokeTesselator()
  {
    _FrsMaterial.setDiffuse(0, 0, 0, 1);
    _overloadFrsMaterial = false;
  }

  virtual ~StrokeTesselator() {}

  /** Builds a line rep contained from a Stroke */
  LineRep *Tesselate(Stroke *iStroke);

  /** Builds a set of lines rep contained under a NodeShape, itself contained under a NodeGroup
   *  from a set of strokes.
   */
  template<class StrokeIterator> NodeGroup *Tesselate(StrokeIterator begin, StrokeIterator end);

  inline void setFrsMaterial(const FrsMaterial &iMaterial)
  {
    _FrsMaterial = iMaterial;
    _overloadFrsMaterial = true;
  }

  inline const FrsMaterial &frs_material() const
  {
    return _FrsMaterial;
  }

 private:
  FrsMaterial _FrsMaterial;
  bool _overloadFrsMaterial;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:StrokeTesselator")
#endif
};

} /* namespace Freestyle */
