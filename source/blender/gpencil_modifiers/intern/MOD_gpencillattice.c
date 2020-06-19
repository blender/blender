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
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"
#include "MOD_gpencil_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static void initData(GpencilModifierData *md)
{
  LatticeGpencilModifierData *gpmd = (LatticeGpencilModifierData *)md;
  gpmd->pass_index = 0;
  gpmd->material = NULL;
  gpmd->object = NULL;
  gpmd->cache_data = NULL;
  gpmd->strength = 1.0f;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
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
                                      mmd->flag & GP_LATTICE_INVERT_MATERIAL)) {
    return;
  }

  if (mmd->cache_data == NULL) {
    return;
  }

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;

    /* verify vertex group */
    const float weight = get_modifier_point_weight(
        dvert, (mmd->flag & GP_LATTICE_INVERT_VGROUP) != 0, def_nr);
    if (weight < 0.0f) {
      continue;
    }
    BKE_lattice_deform_data_eval_co(
        (struct LatticeDeformData *)mmd->cache_data, &pt->x, mmd->strength * weight);
  }
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gps);
}

/* FIXME: Ideally we be doing this on a copy of the main depsgraph
 * (i.e. one where we don't have to worry about restoring state)
 */
static void bakeModifier(Main *bmain, Depsgraph *depsgraph, GpencilModifierData *md, Object *ob)
{
  LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  struct LatticeDeformData *ldata = NULL;
  bGPdata *gpd = ob->data;
  int oldframe = (int)DEG_get_ctime(depsgraph);

  if (mmd->object == NULL) {
    return;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      /* apply lattice effects on this frame
       * NOTE: this assumes that we don't want lattice animation on non-keyframed frames
       */
      CFRA = gpf->framenum;
      BKE_scene_graph_update_for_newframe(depsgraph, bmain);

      /* recalculate lattice data */
      BKE_gpencil_lattice_init(ob);

      /* compute lattice effects on this frame */
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        deformStroke(md, depsgraph, ob, gpl, gpf, gps);
      }
    }
  }

  /* free lingering data */
  ldata = (struct LatticeDeformData *)mmd->cache_data;
  if (ldata) {
    BKE_lattice_deform_data_destroy(ldata);
    mmd->cache_data = NULL;
  }

  /* return frame state and DB to original state */
  CFRA = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph, bmain);
}

static void freeData(GpencilModifierData *md)
{
  LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;
  struct LatticeDeformData *ldata = (struct LatticeDeformData *)mmd->cache_data;
  /* free deform data */
  if (ldata) {
    BKE_lattice_deform_data_destroy(ldata);
  }
}

static bool isDisabled(GpencilModifierData *md, int UNUSED(userRenderParams))
{
  LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the lattice is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return !mmd->object || mmd->object->type != OB_LATTICE;
}

static void updateDepsgraph(GpencilModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  LatticeGpencilModifierData *lmd = (LatticeGpencilModifierData *)md;
  if (lmd->object != NULL) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Lattice Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Lattice Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Lattice Modifier");
}

static void foreachObjectLink(GpencilModifierData *md,
                              Object *ob,
                              ObjectWalkFunc walk,
                              void *userData)
{
  LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;

  walk(userData, ob, &mmd->object, IDWALK_CB_NOP);
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);

  foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *sub, *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  PointerRNA hook_object_ptr = RNA_pointer_get(&ptr, "object");
  bool has_vertex_group = RNA_string_length(&ptr, "vertex_group") != 0;

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &ptr, "object", 0, NULL, ICON_NONE);
  if (!RNA_pointer_is_null(&hook_object_ptr) &&
      RNA_enum_get(&hook_object_ptr, "type") == OB_ARMATURE) {
    PointerRNA hook_object_data_ptr = RNA_pointer_get(&hook_object_ptr, "data");
    uiItemPointerR(
        col, &ptr, "subtarget", &hook_object_data_ptr, "bones", IFACE_("Bone"), ICON_NONE);
  }

  row = uiLayoutRow(layout, true);
  uiItemPointerR(row, &ptr, "vertex_group", &ob_ptr, "vertex_groups", NULL, ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetActive(sub, has_vertex_group);
  uiLayoutSetPropSep(sub, false);
  uiItemR(sub, &ptr, "invert_vertex", 0, "", ICON_ARROW_LEFTRIGHT);

  uiItemR(layout, &ptr, "strength", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  gpencil_modifier_panel_end(layout, &ptr);
}

static void mask_panel_draw(const bContext *C, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(C, panel, true, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Lattice, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Lattice = {
    /* name */ "Lattice",
    /* structName */ "LatticeGpencilModifierData",
    /* structSize */ sizeof(LatticeGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_Single | eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformStroke */ deformStroke,
    /* generateStrokes */ NULL,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
