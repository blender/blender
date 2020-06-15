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
 * The Original Code is Copyright (C) 2011 by Bastien Montagne.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_rand.h"

#include "BLT_translation.h"

#include "DNA_color_types.h" /* CurveMapping. */
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h" /* CurveMapping. */
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"
#include "BKE_texture.h" /* Texture masking. */

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"
#include "MOD_util.h"
#include "MOD_weightvg_util.h"

/**************************************
 * Modifiers functions.               *
 **************************************/
static void initData(ModifierData *md)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;
  wmd->edit_flags = 0;
  wmd->falloff_type = MOD_WVG_MAPPING_NONE;
  wmd->default_weight = 0.0f;

  wmd->cmap_curve = BKE_curvemapping_add(1, 0.0, 0.0, 1.0, 1.0);
  BKE_curvemapping_initialize(wmd->cmap_curve);

  wmd->rem_threshold = 0.01f;
  wmd->add_threshold = 0.01f;

  wmd->mask_constant = 1.0f;
  wmd->mask_tex_use_channel = MOD_WVG_MASK_TEX_USE_INT; /* Use intensity by default. */
  wmd->mask_tex_mapping = MOD_DISP_MAP_LOCAL;
}

static void freeData(ModifierData *md)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;
  BKE_curvemapping_free(wmd->cmap_curve);
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  const WeightVGEditModifierData *wmd = (const WeightVGEditModifierData *)md;
  WeightVGEditModifierData *twmd = (WeightVGEditModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  twmd->cmap_curve = BKE_curvemapping_copy(wmd->cmap_curve);
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

  /* We need vertex groups! */
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;

  /* Ask for UV coordinates if we need them. */
  if (wmd->mask_tex_mapping == MOD_DISP_MAP_UV) {
    r_cddata_masks->fmask |= CD_MASK_MTFACE;
  }

  /* No need to ask for CD_PREVIEW_MLOOPCOL... */
}

