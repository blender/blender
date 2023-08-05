/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>
#include <cstring> /* For #MEMCPY_STRUCT_AFTER. */

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lattice.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static void init_data(GpencilModifierData *md)
{
  LatticeGpencilModifierData *gpmd = (LatticeGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(LatticeGpencilModifierData), modifier);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer *gpl,
                          bGPDframe * /*gpf*/,
                          bGPDstroke *gps)
{
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_LATTICE_INVERT_LAYER,
                                      mmd->flag & GP_LATTICE_INVERT_PASS,
                                      mmd->flag & GP_LATTICE_INVERT_LAYERPASS,
                                      mmd->flag & GP_LATTICE_INVERT_MATERIAL))
  {
    return;
  }

  if (mmd->cache_data == nullptr) {
    return;
  }

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != nullptr ? &gps->dvert[i] : nullptr;

    /* verify vertex group */
    const float weight = get_modifier_point_weight(
        dvert, (mmd->flag & GP_LATTICE_INVERT_VGROUP) != 0, def_nr);
    if (weight < 0.0f) {
      continue;
    }
    BKE_lattice_deform_data_eval_co(
        (LatticeDeformData *)mmd->cache_data, &pt->x, mmd->strength * weight);
  }
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

/* FIXME: Ideally we be doing this on a copy of the main depsgraph
 * (i.e. one where we don't have to worry about restoring state)
 */
static void bake_modifier(Main * /*bmain*/,
                          Depsgraph *depsgraph,
                          GpencilModifierData *md,
                          Object *ob)
{
  LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  LatticeDeformData *ldata = nullptr;
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  int oldframe = int(DEG_get_ctime(depsgraph));

  if ((mmd->object == nullptr) || (mmd->object->type != OB_LATTICE)) {
    return;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      /* Apply lattice effects on this frame
       * NOTE: this assumes that we don't want lattice animation on non-keyframed frames.
       */
      scene->r.cfra = gpf->framenum;
      BKE_scene_graph_update_for_newframe(depsgraph);

      /* Recalculate lattice data. */
      if (mmd->cache_data) {
        BKE_lattice_deform_data_destroy(mmd->cache_data);
      }
      mmd->cache_data = BKE_lattice_deform_data_create(mmd->object, ob);

      /* Compute lattice effects on this frame. */
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        deform_stroke(md, depsgraph, ob, gpl, gpf, gps);
      }
    }
  }

  /* Free lingering data. */
  ldata = (LatticeDeformData *)mmd->cache_data;
  if (ldata) {
    BKE_lattice_deform_data_destroy(ldata);
    mmd->cache_data = nullptr;
  }

  /* Return frame state and DB to original state. */
  scene->r.cfra = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph);
}

static void free_data(GpencilModifierData *md)
{
  LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;
  LatticeDeformData *ldata = (LatticeDeformData *)mmd->cache_data;
  /* free deform data */
  if (ldata) {
    BKE_lattice_deform_data_destroy(ldata);
  }
}

static bool is_disabled(GpencilModifierData *md, bool /*use_render_params*/)
{
  LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the lattice is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return !mmd->object || mmd->object->type != OB_LATTICE;
}

static void update_depsgraph(GpencilModifierData *md,
                             const ModifierUpdateDepsgraphContext *ctx,
                             const int /*mode*/)
{
  LatticeGpencilModifierData *lmd = (LatticeGpencilModifierData *)md;
  if (lmd->object != nullptr) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Lattice Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Lattice Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Lattice Modifier");
}

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *sub, *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA hook_object_ptr = RNA_pointer_get(ptr, "object");
  bool has_vertex_group = RNA_string_length(ptr, "vertex_group") != 0;

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "object", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (!RNA_pointer_is_null(&hook_object_ptr) &&
      RNA_enum_get(&hook_object_ptr, "type") == OB_ARMATURE)
  {
    PointerRNA hook_object_data_ptr = RNA_pointer_get(&hook_object_ptr, "data");
    uiItemPointerR(
        col, ptr, "subtarget", &hook_object_data_ptr, "bones", IFACE_("Bone"), ICON_NONE);
  }

  row = uiLayoutRow(layout, true);
  uiItemPointerR(row, ptr, "vertex_group", &ob_ptr, "vertex_groups", nullptr, ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetActive(sub, has_vertex_group);
  uiLayoutSetPropSep(sub, false);
  uiItemR(sub, ptr, "invert_vertex", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);

  uiItemR(layout, ptr, "strength", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Lattice, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Lattice = {
    /*name*/ N_("Lattice"),
    /*struct_name*/ "LatticeGpencilModifierData",
    /*struct_size*/ sizeof(LatticeGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copy_data*/ copy_data,

    /*deform_stroke*/ deform_stroke,
    /*generate_strokes*/ nullptr,
    /*bake_modifier*/ bake_modifier,
    /*remap_time*/ nullptr,

    /*init_data*/ init_data,
    /*free_data*/ free_data,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*panel_register*/ panel_register,
};
