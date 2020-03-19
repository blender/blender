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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_deform.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mirror.h"
#include "BKE_modifier.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"

static void initData(ModifierData *md)
{
  MirrorModifierData *mmd = (MirrorModifierData *)md;

  mmd->flag |= (MOD_MIR_AXIS_X | MOD_MIR_VGROUP);
  mmd->tolerance = 0.001;
  mmd->mirror_ob = NULL;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  MirrorModifierData *mmd = (MirrorModifierData *)md;

  walk(userData, ob, &mmd->mirror_ob, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MirrorModifierData *mmd = (MirrorModifierData *)md;
  if (mmd->mirror_ob != NULL) {
    DEG_add_object_relation(ctx->node, mmd->mirror_ob, DEG_OB_COMP_TRANSFORM, "Mirror Modifier");
    DEG_add_modifier_to_transform_relation(ctx->node, "Mirror Modifier");
  }
}

static Mesh *mirrorModifier__doMirror(MirrorModifierData *mmd,
                                      const ModifierEvalContext *ctx,
                                      Object *ob,
                                      Mesh *mesh)
{
  Mesh *result = mesh;

  /* check which axes have been toggled and mirror accordingly */
  if (mmd->flag & MOD_MIR_AXIS_X) {
    result = BKE_mesh_mirror_apply_mirror_on_axis(mmd, ctx, ob, result, 0);
  }
  if (mmd->flag & MOD_MIR_AXIS_Y) {
    Mesh *tmp = result;
    result = BKE_mesh_mirror_apply_mirror_on_axis(mmd, ctx, ob, result, 1);
    if (tmp != mesh) {
      /* free intermediate results */
      BKE_id_free(NULL, tmp);
    }
  }
  if (mmd->flag & MOD_MIR_AXIS_Z) {
    Mesh *tmp = result;
    result = BKE_mesh_mirror_apply_mirror_on_axis(mmd, ctx, ob, result, 2);
    if (tmp != mesh) {
      /* free intermediate results */
      BKE_id_free(NULL, tmp);
    }
  }

  return result;
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  MirrorModifierData *mmd = (MirrorModifierData *)md;

  result = mirrorModifier__doMirror(mmd, ctx, ctx->object, mesh);

  if (result != mesh) {
    result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  }
  return result;
}

ModifierTypeInfo modifierType_Mirror = {
    /* name */ "Mirror",
    /* structName */ "MirrorModifierData",
    /* structSize */ sizeof(MirrorModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode |
        eModifierTypeFlag_AcceptsCVs |
        /* this is only the case when 'MOD_MIR_VGROUP' is used */
        eModifierTypeFlag_UsesPreview,

    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
