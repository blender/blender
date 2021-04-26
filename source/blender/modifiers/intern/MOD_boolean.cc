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

#include <cstdio>

#include "BLI_utildefines.h"

#include "BLI_array.hh"
#include "BLI_float4x4.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_vector.hh"

#include "BLT_translation.h"

#include "DNA_collection_types.h"
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_global.h" /* only to check G.debug */
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_boolean_convert.hh"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "bmesh.h"
#include "bmesh_tools.h"
#include "tools/bmesh_boolean.h"
#include "tools/bmesh_intersect.h"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_timeit.hh"
#endif

using blender::Array;
using blender::float4x4;
using blender::Vector;

static void initData(ModifierData *md)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(bmd, modifier));

  MEMCPY_STRUCT_AFTER(bmd, DNA_struct_default_get(BooleanModifierData), modifier);
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;
  Collection *col = bmd->collection;

  if (bmd->flag & eBooleanModifierFlag_Object) {
    return !bmd->object || bmd->object->type != OB_MESH;
  }
  if (bmd->flag & eBooleanModifierFlag_Collection) {
    /* The Exact solver tolerates an empty collection. */
    return !col && bmd->solver != eBooleanModifierSolver_Exact;
  }
  return false;
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;

  walk(userData, ob, (ID **)&bmd->collection, IDWALK_CB_NOP);
  walk(userData, ob, (ID **)&bmd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;
  if ((bmd->flag & eBooleanModifierFlag_Object) && bmd->object != nullptr) {
    DEG_add_object_relation(ctx->node, bmd->object, DEG_OB_COMP_TRANSFORM, "Boolean Modifier");
    DEG_add_object_relation(ctx->node, bmd->object, DEG_OB_COMP_GEOMETRY, "Boolean Modifier");
  }

  Collection *col = bmd->collection;

  if ((bmd->flag & eBooleanModifierFlag_Collection) && col != nullptr) {
    DEG_add_collection_geometry_relation(ctx->node, col, "Boolean Modifier");
  }
  /* We need own transformation as well. */
  DEG_add_modifier_to_transform_relation(ctx->node, "Boolean Modifier");
}

static Mesh *get_quick_mesh(
    Object *ob_self, Mesh *mesh_self, Object *ob_operand_ob, Mesh *mesh_operand_ob, int operation)
{
  Mesh *result = nullptr;

  if (mesh_self->totpoly == 0 || mesh_operand_ob->totpoly == 0) {
    switch (operation) {
      case eBooleanModifierOp_Intersect:
        result = BKE_mesh_new_nomain(0, 0, 0, 0, 0);
        break;

      case eBooleanModifierOp_Union:
        if (mesh_self->totpoly != 0) {
          result = mesh_self;
        }
        else {
          result = (Mesh *)BKE_id_copy_ex(
              nullptr, &mesh_operand_ob->id, nullptr, LIB_ID_COPY_LOCALIZE);

          float imat[4][4];
          float omat[4][4];
          invert_m4_m4(imat, ob_self->obmat);
          mul_m4_m4m4(omat, imat, ob_operand_ob->obmat);

          const int mverts_len = result->totvert;
          MVert *mv = result->mvert;

          for (int i = 0; i < mverts_len; i++, mv++) {
            mul_m4_v3(omat, mv->co);
          }

          result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
        }

        break;

      case eBooleanModifierOp_Difference:
        result = mesh_self;
        break;
    }
  }

  return result;
}

/* has no meaning for faces, do this so we can tell which face is which */
#define BM_FACE_TAG BM_ELEM_DRAW

/**
 * Compare selected/unselected.
 */
static int bm_face_isect_pair(BMFace *f, void *UNUSED(user_data))
{
  return BM_elem_flag_test(f, BM_FACE_TAG) ? 1 : 0;
}

static bool BMD_error_messages(const Object *ob, ModifierData *md)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;
  Collection *col = bmd->collection;

  bool error_returns_result = false;

  const bool operand_collection = (bmd->flag & eBooleanModifierFlag_Collection) != 0;
  const bool use_exact = bmd->solver == eBooleanModifierSolver_Exact;
  const bool operation_intersect = bmd->operation == eBooleanModifierOp_Intersect;

#ifndef WITH_GMP
  /* If compiled without GMP, return a error. */
  if (use_exact) {
    BKE_modifier_set_error(ob, md, "Compiled without GMP, using fast solver");
    error_returns_result = false;
  }
