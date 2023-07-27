/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>
#include <cstring>

#include "BLI_hash.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

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

static float *noise_table(int len, int offset, int seed)
{
  float *table = static_cast<float *>(MEM_callocN(sizeof(float) * len, __func__));
  for (int i = 0; i < len; i++) {
    table[i] = BLI_hash_int_01(BLI_hash_int_2d(seed, i + offset + 1));
  }
  return table;
}

BLI_INLINE float table_sample(float *table, float x)
{
  return interpf(table[int(ceilf(x))], table[int(floor(x))], fractf(x));
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

static void applyLength(GpencilModifierData *md,
                        Depsgraph *depsgraph,
                        bGPdata *gpd,
                        bGPDframe *gpf,
                        bGPDstroke *gps,
                        Object *ob)
{
  bool changed = false;
  LengthGpencilModifierData *lmd = (LengthGpencilModifierData *)md;
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

  float rand[2] = {0.0f, 0.0f};
  if (lmd->rand_start_fac != 0.0 || lmd->rand_end_fac != 0.0) {
    int seed = lmd->seed;

    /* Make sure different modifiers get different seeds. */
    seed += BLI_hash_string(ob->id.name + 2);
    seed += BLI_hash_string(md->name);

    if (lmd->flag & GP_LENGTH_USE_RANDOM) {
      seed += int(DEG_get_ctime(depsgraph)) / lmd->step;
    }

    float rand_offset = BLI_hash_int_01(seed);

    /* Get stroke index for random offset. */
    int rnd_index = BLI_findindex(&gpf->strokes, gps);
    const uint primes[2] = {2, 3};
    double offset[2] = {0.0f, 0.0f};
    double r[2];

    float *noise_table_length = noise_table(4, int(floor(lmd->rand_offset)), seed + 2);

    /* To ensure a nice distribution, we use halton sequence and offset using the seed. */
    BLI_halton_2d(primes, offset, rnd_index, r);
    for (int j = 0; j < 2; j++) {
      float noise = table_sample(noise_table_length, j * 2 + fractf(lmd->rand_offset));

      rand[j] = fmodf(r[j] + rand_offset, 1.0f);
      rand[j] = fabs(fmodf(sin(rand[j] * 12.9898f + j * 78.233f) * 43758.5453f, 1.0f) + noise);
    }

    MEM_SAFE_FREE(noise_table_length);

    first_fac = first_fac + rand[0] * lmd->rand_start_fac;
    second_fac = second_fac + rand[1] * lmd->rand_end_fac;
  }

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
                                     (float(gps->totpoints) - 2) *
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
                         Depsgraph *depsgraph,
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *gpf,
                         bGPDstroke *gps)
{
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
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
                                      lmd->flag & GP_LENGTH_INVERT_MATERIAL))
  {
    return;
  }
  if ((gps->flag & GP_STROKE_CYCLIC) != 0) {
    /* Don't affect cyclic strokes as they have no start/end. */
    return;
  }
  applyLength(md, depsgraph, gpd, gpf, gps, ob);
}

static void bakeModifier(Main * /*bmain*/,
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  generic_bake_deform_stroke(depsgraph, md, ob, false, deformStroke);
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  LengthGpencilModifierData *mmd = (LengthGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void random_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiItemR(layout, ptr, "use_random", 0, IFACE_("Randomize"), ICON_NONE);
}

static void random_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, RNA_boolean_get(ptr, "use_random"));

  uiItemR(layout, ptr, "step", 0, nullptr, ICON_NONE);
}

static void offset_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);
  uiLayoutSetPropSep(layout, true);
  uiItemR(layout, ptr, "random_start_factor", 0, IFACE_("Random Offset Start"), ICON_NONE);
  uiItemR(layout, ptr, "random_end_factor", 0, IFACE_("Random Offset End"), ICON_NONE);
  uiItemR(layout, ptr, "random_offset", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "seed", 0, nullptr, ICON_NONE);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);
  uiItemR(layout, ptr, "mode", 0, nullptr, ICON_NONE);

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

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void curvature_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiItemR(layout, ptr, "use_curvature", 0, IFACE_("Curvature"), ICON_NONE);
}

static void curvature_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_curvature"));

  uiItemR(col, ptr, "point_density", 0, nullptr, ICON_NONE);
  uiItemR(col, ptr, "segment_influence", 0, nullptr, ICON_NONE);
  uiItemR(col, ptr, "max_angle", 0, nullptr, ICON_NONE);
  uiItemR(col, ptr, "invert_curvature", 0, IFACE_("Invert"), ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Length, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "curvature", "", curvature_header_draw, curvature_panel_draw, panel_type);
  PanelType *offset_panel = gpencil_modifier_subpanel_register(
      region_type, "offset", "Random Offsets", nullptr, offset_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "randomize", "", random_header_draw, random_panel_draw, offset_panel);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Length = {
    /*name*/ N_("Length"),
    /*struct_name*/ "LengthGpencilModifierData",
    /*struct_size*/ sizeof(LengthGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copyData*/ copyData,

    /*deformStroke*/ deformStroke,
    /*generateStrokes*/ nullptr,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ nullptr,

    /*initData*/ initData,
    /*freeData*/ nullptr,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ nullptr,
    /*dependsOnTime*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ nullptr,
    /*panelRegister*/ panelRegister,
};
