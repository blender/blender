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

// #define DEBUG_TIME

#include <stdio.h>

#include "BLI_utildefines.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"

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
#include "BKE_mesh_boolean_convert.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

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

#ifdef DEBUG_TIME
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

#ifdef WITH_GMP
const bool bypass_bmesh = true;
#else
const bool bypass_bmesh = false;
#endif

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
  if ((bmd->flag & eBooleanModifierFlag_Object) && bmd->object != NULL) {
    DEG_add_object_relation(ctx->node, bmd->object, DEG_OB_COMP_TRANSFORM, "Boolean Modifier");
    DEG_add_object_relation(ctx->node, bmd->object, DEG_OB_COMP_GEOMETRY, "Boolean Modifier");
  }

  Collection *col = bmd->collection;

  if ((bmd->flag & eBooleanModifierFlag_Collection) && col != NULL) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (col, operand_ob) {
      if (operand_ob->type == OB_MESH && operand_ob != ctx->object) {
        DEG_add_object_relation(ctx->node, operand_ob, DEG_OB_COMP_TRANSFORM, "Boolean Modifier");
        DEG_add_object_relation(ctx->node, operand_ob, DEG_OB_COMP_GEOMETRY, "Boolean Modifier");
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
  /* We need own transformation as well. */
  DEG_add_modifier_to_transform_relation(ctx->node, "Boolean Modifier");
}

static Mesh *get_quick_mesh(
    Object *ob_self, Mesh *mesh_self, Object *ob_operand_ob, Mesh *mesh_operand_ob, int operation)
{
  Mesh *result = NULL;

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
          result = (Mesh *)BKE_id_copy_ex(NULL, &mesh_operand_ob->id, NULL, LIB_ID_COPY_LOCALIZE);

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

static bool BMD_error_messages(const Object *ob, ModifierData *md, Collection *col)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;

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
  BMesh *bm;

  *r_is_flip = (is_negative_m4(object->obmat) != is_negative_m4(operand_ob->obmat));

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh, mesh_operand_ob);

  bm = BM_mesh_create(&allocsize,
                      &((struct BMeshCreateParams){
                          .use_toolflags = false,
                      }));

  BM_mesh_bm_from_me(bm,
                     mesh_operand_ob,
                     &((struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));

  if (UNLIKELY(*r_is_flip)) {
    const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
    BMIter iter;
    BMFace *efa;
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      BM_face_normal_flip_ex(bm, efa, cd_loop_mdisp_offset, true);
    }
  }

  BM_mesh_bm_from_me(bm,
                     mesh,
                     &((struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));

  return bm;
}

/* Snap entries that are near 0 or 1 or -1 to those values. */
static void clean_obmat(float cleaned[4][4], const float mat[4][4])
{
  const float fuzz = 1e-6f;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      float f = mat[i][j];
      if (fabsf(f) <= fuzz) {
        f = 0.0f;
      }
      else if (fabsf(f - 1.0f) <= fuzz) {
        f = 1.0f;
      }
      else if (fabsf(f + 1.0f) <= fuzz) {
        f = -1.0f;
      }
      cleaned[i][j] = f;
    }
  }
}

static void BMD_mesh_intersection(BMesh *bm,
                                  ModifierData *md,
                                  const ModifierEvalContext *ctx,
                                  Mesh *mesh_operand_ob,
                                  Object *object,
                                  Object *operand_ob,
                                  bool is_flip)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;

  /* main bmesh intersection setup */
  /* create tessface & intersect */
  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
  int tottri;
  BMLoop *(*looptris)[3];

#ifdef WITH_GMP
  const bool use_exact = bmd->solver == eBooleanModifierSolver_Exact;
  const bool use_self = (bmd->flag & eBooleanModifierFlag_Self) != 0;
#else
  const bool use_exact = false;
  const bool use_self = false;
