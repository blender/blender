/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"

#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"

namespace blender::seq {

Strip *select_active_get(const Scene *scene)
{
  const Editing *ed = editing_get(scene);

  if (ed == nullptr) {
    return nullptr;
  }

  return ed->act_strip;
}

void select_active_set(Scene *scene, Strip *strip)
{
  Editing *ed = editing_get(scene);

  if (ed == nullptr) {
    return;
  }

  ed->act_strip = strip;
}

bool select_active_get_pair(Scene *scene, Strip **r_strip_act, Strip **r_strip_other)
{
  Editing *ed = editing_get(scene);

  *r_strip_act = select_active_get(scene);

  if (*r_strip_act == nullptr) {
    return false;
  }

  *r_strip_other = nullptr;

  LISTBASE_FOREACH (Strip *, strip, ed->seqbasep) {
    if (strip->flag & SELECT && (strip != (*r_strip_act))) {
      if (*r_strip_other) {
        return false;
      }

      *r_strip_other = strip;
    }
  }

  return (*r_strip_other != nullptr);
}

}  // namespace blender::seq