#endif

  /* If intersect is selected using fast solver, return a error. */
  if (operand_collection && operation_intersect && !use_exact) {
    BKE_modifier_set_error(ob, md, "Cannot execute, intersect only available using exact solver");
    error_returns_result = true;
  }

  /* If the selected collection is empty and using fast solver, return a error. */
  if (operand_collection) {
    if (!use_exact && BKE_collection_is_empty(col)) {
      BKE_modifier_set_error(ob, md, "Cannot execute, fast solver and empty collection");
      error_returns_result = true;
    }

    /* If the selected collection contain non mesh objects, return a error. */
    if (col) {
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (col, operand_ob) {
        if (operand_ob->type != OB_MESH) {
          BKE_modifier_set_error(
              ob, md, "Cannot execute, the selected collection contains non mesh objects");
          error_returns_result = true;
        }
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    }
  }

  return error_returns_result;
}

static BMesh *BMD_mesh_bm_create(
    Mesh *mesh, Object *object, Mesh *mesh_operand_ob, Object *operand_ob, bool *r_is_flip)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__)
#endif

  *r_is_flip = (is_negative_m4(object->obmat) != is_negative_m4(operand_ob->obmat));

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh, mesh_operand_ob);

  BMeshCreateParams bmcp = {false};
  BMesh *bm = BM_mesh_create(&allocsize, &bmcp);

  BMeshFromMeshParams params{};
  params.calc_face_normal = true;
  BM_mesh_bm_from_me(bm, mesh_operand_ob, &params);

  if (UNLIKELY(*r_is_flip)) {
    const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
    BMIter iter;
    BMFace *efa;
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      BM_face_normal_flip_ex(bm, efa, cd_loop_mdisp_offset, true);
    }
  }

  BM_mesh_bm_from_me(bm, mesh, &params);

  return bm;
}

static void BMD_mesh_intersection(BMesh *bm,
                                  ModifierData *md,
                                  const ModifierEvalContext *ctx,
                                  Mesh *mesh_operand_ob,
                                  Object *object,
                                  Object *operand_ob,
                                  bool is_flip)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__)
#endif

  BooleanModifierData *bmd = (BooleanModifierData *)md;

  /* main bmesh intersection setup */
  /* create tessface & intersect */
  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
  int tottri;
  BMLoop *(*looptris)[3] = (BMLoop * (*)[3])
      MEM_malloc_arrayN(looptris_tot, sizeof(*looptris), __func__);

  BM_mesh_calc_tessellation_beauty(bm, looptris, &tottri);

  /* postpone this until after tessellating
   * so we can use the original normals before the vertex are moved */
  {
    BMIter iter;
    int i;
    const int i_verts_end = mesh_operand_ob->totvert;
    const int i_faces_end = mesh_operand_ob->totpoly;

    float imat[4][4];
    float omat[4][4];
    invert_m4_m4(imat, object->obmat);
    mul_m4_m4m4(omat, imat, operand_ob->obmat);

    BMVert *eve;
    i = 0;
    BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
      mul_m4_v3(omat, eve->co);
      if (++i == i_verts_end) {
        break;
      }
    }

    /* we need face normals because of 'BM_face_split_edgenet'
     * we could calculate on the fly too (before calling split). */
    float nmat[3][3];
    copy_m3_m4(nmat, omat);
    invert_m3(nmat);

    if (UNLIKELY(is_flip)) {
      negate_m3(nmat);
    }

    Array<short> material_remap(operand_ob->totcol ? operand_ob->totcol : 1);

    /* Using original (not evaluated) object here since we are writing to it. */
    /* XXX Pretty sure comment above is fully wrong now with CoW & co ? */
    BKE_object_material_remap_calc(ctx->object, operand_ob, material_remap.data());

    BMFace *efa;
    i = 0;
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      mul_transposed_m3_v3(nmat, efa->no);
      normalize_v3(efa->no);

      /* Temp tag to test which side split faces are from. */
      BM_elem_flag_enable(efa, BM_FACE_TAG);

      /* remap material */
      if (LIKELY(efa->mat_nr < operand_ob->totcol)) {
        efa->mat_nr = material_remap[efa->mat_nr];
      }

      if (++i == i_faces_end) {
        break;
      }
    }
  }

  /* not needed, but normals for 'dm' will be invalid,
   * currently this is ok for 'BM_mesh_intersect' */
  // BM_mesh_normals_update(bm);

  bool use_separate = false;
  bool use_dissolve = true;
  bool use_island_connect = true;

  /* change for testing */
  if (G.debug & G_DEBUG) {
    use_separate = (bmd->bm_flag & eBooleanModifierBMeshFlag_BMesh_Separate) != 0;
    use_dissolve = (bmd->bm_flag & eBooleanModifierBMeshFlag_BMesh_NoDissolve) == 0;
    use_island_connect = (bmd->bm_flag & eBooleanModifierBMeshFlag_BMesh_NoConnectRegions) == 0;
  }

  BM_mesh_intersect(bm,
                    looptris,
                    tottri,
                    bm_face_isect_pair,
                    nullptr,
                    false,
                    use_separate,
                    use_dissolve,
                    use_island_connect,
                    false,
                    false,
                    bmd->operation,
                    bmd->double_threshold);

  MEM_freeN(looptris);
}

