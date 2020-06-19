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

#include "BLI_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"

#include "BLT_translation.h"

#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
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
  OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;
  gpmd->pass_index = 0;
  gpmd->factor = 1.0f;
  gpmd->hardeness = 1.0f;
  gpmd->material = NULL;
  gpmd->modify_color = GP_MODIFY_COLOR_BOTH;
  gpmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  if (gpmd->curve_intensity) {
    CurveMapping *curve = gpmd->curve_intensity;
    BKE_curvemapping_initialize(curve);
  }
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  OpacityGpencilModifierData *gmd = (OpacityGpencilModifierData *)md;
  OpacityGpencilModifierData *tgmd = (OpacityGpencilModifierData *)target;

  if (tgmd->curve_intensity != NULL) {
    BKE_curvemapping_free(tgmd->curve_intensity);
    tgmd->curve_intensity = NULL;
  }

  BKE_gpencil_modifier_copydata_generic(md, target);

  tgmd->curve_intensity = BKE_curvemapping_copy(gmd->curve_intensity);
}

/* opacity strokes */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  OpacityGpencilModifierData *mmd = (OpacityGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);
  const bool use_curve = (mmd->flag & GP_OPACITY_CUSTOM_CURVE) != 0 && mmd->curve_intensity;

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_OPACITY_INVERT_LAYER,
                                      mmd->flag & GP_OPACITY_INVERT_PASS,
                                      mmd->flag & GP_OPACITY_INVERT_LAYERPASS,
                                      mmd->flag & GP_OPACITY_INVERT_MATERIAL)) {
    return;
  }

  /* Hardness (at stroke level). */
  if (mmd->modify_color == GP_MODIFY_COLOR_HARDNESS) {
    gps->hardeness *= mmd->hardeness;
    CLAMP(gps->hardeness, 0.0f, 1.0f);

    return;
  }

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;

    /* Stroke using strength. */
    if (mmd->modify_color != GP_MODIFY_COLOR_FILL) {
      /* verify vertex group */
      float weight = get_modifier_point_weight(
          dvert, (mmd->flag & GP_OPACITY_INVERT_VGROUP) != 0, def_nr);
      if (weight < 0.0f) {
        continue;
      }
      /* Custom curve to modulate value. */
      float factor_curve = mmd->factor;
      if (use_curve) {
        float value = (float)i / (gps->totpoints - 1);
        factor_curve *= BKE_curvemapping_evaluateF(mmd->curve_intensity, 0, value);
      }

      if (def_nr < 0) {
        if (mmd->flag & GP_OPACITY_NORMALIZE) {
          pt->strength = factor_curve;
        }
        else {
          pt->strength += factor_curve - 1.0f;
        }
      }
      else {
        /* High factor values, change weight too. */
        if ((factor_curve > 1.0f) && (weight < 1.0f)) {
          weight += factor_curve - 1.0f;
          CLAMP(weight, 0.0f, 1.0f);
        }
        if (mmd->flag & GP_OPACITY_NORMALIZE) {
          pt->strength = factor_curve;
        }
        else {
          pt->strength += (factor_curve - 1) * weight;
        }
      }

      CLAMP(pt->strength, 0.0f, 1.0f);
    }
  }

  /* Fill using opacity factor. */
  if (mmd->modify_color != GP_MODIFY_COLOR_STROKE) {
    gps->fill_opacity_fac = mmd->factor;
    CLAMP(gps->fill_opacity_fac, 0.0f, 1.0f);
  }
}

static void bakeModifier(Main *UNUSED(bmain),
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
  OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;

  if (gpmd->curve_intensity) {
    BKE_curvemapping_free(gpmd->curve_intensity);
  }
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  OpacityGpencilModifierData *mmd = (OpacityGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  int modify_color = RNA_enum_get(&ptr, "modify_color");

  uiItemR(layout, &ptr, "modify_color", 0, NULL, ICON_NONE);

  if (modify_color == GP_MODIFY_COLOR_HARDNESS) {
    uiItemR(layout, &ptr, "hardness", 0, NULL, ICON_NONE);
  }
  else {
    uiItemR(layout, &ptr, "normalize_opacity", 0, NULL, ICON_NONE);
    const char *text = (RNA_boolean_get(&ptr, "normalize_opacity")) ? IFACE_("Strength") :
                                                                      IFACE_("Opacity Factor");
    uiItemR(layout, &ptr, "hardness", 0, text, ICON_NONE);
  }

  gpencil_modifier_panel_end(layout, &ptr);
}

static void mask_panel_draw(const bContext *C, Panel *panel)
{
  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  int modify_color = RNA_enum_get(&ptr, "modify_color");
  bool show_vertex = (modify_color != GP_MODIFY_COLOR_HARDNESS);

  gpencil_modifier_masking_panel_draw(C, panel, true, show_vertex);
}

static void curve_header_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  int modify_color = RNA_enum_get(&ptr, "modify_color");
  uiLayoutSetActive(layout, modify_color != GP_MODIFY_COLOR_HARDNESS);

  gpencil_modifier_curve_header_draw(C, panel);
}

static void curve_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  int modify_color = RNA_enum_get(&ptr, "modify_color");
  uiLayoutSetActive(layout, modify_color != GP_MODIFY_COLOR_HARDNESS);

  gpencil_modifier_curve_panel_draw(C, panel);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Opacity, panel_draw);
  PanelType *mask_panel_type = gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "curve", "", curve_header_draw, curve_panel_draw, mask_panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Opacity = {
    /* name */ "Opacity",
    /* structName */ "OpacityGpencilModifierData",
    /* structSize */ sizeof(OpacityGpencilModifierData),
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
