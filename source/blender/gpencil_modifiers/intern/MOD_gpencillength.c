/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"
#include "MOD_gpencil_util.h"

#include "DEG_depsgraph.h"

static void initData(GpencilModifierData *md)
{
  LengthGpencilModifierData *gpmd = (LengthGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(LengthGpencilModifierData), modifier);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static bool gpencil_modify_stroke(bGPDstroke *gps,
                                  float length,
                                  const float overshoot_fac,
                                  const short len_mode)
{
  bool changed = false;
  if (length == 0.0f) {
    return changed;
  }

  if (length > 0.0f) {
    BKE_gpencil_stroke_stretch(gps, length, overshoot_fac, len_mode);
  }
  else {
    changed |= BKE_gpencil_stroke_shrink(gps, fabs(length), len_mode);
  }

  return changed;
}

static void applyLength(LengthGpencilModifierData *lmd, bGPdata *gpd, bGPDstroke *gps)
{
  bool changed = false;
  const float len = (lmd->mode == GP_LENGTH_ABSOLUTE) ? 1.0f :
                                                        BKE_gpencil_stroke_length(gps, true);
  if (len < FLT_EPSILON) {
    return;
  }

  changed |= gpencil_modify_stroke(gps, len * lmd->start_fac, lmd->overshoot_fac, 1);
  changed |= gpencil_modify_stroke(gps, len * lmd->end_fac, lmd->overshoot_fac, 2);

  if (changed) {
    BKE_gpencil_stroke_geometry_update(gpd, gps);
  }
}

static void bakeModifier(Main *UNUSED(bmain),
                         Depsgraph *UNUSED(depsgraph),
                         GpencilModifierData *md,
                         Object *ob)
{

  bGPdata *gpd = ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LengthGpencilModifierData *lmd = (LengthGpencilModifierData *)md;
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        applyLength(lmd, gpd, gps);
      }
    }
  }
}

/* -------------------------------- */

/* Generic "generateStrokes" callback */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  bGPdata *gpd = ob->data;
  LengthGpencilModifierData *lmd = (LengthGpencilModifierData *)md;
  if (is_stroke_affected_by_modifier(ob,
                                     lmd->layername,
                                     lmd->material,
                                     lmd->pass_index,
                                     lmd->layer_pass,
                                     1,
                                     gpl,
                                     gps,
                                     lmd->flag & GP_LENGTH_INVERT_LAYER,
                                     lmd->flag & GP_LENGTH_INVERT_PASS,
                                     lmd->flag & GP_LENGTH_INVERT_LAYERPASS,
                                     lmd->flag & GP_LENGTH_INVERT_MATERIAL)) {
    applyLength(lmd, gpd, gps);
  }
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  LengthGpencilModifierData *mmd = (LengthGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);
  uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);

  uiLayout *col = uiLayoutColumn(layout, true);

  uiItemR(col, ptr, "start_factor", 0, IFACE_("Start"), ICON_NONE);
  uiItemR(col, ptr, "end_factor", 0, IFACE_("End"), ICON_NONE);

  uiItemR(layout, ptr, "overshoot_factor", UI_ITEM_R_SLIDER, IFACE_("Overshoot"), ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Length, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Length = {
    /* name */ "Length",
    /* structName */ "LengthGpencilModifierData",
    /* structSize */ sizeof(LengthGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformStroke */ deformStroke,
    /* generateStrokes */ NULL,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
