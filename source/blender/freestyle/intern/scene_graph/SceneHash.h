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

#ifndef __FREESTYLE_SCENE_HASH_H__
#define __FREESTYLE_SCENE_HASH_H__

/** \file
 * \ingroup freestyle
 */

#include "IndexedFaceSet.h"
#include "NodeViewLayer.h"
#include "NodeCamera.h"
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

  virtual ~SceneHash()
  {
  }

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
  void adler32(unsigned char *data, int size);

  uint32_t _sum;
  uint32_t _prevSum;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SceneHash")
#endif
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_SCENE_HASH_H__
