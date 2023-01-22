/* SPDX-License-Identifier: GPL-2.0-or-later */

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
  char mainstr[sizeof(bmain->filepath)];
  int success = 0, fileflags;

  BLI_strncpy(mainstr, BKE_main_blendfile_path(bmain), sizeof(mainstr)); /* temporal store */

  fileflags = G.fileflags;
  G.fileflags |= G_FILE_NO_UI;

  if (UNDO_DISK) {
    const BlendFileReadParams params{};
    BlendFileReadReport bf_reports{};
    struct BlendFileData *bfd = BKE_blendfile_read(mfu->filepath, &params, &bf_reports);
    if (bfd != nullptr) {
      BKE_blendfile_read_setup(C, bfd, &params, &bf_reports);
      success = true;
    }
  }
  else {
    struct BlendFileReadParams params = {0};
    params.undo_direction = undo_direction;
    if (!use_old_bmain_data) {
      params.skip_flags |= BLO_READ_SKIP_UNDO_OLD_MAIN;
    }
    BlendFileReadReport blend_file_read_report{};
    struct BlendFileData *bfd = BKE_blendfile_read_from_memfile(
        bmain, &mfu->memfile, &params, nullptr);
    if (bfd != nullptr) {
      BKE_blendfile_read_setup(C, bfd, &params, &blend_file_read_report);
      success = true;
    }
  }

  /* Restore, bmain has been re-allocated. */
  bmain = CTX_data_main(C);
  STRNCPY(bmain->filepath, mainstr);
  G.fileflags = fileflags;

  if (success) {
    /* important not to update time here, else non keyed transforms are lost */
    DEG_tag_on_visible_update(bmain, false);
  }

  return success;
}

MemFileUndoData *BKE_memfile_undo_encode(Main *bmain, MemFileUndoData *mfu_prev)
{
  MemFileUndoData *mfu = MEM_cnew<MemFileUndoData>(__func__);

  /* Include recovery information since undo-data is written out as #BLENDER_QUIT_FILE. */
  const int fileflags = G.fileflags | G_FILE_RECOVER_WRITE;

  /* disk save version */
  if (UNDO_DISK) {
    static int counter = 0;
    char filepath[FILE_MAX];
    char numstr[32];

    /* Calculate current filepath. */
    counter++;
    counter = counter % U.undosteps;

    BLI_snprintf(numstr, sizeof(numstr), "%d.blend", counter);
    BLI_path_join(filepath, sizeof(filepath), BKE_tempdir_session(), numstr);

    const BlendFileWriteParams blend_file_write_params{};
    /* success = */ /* UNUSED */ BLO_write_file(
        bmain, filepath, fileflags, &blend_file_write_params, nullptr);

    BLI_strncpy(mfu->filepath, filepath, sizeof(mfu->filepath));
  }
  else {
    MemFile *prevfile = (mfu_prev) ? &(mfu_prev->memfile) : nullptr;
    if (prevfile) {
      BLO_memfile_clear_future(prevfile);
    }
    /* success = */ /* UNUSED */ BLO_write_file_mem(bmain, prevfile, &mfu->memfile, fileflags);
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
