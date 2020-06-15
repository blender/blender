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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include <string.h>

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_lattice.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

static void initData(ModifierData *md)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;
  lmd->strength = 1.0f;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (lmd->name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(userRenderParams))
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the lattice is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return !lmd->object || lmd->object->type != OB_LATTICE;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;

  walk(userData, ob, &lmd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;
  if (lmd->object != NULL) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Lattice Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Lattice Modifier");
  }
  DEG_add_modifier_to_transform_relation(ctx->node, "Lattice Modifier");
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        struct Mesh *mesh,
                        float (*vertexCos)[3],
                        int numVerts)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;
  struct Mesh *mesh_src = MOD_deform_mesh_eval_get(
      ctx->object, NULL, mesh, NULL, numVerts, false, false);

  MOD_previous_vcos_store(md, vertexCos); /* if next modifier needs original vertices */

  BKE_lattice_deform_coords_with_mesh(lmd->object,
                                      ctx->object,
                                      vertexCos,
                                      numVerts,
                                      lmd->flag,
                                      lmd->name,
                                      lmd->strength,
                                      mesh_src);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          struct BMEditMesh *em,
                          struct Mesh *UNUSED(mesh),
                          float (*vertexCos)[3],
                          int numVerts)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;

  MOD_previous_vcos_store(md, vertexCos); /* if next modifier needs original vertices */

  BKE_lattice_deform_coords_with_editmesh(
      lmd->object, ctx->object, vertexCos, numVerts, lmd->flag, lmd->name, lmd->strength, em);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "object", 0, NULL, ICON_NONE);

  modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);

  uiItemR(layout, &ptr, "strength", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Lattice, panel_draw);
}

ModifierTypeInfo modifierType_Lattice = {
    /* name */ "Lattice",
    /* structName */ "LatticeModifierData",
    /* structSize */ sizeof(LatticeModifierData),
    /* type */ eModifierTypeType_OnlyDeform,
    /* flags */ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ deformVerts,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ deformVertsEM,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
