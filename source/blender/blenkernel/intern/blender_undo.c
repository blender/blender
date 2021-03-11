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

/** \file
 * \ingroup bke
 *
 * Blend file undo (known as 'Global Undo').
 * DNA level diffing for undo.
 */

#ifndef _WIN32
#  include <unistd.h> /* for read close */
#else
#  include <io.h> /* for open close read */
#endif

#include <errno.h>
#include <fcntl.h> /* for open */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
#include "BKE_blender_undo.h" /* own include */
#include "BKE_blendfile.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_undo_system.h"

#include "BLO_readfile.h"
#include "BLO_undofile.h"
#include "BLO_writefile.h"

#include "DEG_depsgraph.h"

/* -------------------------------------------------------------------- */
/** \name Global Undo
 * \{ */

#define UNDO_DISK 0

bool BKE_memfile_undo_decode(MemFileUndoData *mfu,
                             const eUndoStepDir undo_direction,
                             const bool use_old_bmain_data,
                             bContext *C)
{
  Main *bmain = CTX_data_main(C);
  char mainstr[sizeof(bmain->name)];
  int success = 0, fileflags;

  BLI_strncpy(mainstr, BKE_main_blendfile_path(bmain), sizeof(mainstr)); /* temporal store */

  fileflags = G.fileflags;
  G.fileflags |= G_FILE_NO_UI;

  if (UNDO_DISK) {
    const struct BlendFileReadParams params = {0};
    struct BlendFileData *bfd = BKE_blendfile_read(mfu->filename, &params, NULL);
    if (bfd != NULL) {
      BKE_blendfile_read_setup(C, bfd, &params, NULL);
      success = true;
    }
  }
  else {
    struct BlendFileReadParams params = {0};
    params.undo_direction = undo_direction;
    if (!use_old_bmain_data) {
      params.skip_flags |= BLO_READ_SKIP_UNDO_OLD_MAIN;
    }
    struct BlendFileData *bfd = BKE_blendfile_read_from_memfile(
        bmain, &mfu->memfile, &params, NULL);
    if (bfd != NULL) {
      BKE_blendfile_read_setup(C, bfd, &params, NULL);
      success = true;
    }
  }

  /* Restore, bmain has been re-allocated. */
  bmain = CTX_data_main(C);
  BLI_strncpy(bmain->name, mainstr, sizeof(bmain->name));
  G.fileflags = fileflags;

  if (success) {
    /* important not to update time here, else non keyed transforms are lost */
    DEG_on_visible_update(bmain, false);
  }

  return success;
}

MemFileUndoData *BKE_memfile_undo_encode(Main *bmain, MemFileUndoData *mfu_prev)
{
  MemFileUndoData *mfu = MEM_callocN(sizeof(MemFileUndoData), __func__);

  /* disk save version */
  if (UNDO_DISK) {
    static int counter = 0;
    char filename[FILE_MAX];
    char numstr[32];

    /* Calculate current filename. */
    counter++;
    counter = counter % U.undosteps;

    BLI_snprintf(numstr, sizeof(numstr), "%d.blend", counter);
    BLI_join_dirfile(filename, sizeof(filename), BKE_tempdir_session(), numstr);

    /* success = */ /* UNUSED */ BLO_write_file(
        bmain, filename, G.fileflags, &(const struct BlendFileWriteParams){0}, NULL);

    BLI_strncpy(mfu->filename, filename, sizeof(mfu->filename));
  }
  else {
    MemFile *prevfile = (mfu_prev) ? &(mfu_prev->memfile) : NULL;
    if (prevfile) {
      BLO_memfile_clear_future(prevfile);
    }
    /* success = */ /* UNUSED */ BLO_write_file_mem(bmain, prevfile, &mfu->memfile, G.fileflags);
    mfu->undo_size = mfu->memfile.size;
  }

  bmain->is_memfile_undo_written = true;

  return mfu;
}

void BKE_memfile_undo_free(MemFileUndoData *mfu)
{
  BLO_memfile_free(&mfu->memfile);
  MEM_freeN(mfu);
}

/** \} */
