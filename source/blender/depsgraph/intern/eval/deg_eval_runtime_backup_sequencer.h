/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "DNA_session_uuid_types.h"

#include "BLI_session_uuid.h"

#include "intern/depsgraph_type.h"
#include "intern/eval/deg_eval_runtime_backup_sequence.h"

struct Scene;

namespace blender::deg {

struct Depsgraph;

/* Backup of sequencer strips runtime data. */
class SequencerBackup {
 public:
  SequencerBackup(const Depsgraph *depsgraph);

  void init_from_scene(Scene *scene);
  void restore_to_scene(Scene *scene);

  const Depsgraph *depsgraph;

  Map<SessionUUID, SequenceBackup> sequences_backup;
};

}  // namespace blender::deg
