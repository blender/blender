/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_movieclip.h"

#include "DNA_movieclip_types.h"

#include "BLI_utildefines.h"

namespace blender::deg {

MovieClipBackup::MovieClipBackup(const Depsgraph * /*depsgraph*/)
{
  reset();
}

void MovieClipBackup::reset()
{
  anim = nullptr;
  cache = nullptr;
}

void MovieClipBackup::init_from_movieclip(MovieClip *movieclip)
{
  anim = movieclip->anim;
  cache = movieclip->cache;
  /* Clear pointers stored in the movie clip, so they are not freed when copied-on-written
   * datablock is freed for re-allocation. */
  movieclip->anim = nullptr;
  movieclip->cache = nullptr;
}

void MovieClipBackup::restore_to_movieclip(MovieClip *movieclip)
{
  movieclip->anim = anim;
  movieclip->cache = cache;

  reset();
}

}  // namespace blender::deg
