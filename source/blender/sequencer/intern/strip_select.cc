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

#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"

Strip *SEQ_select_active_get(const Scene *scene)
{
  const Editing *ed = SEQ_editing_get(scene);

  if (ed == nullptr) {
    return nullptr;
  }

  return ed->act_seq;
}

void SEQ_select_active_set(Scene *scene, Strip *strip)
{
  Editing *ed = SEQ_editing_get(scene);

  if (ed == nullptr) {
    return;
  }

  ed->act_seq = strip;
}

bool SEQ_select_active_get_pair(Scene *scene, Strip **r_seq_act, Strip **r_seq_other)
{
  Editing *ed = SEQ_editing_get(scene);

  *r_seq_act = SEQ_select_active_get(scene);

  if (*r_seq_act == nullptr) {
    return false;
  }

  *r_seq_other = nullptr;

  LISTBASE_FOREACH (Strip *, strip, ed->seqbasep) {
    if (strip->flag & SELECT && (strip != (*r_seq_act))) {
      if (*r_seq_other) {
        return false;
      }

      *r_seq_other = strip;
    }
  }

  return (*r_seq_other != nullptr);
}
