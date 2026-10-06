/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_context.hh"
#include "BKE_scene_context.hh"

#include "DNA_scene_types.h"

namespace blender::bke {

Scene *scene_or_sequencer_scene_from_context(const bContext &C)
{
  const bool is_sequencer = CTX_wm_space_seq(&C) != nullptr;
  return is_sequencer ? CTX_data_sequencer_scene(&C) : CTX_data_scene(&C);
}

}  // namespace blender::bke
