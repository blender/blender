/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "BLI_map.hh"

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

    /**
     * Specific tag for whether the collection -> children object relations have been built.
     * Purposefully not included in TAG_COMPLETE so it doesn't influence other decisions about
     * whether the collection is considered complete.
     */
    TAG_COLLECTION_CHILDREN_HIERARCHY = (1 << 7),

    /* All ID components has been built. */
    TAG_COMPLETE = (TAG_ANIMATION | TAG_PARAMETERS | TAG_TRANSFORM | TAG_GEOMETRY |
                    TAG_SCENE_COMPOSITOR | TAG_SCENE_SEQUENCER | TAG_SCENE_AUDIO),
  };

  /* Check whether given ID is already handled by builder (or if it's being handled). */
  bool check_is_built(ID *id, int tag = TAG_COMPLETE) const;

  /* Tag given ID as handled/built. */
  void tag_built(ID *id, int tag = TAG_COMPLETE);

  /* Combination of previous two functions, returns truth if ID was already handled, or tags is
   * handled otherwise and return false. */
  bool check_is_built_and_tag(ID *id, int tag = TAG_COMPLETE);

  template<typename T> bool check_is_built(T *datablock, int tag = TAG_COMPLETE) const
  {
    return this->check_is_built(&datablock->id, tag);
  }
  template<typename T> void tag_built(T *datablock, int tag = TAG_COMPLETE)
  {
    this->tag_built(&datablock->id, tag);
  }
  template<typename T> bool check_is_built_and_tag(T *datablock, int tag = TAG_COMPLETE)
  {
    return this->check_is_built_and_tag(&datablock->id, tag);
  }

 protected:
  int get_ID_tag(ID *id) const;

  Map<ID *, int> id_tags_;
};

}  // namespace blender::deg
