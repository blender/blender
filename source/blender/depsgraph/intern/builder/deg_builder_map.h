/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/depsgraph_type.h"

struct ID;

namespace blender::deg {

class BuilderMap {
 public:
  enum {
    TAG_ANIMATION = (1 << 0),
    TAG_PARAMETERS = (1 << 1),
    TAG_TRANSFORM = (1 << 2),
    TAG_GEOMETRY = (1 << 3),

    TAG_SCENE_COMPOSITOR = (1 << 4),
    TAG_SCENE_SEQUENCER = (1 << 5),
    TAG_SCENE_AUDIO = (1 << 6),

    /* All ID components has been built. */
    TAG_COMPLETE = (TAG_ANIMATION | TAG_PARAMETERS | TAG_TRANSFORM | TAG_GEOMETRY |
                    TAG_SCENE_COMPOSITOR | TAG_SCENE_SEQUENCER | TAG_SCENE_AUDIO),
  };

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

  Map<ID *, int> id_tags_;
};

}  // namespace blender::deg
