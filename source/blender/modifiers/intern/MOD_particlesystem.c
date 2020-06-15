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

#include <stddef.h>

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_query.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

static void initData(ModifierData *md)
{
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;
  psmd->psys = NULL;
  psmd->mesh_final = NULL;
  psmd->mesh_original = NULL;
  psmd->totdmvert = psmd->totdmedge = psmd->totdmface = 0;
}
static void freeData(ModifierData *md)
{
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;

  if (psmd->mesh_final) {
    BKE_id_free(NULL, psmd->mesh_final);
    psmd->mesh_final = NULL;
    if (psmd->mesh_original) {
      BKE_id_free(NULL, psmd->mesh_original);
      psmd->mesh_original = NULL;
    }
  }
  psmd->totdmvert = psmd->totdmedge = psmd->totdmface = 0;

  /* ED_object_modifier_remove may have freed this first before calling
   * BKE_modifier_free (which calls this function) */
  if (psmd->psys) {
    psmd->psys->flag |= PSYS_DELETE;
  }
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
  const ParticleSystemModifierData *psmd = (const ParticleSystemModifierData *)md;
#endif
  ParticleSystemModifierData *tpsmd = (ParticleSystemModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  tpsmd->mesh_final = NULL;
  tpsmd->mesh_original = NULL;
  tpsmd->totdmvert = tpsmd->totdmedge = tpsmd->totdmface = 0;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;

  psys_emitter_customdata_mask(psmd->psys, r_cddata_masks);
}

/* saves the current emitter state for a particle system and calculates particles */
static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int numVerts)
{
  Mesh *mesh_src = mesh;
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;
  ParticleSystem *psys = NULL;
  /* float cfra = BKE_scene_frame_get(md->scene); */ /* UNUSED */

  if (ctx->object->particlesystem.first) {
    psys = psmd->psys;
  }
  else {
    return;
  }

  if (!psys_check_enabled(ctx->object, psys, (ctx->flag & MOD_APPLY_RENDER) != 0)) {
    return;
  }

  if (mesh_src == NULL) {
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, NULL, vertexCos, numVerts, false, true);
    if (mesh_src == NULL) {
      return;
    }
  }

  /* clear old dm */
  bool had_mesh_final = (psmd->mesh_final != NULL);
  if (psmd->mesh_final) {
    BKE_id_free(NULL, psmd->mesh_final);
    psmd->mesh_final = NULL;
    if (psmd->mesh_original) {
      BKE_id_free(NULL, psmd->mesh_original);
      psmd->mesh_original = NULL;
    }
  }
  else if (psmd->flag & eParticleSystemFlag_file_loaded) {
    /* in file read mesh just wasn't saved in file so no need to reset everything */
    psmd->flag &= ~eParticleSystemFlag_file_loaded;
    if (psys->particles == NULL) {
      psys->recalc |= ID_RECALC_PSYS_RESET;
    }
    /* TODO(sergey): This is not how particles were working prior to copy on
     * write, but now evaluation is similar to case when one duplicates the
     * object. In that case particles were doing reset here.
     *
     * Don't do reset when entering particle edit mode, as that will destroy the edit mode data.
     * Shouldn't be an issue, since particles are supposed to be evaluated once prior to entering
     * edit mode anyway.
     * Could in theory be an issue when everything is done in a script, but then solution is
     * not known to me. */
    if (ctx->object->mode != OB_MODE_PARTICLE_EDIT) {
      psys->recalc |= ID_RECALC_PSYS_RESET;
    }
  }

  /* make new mesh */
  psmd->mesh_final = BKE_mesh_copy_for_eval(mesh_src, false);
  BKE_mesh_vert_coords_apply(psmd->mesh_final, vertexCos);
  BKE_mesh_calc_normals(psmd->mesh_final);

  BKE_mesh_tessface_ensure(psmd->mesh_final);

  if (!psmd->mesh_final->runtime.deformed_only) {
    /* Get the original mesh from the object, this is what the particles
     * are attached to so in case of non-deform modifiers we need to remap
     * them to the final mesh (typically subdivision surfaces). */
    Mesh *mesh_original = NULL;

    if (ctx->object->type == OB_MESH) {
      BMEditMesh *em = BKE_editmesh_from_object(ctx->object);

      if (em) {
        /* In edit mode get directly from the edit mesh. */
        psmd->mesh_original = BKE_mesh_from_bmesh_for_eval_nomain(em->bm, NULL, mesh);
      }
      else {
        /* Otherwise get regular mesh. */
        mesh_original = ctx->object->data;
      }
    }
    else {
      mesh_original = mesh_src;
    }

    if (mesh_original) {
      /* Make a persistent copy of the mesh. We don't actually need
       * all this data, just some topology for remapping. Could be
       * optimized once. */
      psmd->mesh_original = BKE_mesh_copy_for_eval(mesh_original, false);
    }

    BKE_mesh_tessface_ensure(psmd->mesh_original);
  }

  if (!ELEM(mesh_src, NULL, mesh, psmd->mesh_final)) {
    BKE_id_free(NULL, mesh_src);
  }

  /* Report change in mesh structure.
   * This is an unreliable check for the topology check, but allows some
   * handy configuration like emitting particles from inside particle
   * instance. */
  if (had_mesh_final && (psmd->mesh_final->totvert != psmd->totdmvert ||
                         psmd->mesh_final->totedge != psmd->totdmedge ||
                         psmd->mesh_final->totface != psmd->totdmface)) {
    psys->recalc |= ID_RECALC_PSYS_RESET;
  }
  psmd->totdmvert = psmd->mesh_final->totvert;
  psmd->totdmedge = psmd->mesh_final->totedge;
  psmd->totdmface = psmd->mesh_final->totface;

  if (!(ctx->object->transflag & OB_NO_PSYS_UPDATE)) {
    struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
    psmd->flag &= ~eParticleSystemFlag_psys_updated;
    particle_system_update(
        ctx->depsgraph, scene, ctx->object, psys, (ctx->flag & MOD_APPLY_RENDER) != 0);
    psmd->flag |= eParticleSystemFlag_psys_updated;
  }

  if (DEG_is_active(ctx->depsgraph)) {
    Object *object_orig = DEG_get_original_object(ctx->object);
    ModifierData *md_orig = BKE_modifiers_findby_name(object_orig, psmd->modifier.name);
    BLI_assert(md_orig != NULL);
    ParticleSystemModifierData *psmd_orig = (ParticleSystemModifierData *)md_orig;
    psmd_orig->flag = psmd->flag;
  }
}

