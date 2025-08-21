/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup speditordock
 */

#include "BKE_context.hh"

#include "BLI_listbase.h"

#include "DNA_space_enums.h"

#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"

#include "ED_editor_dock.hh"
#include "editor_dock_intern.hh"

namespace blender::ed::editor_dock {

/* TODO somehow pass docked area? */
ScrArea *lookup_docked_area(bContext *C)
{
  const bScreen *screen = CTX_wm_screen(C);
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (area->flag & AREA_FLAG_DOCKED) {
      return area;
    }
  }
  return nullptr;
}

bool add_editor_poll(bContext *C)
{
  return lookup_docked_area(C) != nullptr;
}

wmOperatorStatus add_editor_exec(bContext *C, wmOperator *op)
{
  const eSpace_Type type = eSpace_Type(RNA_enum_get(op->ptr, "type"));
  if (ELEM(type, SPACE_EMPTY, SPACE_TOPBAR, SPACE_STATUSBAR, SPACE_EDITOR_DOCK)) {
    return OPERATOR_CANCELLED;
  }

  ScrArea *docked_area = lookup_docked_area(C);

  SpaceLink *new_space = add_docked_space(docked_area, type, CTX_data_scene(C));
  activate_docked_space(C, docked_area, new_space);

  return OPERATOR_FINISHED;
}

void SCREEN_OT_editor_dock_add_editor(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Add Editor";
  ot->description = "Choose an editor to add to the editor dock";
  ot->idname = "SCREEN_OT_editor_dock_add_editor";

  /* Callbacks. */
  ot->exec = add_editor_exec;
  // ot->invoke = add_editor_invoke;
  ot->poll = add_editor_poll;

  /* TODO itemf to filter out invalid types? */
  RNA_def_enum(ot->srna, "type", rna_enum_space_type_items, SPACE_EMPTY, "Type", "");
}

void register_operatortypes()
{
  WM_operatortype_append(SCREEN_OT_editor_dock_add_editor);
}

}  // namespace blender::ed::editor_dock