#endif

  looptris = MEM_malloc_arrayN(looptris_tot, sizeof(*looptris), __func__);

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

    if (use_exact) {
      /* The user-expected coplanar faces will actually be coplanar more
       * often if use an object matrix that doesn't multiply by values
       * other than 0, -1, or 1 in the scaling part of the matrix.
       */
      float cleaned_object_obmat[4][4];
      float cleaned_operand_obmat[4][4];
      clean_obmat(cleaned_object_obmat, object->obmat);
      invert_m4_m4(imat, cleaned_object_obmat);
      clean_obmat(cleaned_operand_obmat, operand_ob->obmat);
      mul_m4_m4m4(omat, imat, cleaned_operand_obmat);
    }
    else {
      invert_m4_m4(imat, object->obmat);
      mul_m4_m4m4(omat, imat, operand_ob->obmat);
    }

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
    {
      float nmat[3][3];
      copy_m3_m4(nmat, omat);
      invert_m3(nmat);

      if (UNLIKELY(is_flip)) {
        negate_m3(nmat);
      }

      const short ob_src_totcol = operand_ob->totcol;
      short *material_remap = BLI_array_alloca(material_remap, ob_src_totcol ? ob_src_totcol : 1);

      /* Using original (not evaluated) object here since we are writing to it. */
      /* XXX Pretty sure comment above is fully wrong now with CoW & co ? */
      BKE_object_material_remap_calc(ctx->object, operand_ob, material_remap);

      BMFace *efa;
      i = 0;
      BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
        mul_transposed_m3_v3(nmat, efa->no);
        normalize_v3(efa->no);

        /* Temp tag to test which side split faces are from. */
        BM_elem_flag_enable(efa, BM_FACE_TAG);

        /* remap material */
        if (LIKELY(efa->mat_nr < ob_src_totcol)) {
          efa->mat_nr = material_remap[efa->mat_nr];
        }

        if (++i == i_faces_end) {
          break;
        }
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

  if (use_exact) {
    BM_mesh_boolean(
        bm, looptris, tottri, bm_face_isect_pair, NULL, 2, use_self, false, false, bmd->operation);
  }
  else {
    BM_mesh_intersect(bm,
                      looptris,
                      tottri,
                      bm_face_isect_pair,
                      NULL,
                      false,
                      use_separate,
                      use_dissolve,
                      use_island_connect,
                      false,
                      false,
                      bmd->operation,
                      bmd->double_threshold);
  }
  MEM_freeN(looptris);
}

static int bm_face_isect_nary(BMFace *f, void *user_data)
{
  int *shape = (int *)user_data;
  return shape[BM_elem_index_get(f)];
}