/* disabled particles in editmode for now, until support for proper evaluated mesh
 * updates is coded */
#if 0
static void deformVertsEM(ModifierData *md,
                          Object *ob,
                          BMEditMesh *editData,
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int numVerts)
{
  const bool do_temp_mesh = (mesh == NULL);
  if (do_temp_mesh) {
    mesh = BKE_id_new_nomain(ID_ME, ((ID *)ob->data)->name);
    BM_mesh_bm_to_me(NULL, editData->bm, mesh, &((BMeshToMeshParams){0}));
  }

  deformVerts(md, ob, mesh, vertexCos, numVerts);

  if (derivedData) {
    BKE_id_free(NULL, mesh);
  }
}
#endif

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  Object *ob = ob_ptr.data;
  ModifierData *md = (ModifierData *)ptr.data;
  ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;

  uiItemL(layout, IFACE_("Settings are in the particle tab"), ICON_NONE);

  if (!(ob->mode & OB_MODE_PARTICLE_EDIT)) {
    if (ELEM(psys->part->ren_as, PART_DRAW_GR, PART_DRAW_OB)) {
      uiItemO(layout,
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Convert"),
              ICON_NONE,
              "OBJECT_OT_duplicates_make_real");
    }
    else if (psys->part->ren_as == PART_DRAW_PATH) {
      uiItemO(layout,
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Convert"),
              ICON_NONE,
              "OBJECT_OT_modifier_convert");
    }
  }

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_ParticleSystem, panel_draw);
}

ModifierTypeInfo modifierType_ParticleSystem = {
    /* name */ "ParticleSystem",
    /* structName */ "ParticleSystemModifierData",
    /* structSize */ sizeof(ParticleSystemModifierData),
    /* type */ eModifierTypeType_OnlyDeform,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_UsesPointCache /* |
                          eModifierTypeFlag_SupportsEditmode |
                          eModifierTypeFlag_EnableInEditmode */
    ,

    /* copyData */ copyData,

    /* deformVerts */ deformVerts,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ freeData,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
