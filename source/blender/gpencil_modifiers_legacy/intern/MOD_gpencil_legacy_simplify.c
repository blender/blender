/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>
#include <string.h> /* For #MEMCPY_STRUCT_AFTER. */

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void initData(GpencilModifierData *md)
{
  SimplifyGpencilModifierData *gpmd = (SimplifyGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(SimplifyGpencilModifierData), modifier);
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
                                      mmd->mode == GP_SIMPLIFY_SAMPLE ? 2 : 3,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_SIMPLIFY_INVERT_LAYER,
                                      mmd->flag & GP_SIMPLIFY_INVERT_PASS,
                                      mmd->flag & GP_SIMPLIFY_INVERT_LAYERPASS,
                                      mmd->flag & GP_SIMPLIFY_INVERT_MATERIAL))
  {
    return;
  }
  bGPdata *gpd = ob->data;
  /* Select simplification mode. */
  switch (mmd->mode) {
    case GP_SIMPLIFY_FIXED: {
      for (int i = 0; i < mmd->step; i++) {
        BKE_gpencil_stroke_simplify_fixed(gpd, gps);
      }
      break;
    }
    case GP_SIMPLIFY_ADAPTIVE: {
      /* simplify stroke using Ramer-Douglas-Peucker algorithm */
      BKE_gpencil_stroke_simplify_adaptive(gpd, gps, mmd->factor);
      break;
    }
    case GP_SIMPLIFY_SAMPLE: {
      BKE_gpencil_stroke_sample(gpd, gps, mmd->length, false, mmd->sharp_threshold);
      break;
    }
    case GP_SIMPLIFY_MERGE: {
      BKE_gpencil_stroke_merge_distance(gpd, gpf, gps, mmd->distance, true);
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
  generic_bake_deform_stroke(depsgraph, md, ob, false, deformStroke);
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  SimplifyGpencilModifierData *mmd = (SimplifyGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);

  if (mode == GP_SIMPLIFY_FIXED) {
    uiItemR(layout, ptr, "step", 0, NULL, ICON_NONE);
  }
  else if (mode == GP_SIMPLIFY_ADAPTIVE) {
    uiItemR(layout, ptr, "factor", 0, NULL, ICON_NONE);
  }
  else if (mode == GP_SIMPLIFY_SAMPLE) {
    uiItemR(layout, ptr, "length", 0, NULL, ICON_NONE);
    uiItemR(layout, ptr, "sharp_threshold", 0, NULL, ICON_NONE);
  }
  else if (mode == GP_SIMPLIFY_MERGE) {
    uiItemR(layout, ptr, "distance", 0, NULL, ICON_NONE);
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Simplify, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Simplify = {
    /*name*/ N_("Simplify"),
    /*structName*/ "SimplifyGpencilModifierData",
    /*structSize*/ sizeof(SimplifyGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copyData*/ copyData,

    /*deformStroke*/ deformStroke,
    /*generateStrokes*/ NULL,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ NULL,

    /*initData*/ initData,
    /*freeData*/ NULL,
    /*isDisabled*/ NULL,
    /*updateDepsgraph*/ NULL,
    /*dependsOnTime*/ NULL,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ NULL,
    /*panelRegister*/ panelRegister,
};