#ifdef WITH_GMP

/* Get a mapping from material slot numbers in the src_ob to slot numbers in the dst_ob.
 * If a material doesn't exist in the dst_ob, the mapping just goes to the same slot
 * or to zero if there aren't enough slots in the destination.
 * Caller owns the returned array. */
static Array<short> get_material_remap(Object *dest_ob, Object *src_ob)
{
  int n = dest_ob->totcol;
  if (n <= 0) {
    n = 1;
  }
  Array<short> remap(n);
  BKE_object_material_remap_calc(dest_ob, src_ob, remap.data());
  return remap;
}

static Mesh *exact_boolean_mesh(BooleanModifierData *bmd,
                                const ModifierEvalContext *ctx,
                                Mesh *mesh)
{
  Vector<const Mesh *> meshes;
  Vector<float4x4 *> obmats;
  Vector<Array<short>> material_remaps;

#  ifdef DEBUG_TIME
  SCOPED_TIMER(__func__)
#  endif

  if ((bmd->flag & eBooleanModifierFlag_Object) && bmd->object == nullptr) {
    return mesh;
  }

  meshes.append(mesh);
  obmats.append((float4x4 *)&ctx->object->obmat);
  material_remaps.append({});
  if (bmd->flag & eBooleanModifierFlag_Object) {
    Mesh *mesh_operand = BKE_modifier_get_evaluated_mesh_from_evaluated_object(bmd->object, false);
    if (!mesh_operand) {
      return mesh;
    }
    BKE_mesh_wrapper_ensure_mdata(mesh_operand);
    meshes.append(mesh_operand);
    obmats.append((float4x4 *)&bmd->object->obmat);
    material_remaps.append(get_material_remap(ctx->object, bmd->object));
  }
  else if (bmd->flag & eBooleanModifierFlag_Collection) {
    Collection *collection = bmd->collection;
    /* Allow collection to be empty; then target mesh will just removed self-intersections. */
    if (collection) {
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, ob) {
        if (ob->type == OB_MESH && ob != ctx->object) {
          Mesh *collection_mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob, false);
          if (!collection_mesh) {
            continue;
          }
          BKE_mesh_wrapper_ensure_mdata(collection_mesh);
          meshes.append(collection_mesh);
          obmats.append((float4x4 *)&ob->obmat);
          material_remaps.append(get_material_remap(ctx->object, ob));
        }
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    }
  }

  const bool use_self = (bmd->flag & eBooleanModifierFlag_Self) != 0;
  const bool hole_tolerant = (bmd->flag & eBooleanModifierFlag_HoleTolerant) != 0;
  return blender::meshintersect::direct_mesh_boolean(meshes,
                                                     obmats,
                                                     *(float4x4 *)&ctx->object->obmat,
                                                     material_remaps,
                                                     use_self,
                                                     hole_tolerant,
                                                     bmd->operation);
}
#endif

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;
  Object *object = ctx->object;
  Mesh *result = mesh;
  Collection *collection = bmd->collection;

  /* Return result for certain errors. */
  if (BMD_error_messages(ctx->object, md)) {
    return result;
  }

#ifdef WITH_GMP
  if (bmd->solver == eBooleanModifierSolver_Exact) {
    return exact_boolean_mesh(bmd, ctx, mesh);
  }
#endif

