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
 * \ingroup blenloader
 *
 * Utils to check/validate a Main is in sane state,
 * only checks relations between data-blocks and libraries for now.
 *
 * \note Does not *fix* anything, only reports found errors.
 */

#include <string.h>  // for strrchr strncmp strstr

#include "BLI_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"

#include "MEM_guardedalloc.h"

#include "DNA_sdna_types.h"
#include "DNA_key_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BLO_blend_validate.h"
#include "BLO_readfile.h"

#include "readfile.h"

/**
 * Check (but do *not* fix) that all linked data-blocks are still valid
 * (i.e. pointing to the right library).
 */
bool BLO_main_validate_libraries(Main *bmain, ReportList *reports)
{
  ListBase mainlist;
  bool is_valid = true;

  BKE_main_lock(bmain);

  blo_split_main(&mainlist, bmain);

  ListBase *lbarray[MAX_LIBARRAY];
  int i = set_listbasepointers(bmain, lbarray);
  while (i--) {
    for (ID *id = lbarray[i]->first; id != NULL; id = id->next) {
      if (id->lib != NULL) {
        is_valid = false;
        BKE_reportf(reports,
                    RPT_ERROR,
                    "ID %s is in local database while being linked from library %s!",
                    id->name,
                    id->lib->name);
      }
    }
  }

  for (Main *curmain = bmain->next; curmain != NULL; curmain = curmain->next) {
    Library *curlib = curmain->curlib;
    if (curlib == NULL) {
      BKE_report(reports, RPT_ERROR, "Library database with NULL library data-block!");
      continue;
    }

    BKE_library_filepath_set(bmain, curlib, curlib->name);
    BlendHandle *bh = BLO_blendhandle_from_file(curlib->filepath, reports);

    if (bh == NULL) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Library ID %s not found at expected path %s!",
                  curlib->id.name,
                  curlib->filepath);
      continue;
    }

    i = set_listbasepointers(curmain, lbarray);
    while (i--) {
      ID *id = lbarray[i]->first;
      if (id == NULL) {
        continue;
      }

      if (GS(id->name) == ID_LI) {
        is_valid = false;
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Library ID %s in library %s, this should not happen!",
                    id->name,
                    curlib->name);
        continue;
      }

      int totnames = 0;
      LinkNode *names = BLO_blendhandle_get_datablock_names(bh, GS(id->name), &totnames);
      for (; id != NULL; id = id->next) {
        if (id->lib == NULL) {
          is_valid = false;
          BKE_reportf(reports,
                      RPT_ERROR,
                      "ID %s has NULL lib pointer while being in library %s!",
                      id->name,
                      curlib->name);
          continue;
        }
        if (id->lib != curlib) {
          is_valid = false;
          BKE_reportf(reports, RPT_ERROR, "ID %s has mismatched lib pointer!", id->name);
          continue;
        }

        LinkNode *name = names;
        for (; name; name = name->next) {
          char *str_name = (char *)name->link;
          if (id->name[2] == str_name[0] && STREQ(str_name, id->name + 2)) {
            break;
          }
        }

        if (name == NULL) {
          is_valid = false;
          BKE_reportf(reports,
                      RPT_ERROR,
                      "ID %s not found in library %s anymore!",
                      id->name,
                      id->lib->name);
          continue;
        }
      }

      BLI_linklist_free(names, free);
    }

    BLO_blendhandle_close(bh);
  }

  blo_join_main(&mainlist);

  BLI_assert(BLI_listbase_is_single(&mainlist));
  BLI_assert(mainlist.first == (void *)bmain);

  BKE_main_unlock(bmain);

  return is_valid;
}

/** Check (and fix if needed) that shape key's 'from' pointer is valid. */
bool BLO_main_validate_shapekeys(Main *bmain, ReportList *reports)
{
  ListBase *lb;
  ID *id;
  bool is_valid = true;

  BKE_main_lock(bmain);

  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id) {
      if (!BKE_key_idtype_support(GS(id->name))) {
        break;
      }
      if (id->lib == NULL) {
        /* We assume lib data is valid... */
        Key *shapekey = BKE_key_from_id(id);
        if (shapekey != NULL && shapekey->from != id) {
          is_valid = false;
          BKE_reportf(reports,
                      RPT_ERROR,
                      "ID %s uses shapekey %s, but its 'from' pointer is invalid (%p), fixing...",
                      id->name,
                      shapekey->id.name,
                      shapekey->from);
          shapekey->from = id;
        }
      }
    }
    FOREACH_MAIN_LISTBASE_ID_END;
  }
  FOREACH_MAIN_LISTBASE_END;

  BKE_main_unlock(bmain);

  return is_valid;
}
