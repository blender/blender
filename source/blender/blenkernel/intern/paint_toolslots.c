/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_brush.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_paint.h"

/* -------------------------------------------------------------------- */
/** \name Tool Slot Initialization / Versioning
 *
 * These functions run to update old files (while versioning),
 * take care only to perform low-level functions here.
 * \{ */

void BKE_paint_toolslots_len_ensure(Paint *paint, int len)
{
  /* Tool slots are 'uchar'. */
  BLI_assert(len <= UCHAR_MAX);
  if (paint->tool_slots_len < len) {
    paint->tool_slots = MEM_recallocN(paint->tool_slots, sizeof(*paint->tool_slots) * len);
    paint->tool_slots_len = len;
  }
}

static void paint_toolslots_init(Main *bmain, Paint *paint)
{
  if (paint == NULL) {
    return;
  }
  const eObjectMode ob_mode = paint->runtime.ob_mode;
  BLI_assert(paint->runtime.tool_offset && ob_mode);
  for (Brush *brush = bmain->brushes.first; brush; brush = brush->id.next) {
    if (brush->ob_mode & ob_mode) {
      const int slot_index = BKE_brush_tool_get(brush, paint);
      BKE_paint_toolslots_len_ensure(paint, slot_index + 1);
      if (paint->tool_slots[slot_index].brush == NULL) {
        paint->tool_slots[slot_index].brush = brush;
        id_us_plus(&brush->id);
      }
    }
  }
}

/**
 * Initialize runtime since this is called from versioning code.
 */
static void paint_toolslots_init_with_runtime(Main *bmain, ToolSettings *ts, Paint *paint)
{
  if (paint == NULL) {
    return;
  }

  /* Needed so #Paint_Runtime is updated when versioning. */
  BKE_paint_runtime_init(ts, paint);
  paint_toolslots_init(bmain, paint);
}

void BKE_paint_toolslots_init_from_main(struct Main *bmain)
{
  for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
    ToolSettings *ts = scene->toolsettings;
    paint_toolslots_init_with_runtime(bmain, ts, &ts->imapaint.paint);
    if (ts->sculpt) {
      paint_toolslots_init_with_runtime(bmain, ts, &ts->sculpt->paint);
    }
    if (ts->vpaint) {
      paint_toolslots_init_with_runtime(bmain, ts, &ts->vpaint->paint);
    }
    if (ts->wpaint) {
      paint_toolslots_init_with_runtime(bmain, ts, &ts->wpaint->paint);
    }
    if (ts->uvsculpt) {
      paint_toolslots_init_with_runtime(bmain, ts, &ts->uvsculpt->paint);
    }
    if (ts->gp_paint) {
      paint_toolslots_init_with_runtime(bmain, ts, &ts->gp_paint->paint);
    }
    if (ts->gp_vertexpaint) {
      paint_toolslots_init_with_runtime(bmain, ts, &ts->gp_vertexpaint->paint);
    }
    if (ts->gp_sculptpaint) {
      paint_toolslots_init_with_runtime(bmain, ts, &ts->gp_sculptpaint->paint);
    }
    if (ts->gp_weightpaint) {
      paint_toolslots_init_with_runtime(bmain, ts, &ts->gp_weightpaint->paint);
    }
    if (ts->curves_sculpt) {
      paint_toolslots_init_with_runtime(bmain, ts, &ts->curves_sculpt->paint);
    }
  }
}

/** \} */

void BKE_paint_toolslots_brush_update_ex(Paint *paint, Brush *brush)
{
  const uint tool_offset = paint->runtime.tool_offset;
  UNUSED_VARS_NDEBUG(tool_offset);
  BLI_assert(tool_offset != 0);
  const int slot_index = BKE_brush_tool_get(brush, paint);
  BKE_paint_toolslots_len_ensure(paint, slot_index + 1);
  PaintToolSlot *tslot = &paint->tool_slots[slot_index];
  id_us_plus(&brush->id);
  if (tslot->brush) {
    id_us_min(&tslot->brush->id);
  }
  tslot->brush = brush;
}

void BKE_paint_toolslots_brush_update(Paint *paint)
{
  if (paint->brush == NULL) {
    return;
  }
  BKE_paint_toolslots_brush_update_ex(paint, paint->brush);
}

void BKE_paint_toolslots_brush_validate(Main *bmain, Paint *paint)
{
  /* Clear slots with invalid slots or mode (unlikely but possible). */
  const uint tool_offset = paint->runtime.tool_offset;
  UNUSED_VARS_NDEBUG(tool_offset);
  const eObjectMode ob_mode = paint->runtime.ob_mode;
  BLI_assert(tool_offset && ob_mode);
  for (int i = 0; i < paint->tool_slots_len; i++) {
    PaintToolSlot *tslot = &paint->tool_slots[i];
    if (tslot->brush) {
      if ((i != BKE_brush_tool_get(tslot->brush, paint)) || (tslot->brush->ob_mode & ob_mode) == 0)
      {
        id_us_min(&tslot->brush->id);
        tslot->brush = NULL;
      }
    }
  }

  /* Unlikely but possible the active brush is not currently using a slot. */
  BKE_paint_toolslots_brush_update(paint);

  /* Fill slots from brushes. */
  paint_toolslots_init(bmain, paint);
}

Brush *BKE_paint_toolslots_brush_get(Paint *paint, int slot_index)
{
  if (slot_index < paint->tool_slots_len) {
    PaintToolSlot *tslot = &paint->tool_slots[slot_index];
    return tslot->brush;
  }
  return NULL;
}
