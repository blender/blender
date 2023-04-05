/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 */

#include "IndexedFaceSet.h"
#include "NodeCamera.h"
#include "NodeViewLayer.h"
#include "SceneVisitor.h"

#include "BLI_sys_types.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class SceneHash : public SceneVisitor {
 public:
  inline SceneHash() : SceneVisitor()
  {
    _sum = 1;
  }

  virtual ~SceneHash() {}

  VISIT_DECL(NodeCamera);
  VISIT_DECL(NodeViewLayer);
  VISIT_DECL(IndexedFaceSet);

  string toString();

  inline bool match()
  {
    return _sum == _prevSum;
  }

  inline void store()
  {
    _prevSum = _sum;
  }

  inline void reset()
  {
    _sum = 1;
  }

 private:
  void adler32(const unsigned char *data, int size);

  uint32_t _sum;
  uint32_t _prevSum;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SceneHash")
#endif
};

} /* namespace Freestyle */
