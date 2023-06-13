/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 *
 * Utils to check/validate a Main is in sane state,
 * only checks relations between data-blocks and libraries for now.
 *
 * \note Does not *fix* anything, only reports found errors.
 */

#include <cstring> /* for #strrchr #strncmp #strstr */

#include "BLI_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_sdna_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_lib_remap.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BLO_blend_validate.h"
#include "BLO_readfile.h"

#include "readfile.h"

bool BLO_main_validate_libraries(Main *bmain, ReportList *reports)
{
  ListBase mainlist;
  bool is_valid = true;

  BKE_main_lock(bmain);

  blo_split_main(&mainlist, bmain);

  ListBase *lbarray[INDEX_ID_MAX];
  int i = set_listbasepointers(bmain, lbarray);
  while (i--) {
    for (ID *id = static_cast<ID *>(lbarray[i]->first); id != nullptr;
         id = static_cast<ID *>(id->next))
    {
      if (ID_IS_LINKED(id)) {
        is_valid = false;
        BKE_reportf(reports,
                    RPT_ERROR,
                    "ID %s is in local database while being linked from library %s!",
                    id->name,
                    id->lib->filepath);
      }
    }
  }

  for (Main *curmain = bmain->next; curmain != nullptr; curmain = curmain->next) {
    Library *curlib = curmain->curlib;
    if (curlib == nullptr) {
      BKE_report(reports, RPT_ERROR, "Library database with null library data-block pointer!");
      continue;
    }

    BKE_library_filepath_set(bmain, curlib, curlib->filepath);
    BlendFileReadReport bf_reports{};
    bf_reports.reports = reports;
    BlendHandle *bh = BLO_blendhandle_from_file(curlib->filepath_abs, &bf_reports);

    if (bh == nullptr) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Library ID %s not found at expected path %s!",
                  curlib->id.name,
                  curlib->filepath_abs);
      continue;
    }

    i = set_listbasepointers(curmain, lbarray);
    while (i--) {
      ID *id = static_cast<ID *>(lbarray[i]->first);
      if (id == nullptr) {
        continue;
      }

      if (GS(id->name) == ID_LI) {
        is_valid = false;
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Library ID %s in library %s, this should not happen!",
                    id->name,
                    curlib->filepath);
        continue;
      }

      int totnames = 0;
      LinkNode *names = BLO_blendhandle_get_datablock_names(bh, GS(id->name), false, &totnames);
      for (; id != nullptr; id = static_cast<ID *>(id->next)) {
        if (!ID_IS_LINKED(id)) {
          is_valid = false;
          BKE_reportf(reports,
                      RPT_ERROR,
                      "ID %s has null lib pointer while being in library %s!",
                      id->name,
                      curlib->filepath);
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

        if (name == nullptr) {
          is_valid = false;
          BKE_reportf(reports,
                      RPT_ERROR,
                      "ID %s not found in library %s anymore!",
                      id->name,
                      id->lib->filepath);
          continue;
        }
      }

      BLI_linklist_freeN(names);
    }

    BLO_blendhandle_close(bh);
  }

  blo_join_main(&mainlist);

  BLI_assert(BLI_listbase_is_single(&mainlist));
  BLI_assert(mainlist.first == (void *)bmain);

  BKE_main_unlock(bmain);

  return is_valid;
}

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
      if (!ID_IS_LINKED(id)) {
        /* We assume lib data is valid... */
        Key *shapekey = BKE_key_from_id(id);
        if (shapekey != nullptr && shapekey->from != id) {
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

  /* NOTE: #BKE_id_delete also locks `bmain`, so we need to do this loop outside of the lock here.
   */
  LISTBASE_FOREACH_MUTABLE (Key *, shapekey, &bmain->shapekeys) {
    if (shapekey->from != nullptr) {
      continue;
    }

    BKE_reportf(reports,
                RPT_ERROR,
                "Shapekey %s has an invalid 'from' pointer (%p), it will be deleted",
                shapekey->id.name,
                shapekey->from);
    /* NOTE: also need to remap UI data ID pointers here, since `bmain` is not the current
     * `G_MAIN`, default UI-handling remapping callback (defined by call to
     * `BKE_library_callback_remap_editor_id_reference_set`) won't work on expected data here. */
    BKE_id_delete_ex(bmain, shapekey, ID_REMAP_FORCE_UI_POINTERS);
  }

  return is_valid;
}