#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__)
#endif

  if (bmd->flag & eBooleanModifierFlag_Object) {
    if (bmd->object == nullptr) {
      return result;
    }

    Object *operand_ob = bmd->object;

    Mesh *mesh_operand_ob = BKE_modifier_get_evaluated_mesh_from_evaluated_object(operand_ob,
                                                                                  false);

    if (mesh_operand_ob) {
      /* XXX This is utterly non-optimal, we may go from a bmesh to a mesh back to a bmesh!
       * But for 2.90 better not try to be smart here. */
      BKE_mesh_wrapper_ensure_mdata(mesh_operand_ob);
      /* when one of objects is empty (has got no faces) we could speed up
       * calculation a bit returning one of objects' derived meshes (or empty one)
       * Returning mesh is depended on modifiers operation (sergey) */
      result = get_quick_mesh(object, mesh, operand_ob, mesh_operand_ob, bmd->operation);

      if (result == nullptr) {
        bool is_flip;
        BMesh *bm = BMD_mesh_bm_create(mesh, object, mesh_operand_ob, operand_ob, &is_flip);

        BMD_mesh_intersection(bm, md, ctx, mesh_operand_ob, object, operand_ob, is_flip);

        result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh);

        BM_mesh_free(bm);
        result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
      }

      if (result == nullptr) {
        BKE_modifier_set_error(object, md, "Cannot execute boolean operation");
      }
    }
  }
  else {
    if (collection == nullptr) {
      return result;
    }

    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, operand_ob) {
      if (operand_ob->type == OB_MESH && operand_ob != ctx->object) {
        Mesh *mesh_operand_ob = BKE_modifier_get_evaluated_mesh_from_evaluated_object(operand_ob,
                                                                                      false);

        if (mesh_operand_ob) {
          /* XXX This is utterly non-optimal, we may go from a bmesh to a mesh back to a bmesh!
           * But for 2.90 better not try to be smart here. */
          BKE_mesh_wrapper_ensure_mdata(mesh_operand_ob);

          bool is_flip;
          BMesh *bm = BMD_mesh_bm_create(mesh, object, mesh_operand_ob, operand_ob, &is_flip);

          BMD_mesh_intersection(bm, md, ctx, mesh_operand_ob, object, operand_ob, is_flip);

          /* Needed for multiple objects to work. */
          BMeshToMeshParams params{};
          params.calc_object_remap = false;
          BM_mesh_bm_to_me(nullptr, bm, mesh, &params);

          result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh);
          BM_mesh_free(bm);
          result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
        }
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }

  return result;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *UNUSED(md),
                             CustomData_MeshMasks *r_cddata_masks)
{
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  r_cddata_masks->emask |= CD_MASK_MEDGE;
  r_cddata_masks->fmask |= CD_MASK_MTFACE;
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiItemR(layout, ptr, "operation", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "operand_type", 0, nullptr, ICON_NONE);
  if (RNA_enum_get(ptr, "operand_type") == eBooleanModifierFlag_Object) {
    uiItemR(layout, ptr, "object", 0, nullptr, ICON_NONE);
  }
  else {
    uiItemR(layout, ptr, "collection", 0, nullptr, ICON_NONE);
  }

  uiItemR(layout, ptr, "solver", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void solver_options_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  const bool use_exact = RNA_enum_get(ptr, "solver") == eBooleanModifierSolver_Exact;

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, true);
  if (use_exact) {
    /* When operand is collection, we always use_self. */
    if (RNA_enum_get(ptr, "operand_type") == eBooleanModifierFlag_Object) {
      uiItemR(col, ptr, "use_self", 0, nullptr, ICON_NONE);
    }
    uiItemR(col, ptr, "use_hole_tolerant", 0, nullptr, ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "double_threshold", 0, nullptr, ICON_NONE);
  }

  if (G.debug) {
    uiItemR(col, ptr, "debug_options", 0, nullptr, ICON_NONE);
  }
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel = modifier_panel_register(region_type, eModifierType_Boolean, panel_draw);
  modifier_subpanel_register(
      region_type, "solver_options", "Solver Options", nullptr, solver_options_panel_draw, panel);
}

ModifierTypeInfo modifierType_Boolean = {
    /* name */ "Boolean",
    /* structName */ "BooleanModifierData",
    /* structSize */ sizeof(BooleanModifierData),
    /* srna */ &RNA_BooleanModifier,
    /* type */ eModifierTypeType_Nonconstructive,
    /* flags */
    (ModifierTypeFlag)(eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode),
    /* icon */ ICON_MOD_BOOLEAN,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ nullptr,
    /* deformMatrices */ nullptr,
    /* deformVertsEM */ nullptr,
    /* deformMatricesEM */ nullptr,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ nullptr,
    /* modifyGeometrySet */ nullptr,
    /* modifyVolume */ nullptr,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ nullptr,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ nullptr,
    /* dependsOnNormals */ nullptr,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ nullptr,
    /* freeRuntimeData */ nullptr,
    /* panelRegister */ panelRegister,
    /* blendWrite */ nullptr,
    /* blendRead */ nullptr,
};
