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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editor/io
 */

#include "MEM_guardedalloc.h"

#include "DNA_cachefile_types.h"
#include "DNA_space_types.h"

#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_cachefile.h"
#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "io_cache.h"

static void cachefile_init(bContext *C, wmOperator *op)
{
  PropertyPointerRNA *pprop;

  op->customdata = pprop = MEM_callocN(sizeof(PropertyPointerRNA), "OpenPropertyPointerRNA");
  UI_context_active_but_prop_get_templateID(C, &pprop->ptr, &pprop->prop);
}

static int cachefile_open_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    char filepath[FILE_MAX];
    Main *bmain = CTX_data_main(C);

    BLI_strncpy(filepath, BKE_main_blendfile_path(bmain), sizeof(filepath));
    BLI_path_extension_replace(filepath, sizeof(filepath), ".abc");
    RNA_string_set(op->ptr, "filepath", filepath);
  }

  cachefile_init(C, op);

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;

  UNUSED_VARS(event);
}

static void open_cancel(bContext *UNUSED(C), wmOperator *op)
{
  MEM_freeN(op->customdata);
  op->customdata = NULL;
}

static int cachefile_open_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }

  char filename[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filename);

  Main *bmain = CTX_data_main(C);

  CacheFile *cache_file = BKE_libblock_alloc(bmain, ID_CF, BLI_path_basename(filename), 0);
  BLI_strncpy(cache_file->filepath, filename, FILE_MAX);
  DEG_id_tag_update(&cache_file->id, ID_RECALC_COPY_ON_WRITE);

  /* Will be set when running invoke, not exec directly. */
  if (op->customdata != NULL) {
    /* hook into UI */
    PropertyPointerRNA *pprop = op->customdata;
    if (pprop->prop) {
      /* When creating new ID blocks, use is already 1, but RNA
       * pointer see also increases user, so this compensates it. */
      id_us_min(&cache_file->id);

      PointerRNA idptr;
      RNA_id_pointer_create(&cache_file->id, &idptr);
      RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr, NULL);
      RNA_property_update(C, &pprop->ptr, pprop->prop);
    }

    MEM_freeN(op->customdata);
  }

  return OPERATOR_FINISHED;
}

void CACHEFILE_OT_open(wmOperatorType *ot)
{
  ot->name = "Open Cache File";
  ot->description = "Load a cache file";
  ot->idname = "CACHEFILE_OT_open";

  ot->invoke = cachefile_open_invoke;
  ot->exec = cachefile_open_exec;
  ot->cancel = open_cancel;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_ALEMBIC | FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/* ***************************** Reload Operator **************************** */

static int cachefile_reload_exec(bContext *C, wmOperator *UNUSED(op))
{
  CacheFile *cache_file = CTX_data_edit_cachefile(C);

  if (!cache_file) {
    return OPERATOR_CANCELLED;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_cachefile_reload(depsgraph, cache_file);

  return OPERATOR_FINISHED;
}

void CACHEFILE_OT_reload(wmOperatorType *ot)
{
  ot->name = "Refresh Archive";
  ot->description = "Update objects paths list with new data from the archive";
  ot->idname = "CACHEFILE_OT_reload";

  /* api callbacks */
  ot->exec = cachefile_reload_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
