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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 *
 * Contains code specific to the `Library` ID type.
 */

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

/* all types are needed here, in order to do memory operations */
#include "DNA_ID.h"

#include "BLI_utildefines.h"

#include "BLI_blenlib.h"

#include "BLT_translation.h"

#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"

/* Unused currently. */
// static CLG_LogRef LOG = {.identifier = "bke.library"};

static void library_free_data(ID *id)
{
  Library *library = (Library *)id;
  if (library->packedfile) {
    BKE_packedfile_free(library->packedfile);
  }
}

static void library_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Library *lib = (Library *)id;
  BKE_LIB_FOREACHID_PROCESS(data, lib->parent, IDWALK_CB_NEVER_SELF);
}

IDTypeInfo IDType_ID_LI = {
    .id_code = ID_LI,
    .id_filter = 0,
    .main_listbase_index = INDEX_ID_LI,
    .struct_size = sizeof(Library),
    .name = "Library",
    .name_plural = "libraries",
    .translation_context = BLT_I18NCONTEXT_ID_LIBRARY,
    .flags = IDTYPE_FLAGS_NO_COPY | IDTYPE_FLAGS_NO_LIBLINKING | IDTYPE_FLAGS_NO_MAKELOCAL,

    .init_data = NULL,
    .copy_data = NULL,
    .free_data = library_free_data,
    .make_local = NULL,
    .foreach_id = library_foreach_id,
};

void BKE_library_filepath_set(Main *bmain, Library *lib, const char *filepath)
{
  /* in some cases this is used to update the absolute path from the
   * relative */
  if (lib->name != filepath) {
    BLI_strncpy(lib->name, filepath, sizeof(lib->name));
  }

  BLI_strncpy(lib->filepath_abs, filepath, sizeof(lib->filepath_abs));

  /* Not essential but set `filepath_abs` is an absolute copy of value which
   * is more useful if its kept in sync. */
  if (BLI_path_is_rel(lib->filepath_abs)) {
    /* note that the file may be unsaved, in this case, setting the
     * `filepath_abs` on an indirectly linked path is not allowed from the
     * outliner, and its not really supported but allow from here for now
     * since making local could cause this to be directly linked - campbell
     */
    /* Never make paths relative to parent lib - reading code (blenloader) always set *all*
     * lib->name relative to current main, not to their parent for indirectly linked ones. */
    const char *basepath = BKE_main_blendfile_path(bmain);
    BLI_path_abs(lib->filepath_abs, basepath);
  }
}
