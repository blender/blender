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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edobj
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_object_types.h"
#include "DNA_volume_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_volume.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_image.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "object_intern.h"

/* Volume Add */

static Object *object_volume_add(bContext *C, wmOperator *op, const char *name)
{
  ushort local_view_bits;
  float loc[3], rot[3];

  if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, NULL, &local_view_bits, NULL)) {
    return false;
  }
  return ED_object_add_type(C, OB_VOLUME, name, loc, rot, false, local_view_bits);
}

static int object_volume_add_exec(bContext *C, wmOperator *op)
{
  return (object_volume_add(C, op, NULL) != NULL) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void OBJECT_OT_volume_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Volume";
  ot->description = "Add a volume object to the scene";
  ot->idname = "OBJECT_OT_volume_add";

  /* api callbacks */
  ot->exec = object_volume_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ED_object_add_generic_props(ot, false);
}

/* Volume Import */

static int volume_import_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const bool is_relative_path = RNA_boolean_get(op->ptr, "relative_path");
  bool imported = false;

  ListBase ranges = ED_image_filesel_detect_sequences(bmain, op, false);
  LISTBASE_FOREACH (ImageFrameRange *, range, &ranges) {
    char filename[FILE_MAX];
    BLI_split_file_part(range->filepath, filename, sizeof(filename));
    BLI_path_extension_replace(filename, sizeof(filename), "");

    Object *object = object_volume_add(C, op, filename);
    Volume *volume = (Volume *)object->data;

    STRNCPY(volume->filepath, range->filepath);
    if (is_relative_path) {
      BLI_path_rel(volume->filepath, BKE_main_blendfile_path(bmain));
    }

    if (!BKE_volume_load(volume, bmain)) {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Volume \"%s\" failed to load: %s",
                  filename,
                  BKE_volume_grids_error_msg(volume));
      BKE_id_delete(bmain, &object->id);
      BKE_id_delete(bmain, &volume->id);
      continue;
    }
    else if (BKE_volume_is_points_only(volume)) {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Volume \"%s\" contains points, only voxel grids are supported",
                  filename);
      BKE_id_delete(bmain, &object->id);
      BKE_id_delete(bmain, &volume->id);
      continue;
    }

    /* Set sequence parameters after trying to load the first frame, for file validation we want
     * to use a consistent frame rather than whatever corresponds to the current scene frame. */
    volume->is_sequence = (range->length > 1);
    volume->frame_duration = (volume->is_sequence) ? range->length : 0;
    volume->frame_start = 1;
    volume->frame_offset = (volume->is_sequence) ? range->offset - 1 : 0;

    if (BKE_volume_is_y_up(volume)) {
      object->rot[0] += M_PI_2;
    }

    BKE_volume_unload(volume);

    imported = true;
  }
  BLI_freelistN(&ranges);

  return (imported) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int volume_import_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    return volume_import_exec(C, op);
  }

  RNA_string_set(op->ptr, "filepath", U.textudir);
  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* called by other space types too */
void OBJECT_OT_volume_import(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Import OpenVDB Volume";
  ot->description = "Import OpenVDB volume file";
  ot->idname = "OBJECT_OT_volume_import";

  /* api callbacks */
  ot->exec = volume_import_exec;
  ot->invoke = volume_import_invoke;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_VOLUME,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILES |
                                     WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);

  RNA_def_boolean(
      ot->srna,
      "use_sequence_detection",
      true,
      "Detect Sequences",
      "Automatically detect animated sequences in selected volume files (based on file names)");

  ED_object_add_generic_props(ot, false);
}
