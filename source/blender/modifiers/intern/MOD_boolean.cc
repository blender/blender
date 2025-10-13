/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_array.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_collection.hh"
#include "BKE_global.hh" /* only to check G.debug */
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_ui_common.hh"

#include "MEM_guardedalloc.h"

#include "GEO_mesh_boolean.hh"
#include "GEO_randomize.hh"

#include "bmesh.hh"
#include "tools/bmesh_intersect.hh"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_timeit.hh"
#endif

using blender::Array;
using blender::float3;
using blender::float4x4;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Span;
using blender::Vector;
using blender::VectorSet;

static void init_data(ModifierData *md)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(bmd, modifier));

  MEMCPY_STRUCT_AFTER(bmd, DNA_struct_default_get(BooleanModifierData), modifier);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;
  Collection *col = bmd->collection;

  if (bmd->flag & eBooleanModifierFlag_Object) {
    return !bmd->object || bmd->object->type != OB_MESH;
  }
  if (bmd->flag & eBooleanModifierFlag_Collection) {
    /* The Exact solver tolerates an empty collection. */
    return !col && bmd->solver != eBooleanModifierSolver_Mesh_Arr;
  }
  return false;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;

  walk(user_data, ob, (ID **)&bmd->collection, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&bmd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
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
  DEG_add_depends_on_transform_relation(ctx->node, "Boolean Modifier");
}

