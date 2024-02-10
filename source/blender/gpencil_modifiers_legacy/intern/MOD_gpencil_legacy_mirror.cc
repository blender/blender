/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>

#include "BLI_utildefines.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

static void init_data(GpencilModifierData *md)
{
  MirrorGpencilModifierData *gpmd = (MirrorGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(MirrorGpencilModifierData), modifier);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

/* Mirror is using current object as origin. */
static void update_mirror_local(bGPDstroke *gps, int axis)
{
  int i;
  bGPDspoint *pt;
  float factor[3] = {1.0f, 1.0f, 1.0f};
  factor[axis] = -1.0f;

  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    mul_v3_v3(&pt->x, factor);
  }
}

/* Mirror is using other object as origin. */
static void update_mirror_object(Object *ob,
                                 MirrorGpencilModifierData *mmd,
                                 bGPDstroke *gps,
                                 int axis)
{
  float mtx[4][4];
  unit_m4(mtx);
  mtx[axis][axis] = -1.0f;

  float tmp[4][4];
  float itmp[4][4];
  invert_m4_m4(tmp, mmd->object->object_to_world);
  mul_m4_m4m4(tmp, tmp, ob->object_to_world);
  invert_m4_m4(itmp, tmp);
  mul_m4_series(mtx, itmp, mtx, tmp);

  for (int i = 0; i < gps->totpoints; i++) {
    mul_m4_v3(mtx, &gps->points[i].x);
  }
}

static void update_position(Object *ob, MirrorGpencilModifierData *mmd, bGPDstroke *gps, int axis)
{
  if (mmd->object == nullptr) {
    update_mirror_local(gps, axis);
  }
  else {
    update_mirror_object(ob, mmd, gps, axis);
  }
}

static void generate_geometry(
    GpencilModifierData *md, Object *ob, bGPDlayer *gpl, bGPDframe *gpf, const bool update)
{
  MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  bGPDstroke *gps, *gps_new = nullptr;
  int tot_strokes;
  int i;

  /* check each axis for mirroring */
  for (int xi = 0; xi < 3; xi++) {
    if (mmd->flag & (GP_MIRROR_AXIS_X << xi)) {

      /* count strokes to avoid infinite loop after adding new strokes to tail of listbase */
      tot_strokes = BLI_listbase_count(&gpf->strokes);

      for (i = 0, gps = static_cast<bGPDstroke *>(gpf->strokes.first); i < tot_strokes;
           i++, gps = gps->next)
      {
        if (is_stroke_affected_by_modifier(ob,
                                           mmd->layername,
                                           mmd->material,
                                           mmd->pass_index,
                                           mmd->layer_pass,
                                           1,
                                           gpl,
                                           gps,
                                           mmd->flag & GP_MIRROR_INVERT_LAYER,
                                           mmd->flag & GP_MIRROR_INVERT_PASS,
                                           mmd->flag & GP_MIRROR_INVERT_LAYERPASS,
                                           mmd->flag & GP_MIRROR_INVERT_MATERIAL))
        {
          gps_new = BKE_gpencil_stroke_duplicate(gps, true, true);
          update_position(ob, mmd, gps_new, xi);
          if (update) {
            BKE_gpencil_stroke_geometry_update(gpd, gps_new);
          }
          BLI_addtail(&gpf->strokes, gps_new);
        }
      }
    }
  }
}

/* Generic "generate_strokes" callback */
static void generate_strokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = (bGPdata *)ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == nullptr) {
      continue;
    }
    generate_geometry(md, ob, gpl, gpf, false);
  }
}

static void bake_modifier(Main * /*bmain*/,
                          Depsgraph *depsgraph,
                          GpencilModifierData *md,
                          Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  int oldframe = int(DEG_get_ctime(depsgraph));

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      /* apply mirror effects on this frame */
      scene->r.cfra = gpf->framenum;
      BKE_scene_graph_update_for_newframe(depsgraph);

      /* compute mirror effects on this frame */
      generate_geometry(md, ob, gpl, gpf, true);
    }
  }

  /* return frame state and DB to original state */
  scene->r.cfra = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph);
}

static bool is_disabled(GpencilModifierData * /*md*/, bool /*use_render_params*/)
{
  // MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;

  return false;
}

static void update_depsgraph(GpencilModifierData *md,
                             const ModifierUpdateDepsgraphContext *ctx,
                             const int /*mode*/)
{
  MirrorGpencilModifierData *lmd = (MirrorGpencilModifierData *)md;
  if (lmd->object != nullptr) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Mirror Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Mirror Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Mirror Modifier");
}

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;
  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Axis"));
  uiItemR(row, ptr, "use_axis_x", toggles_flag, nullptr, ICON_NONE);
  uiItemR(row, ptr, "use_axis_y", toggles_flag, nullptr, ICON_NONE);
  uiItemR(row, ptr, "use_axis_z", toggles_flag, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "object", UI_ITEM_NONE, nullptr, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Mirror, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Mirror = {
    /*name*/ N_("Mirror"),
    /*struct_name*/ "MirrorGpencilModifierData",
    /*struct_size*/ sizeof(MirrorGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copy_data*/ copy_data,

    /*deform_stroke*/ nullptr,
    /*generate_strokes*/ generate_strokes,
    /*bake_modifier*/ bake_modifier,
    /*remap_time*/ nullptr,

    /*init_data*/ init_data,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*panel_register*/ panel_register,
};