/* The Exact solver can do all operands of a collection at once. */
static Mesh *collection_boolean_exact(BooleanModifierData *bmd,
                                      const ModifierEvalContext *ctx,
                                      Mesh *mesh)
{
  int i;
  Mesh *result = mesh;
  Collection *col = bmd->collection;
  int num_shapes = 1;
  Mesh **meshes = NULL;
  Object **objects = NULL;
  BLI_array_declare(meshes);
  BLI_array_declare(objects);
  BMAllocTemplate bat;
  bat.totvert = mesh->totvert;
  bat.totedge = mesh->totedge;
  bat.totloop = mesh->totloop;
  bat.totface = mesh->totpoly;
  BLI_array_append(meshes, mesh);
  BLI_array_append(objects, ctx->object);
  Mesh *col_mesh;
  /* Allow col to be empty: then target mesh will just remove self-intersections. */
  if (col) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (col, ob) {
      if (ob->type == OB_MESH && ob != ctx->object) {
        col_mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob, false);
        /* XXX This is utterly non-optimal, we may go from a bmesh to a mesh back to a bmesh!
         * But for 2.90 better not try to be smart here. */
        BKE_mesh_wrapper_ensure_mdata(col_mesh);
        BLI_array_append(meshes, col_mesh);
        BLI_array_append(objects, ob);
        bat.totvert += col_mesh->totvert;
        bat.totedge += col_mesh->totedge;
        bat.totloop += col_mesh->totloop;
        bat.totface += col_mesh->totpoly;
        ++num_shapes;
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
  int *shape_face_end = MEM_mallocN(num_shapes * sizeof(int), __func__);
  int *shape_vert_end = MEM_mallocN(num_shapes * sizeof(int), __func__);
  bool is_neg_mat0 = is_negative_m4(ctx->object->obmat);
  BMesh *bm = BM_mesh_create(&bat,
                             &((struct BMeshCreateParams){
                                 .use_toolflags = false,
                             }));
  for (i = 0; i < num_shapes; i++) {
    Mesh *me = meshes[i];
    Object *ob = objects[i];
    /* Need normals for triangulation. */
    BM_mesh_bm_from_me(bm,
                       me,
                       &((struct BMeshFromMeshParams){
                           .calc_face_normal = true,
                       }));
    shape_face_end[i] = me->totpoly + (i == 0 ? 0 : shape_face_end[i - 1]);
    shape_vert_end[i] = me->totvert + (i == 0 ? 0 : shape_vert_end[i - 1]);
    if (i > 0) {
      bool is_flip = (is_neg_mat0 != is_negative_m4(ob->obmat));
      if (UNLIKELY(is_flip)) {
        const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
        BMIter iter;
        BMFace *efa;
        BM_mesh_elem_index_ensure(bm, BM_FACE);
        BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
          if (BM_elem_index_get(efa) >= shape_face_end[i - 1]) {
            BM_face_normal_flip_ex(bm, efa, cd_loop_mdisp_offset, true);
          }
        }
      }
    }
  }

  /* Triangulate the mesh. */
  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
  int tottri;
  BMLoop *(*looptris)[3];
  looptris = MEM_malloc_arrayN(looptris_tot, sizeof(*looptris), __func__);
  BM_mesh_calc_tessellation_beauty(bm, looptris, &tottri);

  /* Move the vertices of all but the first shape into transformation space of first mesh.
   * Do this after tesselation so don't need to recalculate normals.
   * The Exact solver doesn't need normals on the input faces. */
  float imat[4][4];
  float omat[4][4];
  float cleaned_object_obmat[4][4];
  clean_obmat(cleaned_object_obmat, ctx->object->obmat);
  invert_m4_m4(imat, cleaned_object_obmat);
  int curshape = 0;
  int curshape_vert_end = shape_vert_end[0];
  BMVert *eve;
  BMIter iter;
  i = 0;
  BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
    if (i == curshape_vert_end) {
      curshape++;
      curshape_vert_end = shape_vert_end[curshape];
      clean_obmat(cleaned_object_obmat, objects[curshape]->obmat);
      mul_m4_m4m4(omat, imat, cleaned_object_obmat);
    }
    if (curshape > 0) {
      mul_m4_v3(omat, eve->co);
    }
    i++;
  }

  /* Remap the materials. Fill a shape array for test function. Calculate normals. */
  int *shape = MEM_mallocN(bm->totface * sizeof(int), __func__);
  curshape = 0;
  int curshape_face_end = shape_face_end[0];
  int curshape_ncol = ctx->object->totcol;
  short *material_remap = NULL;
  BMFace *efa;
  i = 0;
  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    if (i == curshape_face_end) {
      curshape++;
      curshape_face_end = shape_face_end[curshape];
      if (material_remap != NULL) {
        MEM_freeN(material_remap);
      }
      curshape_ncol = objects[curshape]->totcol;
      material_remap = MEM_mallocN(curshape_ncol ? curshape_ncol : 1, __func__);
      BKE_object_material_remap_calc(ctx->object, objects[curshape], material_remap);
    }
    shape[i] = curshape;
    if (curshape > 0) {
      /* Normals for other shapes changed because vertex positions changed.
       * Boolean doesn't need these, but post-boolean code (interpolation) does. */
      BM_face_normal_update(efa);
      if (LIKELY(efa->mat_nr < curshape_ncol)) {
        efa->mat_nr = material_remap[efa->mat_nr];
      }
    }
    i++;
  }

  BM_mesh_elem_index_ensure(bm, BM_FACE);
  BM_mesh_boolean(bm,
                  looptris,
                  tottri,
                  bm_face_isect_nary,
                  shape,
                  num_shapes,
                  true,
                  false,
                  false,
                  bmd->operation);

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL, mesh);
  BM_mesh_free(bm);
  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  MEM_freeN(shape);
  MEM_freeN(shape_face_end);
  MEM_freeN(shape_vert_end);
  MEM_freeN(looptris);
  if (material_remap != NULL) {
    MEM_freeN(material_remap);
  }
  BLI_array_free(meshes);
  BLI_array_free(objects);
  return result;
}