static Mesh *get_quick_mesh(
    Object *ob_self, Mesh *mesh_self, Object *ob_operand_ob, Mesh *mesh_operand_ob, int operation)
{
  Mesh *result = nullptr;

  if (mesh_self->faces_num == 0 || mesh_operand_ob->faces_num == 0) {
    switch (operation) {
      case eBooleanModifierOp_Intersect:
        result = BKE_mesh_new_nomain(0, 0, 0, 0);
        break;

      case eBooleanModifierOp_Union:
        if (mesh_self->faces_num != 0) {
          result = mesh_self;
        }
        else {
          result = (Mesh *)BKE_id_copy_ex(
              nullptr, &mesh_operand_ob->id, nullptr, LIB_ID_COPY_LOCALIZE);

          float imat[4][4];
          float omat[4][4];
          invert_m4_m4(imat, ob_self->object_to_world().ptr());
          mul_m4_m4m4(omat, imat, ob_operand_ob->object_to_world().ptr());

          MutableSpan<float3> positions = result->vert_positions_for_write();
          for (const int i : positions.index_range()) {
            mul_m4_v3(omat, positions[i]);
          }

          result->tag_positions_changed();
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
#define BM_FACE_TAG BM_ELEM_SELECT_UV

/**
 * Compare selected/unselected.
 */
static int bm_face_isect_pair(BMFace *f, void * /*user_data*/)
{
  return BM_elem_flag_test(f, BM_FACE_TAG) ? 1 : 0;
}

static bool BMD_error_messages(const Object *ob, ModifierData *md)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;
  Collection *col = bmd->collection;

  bool error_returns_result = false;

  const bool operand_collection = (bmd->flag & eBooleanModifierFlag_Collection) != 0;
  const bool use_exact = bmd->solver == eBooleanModifierSolver_Mesh_Arr;
  const bool use_manifold = bmd->solver == eBooleanModifierSolver_Manifold;
  const bool operation_intersect = bmd->operation == eBooleanModifierOp_Intersect;

#ifndef WITH_GMP
  /* If compiled without GMP, return a error. */
  if (use_exact) {
    BKE_modifier_set_error(ob, md, "Compiled without GMP, using fast solver");
    error_returns_result = false;
  }
#endif

  /* If intersect is selected using fast solver, return a error. */
  if (operand_collection && operation_intersect && !(use_exact || use_manifold)) {
    BKE_modifier_set_error(ob, md, "Cannot execute, intersect only available using exact solver");
    error_returns_result = true;
  }

  /* If the selected collection is empty and using fast solver, return a error. */
  if (operand_collection) {
    if (!use_exact && BKE_collection_is_empty(col)) {
      BKE_modifier_set_error(ob, md, "Cannot execute, non-exact solver and empty collection");
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
  SCOPED_TIMER(__func__);
#endif

  *r_is_flip = (is_negative_m4(object->object_to_world().ptr()) !=
                is_negative_m4(operand_ob->object_to_world().ptr()));

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh, mesh_operand_ob);

  BMeshCreateParams bmesh_create_params{};
  BMesh *bm = BM_mesh_create(&allocsize, &bmesh_create_params);

  /* Keep `mesh` first, needed so active layers are set based on `mesh` not `mesh_operand_ob`,
   * otherwise the wrong active render layer is used, see #92384.
   *
   * NOTE: while initializing customer data layers the is not essential,
   * it avoids the overhead of having to re-allocate #BMHeader.data when the 2nd mesh is added
   * (if it contains additional custom-data layers). */
  const Mesh *mesh_array[2] = {mesh, mesh_operand_ob};
  BM_mesh_copy_init_customdata_from_mesh_array(bm, mesh_array, ARRAY_SIZE(mesh_array), &allocsize);

  BMeshFromMeshParams bmesh_from_mesh_params{};
  bmesh_from_mesh_params.calc_face_normal = true;
  bmesh_from_mesh_params.calc_vert_normal = true;
  BM_mesh_bm_from_me(bm, mesh_operand_ob, &bmesh_from_mesh_params);

  if (UNLIKELY(*r_is_flip)) {
    const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
    BMIter iter;
    BMFace *efa;
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      BM_face_normal_flip_ex(bm, efa, cd_loop_mdisp_offset, true);
    }
  }

  BM_mesh_bm_from_me(bm, mesh, &bmesh_from_mesh_params);

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
  SCOPED_TIMER(__func__);
#endif

  BooleanModifierData *bmd = (BooleanModifierData *)md;

  /* Main BMesh intersection setup. */
  /* Create tessellation & intersect. */
  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
  blender::Array<std::array<BMLoop *, 3>> looptris(looptris_tot);
  BM_mesh_calc_tessellation_beauty(bm, looptris);

  /* postpone this until after tessellating
   * so we can use the original normals before the vertex are moved */
  {
    BMIter iter;
    int i;
    const int i_verts_end = mesh_operand_ob->verts_num;
    const int i_faces_end = mesh_operand_ob->faces_num;

    float imat[4][4];
    float omat[4][4];
    invert_m4_m4(imat, object->object_to_world().ptr());
    mul_m4_m4m4(omat, imat, operand_ob->object_to_world().ptr());

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
    /* XXX Pretty sure comment above is fully wrong now with copy-on-eval & co ? */
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
      else {
        efa->mat_nr = 0;
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
}

#ifdef WITH_GMP

/* Get a mapping from material slot numbers in the src_ob to slot numbers in the dst_ob.
 * If a material doesn't exist in the dst_ob, the mapping just goes to the same slot
 * or to zero if there aren't enough slots in the destination. */
static Array<short> get_material_remap_index_based(Object *dest_ob, Object *src_ob)
{
  const int n = src_ob->totcol;
  if (n <= 0) {
    return Array<short>(1, 0);
  }
  Array<short> remap(n);
  BKE_object_material_remap_calc(dest_ob, src_ob, remap.data());
  return remap;
}

/* Get a mapping from material slot numbers in the source geometry to slot numbers in the result
 * geometry. The material is added to the result geometry if it doesn't already use it. */
static Array<short> get_material_remap_transfer(Object &object,
                                                const Mesh &mesh,
                                                VectorSet<Material *> &materials)
{
  const int material_num = mesh.totcol;
  Array<short> map(material_num);
  for (const int i : IndexRange(material_num)) {
    Material *material = BKE_object_material_get_eval(&object, i + 1);
    map[i] = material ? materials.index_of_or_add(material) : -1;
  }
  return map;
}

static Mesh *non_float_boolean_mesh(BooleanModifierData *bmd,
                                    const ModifierEvalContext *ctx,
                                    Mesh *mesh)
{
  const float4x4 &world_to_object = ctx->object->world_to_object();
  Vector<const Mesh *> meshes;
  Vector<float4x4> transforms;

  Vector<Array<short>> material_remaps;

#  ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#  endif

  if ((bmd->flag & eBooleanModifierFlag_Object) && bmd->object == nullptr) {
    return mesh;
  }

  blender::geometry::boolean::Solver solver = bmd->solver == eBooleanModifierSolver_Mesh_Arr ?
                                                  blender::geometry::boolean::Solver::MeshArr :
                                                  blender::geometry::boolean::Solver::Manifold;
  meshes.append(mesh);
  transforms.append(float4x4::identity());
  material_remaps.append({});

  const BooleanModifierMaterialMode material_mode = BooleanModifierMaterialMode(
      bmd->material_mode);
  VectorSet<Material *> materials;
  if (material_mode == eBooleanModifierMaterialMode_Transfer) {
    if (mesh->totcol == 0) {
      /* Necessary for faces using the default material when there are no material slots. */
      materials.add(nullptr);
    }
    else {
      materials.add_multiple({mesh->mat, mesh->totcol});
    }
  }

  if (bmd->flag & eBooleanModifierFlag_Object) {
    Mesh *mesh_operand = BKE_modifier_get_evaluated_mesh_from_evaluated_object(bmd->object);
    if (!mesh_operand) {
      return mesh;
    }
    BKE_mesh_wrapper_ensure_mdata(mesh_operand);
    meshes.append(mesh_operand);
    transforms.append(world_to_object * bmd->object->object_to_world());
    if (material_mode == eBooleanModifierMaterialMode_Index) {
      material_remaps.append(get_material_remap_index_based(ctx->object, bmd->object));
    }
    else {
      material_remaps.append(get_material_remap_transfer(*bmd->object, *mesh_operand, materials));
    }
  }
  else if (bmd->flag & eBooleanModifierFlag_Collection) {
    Collection *collection = bmd->collection;
    /* Allow collection to be empty; then target mesh will just removed self-intersections. */
    if (collection) {
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, ob) {
        if (ob->type == OB_MESH && ob != ctx->object) {
          Mesh *collection_mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob);
          if (!collection_mesh) {
            continue;
          }
          BKE_mesh_wrapper_ensure_mdata(collection_mesh);
          meshes.append(collection_mesh);
          transforms.append(world_to_object * ob->object_to_world());
          if (material_mode == eBooleanModifierMaterialMode_Index) {
            material_remaps.append(get_material_remap_index_based(ctx->object, ob));
          }
          else {
            material_remaps.append(get_material_remap_transfer(*ob, *collection_mesh, materials));
          }
        }
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    }
  }

  const bool use_self = (bmd->flag & eBooleanModifierFlag_Self) != 0;
  const bool hole_tolerant = (bmd->flag & eBooleanModifierFlag_HoleTolerant) != 0;
  blender::geometry::boolean::BooleanOpParameters op_params;
  op_params.boolean_mode = blender::geometry::boolean::Operation(bmd->operation);
  op_params.no_self_intersections = !use_self;
  op_params.watertight = !hole_tolerant;
  op_params.no_nested_components = false;
  blender::geometry::boolean::BooleanError error =
      blender::geometry::boolean::BooleanError::NoError;
  Mesh *result = blender::geometry::boolean::mesh_boolean(
      meshes, transforms, material_remaps, op_params, solver, nullptr, &error);

  if (error != blender::geometry::boolean::BooleanError::NoError) {
    if (error == blender::geometry::boolean::BooleanError::NonManifold) {
      BKE_modifier_set_error(
          ctx->object, (ModifierData *)bmd, "Cannot execute, non-manifold inputs");
    }
    else if (error == blender::geometry::boolean::BooleanError::UnknownError) {
      BKE_modifier_set_error(ctx->object, (ModifierData *)(bmd), "Cannot execute, unknown error");
    }
    return result;
  }
  if (material_mode == eBooleanModifierMaterialMode_Transfer) {
    MEM_SAFE_FREE(result->mat);
    result->mat = MEM_malloc_arrayN<Material *>(size_t(materials.size()), __func__);
    result->totcol = materials.size();
    MutableSpan(result->mat, result->totcol).copy_from(materials);
  }

  blender::geometry::debug_randomize_mesh_order(result);

  return result;
}
#endif

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
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
  if (bmd->solver != eBooleanModifierSolver_Float) {
    return non_float_boolean_mesh(bmd, ctx, mesh);
  }
#endif

#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif

  if (bmd->flag & eBooleanModifierFlag_Object) {
    if (bmd->object == nullptr) {
      return result;
    }

    Object *operand_ob = bmd->object;

    Mesh *mesh_operand_ob = BKE_modifier_get_evaluated_mesh_from_evaluated_object(operand_ob);

    if (mesh_operand_ob) {
      /* XXX This is utterly non-optimal, we may go from a bmesh to a mesh back to a bmesh!
       * But for 2.90 better not try to be smart here. */
      BKE_mesh_wrapper_ensure_mdata(mesh_operand_ob);
      /* when one of objects is empty (has got no faces) we could speed up
       * calculation a bit returning one of objects' derived meshes (or empty one)
       * Returning mesh is dependent on modifiers operation (sergey) */
      result = get_quick_mesh(object, mesh, operand_ob, mesh_operand_ob, bmd->operation);

      if (result == nullptr) {
        bool is_flip;
        BMesh *bm = BMD_mesh_bm_create(mesh, object, mesh_operand_ob, operand_ob, &is_flip);

        BMD_mesh_intersection(bm, md, ctx, mesh_operand_ob, object, operand_ob, is_flip);

        result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh);

        BM_mesh_free(bm);
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
        Mesh *mesh_operand_ob = BKE_modifier_get_evaluated_mesh_from_evaluated_object(operand_ob);

        if (mesh_operand_ob == nullptr) {
          continue;
        }

        /* XXX This is utterly non-optimal, we may go from a bmesh to a mesh back to a bmesh!
         * But for 2.90 better not try to be smart here. */
        BKE_mesh_wrapper_ensure_mdata(mesh_operand_ob);

        bool is_flip;
        BMesh *bm = BMD_mesh_bm_create(result, object, mesh_operand_ob, operand_ob, &is_flip);

        BMD_mesh_intersection(bm, md, ctx, mesh_operand_ob, object, operand_ob, is_flip);

        /* Needed for multiple objects to work. */
        if (result == mesh) {
          result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh);
        }
        else {
          BMeshToMeshParams bmesh_to_mesh_params{};
          bmesh_to_mesh_params.calc_object_remap = false;
          BM_mesh_bm_to_me(nullptr, bm, result, &bmesh_to_mesh_params);
        }
        BM_mesh_free(bm);
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }

  blender::geometry::debug_randomize_mesh_order(result);

  return result;
}

static void required_data_mask(ModifierData * /*md*/, CustomData_MeshMasks *r_cddata_masks)
{
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  r_cddata_masks->fmask |= CD_MASK_MTFACE;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->prop(ptr, "operation", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  layout->use_property_split_set(true);

  layout->prop(ptr, "operand_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (RNA_enum_get(ptr, "operand_type") == eBooleanModifierFlag_Object) {
    layout->prop(ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else {
    layout->prop(ptr, "collection", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  layout->prop(ptr, "solver", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void solver_options_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  const bool use_exact = RNA_enum_get(ptr, "solver") == eBooleanModifierSolver_Mesh_Arr;
  const bool use_manifold = RNA_enum_get(ptr, "solver") == eBooleanModifierSolver_Manifold;

  layout->use_property_split_set(true);

  uiLayout *col = &layout->column(true);
  if (use_exact) {
    col->prop(ptr, "material_mode", UI_ITEM_NONE, IFACE_("Materials"), ICON_NONE);
    /* When operand is collection, we always use_self. */
    if (RNA_enum_get(ptr, "operand_type") == eBooleanModifierFlag_Object) {
      col->prop(ptr, "use_self", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
    col->prop(ptr, "use_hole_tolerant", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (use_manifold) {
    col->prop(ptr, "material_mode", UI_ITEM_NONE, IFACE_("Materials"), ICON_NONE);
  }
  else {
    col->prop(ptr, "double_threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (G.debug) {
    col->prop(ptr, "debug_options", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel = modifier_panel_register(region_type, eModifierType_Boolean, panel_draw);
  modifier_subpanel_register(
      region_type, "solver_options", "Solver Options", nullptr, solver_options_panel_draw, panel);
}

ModifierTypeInfo modifierType_Boolean = {
    /*idname*/ "Boolean",
    /*name*/ N_("Boolean"),
    /*struct_name*/ "BooleanModifierData",
    /*struct_size*/ sizeof(BooleanModifierData),
    /*srna*/ &RNA_BooleanModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/
    (eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode),
    /*icon*/ ICON_MOD_BOOLEAN,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
