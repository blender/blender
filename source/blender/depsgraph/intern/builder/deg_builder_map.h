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
 *
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/depsgraph_type.h"

struct GSet;
struct ID;

namespace DEG {

class BuilderMap {
 public:
  enum {
    TAG_ANIMATION = (1 << 0),
    TAG_PARAMETERS = (1 << 1),
    TAG_TRANSFORM = (1 << 2),
    TAG_GEOMETRY = (1 << 3),

    /* All ID components has been built. */
    TAG_COMPLETE = (TAG_ANIMATION | TAG_PARAMETERS | TAG_TRANSFORM | TAG_GEOMETRY),
  };

  BuilderMap();
  ~BuilderMap();

  /* Check whether given ID is already handled by builder (or if it's being handled). */
  bool checkIsBuilt(ID *id, int tag = TAG_COMPLETE) const;

  /* Tag given ID as handled/built. */
  void tagBuild(ID *id, int tag = TAG_COMPLETE);

  /* Combination of previous two functions, returns truth if ID was already handled, or tags is
   * handled otherwise and return false. */
  bool checkIsBuiltAndTag(ID *id, int tag = TAG_COMPLETE);

  template<typename T> bool checkIsBuilt(T *datablock, int tag = TAG_COMPLETE) const
  {
    return checkIsBuilt(&datablock->id, tag);
  }
  template<typename T> void tagBuild(T *datablock, int tag = TAG_COMPLETE)
  {
    tagBuild(&datablock->id, tag);
  }
  template<typename T> bool checkIsBuiltAndTag(T *datablock, int tag = TAG_COMPLETE)
  {
    return checkIsBuiltAndTag(&datablock->id, tag);
  }

 protected:
  int getIDTag(ID *id) const;

  typedef map<ID *, int> IDTagMap;
  IDTagMap id_tags_;
};

}  // namespace DEG
