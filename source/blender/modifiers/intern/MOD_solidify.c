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

#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"

#include "BKE_particle.h"

#include "MOD_modifiertypes.h"

#include "MOD_solidify_util.h"

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

static bool dependsOnNormals(ModifierData *md)
{
  const SolidifyModifierData *smd = (SolidifyModifierData *)md;
  /* even when we calculate our own normals,
   * the vertex normals are used as a fallback
   * if manifold is enabled vertex normals are not used */
  return smd->mode == MOD_SOLIDIFY_MODE_EXTRUDE;
}

static void initData(ModifierData *md)
{
  SolidifyModifierData *smd = (SolidifyModifierData *)md;
  smd->offset = 0.01f;
  smd->offset_fac = -1.0f;
  smd->flag = MOD_SOLIDIFY_RIM;
  smd->mode = MOD_SOLIDIFY_MODE_EXTRUDE;
  smd->nonmanifold_offset_mode = MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_CONSTRAINTS;
  smd->nonmanifold_boundary_mode = MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_NONE;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  SolidifyModifierData *smd = (SolidifyModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (smd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  const SolidifyModifierData *smd = (SolidifyModifierData *)md;
  switch (smd->mode) {
    case MOD_SOLIDIFY_MODE_EXTRUDE:
      return MOD_solidify_extrude_applyModifier(md, ctx, mesh);
    case MOD_SOLIDIFY_MODE_NONMANIFOLD:
      return MOD_solidify_nonmanifold_applyModifier(md, ctx, mesh);
    default:
      BLI_assert(0);
  }
  return mesh;
}

ModifierTypeInfo modifierType_Solidify = {
    /* name */ "Solidify",
    /* structName */ "SolidifyModifierData",
    /* structSize */ sizeof(SolidifyModifierData),
    /* type */ eModifierTypeType_Constructive,

    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsMapping | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,

    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
