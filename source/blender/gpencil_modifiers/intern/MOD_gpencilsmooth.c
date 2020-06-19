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
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
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
  SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;
  gpmd->pass_index = 0;
  gpmd->flag |= GP_SMOOTH_MOD_LOCATION;
  gpmd->factor = 0.5f;
  gpmd->material = NULL;
  gpmd->step = 1;

  gpmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  if (gpmd->curve_intensity) {
    CurveMapping *curve = gpmd->curve_intensity;
    BKE_curvemapping_initialize(curve);
  }
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  SmoothGpencilModifierData *gmd = (SmoothGpencilModifierData *)md;
  SmoothGpencilModifierData *tgmd = (SmoothGpencilModifierData *)target;

  if (tgmd->curve_intensity != NULL) {
    BKE_curvemapping_free(tgmd->curve_intensity);
    tgmd->curve_intensity = NULL;
  }

  BKE_gpencil_modifier_copydata_generic(md, target);

  tgmd->curve_intensity = BKE_curvemapping_copy(gmd->curve_intensity);
}

/* aply smooth effect based on stroke direction */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  SmoothGpencilModifierData *mmd = (SmoothGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);
  const bool use_curve = (mmd->flag & GP_SMOOTH_CUSTOM_CURVE) != 0 && mmd->curve_intensity;

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      3,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_SMOOTH_INVERT_LAYER,
                                      mmd->flag & GP_SMOOTH_INVERT_PASS,
                                      mmd->flag & GP_SMOOTH_INVERT_LAYERPASS,
                                      mmd->flag & GP_SMOOTH_INVERT_MATERIAL)) {
    return;
  }

  /* smooth stroke */
  if (mmd->factor > 0.0f) {
    for (int r = 0; r < mmd->step; r++) {
      for (int i = 0; i < gps->totpoints; i++) {
        MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;

        /* verify vertex group */
        float weight = get_modifier_point_weight(
            dvert, (mmd->flag & GP_SMOOTH_INVERT_VGROUP) != 0, def_nr);
        if (weight < 0.0f) {
          continue;
        }

        /* Custom curve to modulate value. */
        if (use_curve) {
          float value = (float)i / (gps->totpoints - 1);
          weight *= BKE_curvemapping_evaluateF(mmd->curve_intensity, 0, value);
        }

        const float val = mmd->factor * weight;
        /* perform smoothing */
        if (mmd->flag & GP_SMOOTH_MOD_LOCATION) {
          BKE_gpencil_stroke_smooth(gps, i, val);
        }
        if (mmd->flag & GP_SMOOTH_MOD_STRENGTH) {
          BKE_gpencil_stroke_smooth_strength(gps, i, val);
        }
        if ((mmd->flag & GP_SMOOTH_MOD_THICKNESS) && (val > 0.0f)) {
          /* thickness need to repeat process several times */
          for (int r2 = 0; r2 < r * 10; r2++) {
            BKE_gpencil_stroke_smooth_thickness(gps, i, val);
          }
        }
        if (mmd->flag & GP_SMOOTH_MOD_UV) {
          BKE_gpencil_stroke_smooth_uv(gps, i, val);
        }
      }
    }
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

static void freeData(GpencilModifierData *md)
{
  SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;

  if (gpmd->curve_intensity) {
    BKE_curvemapping_free(gpmd->curve_intensity);
  }
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  SmoothGpencilModifierData *mmd = (SmoothGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  row = uiLayoutRow(layout, true);
  uiItemR(row, &ptr, "use_edit_position", UI_ITEM_R_TOGGLE, IFACE_("Position"), ICON_NONE);
  uiItemR(row, &ptr, "use_edit_strength", UI_ITEM_R_TOGGLE, IFACE_("Stength"), ICON_NONE);
  uiItemR(row, &ptr, "use_edit_thickness", UI_ITEM_R_TOGGLE, IFACE_("Thickness"), ICON_NONE);
  uiItemR(row, &ptr, "use_edit_uv", UI_ITEM_R_TOGGLE, IFACE_("UV"), ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "factor", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "step", 0, IFACE_("Repeat"), ICON_NONE);

  gpencil_modifier_panel_end(layout, &ptr);
}

static void mask_panel_draw(const bContext *C, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(C, panel, true, true);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Smooth, panel_draw);
  PanelType *mask_panel_type = gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(region_type,
                                     "curve",
                                     "",
                                     gpencil_modifier_curve_header_draw,
                                     gpencil_modifier_curve_panel_draw,
                                     mask_panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Smooth = {
    /* name */ "Smooth",
    /* structName */ "SmoothGpencilModifierData",
    /* structSize */ sizeof(SmoothGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformStroke */ deformStroke,
    /* generateStrokes */ NULL,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ freeData,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
