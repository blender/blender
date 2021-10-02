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
                                  const float length,
                                  const float overshoot_fac,
                                  const short len_mode,
                                  const bool use_curvature,
                                  const int extra_point_count,
                                  const float segment_influence,
                                  const float max_angle,
                                  const bool invert_curvature)
{
  bool changed = false;
  if (length == 0.0f) {
    return changed;
  }

  if (length > 0.0f) {
    changed = BKE_gpencil_stroke_stretch(gps,
                                         length,
                                         overshoot_fac,
                                         len_mode,
                                         use_curvature,
                                         extra_point_count,
                                         segment_influence,
                                         max_angle,
                                         invert_curvature);
  }
  else {
    changed = BKE_gpencil_stroke_shrink(gps, fabs(length), len_mode);
  }

  return changed;
}

static void applyLength(LengthGpencilModifierData *lmd, bGPdata *gpd, bGPDstroke *gps)
{
  bool changed = false;
  const float len = (lmd->mode == GP_LENGTH_ABSOLUTE) ? 1.0f :
                                                        BKE_gpencil_stroke_length(gps, true);
  const int totpoints = gps->totpoints;
  if (len < FLT_EPSILON) {
    return;
  }

  /* Always do the stretching first since it might depend on points which could be deleted by the
   * shrink. */
  float first_fac = lmd->start_fac;
  int first_mode = 1;
  float second_fac = lmd->end_fac;
  int second_mode = 2;
  if (first_fac < 0) {
    SWAP(float, first_fac, second_fac);
    SWAP(int, first_mode, second_mode);
  }

  const int first_extra_point_count = ceil(first_fac * lmd->point_density);
  const int second_extra_point_count = ceil(second_fac * lmd->point_density);

  changed |= gpencil_modify_stroke(gps,
                                   len * first_fac,
                                   lmd->overshoot_fac,
                                   first_mode,
                                   lmd->flag & GP_LENGTH_USE_CURVATURE,
                                   first_extra_point_count,
                                   lmd->segment_influence,
                                   lmd->max_angle,
                                   lmd->flag & GP_LENGTH_INVERT_CURVATURE);
  /* HACK: The second #overshoot_fac needs to be adjusted because it is not
   * done in the same stretch call, because it can have a different length.
   * The adjustment needs to be stable when
   * `ceil(overshoot_fac*(gps->totpoints - 2))` is used in stretch and never
   * produce a result higher than `totpoints - 2`. */
  const float second_overshoot_fac = lmd->overshoot_fac * (totpoints - 2) /
                                     ((float)gps->totpoints - 2) *
                                     (1.0f - 0.1f / (totpoints - 1.0f));
  changed |= gpencil_modify_stroke(gps,
                                   len * second_fac,
                                   second_overshoot_fac,
                                   second_mode,
                                   lmd->flag & GP_LENGTH_USE_CURVATURE,
                                   second_extra_point_count,
                                   lmd->segment_influence,
                                   lmd->max_angle,
                                   lmd->flag & GP_LENGTH_INVERT_CURVATURE);

  if (changed) {
    BKE_gpencil_stroke_geometry_update(gpd, gps);
  }
}

static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  bGPdata *gpd = ob->data;
  LengthGpencilModifierData *lmd = (LengthGpencilModifierData *)md;
  if (!is_stroke_affected_by_modifier(ob,
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
    return;
  }
  if ((gps->flag & GP_STROKE_CYCLIC) != 0) {
    /* Don't affect cyclic strokes as they have no start/end. */
    return;
  }
  applyLength(lmd, gpd, gps);
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

  if (RNA_enum_get(ptr, "mode") == GP_LENGTH_RELATIVE) {
    uiItemR(col, ptr, "start_factor", 0, IFACE_("Start"), ICON_NONE);
    uiItemR(col, ptr, "end_factor", 0, IFACE_("End"), ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "start_length", 0, IFACE_("Start"), ICON_NONE);
    uiItemR(col, ptr, "end_length", 0, IFACE_("End"), ICON_NONE);
  }

  uiItemR(layout, ptr, "overshoot_factor", UI_ITEM_R_SLIDER, IFACE_("Used Length"), ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void curvature_header_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiItemR(layout, ptr, "use_curvature", 0, IFACE_("Curvature"), ICON_NONE);
}

static void curvature_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_curvature"));

  uiItemR(col, ptr, "point_density", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "segment_influence", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "max_angle", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "invert_curvature", 0, IFACE_("Invert"), ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Length, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "curvature", "", curvature_header_draw, curvature_panel_draw, panel_type);
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
