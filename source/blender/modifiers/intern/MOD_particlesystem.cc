/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstddef>
#include <cstring>

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_legacy_convert.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_query.h"

#include "BLO_read_write.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void initData(ModifierData *md)
{
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(psmd, modifier));

  MEMCPY_STRUCT_AFTER(psmd, DNA_struct_default_get(ParticleSystemModifierData), modifier);
}
static void freeData(ModifierData *md)
{
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;

  if (psmd->mesh_final) {
    BKE_id_free(nullptr, psmd->mesh_final);
    psmd->mesh_final = nullptr;
    if (psmd->mesh_original) {
      BKE_id_free(nullptr, psmd->mesh_original);
      psmd->mesh_original = nullptr;
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

  /* NOTE: `psys` pointer here is just copied over from `md` to `target`. This is dangerous, as it
   * will generate invalid data in case we are copying between different objects. Extra external
   * code has to be called then to ensure proper remapping of that pointer. See e.g.
   * `BKE_object_copy_particlesystems` or `BKE_object_copy_modifier`. */

  tpsmd->mesh_final = nullptr;
  tpsmd->mesh_original = nullptr;
  tpsmd->totdmvert = tpsmd->totdmedge = tpsmd->totdmface = 0;
}

static void requiredDataMask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;

  psys_emitter_customdata_mask(psmd->psys, r_cddata_masks);
}

/* saves the current emitter state for a particle system and calculates particles */
static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int /*verts_num*/)
{
  Mesh *mesh_src = mesh;
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;
  ParticleSystem *psys = nullptr;

  if (ctx->object->particlesystem.first) {
    psys = psmd->psys;
  }
  else {
    return;
  }

  if (!psys_check_enabled(ctx->object, psys, (ctx->flag & MOD_APPLY_RENDER) != 0)) {
    return;
  }

  if (mesh_src == nullptr) {
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, nullptr, nullptr, vertexCos);
    if (mesh_src == nullptr) {
      return;
    }
    BKE_mesh_orco_ensure(ctx->object, mesh_src);
  }

  /* Clear old evaluated mesh. */
  bool had_mesh_final = (psmd->mesh_final != nullptr);
  if (psmd->mesh_final) {
    BKE_id_free(nullptr, psmd->mesh_final);
    psmd->mesh_final = nullptr;
    if (psmd->mesh_original) {
      BKE_id_free(nullptr, psmd->mesh_original);
      psmd->mesh_original = nullptr;
    }
  }
  else if (psmd->flag & eParticleSystemFlag_file_loaded) {
    /* in file read mesh just wasn't saved in file so no need to reset everything */
    psmd->flag &= ~eParticleSystemFlag_file_loaded;
    if (psys->particles == nullptr) {
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
  psmd->mesh_final = BKE_mesh_copy_for_eval(mesh_src);
  BKE_mesh_vert_coords_apply(psmd->mesh_final, vertexCos);

  BKE_mesh_tessface_ensure(psmd->mesh_final);

  if (!psmd->mesh_final->runtime->deformed_only) {
    /* Get the original mesh from the object, this is what the particles
     * are attached to so in case of non-deform modifiers we need to remap
     * them to the final mesh (typically subdivision surfaces). */
    Mesh *mesh_original = nullptr;

    if (ctx->object->type == OB_MESH) {
      BMEditMesh *em = BKE_editmesh_from_object(ctx->object);

      if (em) {
        /* In edit mode get directly from the edit mesh. */
        psmd->mesh_original = BKE_mesh_from_bmesh_for_eval_nomain(em->bm, nullptr, mesh);
      }
      else {
        /* Otherwise get regular mesh. */
        mesh_original = static_cast<Mesh *>(ctx->object->data);
      }
    }
    else {
      mesh_original = mesh_src;
    }

    if (mesh_original) {
      /* Make a persistent copy of the mesh. We don't actually need
       * all this data, just some topology for remapping. Could be
       * optimized once. */
      psmd->mesh_original = BKE_mesh_copy_for_eval(mesh_original);
    }

    BKE_mesh_tessface_ensure(psmd->mesh_original);
  }

  if (!ELEM(mesh_src, nullptr, mesh, psmd->mesh_final)) {
    BKE_id_free(nullptr, mesh_src);
  }

  /* Report change in mesh structure.
   * This is an unreliable check for the topology check, but allows some
   * handy configuration like emitting particles from inside particle
   * instance. */
  if (had_mesh_final && (psmd->mesh_final->totvert != psmd->totdmvert ||
                         psmd->mesh_final->totedge != psmd->totdmedge ||
                         psmd->mesh_final->totface != psmd->totdmface))
  {
    psys->recalc |= ID_RECALC_PSYS_RESET;
  }
  psmd->totdmvert = psmd->mesh_final->totvert;
  psmd->totdmedge = psmd->mesh_final->totedge;
  psmd->totdmface = psmd->mesh_final->totface;

  {
    Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
    psmd->flag &= ~eParticleSystemFlag_psys_updated;
    particle_system_update(
        ctx->depsgraph, scene, ctx->object, psys, (ctx->flag & MOD_APPLY_RENDER) != 0);
    psmd->flag |= eParticleSystemFlag_psys_updated;
  }

  if (DEG_is_active(ctx->depsgraph)) {
    Object *object_orig = DEG_get_original_object(ctx->object);
    ModifierData *md_orig = BKE_modifiers_findby_name(object_orig, psmd->modifier.name);
    BLI_assert(md_orig != nullptr);
    ParticleSystemModifierData *psmd_orig = (ParticleSystemModifierData *)md_orig;
    psmd_orig->flag = psmd->flag;
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  Object *ob = static_cast<Object *>(ob_ptr.data);
  ModifierData *md = (ModifierData *)ptr->data;
  ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;

  uiItemL(layout, TIP_("Settings are in the particle tab"), ICON_NONE);

  if (!(ob->mode & OB_MODE_PARTICLE_EDIT)) {
    if (ELEM(psys->part->ren_as, PART_DRAW_GR, PART_DRAW_OB)) {
      uiItemO(layout,
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Make Instances Real"),
              ICON_NONE,
              "OBJECT_OT_duplicates_make_real");
    }
    else if (psys->part->ren_as == PART_DRAW_PATH) {
      uiItemO(layout,
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Convert to Mesh"),
              ICON_NONE,
              "OBJECT_OT_modifier_convert");
    }
  }

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_ParticleSystem, panel_draw);
}

static void blendRead(BlendDataReader *reader, ModifierData *md)
{
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;

  psmd->mesh_final = nullptr;
  psmd->mesh_original = nullptr;
  /* This is written as part of ob->particlesystem. */
  BLO_read_data_address(reader, &psmd->psys);
  psmd->flag &= ~eParticleSystemFlag_psys_updated;
  psmd->flag |= eParticleSystemFlag_file_loaded;
}

ModifierTypeInfo modifierType_ParticleSystem = {
    /*name*/ N_("ParticleSystem"),
    /*structName*/ "ParticleSystemModifierData",
    /*structSize*/ sizeof(ParticleSystemModifierData),
    /*srna*/ &RNA_ParticleSystemModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_UsesPointCache,
    /*icon*/ ICON_MOD_PARTICLES,

    /*copyData*/ copyData,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ nullptr,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ requiredDataMask,
    /*freeData*/ freeData,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ nullptr,
    /*dependsOnTime*/ nullptr,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ nullptr,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ blendRead,
};