static bool dependsOnTime(ModifierData *md)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

  if (wmd->mask_texture) {
    return BKE_texture_dependsOnTime(wmd->mask_texture);
  }
  return false;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;
  walk(userData, ob, &wmd->mask_tex_map_obj, IDWALK_CB_NOP);
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

  walk(userData, ob, (ID **)&wmd->mask_texture, IDWALK_CB_USER);

  foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void foreachTexLink(ModifierData *md, Object *ob, TexWalkFunc walk, void *userData)
{
  walk(userData, ob, md, "mask_texture");
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;
  bool need_transform_relation = false;

  if (wmd->mask_texture != NULL) {
    DEG_add_generic_id_relation(ctx->node, &wmd->mask_texture->id, "WeightVGEdit Modifier");

    if (wmd->mask_tex_map_obj != NULL && wmd->mask_tex_mapping == MOD_DISP_MAP_OBJECT) {
      MOD_depsgraph_update_object_bone_relation(
          ctx->node, wmd->mask_tex_map_obj, wmd->mask_tex_map_bone, "WeightVGEdit Modifier");
      need_transform_relation = true;
    }
    else if (wmd->mask_tex_mapping == MOD_DISP_MAP_GLOBAL) {
      need_transform_relation = true;
    }
  }

  if (need_transform_relation) {
    DEG_add_modifier_to_transform_relation(ctx->node, "WeightVGEdit Modifier");
  }
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;
  /* If no vertex group, bypass. */
  return (wmd->defgrp_name[0] == '\0');
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  BLI_assert(mesh != NULL);

  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

  MDeformVert *dvert = NULL;
  MDeformWeight **dw = NULL;
  float *org_w; /* Array original weights. */
  float *new_w; /* Array new weights. */
  int i;
  const bool invert_vgroup_mask = (wmd->edit_flags & MOD_WVG_EDIT_INVERT_VGROUP_MASK) != 0;

  /* Flags. */
  const bool do_add = (wmd->edit_flags & MOD_WVG_EDIT_ADD2VG) != 0;
  const bool do_rem = (wmd->edit_flags & MOD_WVG_EDIT_REMFVG) != 0;
  /* Only do weight-preview in Object, Sculpt and Pose modes! */
#if 0
  const bool do_prev = (wmd->modifier.mode & eModifierMode_DoWeightPreview);
#endif

  /* Get number of verts. */
  const int numVerts = mesh->totvert;

  /* Check if we can just return the original mesh.
   * Must have verts and therefore verts assigned to vgroups to do anything useful!
   */
  if ((numVerts == 0) || BLI_listbase_is_empty(&ctx->object->defbase)) {
    return mesh;
  }

  /* Get vgroup idx from its name. */
  const int defgrp_index = BKE_object_defgroup_name_index(ctx->object, wmd->defgrp_name);
  if (defgrp_index == -1) {
    return mesh;
  }

  const bool has_mdef = CustomData_has_layer(&mesh->vdata, CD_MDEFORMVERT);
  /* If no vertices were ever added to an object's vgroup, dvert might be NULL. */
  if (!has_mdef) {
    /* If this modifier is not allowed to add vertices, just return. */
    if (!do_add) {
      return mesh;
    }
  }

  if (has_mdef) {
    dvert = CustomData_duplicate_referenced_layer(&mesh->vdata, CD_MDEFORMVERT, numVerts);
  }
  else {
    /* Add a valid data layer! */
    dvert = CustomData_add_layer(&mesh->vdata, CD_MDEFORMVERT, CD_CALLOC, NULL, numVerts);
  }
  /* Ultimate security check. */
  if (!dvert) {
    return mesh;
  }
  mesh->dvert = dvert;

  /* Get org weights, assuming 0.0 for vertices not in given vgroup. */
  org_w = MEM_malloc_arrayN(numVerts, sizeof(float), "WeightVGEdit Modifier, org_w");
  new_w = MEM_malloc_arrayN(numVerts, sizeof(float), "WeightVGEdit Modifier, new_w");
  dw = MEM_malloc_arrayN(numVerts, sizeof(MDeformWeight *), "WeightVGEdit Modifier, dw");
  for (i = 0; i < numVerts; i++) {
    dw[i] = BKE_defvert_find_index(&dvert[i], defgrp_index);
    if (dw[i]) {
      org_w[i] = new_w[i] = dw[i]->weight;
    }
    else {
      org_w[i] = new_w[i] = wmd->default_weight;
    }
  }

  /* Do mapping. */
  const bool do_invert_mapping = (wmd->edit_flags & MOD_WVG_INVERT_FALLOFF) != 0;
  const bool do_normalize = (wmd->edit_flags & MOD_WVG_EDIT_WEIGHTS_NORMALIZE) != 0;
  if (do_invert_mapping || wmd->falloff_type != MOD_WVG_MAPPING_NONE) {
    RNG *rng = NULL;

    if (wmd->falloff_type == MOD_WVG_MAPPING_RANDOM) {
      rng = BLI_rng_new_srandom(BLI_ghashutil_strhash(ctx->object->id.name + 2));
    }

    weightvg_do_map(numVerts, new_w, wmd->falloff_type, do_invert_mapping, wmd->cmap_curve, rng);

    if (rng) {
      BLI_rng_free(rng);
    }
  }

  /* Do masking. */
  struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  weightvg_do_mask(ctx,
                   numVerts,
                   NULL,
                   org_w,
                   new_w,
                   ctx->object,
                   mesh,
                   wmd->mask_constant,
                   wmd->mask_defgrp_name,
                   scene,
                   wmd->mask_texture,
                   wmd->mask_tex_use_channel,
                   wmd->mask_tex_mapping,
                   wmd->mask_tex_map_obj,
                   wmd->mask_tex_map_bone,
                   wmd->mask_tex_uvlayer_name,
                   invert_vgroup_mask);

  /* Update/add/remove from vgroup. */
  weightvg_update_vg(dvert,
                     defgrp_index,
                     dw,
                     numVerts,
                     NULL,
                     org_w,
                     do_add,
                     wmd->add_threshold,
                     do_rem,
                     wmd->rem_threshold,
                     do_normalize);

  /* If weight preview enabled... */
#if 0 /* XXX Currently done in mod stack :/ */
  if (do_prev) {
    DM_update_weight_mcol(ob, dm, 0, org_w, 0, NULL);
  }
#endif

  /* Freeing stuff. */
  MEM_freeN(org_w);
  MEM_freeN(new_w);
  MEM_freeN(dw);

  /* Return the vgroup-modified mesh. */
  return mesh;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *sub, *col, *row;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, true);
  uiItemPointerR(col, &ptr, "vertex_group", &ob_ptr, "vertex_groups", NULL, ICON_NONE);

  uiItemR(layout, &ptr, "default_weight", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  col = uiLayoutColumnWithHeading(layout, false, IFACE_("Group Add"));
  row = uiLayoutRow(col, true);
  uiLayoutSetPropDecorate(row, false);
  sub = uiLayoutRow(row, true);
  uiItemR(sub, &ptr, "use_add", 0, "", ICON_NONE);
  sub = uiLayoutRow(sub, true);
  uiLayoutSetActive(sub, RNA_boolean_get(&ptr, "use_add"));
  uiLayoutSetPropSep(sub, false);
  uiItemR(sub, &ptr, "add_threshold", UI_ITEM_R_SLIDER, "Threshold", ICON_NONE);
  uiItemDecoratorR(row, &ptr, "add_threshold", 0);

  col = uiLayoutColumnWithHeading(layout, false, IFACE_("Group Remove"));
  row = uiLayoutRow(col, true);
  uiLayoutSetPropDecorate(row, false);
  sub = uiLayoutRow(row, true);
  uiItemR(sub, &ptr, "use_remove", 0, "", ICON_NONE);
  sub = uiLayoutRow(sub, true);
  uiLayoutSetActive(sub, RNA_boolean_get(&ptr, "use_remove"));
  uiLayoutSetPropSep(sub, false);
  uiItemR(sub, &ptr, "remove_threshold", UI_ITEM_R_SLIDER, "Threshold", ICON_NONE);
  uiItemDecoratorR(row, &ptr, "remove_threshold", 0);

  uiItemR(layout, &ptr, "normalize", 0, NULL, ICON_NONE);

  modifier_panel_end(layout, &ptr);
}

static void falloff_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRow(layout, true);
  uiItemR(row, &ptr, "falloff_type", 0, IFACE_("Type"), ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetPropSep(sub, false);
  uiItemR(row, &ptr, "invert_falloff", 0, "", ICON_ARROW_LEFTRIGHT);
  if (RNA_enum_get(&ptr, "falloff_type") == MOD_WVG_MAPPING_CURVE) {
    uiTemplateCurveMapping(layout, &ptr, "map_curve", 0, false, false, false, false);
  }
}

static void influence_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  weightvg_ui_common(C, &ob_ptr, &ptr, layout);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_WeightVGEdit, panel_draw);
  modifier_subpanel_register(
      region_type, "falloff", "Falloff", NULL, falloff_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "influence", "Influence", NULL, influence_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_WeightVGEdit = {
    /* name */ "VertexWeightEdit",
    /* structName */ "WeightVGEditModifierData",
    /* structSize */ sizeof(WeightVGEditModifierData),
    /* type */ eModifierTypeType_NonGeometrical,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_UsesPreview,

    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ dependsOnTime,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ foreachTexLink,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
