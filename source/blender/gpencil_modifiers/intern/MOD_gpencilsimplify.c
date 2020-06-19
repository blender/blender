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
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_vec_types.h"

#include "BKE_context.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"
#include "MOD_gpencil_util.h"

static void initData(GpencilModifierData *md)
{
  SimplifyGpencilModifierData *gpmd = (SimplifyGpencilModifierData *)md;
  gpmd->pass_index = 0;
  gpmd->step = 1;
  gpmd->factor = 0.0f;
  gpmd->length = 0.1f;
  gpmd->distance = 0.1f;
  gpmd->material = NULL;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *gpf,
                         bGPDstroke *gps)
{
  SimplifyGpencilModifierData *mmd = (SimplifyGpencilModifierData *)md;

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      mmd->mode == GP_SIMPLIFY_SAMPLE ? 3 : 4,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_SIMPLIFY_INVERT_LAYER,
                                      mmd->flag & GP_SIMPLIFY_INVERT_PASS,
                                      mmd->flag & GP_SIMPLIFY_INVERT_LAYERPASS,
                                      mmd->flag & GP_SIMPLIFY_INVERT_MATERIAL)) {
    return;
  }

  /* Select simplification mode. */
  switch (mmd->mode) {
    case GP_SIMPLIFY_FIXED: {
      for (int i = 0; i < mmd->step; i++) {
        BKE_gpencil_stroke_simplify_fixed(gps);
      }
      break;
    }
    case GP_SIMPLIFY_ADAPTIVE: {
      /* simplify stroke using Ramer-Douglas-Peucker algorithm */
      BKE_gpencil_stroke_simplify_adaptive(gps, mmd->factor);
      break;
    }
    case GP_SIMPLIFY_SAMPLE: {
      BKE_gpencil_stroke_sample(gps, mmd->length, false);
      break;
    }
    case GP_SIMPLIFY_MERGE: {
      BKE_gpencil_stroke_merge_distance(gpf, gps, mmd->distance, true);
      break;
    }
    default:
      break;
  }
}

static void bakeModifier(struct Main *UNUSED(bmain),
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  bGPdata *gpd = ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        deformStroke(md, depsgraph, ob, gpl, gpf, gps);
      }
    }
  }
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  SimplifyGpencilModifierData *mmd = (SimplifyGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  int mode = RNA_enum_get(&ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "mode", 0, NULL, ICON_NONE);

  if (mode == GP_SIMPLIFY_FIXED) {
    uiItemR(layout, &ptr, "step", 0, NULL, ICON_NONE);
  }
  else if (mode == GP_SIMPLIFY_ADAPTIVE) {
    uiItemR(layout, &ptr, "factor", 0, NULL, ICON_NONE);
  }
  else if (mode == GP_SIMPLIFY_SAMPLE) {
    uiItemR(layout, &ptr, "length", 0, NULL, ICON_NONE);
  }
  else if (mode == GP_SIMPLIFY_MERGE) {
    uiItemR(layout, &ptr, "distance", 0, NULL, ICON_NONE);
  }

  gpencil_modifier_panel_end(layout, &ptr);
}

static void mask_panel_draw(const bContext *C, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(C, panel, true, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Simplify, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Simplify = {
    /* name */ "Simplify",
    /* structName */ "SimplifyGpencilModifierData",
    /* structSize */ sizeof(SimplifyGpencilModifierData),
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
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
