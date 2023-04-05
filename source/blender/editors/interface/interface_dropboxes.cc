/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.h"

#include "BLI_string.h"
#include "BLT_translation.h"

#include "DNA_material_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "WM_api.h"

#include "UI_interface.h"
#include "UI_interface.hh"

using namespace blender::ui;

/* -------------------------------------------------------------------- */
/** \name View Drag/Drop Callbacks
 * \{ */

static bool ui_view_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  const ARegion *region = CTX_wm_region(C);

  std::unique_ptr<DropTargetInterface> drop_target = region_views_find_drop_target_at(region,
                                                                                      event->xy);
  if (!drop_target) {
    return false;
  }

  if (drag->drop_state.free_disabled_info) {
    MEM_SAFE_FREE(drag->drop_state.disabled_info);
  }
  drag->drop_state.free_disabled_info = false;

  return drop_target->can_drop(*drag, &drag->drop_state.disabled_info);
}

static char *ui_view_drop_tooltip(bContext *C, wmDrag *drag, const int xy[2], wmDropBox * /*drop*/)
{
  const ARegion *region = CTX_wm_region(C);
  std::unique_ptr<DropTargetInterface> drop_target = region_views_find_drop_target_at(region, xy);

  return drop_target_tooltip(*drop_target, *drag);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Name Drag/Drop Callbacks
 * \{ */

static bool ui_drop_name_poll(struct bContext *C, wmDrag *drag, const wmEvent * /*event*/)
{
  return UI_but_active_drop_name(C) && (drag->type == WM_DRAG_ID);
}

static void ui_drop_name_copy(bContext * /*C*/, wmDrag *drag, wmDropBox *drop)
{
  const ID *id = WM_drag_get_local_ID(drag, 0);
  RNA_string_set(drop->ptr, "string", id->name + 2);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Drag/Drop Callbacks
 * \{ */

static bool ui_drop_material_poll(bContext *C, wmDrag *drag, const wmEvent * /*event*/)
{
  PointerRNA mat_slot = CTX_data_pointer_get_type(C, "material_slot", &RNA_MaterialSlot);
  return WM_drag_is_ID_type(drag, ID_MA) && !RNA_pointer_is_null(&mat_slot);
}

static void ui_drop_material_copy(bContext * /*C*/, wmDrag *drag, wmDropBox *drop)
{
  const ID *id = WM_drag_get_local_ID_or_import_from_asset(drag, ID_MA);
  RNA_int_set(drop->ptr, "session_uuid", int(id->session_uuid));
}

static char *ui_drop_material_tooltip(bContext *C,
                                      wmDrag *drag,
                                      const int /*xy*/[2],
                                      struct wmDropBox * /*drop*/)
{
  PointerRNA rna_ptr = CTX_data_pointer_get_type(C, "object", &RNA_Object);
  Object *ob = (Object *)rna_ptr.data;
  BLI_assert(ob);

  PointerRNA mat_slot = CTX_data_pointer_get_type(C, "material_slot", &RNA_MaterialSlot);
  BLI_assert(mat_slot.data);

  const int target_slot = RNA_int_get(&mat_slot, "slot_index") + 1;

  PointerRNA rna_prev_material = RNA_pointer_get(&mat_slot, "material");
  Material *prev_mat_in_slot = (Material *)rna_prev_material.data;
  const char *dragged_material_name = WM_drag_get_item_name(drag);

  char *result;
  if (prev_mat_in_slot) {
    const char *tooltip = TIP_("Drop %s on slot %d (replacing %s) of %s");
    result = BLI_sprintfN(tooltip,
                          dragged_material_name,
                          target_slot,
                          prev_mat_in_slot->id.name + 2,
                          ob->id.name + 2);
  }
  else if (target_slot == ob->actcol) {
    const char *tooltip = TIP_("Drop %s on slot %d (active slot) of %s");
    result = BLI_sprintfN(tooltip, dragged_material_name, target_slot, ob->id.name + 2);
  }
  else {
    const char *tooltip = TIP_("Drop %s on slot %d of %s");
    result = BLI_sprintfN(tooltip, dragged_material_name, target_slot, ob->id.name + 2);
  }

  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add User Interface Drop Boxes
 * \{ */

void ED_dropboxes_ui()
{
  ListBase *lb = WM_dropboxmap_find("User Interface", SPACE_EMPTY, 0);

  WM_dropbox_add(lb, "UI_OT_view_drop", ui_view_drop_poll, nullptr, nullptr, ui_view_drop_tooltip);
  WM_dropbox_add(lb,
                 "UI_OT_drop_name",
                 ui_drop_name_poll,
                 ui_drop_name_copy,
                 WM_drag_free_imported_drag_ID,
                 nullptr);
  WM_dropbox_add(lb,
                 "UI_OT_drop_material",
                 ui_drop_material_poll,
                 ui_drop_material_copy,
                 WM_drag_free_imported_drag_ID,
                 ui_drop_material_tooltip);
}

/** \} */