#ifdef WITH_GMP
/* New method: bypass trip through BMesh. */
static Mesh *exact_boolean_mesh(BooleanModifierData *bmd,
                                const ModifierEvalContext *ctx,
                                Mesh *mesh)
{
  Mesh *result;
  Mesh *mesh_operand;
  Mesh **meshes = NULL;
  const float(**obmats)[4][4] = NULL;
  BLI_array_declare(meshes);
  BLI_array_declare(obmats);

#  ifdef DEBUG_TIME
  TIMEIT_START(boolean_bmesh);
#  endif

  BLI_array_append(meshes, mesh);
  BLI_array_append(obmats, &ctx->object->obmat);
  if (bmd->flag & eBooleanModifierFlag_Object) {
    if (bmd->object == NULL) {
      return mesh;
    }
    mesh_operand = BKE_modifier_get_evaluated_mesh_from_evaluated_object(bmd->object, false);
    BKE_mesh_wrapper_ensure_mdata(mesh_operand);
    BLI_array_append(meshes, mesh_operand);
    BLI_array_append(obmats, &bmd->object->obmat);
  }
  else if (bmd->flag & eBooleanModifierFlag_Collection) {
    Collection *collection = bmd->collection;
    /* Allow collection to be empty: then target mesh will just removed self-intersections. */
    if (collection) {
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, ob) {
        if (ob->type == OB_MESH && ob != ctx->object) {
          Mesh *collection_mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob, false);
          BKE_mesh_wrapper_ensure_mdata(collection_mesh);
          BLI_array_append(meshes, collection_mesh);
          BLI_array_append(obmats, &ob->obmat);
        }
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    }
  }

  const bool use_self = (bmd->flag & eBooleanModifierFlag_Self) != 0;
  const bool hole_tolerant = (bmd->flag & eBooleanModifierFlag_HoleTolerant) != 0;
  result = BKE_mesh_boolean((const Mesh **)meshes,
                            (const float(**)[4][4])obmats,
                            BLI_array_len(meshes),
                            use_self,
                            hole_tolerant,
                            bmd->operation);

  BLI_array_free(meshes);
  BLI_array_free(obmats);

#  ifdef DEBUG_TIME
  TIMEIT_END(boolean_bmesh);
#  endif

  return result;
}
#endif

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;
  Object *object = ctx->object;
  Mesh *result = mesh;
  Mesh *mesh_operand_ob;
  BMesh *bm;
  Collection *collection = bmd->collection;

  bool is_flip = false;
  const bool confirm_return = true;
#ifdef WITH_GMP
  const bool use_exact = bmd->solver == eBooleanModifierSolver_Exact;
  if (use_exact && bypass_bmesh) {
    return exact_boolean_mesh(bmd, ctx, mesh);
  }
#else
  const bool use_exact = false;
#endif

#ifdef DEBUG_TIME
  TIMEIT_START(boolean_bmesh);
