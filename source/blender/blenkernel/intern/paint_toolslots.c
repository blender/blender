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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bke
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "BLI_utildefines.h"

#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_brush.h"
#include "BKE_paint.h"

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

void BKE_paint_toolslots_init_from_main(struct Main *bmain)
{
  for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
    ToolSettings *ts = scene->toolsettings;
    paint_toolslots_init(bmain, &ts->imapaint.paint);
    paint_toolslots_init(bmain, &ts->sculpt->paint);
    paint_toolslots_init(bmain, &ts->vpaint->paint);
    paint_toolslots_init(bmain, &ts->wpaint->paint);
    paint_toolslots_init(bmain, &ts->uvsculpt->paint);
    paint_toolslots_init(bmain, &ts->gp_paint->paint);
  }
}

void BKE_paint_toolslots_brush_update_ex(Paint *paint, Brush *brush)
{
  const uint tool_offset = paint->runtime.tool_offset;
  UNUSED_VARS_NDEBUG(tool_offset);
  BLI_assert(tool_offset != 0);
  const int slot_index = BKE_brush_tool_get(brush, paint);
  BKE_paint_toolslots_len_ensure(paint, slot_index + 1);
  PaintToolSlot *tslot = &paint->tool_slots[slot_index];
  id_us_plus(&brush->id);
  id_us_min(&tslot->brush->id);
  tslot->brush = brush;
}

void BKE_paint_toolslots_brush_update(Paint *paint)
{
  if (paint->brush == NULL) {
    return;
  }
  BKE_paint_toolslots_brush_update_ex(paint, paint->brush);
}

/**
 * Run this to ensure brush types are set for each slot on entering modes
 * (for new scenes for example).
 */
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
      if ((i != BKE_brush_tool_get(tslot->brush, paint)) ||
          (tslot->brush->ob_mode & ob_mode) == 0) {
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
