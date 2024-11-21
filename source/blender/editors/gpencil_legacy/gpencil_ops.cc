/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 */

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "BLI_sys_types.h"

#include "BKE_context.hh"
#include "BKE_paint.hh"

#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

#include "ED_gpencil_legacy.hh"

#include "gpencil_intern.hh"

void ED_keymap_gpencil_legacy(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Grease Pencil", SPACE_EMPTY, RGN_TYPE_WINDOW);
}

void ED_operatortypes_gpencil_legacy()
{
  /* Annotations -------------------- */

  WM_operatortype_append(GPENCIL_OT_annotate);
  WM_operatortype_append(GPENCIL_OT_annotation_add);
  WM_operatortype_append(GPENCIL_OT_data_unlink);
  WM_operatortype_append(GPENCIL_OT_layer_annotation_add);
  WM_operatortype_append(GPENCIL_OT_layer_annotation_remove);
  WM_operatortype_append(GPENCIL_OT_layer_annotation_move);
  WM_operatortype_append(GPENCIL_OT_annotation_active_frame_delete);

  /* Editing (Time) --------------- */
}