#endif

  if (bmd->flag & eBooleanModifierFlag_Object) {
    if (bmd->object == NULL) {
      return result;
    }

    BMD_error_messages(ctx->object, md, NULL);

    Object *operand_ob = bmd->object;

#ifdef DEBUG_TIME
    TIMEIT_BLOCK_INIT(operand_get_evaluated_mesh);
    TIMEIT_BLOCK_START(operand_get_evaluated_mesh);
#endif
    mesh_operand_ob = BKE_modifier_get_evaluated_mesh_from_evaluated_object(operand_ob, false);
#ifdef DEBUG_TIME
    TIMEIT_BLOCK_END(operand_get_evaluated_mesh);
    TIMEIT_BLOCK_STATS(operand_get_evaluated_mesh);
#endif

    if (mesh_operand_ob) {
      /* XXX This is utterly non-optimal, we may go from a bmesh to a mesh back to a bmesh!
       * But for 2.90 better not try to be smart here. */
      BKE_mesh_wrapper_ensure_mdata(mesh_operand_ob);
      /* when one of objects is empty (has got no faces) we could speed up
       * calculation a bit returning one of objects' derived meshes (or empty one)
       * Returning mesh is depended on modifiers operation (sergey) */
      result = get_quick_mesh(object, mesh, operand_ob, mesh_operand_ob, bmd->operation);

      if (result == NULL) {
#ifdef DEBUG_TIME
        TIMEIT_BLOCK_INIT(object_BMD_mesh_bm_create);
        TIMEIT_BLOCK_START(object_BMD_mesh_bm_create);
#endif
        bm = BMD_mesh_bm_create(mesh, object, mesh_operand_ob, operand_ob, &is_flip);
#ifdef DEBUG_TIME
        TIMEIT_BLOCK_END(object_BMD_mesh_bm_create);
        TIMEIT_BLOCK_STATS(object_BMD_mesh_bm_create);
#endif

#ifdef DEBUG_TIME
        TIMEIT_BLOCK_INIT(BMD_mesh_intersection);
        TIMEIT_BLOCK_START(BMD_mesh_intersection);
#endif
        BMD_mesh_intersection(bm, md, ctx, mesh_operand_ob, object, operand_ob, is_flip);
#ifdef DEBUG_TIME
        TIMEIT_BLOCK_END(BMD_mesh_intersection);
        TIMEIT_BLOCK_STATS(BMD_mesh_intersection);
#endif

#ifdef DEBUG_TIME
        TIMEIT_BLOCK_INIT(BKE_mesh_from_bmesh_for_eval_nomain);
        TIMEIT_BLOCK_START(BKE_mesh_from_bmesh_for_eval_nomain);
#endif
        result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL, mesh);
#ifdef DEBUG_TIME
        TIMEIT_BLOCK_END(BKE_mesh_from_bmesh_for_eval_nomain);
        TIMEIT_BLOCK_STATS(BKE_mesh_from_bmesh_for_eval_nomain);
#endif
        BM_mesh_free(bm);
        result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
      }

      /* if new mesh returned, return it; otherwise there was
       * an error, so delete the modifier object */
      if (result == NULL) {
        BKE_modifier_set_error(object, md, "Cannot execute boolean operation");
      }
    }
  }

  else {
    if (collection == NULL && !use_exact) {
      return result;
    }

    /* Return result for certain errors. */
    if (BMD_error_messages(ctx->object, md, collection) == confirm_return) {
      return result;
    }

    if (use_exact) {
      result = collection_boolean_exact(bmd, ctx, mesh);
    }
    else {
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, operand_ob) {
        if (operand_ob->type == OB_MESH && operand_ob != ctx->object) {

          mesh_operand_ob = BKE_modifier_get_evaluated_mesh_from_evaluated_object(operand_ob,
                                                                                  false);

          if (mesh_operand_ob) {
            /* XXX This is utterly non-optimal, we may go from a bmesh to a mesh back to a bmesh!
             * But for 2.90 better not try to be smart here. */
            BKE_mesh_wrapper_ensure_mdata(mesh_operand_ob);

            bm = BMD_mesh_bm_create(mesh, object, mesh_operand_ob, operand_ob, &is_flip);

            BMD_mesh_intersection(bm, md, ctx, mesh_operand_ob, object, operand_ob, is_flip);

            /* Needed for multiple objects to work. */
            BM_mesh_bm_to_me(NULL,
                             bm,
                             mesh,
                             (&(struct BMeshToMeshParams){
                                 .calc_object_remap = false,
                             }));

            result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL, mesh);
            BM_mesh_free(bm);
            result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
          }
        }
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    }
  }

#ifdef DEBUG_TIME
  TIMEIT_END(boolean_bmesh);
#endif

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

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, NULL);

  uiItemR(layout, ptr, "operation", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "operand_type", 0, NULL, ICON_NONE);

  const bool operand_object = RNA_enum_get(ptr, "operand_type") == eBooleanModifierFlag_Object;

  if (operand_object) {
    uiItemR(layout, ptr, "object", 0, NULL, ICON_NONE);
  }
  else {
    uiItemR(layout, ptr, "collection", 0, NULL, ICON_NONE);
  }

  uiItemR(layout, ptr, "solver", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void solver_options_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, NULL);

  const bool use_exact = RNA_enum_get(ptr, "solver") == eBooleanModifierSolver_Exact;
  const bool operand_object = RNA_enum_get(ptr, "operand_type") == eBooleanModifierFlag_Object;

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, true);
  if (use_exact) {
    /* When operand is collection, we always use_self. */
    if (operand_object) {
      uiItemR(col, ptr, "use_self", 0, NULL, ICON_NONE);
    }
    uiItemR(col, ptr, "use_hole_tolerant", 0, NULL, ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "double_threshold", 0, NULL, ICON_NONE);
  }

  if (G.debug) {
    uiItemR(col, ptr, "debug_options", 0, NULL, ICON_NONE);
  }
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel = modifier_panel_register(region_type, eModifierType_Boolean, panel_draw);
  modifier_subpanel_register(
      region_type, "solver_options", "Solver Options", NULL, solver_options_panel_draw, panel);
}

ModifierTypeInfo modifierType_Boolean = {
    /* name */ "Boolean",
    /* structName */ "BooleanModifierData",
    /* structSize */ sizeof(BooleanModifierData),
    /* srna */ &RNA_BooleanModifier,
    /* type */ eModifierTypeType_Nonconstructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,
    /* icon */ ICON_MOD_BOOLEAN,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyGeometrySet */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
