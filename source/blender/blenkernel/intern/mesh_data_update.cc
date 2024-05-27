/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <climits>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_customdata_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_bvhutils.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_editmesh_cache.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_iterators.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_tangent.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_subdiv_modifier.hh"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "BKE_shrinkwrap.hh"
#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "CLG_log.h"

#ifdef WITH_OPENSUBDIV
#  include "DNA_userdef_types.h"
#endif

namespace blender::bke {

/* very slow! enable for testing only! */
// #define USE_MODIFIER_VALIDATE

#ifdef USE_MODIFIER_VALIDATE
#  define ASSERT_IS_VALID_MESH(mesh) \
    (BLI_assert((mesh == nullptr) || (BKE_mesh_is_valid(mesh) == true)))
#else
#  define ASSERT_IS_VALID_MESH(mesh)
#endif

static void mesh_init_origspace(Mesh &mesh);

void mesh_eval_to_meshkey(const Mesh *me_deformed, Mesh *mesh, KeyBlock *kb)
{
  /* Just a shallow wrapper around #BKE_keyblock_convert_from_mesh,
   * that ensures both evaluated mesh and original one has same number of vertices. */

  const int totvert = me_deformed->verts_num;

  if (totvert == 0 || mesh->verts_num == 0 || mesh->verts_num != totvert) {
    return;
  }

  BKE_keyblock_convert_from_mesh(me_deformed, mesh->key, kb);
}

static void mesh_set_only_copy(Mesh *mesh, const CustomData_MeshMasks *mask)
{
  CustomData_set_only_copy(&mesh->vert_data, mask->vmask);
  CustomData_set_only_copy(&mesh->edge_data, mask->emask);
  CustomData_set_only_copy(&mesh->fdata_legacy, mask->fmask);
  /* this wasn't in 2.63 and is disabled for 2.64 because it gives problems with
   * weight paint mode when there are modifiers applied, needs further investigation,
   * see replies to r50969, Campbell */
#if 0
  CustomData_set_only_copy(&mesh->ldata, mask->lmask);
  CustomData_set_only_copy(&mesh->pdata, mask->pmask);
#endif
}

/* orco custom data layer */
static Span<float3> get_orco_coords(const Object &ob,
                                    const BMEditMesh *em,
                                    eCustomDataType layer_type,
                                    Array<float3> &storage)
{
  if (layer_type == CD_ORCO) {

    if (em) {
      storage = BM_mesh_vert_coords_alloc(em->bm);
      return storage;
    }
    storage = BKE_mesh_orco_verts_get(&ob);
    return storage;
  }
  if (layer_type == CD_CLOTH_ORCO) {
    /* apply shape key for cloth, this should really be solved
     * by a more flexible customdata system, but not simple */
    if (!em) {
      const ClothModifierData *clmd = (const ClothModifierData *)BKE_modifiers_findby_type(
          &ob, eModifierType_Cloth);
      if (clmd && clmd->sim_parms->shapekey_rest) {
        const KeyBlock *kb = BKE_keyblock_find_by_index(
            BKE_key_from_object(const_cast<Object *>(&ob)), clmd->sim_parms->shapekey_rest);

        if (kb && kb->data) {
          return {static_cast<const float3 *>(kb->data), kb->totelem};
        }
      }
    }

    return {};
  }

  return {};
}

static Mesh *create_orco_mesh(const Object &ob,
                              const Mesh &mesh,
                              const BMEditMesh *em,
                              eCustomDataType layer)
{
  Mesh *orco_mesh;
  if (em) {
    orco_mesh = BKE_mesh_from_bmesh_for_eval_nomain(em->bm, nullptr, &mesh);
    BKE_mesh_ensure_default_orig_index_customdata(orco_mesh);
  }
  else {
    orco_mesh = BKE_mesh_copy_for_eval(mesh);
  }

  Array<float3> storage;
  const Span<float3> orco = get_orco_coords(ob, em, layer, storage);

  if (!orco.is_empty()) {
    orco_mesh->vert_positions_for_write().copy_from(orco);
    orco_mesh->tag_positions_changed();
  }

  return orco_mesh;
}

static MutableSpan<float3> orco_coord_layer_ensure(Mesh &mesh, const eCustomDataType layer)
{
  void *data = CustomData_get_layer_for_write(&mesh.vert_data, layer, mesh.verts_num);
  if (!data) {
    data = CustomData_add_layer(&mesh.vert_data, layer, CD_CONSTRUCT, mesh.verts_num);
  }
  return MutableSpan(reinterpret_cast<float3 *>(data), mesh.verts_num);
}

static void add_orco_mesh(Object &ob,
                          const BMEditMesh *em,
                          Mesh &mesh,
                          const Mesh *mesh_orco,
                          const eCustomDataType layer)
{
  const int totvert = mesh.verts_num;

  MutableSpan<float3> layer_orco;
  if (mesh_orco) {
    layer_orco = orco_coord_layer_ensure(mesh, layer);

    if (mesh_orco->verts_num == totvert) {
      layer_orco.copy_from(mesh_orco->vert_positions());
    }
    else {
      layer_orco.copy_from(mesh.vert_positions());
    }
  }
  else {
    /* TODO(sybren): totvert should potentially change here, as ob->data
     * or em may have a different number of vertices than dm. */
    Array<float3> storage;
    const Span<float3> orco = get_orco_coords(ob, em, layer, storage);
    if (!orco.is_empty()) {
      layer_orco = orco_coord_layer_ensure(mesh, layer);
      layer_orco.copy_from(orco);
    }
  }

  if (!layer_orco.is_empty()) {
    if (layer == CD_ORCO) {
      BKE_mesh_orco_verts_transform((Mesh *)ob.data, layer_orco, false);
    }
  }
}

/* Does final touches to the final evaluated mesh, making sure it is perfectly usable.
 *
 * This is needed because certain information is not passed along intermediate meshes allocated
 * during stack evaluation.
 */
static void mesh_calc_finalize(const Mesh &mesh_input, Mesh &mesh_eval)
{
  /* Make sure the name is the same. This is because mesh allocation from template does not
   * take care of naming. */
  STRNCPY(mesh_eval.id.name, mesh_input.id.name);
  /* Make evaluated mesh to share same edit mesh pointer as original and copied meshes. */
  mesh_eval.runtime->edit_mesh = mesh_input.runtime->edit_mesh;
}

/**
 * Modifies the given mesh and geometry set. The mesh is not passed as part of the mesh component
 * in the \a geometry_set input, it is only passed in \a input_mesh and returned in the return
 * value.
 *
 * The purpose of the geometry set is to store all geometry components that are generated
 * by modifiers to allow outputting non-mesh data from modifiers.
 */
static Mesh *modifier_modify_mesh_and_geometry_set(ModifierData *md,
                                                   const ModifierEvalContext &mectx,
                                                   Mesh *input_mesh,
                                                   GeometrySet &geometry_set)
{
  Mesh *mesh_output = nullptr;
  const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);
  if (mti->modify_geometry_set == nullptr) {
    mesh_output = BKE_modifier_modify_mesh(md, &mectx, input_mesh);
  }
  else {
    /* For performance reasons, this should be called by the modifier and/or nodes themselves at
     * some point. */
    BKE_mesh_wrapper_ensure_mdata(input_mesh);

    /* Replace only the mesh rather than the whole component, because the entire #MeshComponent
     * might have been replaced by data from a different object in the node tree, which means the
     * component contains vertex group name data for that object that should not be removed. */
    geometry_set.replace_mesh(input_mesh, GeometryOwnershipType::Editable);

    /* Let the modifier change the geometry set. */
    mti->modify_geometry_set(md, &mectx, &geometry_set);

    /* Release the mesh from the geometry set again. */
    if (geometry_set.has<MeshComponent>()) {
      MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
      if (mesh_component.get() != input_mesh) {
        /* Make sure the mesh component actually owns the mesh before taking over ownership. */
        mesh_component.ensure_owns_direct_data();
      }
      mesh_output = mesh_component.release();
    }
    /* Need to ensure that non-mesh data is also owned by the geometry set. Otherwise it might be
     * freed while there is still a reference to it in the geometry. */
    geometry_set.ensure_owns_direct_data();

    /* Return an empty mesh instead of null. */
    if (mesh_output == nullptr) {
      mesh_output = BKE_mesh_new_nomain(0, 0, 0, 0);
      BKE_mesh_copy_parameters_for_eval(mesh_output, input_mesh);
    }
  }

  return mesh_output;
}

static void set_rest_position(Mesh &mesh)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const AttributeReader positions = attributes.lookup<float3>("position");
  attributes.remove("rest_position");
  if (positions) {
    if (positions.sharing_info && positions.varray.is_span()) {
      attributes.add<float3>("rest_position",
                             AttrDomain::Point,
                             AttributeInitShared(positions.varray.get_internal_span().data(),
                                                 *positions.sharing_info));
    }
    else {
      attributes.add<float3>(
          "rest_position", AttrDomain::Point, AttributeInitVArray(positions.varray));
    }
  }
}

static void mesh_calc_modifiers(Depsgraph &depsgraph,
                                const Scene &scene,
                                Object &ob,
                                const bool use_deform,
                                const bool need_mapping,
                                const CustomData_MeshMasks &dataMask,
                                const bool use_cache,
                                const bool allow_shared_mesh,
                                /* return args */
                                Mesh **r_deform,
                                Mesh **r_final,
                                GeometrySet **r_geometry_set)
{
  /* Input mesh shouldn't be modified. */
  Mesh &mesh_input = *static_cast<Mesh *>(ob.data);
  /* The final mesh is the result of calculating all enabled modifiers. */
  Mesh *mesh = nullptr;
  /* The result of calculating all leading deform modifiers. */
  Mesh *mesh_deform = nullptr;
  /* This geometry set contains the non-mesh data that might be generated by modifiers. */
  GeometrySet geometry_set_final;

  BLI_assert((mesh_input.id.tag & LIB_TAG_COPIED_ON_EVAL_FINAL_RESULT) == 0);

  /* Mesh with constructive modifiers but no deformation applied. Tracked
   * along with final mesh if undeformed / orco coordinates are requested
   * for texturing. */
  Mesh *mesh_orco = nullptr;
  Mesh *mesh_orco_cloth = nullptr;

  /* Modifier evaluation modes. */
  const bool use_render = (DEG_get_mode(&depsgraph) == DAG_EVAL_RENDER);
  const int required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;

  /* Sculpt can skip certain modifiers. */
  const bool has_multires = BKE_sculpt_multires_active(&scene, &ob) != nullptr;
  bool multires_applied = false;
  const bool sculpt_mode = ob.mode & OB_MODE_SCULPT && ob.sculpt && !use_render;
  const bool sculpt_dyntopo = (sculpt_mode && ob.sculpt->bm) && !use_render;

  /* Modifier evaluation contexts for different types of modifiers. */
  ModifierApplyFlag apply_render = use_render ? MOD_APPLY_RENDER : ModifierApplyFlag(0);
  ModifierApplyFlag apply_cache = use_cache ? MOD_APPLY_USECACHE : ModifierApplyFlag(0);
  const ModifierEvalContext mectx = {&depsgraph, &ob, apply_render | apply_cache};
  const ModifierEvalContext mectx_orco = {&depsgraph, &ob, apply_render | MOD_APPLY_ORCO};

  /* Get effective list of modifiers to execute. Some effects like shape keys
   * are added as virtual modifiers before the user created modifiers. */
  VirtualModifierData virtual_modifier_data;
  ModifierData *firstmd = BKE_modifiers_get_virtual_modifierlist(&ob, &virtual_modifier_data);
  ModifierData *md = firstmd;

  /* Compute accumulated datamasks needed by each modifier. It helps to do
   * this fine grained so that for example vertex groups are preserved up to
   * an armature modifier, but not through a following subsurf modifier where
   * subdividing them is expensive. */
  CustomData_MeshMasks final_datamask = dataMask;
  CDMaskLink *datamasks = BKE_modifier_calc_data_masks(&scene, md, &final_datamask, required_mode);
  CDMaskLink *md_datamask = datamasks;
  /* XXX Always copying POLYINDEX, else tessellated data are no more valid! */
  CustomData_MeshMasks append_mask = CD_MASK_BAREMESH_ORIGINDEX;

  /* Clear errors before evaluation. */
  BKE_modifiers_clear_errors(&ob);

  if (ob.modifier_flag & OB_MODIFIER_FLAG_ADD_REST_POSITION) {
    if (mesh == nullptr) {
      mesh = BKE_mesh_copy_for_eval(mesh_input);
      ASSERT_IS_VALID_MESH(mesh);
    }
    set_rest_position(*mesh);
  }

  /* Apply all leading deform modifiers. */
  if (use_deform) {
    for (; md; md = md->next, md_datamask = md_datamask->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

      if (!BKE_modifier_is_enabled(&scene, md, required_mode)) {
        continue;
      }

      if (mti->type == ModifierTypeType::OnlyDeform && !sculpt_dyntopo) {
        ScopedModifierTimer modifier_timer{*md};
        if (!mesh) {
          mesh = BKE_mesh_copy_for_eval(mesh_input);
          ASSERT_IS_VALID_MESH(mesh);
        }

        if (mti->required_data_mask) {
          CustomData_MeshMasks mask{};
          mti->required_data_mask(md, &mask);
          if (mask.vmask & CD_MASK_ORCO) {
            add_orco_mesh(ob, nullptr, *mesh, nullptr, CD_ORCO);
          }
        }

        BKE_modifier_deform_verts(md, &mectx, mesh, mesh->vert_positions_for_write());
      }
      else {
        break;
      }
    }

    /* Result of all leading deforming modifiers is cached for
     * places that wish to use the original mesh but with deformed
     * coordinates (like vertex paint). */
    if (r_deform) {
      mesh_deform = BKE_mesh_copy_for_eval(mesh ? *mesh : mesh_input);
    }
  }

  /* Apply all remaining constructive and deforming modifiers. */
  bool have_non_onlydeform_modifiers_applied = false;
  for (; md; md = md->next, md_datamask = md_datamask->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

    if (!BKE_modifier_is_enabled(&scene, md, required_mode)) {
      continue;
    }

    if (mti->type == ModifierTypeType::OnlyDeform && !use_deform) {
      continue;
    }

    if ((mti->flags & eModifierTypeFlag_RequiresOriginalData) &&
        have_non_onlydeform_modifiers_applied)
    {
      BKE_modifier_set_error(&ob, md, "Modifier requires original data, bad stack position");
      continue;
    }

    if (sculpt_mode && (!has_multires || multires_applied || sculpt_dyntopo)) {
      bool unsupported = false;

      if (md->type == eModifierType_Multires && ((MultiresModifierData *)md)->sculptlvl == 0) {
        /* If multires is on level 0 skip it silently without warning message. */
        if (!sculpt_dyntopo) {
          continue;
        }
      }

      if (sculpt_dyntopo) {
        unsupported = true;
      }

      if (scene.toolsettings->sculpt->flags & SCULPT_ONLY_DEFORM) {
        unsupported |= (mti->type != ModifierTypeType::OnlyDeform);
      }

      unsupported |= multires_applied;

      if (unsupported) {
        if (sculpt_dyntopo) {
          BKE_modifier_set_error(&ob, md, "Not supported in dyntopo");
        }
        else {
          BKE_modifier_set_error(&ob, md, "Not supported in sculpt mode");
        }
        continue;
      }
    }

    if (need_mapping && !BKE_modifier_supports_mapping(md)) {
      continue;
    }

    ScopedModifierTimer modifier_timer{*md};

    /* Add orco mesh as layer if needed by this modifier. */
    if (mesh && mesh_orco && mti->required_data_mask) {
      CustomData_MeshMasks mask = {0};
      mti->required_data_mask(md, &mask);
      if (mask.vmask & CD_MASK_ORCO) {
        add_orco_mesh(ob, nullptr, *mesh, mesh_orco, CD_ORCO);
      }
    }

    if (mti->type == ModifierTypeType::OnlyDeform) {
      if (!mesh) {
        mesh = BKE_mesh_copy_for_eval(mesh_input);
        ASSERT_IS_VALID_MESH(mesh);
      }
      BKE_modifier_deform_verts(md, &mectx, mesh, mesh->vert_positions_for_write());
    }
    else {
      bool check_for_needs_mapping = false;
      if (mesh != nullptr) {
        if (have_non_onlydeform_modifiers_applied == false) {
          /* If we only deformed, we won't have initialized #CD_ORIGINDEX.
           * as this is the only part of the function that initializes mapping. */
          check_for_needs_mapping = true;
        }
      }
      else {
        mesh = BKE_mesh_copy_for_eval(mesh_input);
        ASSERT_IS_VALID_MESH(mesh);
        check_for_needs_mapping = true;
      }

      have_non_onlydeform_modifiers_applied = true;

      /* determine which data layers are needed by following modifiers */
      CustomData_MeshMasks nextmask = md_datamask->next ? md_datamask->next->mask : final_datamask;

      if (check_for_needs_mapping) {
        /* Initialize original indices the first time we evaluate a
         * constructive modifier. Modifiers will then do mapping mostly
         * automatic by copying them through CustomData_copy_data along
         * with other data.
         *
         * These are created when either requested by evaluation, or if
         * following modifiers requested them. */
        if (need_mapping ||
            ((nextmask.vmask | nextmask.emask | nextmask.pmask) & CD_MASK_ORIGINDEX))
        {
          /* calc */
          CustomData_add_layer(&mesh->vert_data, CD_ORIGINDEX, CD_CONSTRUCT, mesh->verts_num);
          CustomData_add_layer(&mesh->edge_data, CD_ORIGINDEX, CD_CONSTRUCT, mesh->edges_num);
          CustomData_add_layer(&mesh->face_data, CD_ORIGINDEX, CD_CONSTRUCT, mesh->faces_num);

          /* Not worth parallelizing this,
           * gives less than 0.1% overall speedup in best of best cases... */
          range_vn_i((int *)CustomData_get_layer_for_write(
                         &mesh->vert_data, CD_ORIGINDEX, mesh->verts_num),
                     mesh->verts_num,
                     0);
          range_vn_i((int *)CustomData_get_layer_for_write(
                         &mesh->edge_data, CD_ORIGINDEX, mesh->edges_num),
                     mesh->edges_num,
                     0);
          range_vn_i((int *)CustomData_get_layer_for_write(
                         &mesh->face_data, CD_ORIGINDEX, mesh->faces_num),
                     mesh->faces_num,
                     0);
        }
      }

      /* set the Mesh to only copy needed data */
      CustomData_MeshMasks mask = md_datamask->mask;
      /* needMapping check here fixes bug #28112, otherwise it's
       * possible that it won't be copied */
      CustomData_MeshMasks_update(&mask, &append_mask);
      if (need_mapping) {
        mask.vmask |= CD_MASK_ORIGINDEX;
        mask.emask |= CD_MASK_ORIGINDEX;
        mask.pmask |= CD_MASK_ORIGINDEX;
      }
      mesh_set_only_copy(mesh, &mask);

      /* add cloth rest shape key if needed */
      if (mask.vmask & CD_MASK_CLOTH_ORCO) {
        add_orco_mesh(ob, nullptr, *mesh, mesh_orco, CD_CLOTH_ORCO);
      }

      /* add an origspace layer if needed */
      if ((md_datamask->mask.lmask) & CD_MASK_ORIGSPACE_MLOOP) {
        if (!CustomData_has_layer(&mesh->corner_data, CD_ORIGSPACE_MLOOP)) {
          CustomData_add_layer(
              &mesh->corner_data, CD_ORIGSPACE_MLOOP, CD_SET_DEFAULT, mesh->corners_num);
          mesh_init_origspace(*mesh);
        }
      }

      Mesh *mesh_next = modifier_modify_mesh_and_geometry_set(md, mectx, mesh, geometry_set_final);
      ASSERT_IS_VALID_MESH(mesh_next);

      if (mesh_next) {
        /* if the modifier returned a new mesh, release the old one */
        if (mesh != mesh_next) {
          BLI_assert(mesh != &mesh_input);
          BKE_id_free(nullptr, mesh);
        }
        mesh = mesh_next;
      }

      /* create an orco mesh in parallel */
      if (nextmask.vmask & CD_MASK_ORCO) {
        if (!mesh_orco) {
          mesh_orco = create_orco_mesh(ob, mesh_input, nullptr, CD_ORCO);
        }

        nextmask.vmask &= ~CD_MASK_ORCO;
        CustomData_MeshMasks temp_cddata_masks = {0};
        temp_cddata_masks.vmask = CD_MASK_ORIGINDEX;
        temp_cddata_masks.emask = CD_MASK_ORIGINDEX;
        temp_cddata_masks.fmask = CD_MASK_ORIGINDEX;
        temp_cddata_masks.pmask = CD_MASK_ORIGINDEX;

        if (mti->required_data_mask != nullptr) {
          mti->required_data_mask(md, &temp_cddata_masks);
        }
        CustomData_MeshMasks_update(&temp_cddata_masks, &nextmask);
        mesh_set_only_copy(mesh_orco, &temp_cddata_masks);

        mesh_next = BKE_modifier_modify_mesh(md, &mectx_orco, mesh_orco);
        ASSERT_IS_VALID_MESH(mesh_next);

        if (mesh_next) {
          /* if the modifier returned a new mesh, release the old one */
          if (mesh_orco != mesh_next) {
            BLI_assert(mesh_orco != &mesh_input);
            BKE_id_free(nullptr, mesh_orco);
          }

          mesh_orco = mesh_next;
        }
      }

      /* create cloth orco mesh in parallel */
      if (nextmask.vmask & CD_MASK_CLOTH_ORCO) {
        if (!mesh_orco_cloth) {
          mesh_orco_cloth = create_orco_mesh(ob, mesh_input, nullptr, CD_CLOTH_ORCO);
        }

        nextmask.vmask &= ~CD_MASK_CLOTH_ORCO;
        nextmask.vmask |= CD_MASK_ORIGINDEX;
        nextmask.emask |= CD_MASK_ORIGINDEX;
        nextmask.pmask |= CD_MASK_ORIGINDEX;
        mesh_set_only_copy(mesh_orco_cloth, &nextmask);

        mesh_next = BKE_modifier_modify_mesh(md, &mectx_orco, mesh_orco_cloth);
        ASSERT_IS_VALID_MESH(mesh_next);

        if (mesh_next) {
          /* if the modifier returned a new mesh, release the old one */
          if (mesh_orco_cloth != mesh_next) {
            BLI_assert(mesh_orco != &mesh_input);
            BKE_id_free(nullptr, mesh_orco_cloth);
          }

          mesh_orco_cloth = mesh_next;
        }
      }

      mesh->runtime->deformed_only = false;
    }

    if (sculpt_mode && md->type == eModifierType_Multires) {
      multires_applied = true;
    }
  }

  BLI_linklist_free((LinkNode *)datamasks, nullptr);

  for (md = firstmd; md; md = md->next) {
    BKE_modifier_free_temporary_data(md);
  }

  if (mesh == nullptr) {
    if (allow_shared_mesh) {
      mesh = &mesh_input;
    }
    else {
      mesh = BKE_mesh_copy_for_eval(mesh_input);
    }
  }

  /* Denotes whether the object which the modifier stack came from owns the mesh or whether the
   * mesh is shared across multiple objects since there are no effective modifiers. */
  const bool is_own_mesh = (mesh != &mesh_input);

  /* Add orco coordinates to final and deformed mesh if requested. */
  if (final_datamask.vmask & CD_MASK_ORCO) {
    /* No need in ORCO layer if the mesh was not deformed or modified: undeformed mesh in this case
     * matches input mesh. */
    if (is_own_mesh) {
      add_orco_mesh(ob, nullptr, *mesh, mesh_orco, CD_ORCO);
    }

    if (mesh_deform) {
      add_orco_mesh(ob, nullptr, *mesh_deform, nullptr, CD_ORCO);
    }
  }

  if (mesh_orco) {
    BKE_id_free(nullptr, mesh_orco);
  }
  if (mesh_orco_cloth) {
    BKE_id_free(nullptr, mesh_orco_cloth);
  }

  /* Remove temporary data layer only needed for modifier evaluation.
   * Save some memory, and ensure GPU subdivision does not need to deal with this. */
  CustomData_free_layers(&mesh->vert_data, CD_CLOTH_ORCO, mesh->verts_num);

  /* Compute normals. */
  if (is_own_mesh) {
    mesh_calc_finalize(mesh_input, *mesh);
  }
  else {
    MeshRuntime *runtime = mesh_input.runtime;
    if (runtime->mesh_eval == nullptr) {
      std::lock_guard lock{mesh_input.runtime->eval_mutex};
      if (runtime->mesh_eval == nullptr) {
        /* Not yet finalized by any instance, do it now
         * Isolate since computing normals is multithreaded and we are holding a lock. */
        threading::isolate_task([&] {
          mesh = BKE_mesh_copy_for_eval(mesh_input);
          mesh_calc_finalize(mesh_input, *mesh);
          runtime->mesh_eval = mesh;
        });
      }
      else {
        /* Already finalized by another instance, reuse. */
        mesh = runtime->mesh_eval;
      }
    }
    else {
      /* Already finalized by another instance, reuse. */
      mesh = runtime->mesh_eval;
    }
  }

  /* Return final mesh */
  *r_final = mesh;
  if (r_deform) {
    *r_deform = mesh_deform;
  }
  if (r_geometry_set) {
    *r_geometry_set = new GeometrySet(std::move(geometry_set_final));
  }
}

bool editbmesh_modifier_is_enabled(const Scene *scene,
                                   const Object *ob,
                                   ModifierData *md,
                                   bool has_prev_mesh)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);
  const int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;

  if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
    return false;
  }

  if ((mti->flags & eModifierTypeFlag_RequiresOriginalData) && has_prev_mesh) {
    BKE_modifier_set_error(ob, md, "Modifier requires original data, bad stack position");
    return false;
  }

  return true;
}

static MutableSpan<float3> mesh_wrapper_vert_coords_ensure_for_write(Mesh *mesh)
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      if (mesh->runtime->edit_data->vert_positions.is_empty()) {
        mesh->runtime->edit_data->vert_positions = BM_mesh_vert_coords_alloc(
            mesh->runtime->edit_mesh->bm);
      }
      return mesh->runtime->edit_data->vert_positions;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return mesh->vert_positions_for_write();
  }
  BLI_assert_unreachable();
  return {};
}

static void editbmesh_calc_modifiers(Depsgraph &depsgraph,
                                     const Scene &scene,
                                     Object &ob,
                                     const CustomData_MeshMasks &dataMask,
                                     /* return args */
                                     Mesh **r_cage,
                                     Mesh **r_final,
                                     GeometrySet **r_geometry_set)
{
  Mesh &mesh_input = *static_cast<Mesh *>(ob.data);
  BMEditMesh &em_input = *mesh_input.runtime->edit_mesh;

  Mesh *mesh_cage = nullptr;
  /* This geometry set contains the non-mesh data that might be generated by modifiers. */
  GeometrySet geometry_set_final;

  /* Mesh with constructive modifiers but no deformation applied. Tracked
   * along with final mesh if undeformed / orco coordinates are requested
   * for texturing. */
  Mesh *mesh_orco = nullptr;

  /* Modifier evaluation modes. */
  const int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;

  const bool use_render = (DEG_get_mode(&depsgraph) == DAG_EVAL_RENDER);
  /* Modifier evaluation contexts for different types of modifiers. */
  ModifierApplyFlag apply_render = use_render ? MOD_APPLY_RENDER : ModifierApplyFlag(0);
  const ModifierEvalContext mectx = {&depsgraph, &ob, MOD_APPLY_USECACHE | apply_render};
  const ModifierEvalContext mectx_orco = {&depsgraph, &ob, MOD_APPLY_ORCO};

  /* Get effective list of modifiers to execute. Some effects like shape keys
   * are added as virtual modifiers before the user created modifiers. */
  VirtualModifierData virtual_modifier_data;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(&ob, &virtual_modifier_data);

  /* Compute accumulated datamasks needed by each modifier. It helps to do
   * this fine grained so that for example vertex groups are preserved up to
   * an armature modifier, but not through a following subsurf modifier where
   * subdividing them is expensive. */
  CustomData_MeshMasks final_datamask = dataMask;
  CDMaskLink *datamasks = BKE_modifier_calc_data_masks(&scene, md, &final_datamask, required_mode);
  CDMaskLink *md_datamask = datamasks;
  CustomData_MeshMasks append_mask = CD_MASK_BAREMESH;

  Mesh *mesh = BKE_mesh_wrapper_from_editmesh(
      mesh_input.runtime->edit_mesh, &final_datamask, &mesh_input);

  int cageIndex = BKE_modifiers_get_cage_index(&scene, &ob, nullptr, true);
  if (r_cage && cageIndex == -1) {
    mesh_cage = mesh;
  }

  /* The mesh from edit mode should not have any original index layers already, since those
   * are added during evaluation when necessary and are redundant on an original mesh. */
  BLI_assert(CustomData_get_layer(&em_input.bm->pdata, CD_ORIGINDEX) == nullptr &&
             CustomData_get_layer(&em_input.bm->edata, CD_ORIGINDEX) == nullptr &&
             CustomData_get_layer(&em_input.bm->pdata, CD_ORIGINDEX) == nullptr);

  /* Clear errors before evaluation. */
  BKE_modifiers_clear_errors(&ob);

  if (ob.modifier_flag & OB_MODIFIER_FLAG_ADD_REST_POSITION) {
    BKE_mesh_wrapper_ensure_mdata(mesh);
    set_rest_position(*mesh);
  }

  bool non_deform_modifier_applied = false;
  for (int i = 0; md; i++, md = md->next, md_datamask = md_datamask->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);
    if (!editbmesh_modifier_is_enabled(&scene, &ob, md, non_deform_modifier_applied)) {
      continue;
    }

    ScopedModifierTimer modifier_timer{*md};

    /* Add an orco mesh as layer if needed by this modifier. */
    if (mesh_orco && mti->required_data_mask) {
      CustomData_MeshMasks mask = {0};
      mti->required_data_mask(md, &mask);
      if (mask.vmask & CD_MASK_ORCO) {
        add_orco_mesh(ob, &em_input, *mesh, mesh_orco, CD_ORCO);
      }
    }

    if (mesh == mesh_cage) {
      /* If the cage mesh has already been assigned, we have passed the cage index in the modifier
       * list. If the cage and final meshes are still the same, duplicate the final mesh so the
       * cage mesh isn't modified anymore. */
      mesh = BKE_mesh_copy_for_eval(*mesh);
      if (mesh_cage->runtime->edit_mesh) {
        mesh->runtime->edit_mesh = mesh_cage->runtime->edit_mesh;
        mesh->runtime->is_original_bmesh = true;
        if (mesh_cage->runtime->edit_data) {
          mesh->runtime->edit_data = std::make_unique<EditMeshData>(
              *mesh_cage->runtime->edit_data);
        }
      }
    }

    if (mti->type == ModifierTypeType::OnlyDeform) {
      if (mti->deform_verts_EM) {
        BKE_modifier_deform_vertsEM(
            md, &mectx, &em_input, mesh, mesh_wrapper_vert_coords_ensure_for_write(mesh));
        BKE_mesh_wrapper_tag_positions_changed(mesh);
      }
      else {
        BKE_mesh_wrapper_ensure_mdata(mesh);
        BKE_modifier_deform_verts(md, &mectx, mesh, mesh->vert_positions_for_write());
        mesh->tag_positions_changed();
      }
    }
    else {
      non_deform_modifier_applied = true;

      /* create an orco derivedmesh in parallel */
      CustomData_MeshMasks mask = md_datamask->mask;
      if (mask.vmask & CD_MASK_ORCO) {
        if (!mesh_orco) {
          mesh_orco = create_orco_mesh(ob, mesh_input, &em_input, CD_ORCO);
        }

        mask.vmask &= ~CD_MASK_ORCO;
        mask.vmask |= CD_MASK_ORIGINDEX;
        mask.emask |= CD_MASK_ORIGINDEX;
        mask.pmask |= CD_MASK_ORIGINDEX;
        mesh_set_only_copy(mesh_orco, &mask);

        Mesh *mesh_next = BKE_modifier_modify_mesh(md, &mectx_orco, mesh_orco);
        ASSERT_IS_VALID_MESH(mesh_next);

        if (mesh_next) {
          /* if the modifier returned a new dm, release the old one */
          if (mesh_orco && mesh_orco != mesh_next) {
            BKE_id_free(nullptr, mesh_orco);
          }
          mesh_orco = mesh_next;
        }
      }

      /* set the DerivedMesh to only copy needed data */
      CustomData_MeshMasks_update(&mask, &append_mask);
      /* XXX WHAT? overwrites mask ??? */
      /* CD_MASK_ORCO may have been cleared above */
      mask = md_datamask->mask;
      mask.vmask |= CD_MASK_ORIGINDEX;
      mask.emask |= CD_MASK_ORIGINDEX;
      mask.pmask |= CD_MASK_ORIGINDEX;

      mesh_set_only_copy(mesh, &mask);

      if (mask.lmask & CD_MASK_ORIGSPACE_MLOOP) {
        if (!CustomData_has_layer(&mesh->corner_data, CD_ORIGSPACE_MLOOP)) {
          CustomData_add_layer(
              &mesh->corner_data, CD_ORIGSPACE_MLOOP, CD_SET_DEFAULT, mesh->corners_num);
          mesh_init_origspace(*mesh);
        }
      }

      Mesh *mesh_next = modifier_modify_mesh_and_geometry_set(md, mectx, mesh, geometry_set_final);
      ASSERT_IS_VALID_MESH(mesh_next);

      if (mesh_next) {
        if (mesh != mesh_next) {
          BKE_id_free(nullptr, mesh);
        }
        mesh = mesh_next;
      }
      mesh->runtime->deformed_only = false;
    }

    if (r_cage && i == cageIndex) {
      mesh_cage = mesh;
    }
  }

  BLI_linklist_free((LinkNode *)datamasks, nullptr);

  /* Add orco coordinates to final and deformed mesh if requested. */
  if (final_datamask.vmask & CD_MASK_ORCO) {
    /* FIXME(@ideasman42): avoid the need to convert to mesh data just to add an orco layer. */
    BKE_mesh_wrapper_ensure_mdata(mesh);

    add_orco_mesh(ob, &em_input, *mesh, mesh_orco, CD_ORCO);
  }

  if (mesh_orco) {
    BKE_id_free(nullptr, mesh_orco);
  }

  /* Return final mesh. */
  *r_final = mesh;
  if (r_cage) {
    *r_cage = mesh_cage;
  }
  if (r_geometry_set) {
    *r_geometry_set = new GeometrySet(std::move(geometry_set_final));
  }
}

static void mesh_build_extra_data(const Depsgraph &depsgraph,
                                  const Object &ob,
                                  const Mesh &mesh_eval)
{
  uint32_t eval_flags = DEG_get_eval_flags_for_id(&depsgraph, &ob.id);

  if (eval_flags & DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY) {
    shrinkwrap::boundary_cache_ensure(mesh_eval);
  }
}

static void mesh_build_data(Depsgraph &depsgraph,
                            const Scene &scene,
                            Object &ob,
                            const CustomData_MeshMasks &dataMask,
                            const bool need_mapping)
{
#if 0 /* XXX This is already taken care of in #mesh_calc_modifiers... */
  if (need_mapping) {
    /* Also add the flag so that it is recorded in lastDataMask. */
    dataMask->vmask |= CD_MASK_ORIGINDEX;
    dataMask->emask |= CD_MASK_ORIGINDEX;
    dataMask->pmask |= CD_MASK_ORIGINDEX;
  }
#endif

  Mesh *mesh_eval = nullptr, *mesh_deform_eval = nullptr;
  GeometrySet *geometry_set_eval = nullptr;
  mesh_calc_modifiers(depsgraph,
                      scene,
                      ob,
                      true,
                      need_mapping,
                      dataMask,
                      true,
                      true,
                      &mesh_deform_eval,
                      &mesh_eval,
                      &geometry_set_eval);

  /* The modifier stack evaluation is storing result in mesh->runtime.mesh_eval, but this result
   * is not guaranteed to be owned by object.
   *
   * Check ownership now, since later on we can not go to a mesh owned by someone else via
   * object's runtime: this could cause access freed data on depsgraph destruction (mesh who owns
   * the final result might be freed prior to object). */
  Mesh *mesh = (Mesh *)ob.data;
  const bool is_mesh_eval_owned = (mesh_eval != mesh->runtime->mesh_eval);
  BKE_object_eval_assign_data(&ob, &mesh_eval->id, is_mesh_eval_owned);

  /* Add the final mesh as a non-owning component to the geometry set. */
  MeshComponent &mesh_component = geometry_set_eval->get_component_for_write<MeshComponent>();
  mesh_component.replace(mesh_eval, GeometryOwnershipType::Editable);
  ob.runtime->geometry_set_eval = geometry_set_eval;

  ob.runtime->mesh_deform_eval = mesh_deform_eval;
  ob.runtime->last_data_mask = dataMask;
  ob.runtime->last_need_mapping = need_mapping;

  /* Make sure that drivers can target shapekey properties.
   * Note that this causes a potential inconsistency, as the shapekey may have a
   * different topology than the evaluated mesh. */
  BLI_assert(mesh->key == nullptr || DEG_is_evaluated_id(&mesh->key->id));
  mesh_eval->key = mesh->key;

  if ((ob.mode & OB_MODE_ALL_SCULPT) && ob.sculpt) {
    if (DEG_is_active(&depsgraph)) {
      BKE_sculpt_update_object_after_eval(&depsgraph, &ob);
    }
  }

  mesh_build_extra_data(depsgraph, ob, *mesh_eval);
}

static void editbmesh_build_data(Depsgraph &depsgraph,
                                 const Scene &scene,
                                 Object &obedit,
                                 CustomData_MeshMasks &dataMask)
{
  Mesh *mesh = static_cast<Mesh *>(obedit.data);
  Mesh *me_cage;
  Mesh *me_final;
  GeometrySet *non_mesh_components;

  editbmesh_calc_modifiers(
      depsgraph, scene, obedit, dataMask, &me_cage, &me_final, &non_mesh_components);

  /* The modifier stack result is expected to share edit mesh pointer with the input.
   * This is similar `mesh_calc_finalize()`. */
  BKE_mesh_free_editmesh(me_final);
  BKE_mesh_free_editmesh(me_cage);
  me_final->runtime->edit_mesh = me_cage->runtime->edit_mesh = mesh->runtime->edit_mesh;

  /* Object has edit_mesh but is not in edit mode (object shares mesh datablock with another object
   * with is in edit mode).
   * Convert edit mesh to mesh until the draw manager can draw mesh wrapper which is not in the
   * edit mode. */
  if (!(obedit.mode & OB_MODE_EDIT)) {
    BKE_mesh_wrapper_ensure_mdata(me_final);
    if (me_final != me_cage) {
      BKE_mesh_wrapper_ensure_mdata(me_cage);
    }
  }

  const bool is_mesh_eval_owned = (me_final != mesh->runtime->mesh_eval);
  BKE_object_eval_assign_data(&obedit, &me_final->id, is_mesh_eval_owned);

  /* Make sure that drivers can target shapekey properties.
   * Note that this causes a potential inconsistency, as the shapekey may have a
   * different topology than the evaluated mesh. */
  BLI_assert(mesh->key == nullptr || DEG_is_evaluated_id(&mesh->key->id));
  me_final->key = mesh->key;

  obedit.runtime->editmesh_eval_cage = me_cage;

  obedit.runtime->geometry_set_eval = non_mesh_components;

  obedit.runtime->last_data_mask = dataMask;
}

static void object_get_datamask(const Depsgraph &depsgraph,
                                Object &ob,
                                CustomData_MeshMasks &r_mask,
                                bool *r_need_mapping)
{
  Scene *scene = DEG_get_evaluated_scene(&depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(&depsgraph);

  DEG_get_customdata_mask_for_object(&depsgraph, &ob, &r_mask);

  if (r_need_mapping) {
    *r_need_mapping = false;
  }

  /* Must never access original objects when dependency graph is not active: it might be already
   * freed. */
  if (!DEG_is_active(&depsgraph)) {
    return;
  }

  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *actob = BKE_view_layer_active_object_get(view_layer);
  if (actob) {
    actob = DEG_get_original_object(actob);
  }
  if (DEG_get_original_object(&ob) == actob) {
    bool editing = BKE_paint_select_face_test(actob);

    /* weight paint and face select need original indices because of selection buffer drawing */
    if (r_need_mapping) {
      *r_need_mapping = (editing || (ob.mode & (OB_MODE_WEIGHT_PAINT | OB_MODE_VERTEX_PAINT)));
    }

    /* Check if we need #MTFace & loop-color due to face select or texture paint. */
    if ((ob.mode & OB_MODE_TEXTURE_PAINT) || editing) {
      r_mask.lmask |= CD_MASK_PROP_FLOAT2 | CD_MASK_PROP_BYTE_COLOR;
      r_mask.fmask |= CD_MASK_MTFACE;
    }

    /* Check if we need loop-color due to vertex paint or weight-paint. */
    if (ob.mode & OB_MODE_VERTEX_PAINT) {
      r_mask.lmask |= CD_MASK_PROP_BYTE_COLOR;
    }

    if (ob.mode & OB_MODE_WEIGHT_PAINT) {
      r_mask.vmask |= CD_MASK_MDEFORMVERT;
    }

    if (ob.mode & OB_MODE_EDIT) {
      r_mask.vmask |= CD_MASK_MVERT_SKIN;
    }
  }
}

void mesh_data_update(Depsgraph &depsgraph,
                      const Scene &scene,
                      Object &ob,
                      const CustomData_MeshMasks &dataMask)
{
  BLI_assert(ob.type == OB_MESH);

  /* Evaluated meshes aren't supposed to be created on original instances. If you do,
   * they aren't cleaned up properly on mode switch, causing crashes, e.g #58150. */
  BLI_assert(ob.id.tag & LIB_TAG_COPIED_ON_EVAL);

  BKE_object_free_derived_caches(&ob);
  if (DEG_is_active(&depsgraph)) {
    BKE_sculpt_update_object_before_eval(&ob);
  }

  /* NOTE: Access the `edit_mesh` after freeing the derived caches, so that `ob.data` is restored
   * to the pre-evaluated state. This is because the evaluated state is not necessarily sharing the
   * `edit_mesh` pointer with the input. For example, if the object is first evaluated in the
   * object mode, and then user in another scene moves object to edit mode. */
  Mesh *mesh = static_cast<Mesh *>(ob.data);

  bool need_mapping;
  CustomData_MeshMasks cddata_masks = dataMask;
  object_get_datamask(depsgraph, ob, cddata_masks, &need_mapping);

  if (mesh->runtime->edit_mesh) {
    editbmesh_build_data(depsgraph, scene, ob, cddata_masks);
  }
  else {
    mesh_build_data(depsgraph, scene, ob, cddata_masks, need_mapping);
  }
}

Mesh *mesh_get_eval_deform(Depsgraph *depsgraph,
                           const Scene *scene,
                           Object *ob,
                           const CustomData_MeshMasks *dataMask)
{
  BMEditMesh *em = ((Mesh *)ob->data)->runtime->edit_mesh.get();
  if (em != nullptr) {
    /* There is no such a concept as deformed mesh in edit mode.
     * Explicitly disallow this request so that the evaluated result is not modified with evaluated
     * result from the wrong mode. */
    BLI_assert_msg(0, "Request of derformed mesh of object which is in edit mode");
    return nullptr;
  }

  /* This function isn't thread-safe and can't be used during evaluation. */
  BLI_assert(DEG_is_evaluating(depsgraph) == false);

  /* Evaluated meshes aren't supposed to be created on original instances. If you do,
   * they aren't cleaned up properly on mode switch, causing crashes, e.g #58150. */
  BLI_assert(ob->id.tag & LIB_TAG_COPIED_ON_EVAL);

  /* If there's no evaluated mesh or the last data mask used doesn't include
   * the data we need, rebuild the evaluated mesh. */
  bool need_mapping;

  CustomData_MeshMasks cddata_masks = *dataMask;
  object_get_datamask(*depsgraph, *ob, cddata_masks, &need_mapping);

  if (!ob->runtime->mesh_deform_eval ||
      !CustomData_MeshMasks_are_matching(&(ob->runtime->last_data_mask), &cddata_masks) ||
      (need_mapping && !ob->runtime->last_need_mapping))
  {
    CustomData_MeshMasks_update(&cddata_masks, &ob->runtime->last_data_mask);
    mesh_build_data(
        *depsgraph, *scene, *ob, cddata_masks, need_mapping || ob->runtime->last_need_mapping);
  }

  return ob->runtime->mesh_deform_eval;
}

Mesh *mesh_create_eval_final(Depsgraph *depsgraph,
                             const Scene *scene,
                             Object *ob,
                             const CustomData_MeshMasks *dataMask)
{
  Mesh *result;
  mesh_calc_modifiers(
      *depsgraph, *scene, *ob, true, false, *dataMask, false, false, nullptr, &result, nullptr);
  return result;
}

Mesh *mesh_create_eval_no_deform(Depsgraph *depsgraph,
                                 const Scene *scene,
                                 Object *ob,
                                 const CustomData_MeshMasks *dataMask)
{
  Mesh *result;
  mesh_calc_modifiers(
      *depsgraph, *scene, *ob, false, false, *dataMask, false, false, nullptr, &result, nullptr);
  return result;
}

Mesh *mesh_create_eval_no_deform_render(Depsgraph *depsgraph,
                                        const Scene *scene,
                                        Object *ob,
                                        const CustomData_MeshMasks *dataMask)
{
  Mesh *result;
  mesh_calc_modifiers(
      *depsgraph, *scene, *ob, false, false, *dataMask, false, false, nullptr, &result, nullptr);
  return result;
}

Mesh *editbmesh_get_eval_cage(Depsgraph *depsgraph,
                              const Scene *scene,
                              Object *obedit,
                              BMEditMesh * /*em*/,
                              const CustomData_MeshMasks *dataMask)
{
  CustomData_MeshMasks cddata_masks = *dataMask;

  /* If there's no evaluated mesh or the last data mask used doesn't include
   * the data we need, rebuild the evaluated mesh. */
  object_get_datamask(*depsgraph, *obedit, cddata_masks, nullptr);

  if (!obedit->runtime->editmesh_eval_cage ||
      !CustomData_MeshMasks_are_matching(&(obedit->runtime->last_data_mask), &cddata_masks))
  {
    editbmesh_build_data(*depsgraph, *scene, *obedit, cddata_masks);
  }

  return obedit->runtime->editmesh_eval_cage;
}

Mesh *editbmesh_get_eval_cage_from_orig(Depsgraph *depsgraph,
                                        const Scene *scene,
                                        Object *obedit,
                                        const CustomData_MeshMasks *dataMask)
{
  BLI_assert((obedit->id.tag & LIB_TAG_COPIED_ON_EVAL) == 0);
  const Scene *scene_eval = (const Scene *)DEG_get_evaluated_id(depsgraph, (ID *)&scene->id);
  Object *obedit_eval = (Object *)DEG_get_evaluated_id(depsgraph, &obedit->id);
  BMEditMesh *em_eval = BKE_editmesh_from_object(obedit_eval);
  return editbmesh_get_eval_cage(depsgraph, scene_eval, obedit_eval, em_eval, dataMask);
}

/* same as above but for vert coords */
struct MappedUserData {
  float (*vertexcos)[3];
  BLI_bitmap *vertex_visit;
};

static void make_vertexcos__mapFunc(void *user_data,
                                    int index,
                                    const float co[3],
                                    const float /*no*/[3])
{
  MappedUserData *mappedData = (MappedUserData *)user_data;

  if (BLI_BITMAP_TEST(mappedData->vertex_visit, index) == 0) {
    /* we need coord from prototype vertex, not from copies,
     * assume they stored in the beginning of vertex array stored in DM
     * (mirror modifier for eg does this) */
    copy_v3_v3(mappedData->vertexcos[index], co);
    BLI_BITMAP_ENABLE(mappedData->vertex_visit, index);
  }
}

void mesh_get_mapped_verts_coords(Mesh *mesh_eval, MutableSpan<float3> r_cos)
{
  if (mesh_eval->runtime->deformed_only == false) {
    MappedUserData user_data;
    r_cos.fill(float3(0));
    user_data.vertexcos = reinterpret_cast<float(*)[3]>(r_cos.data());
    user_data.vertex_visit = BLI_BITMAP_NEW(r_cos.size(), "vertexcos flags");
    BKE_mesh_foreach_mapped_vert(mesh_eval, make_vertexcos__mapFunc, &user_data, MESH_FOREACH_NOP);
    MEM_freeN(user_data.vertex_visit);
  }
  else {
    r_cos.copy_from(mesh_eval->vert_positions());
  }
}

static void mesh_init_origspace(Mesh &mesh)
{
  const float default_osf[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

  OrigSpaceLoop *lof_array = (OrigSpaceLoop *)CustomData_get_layer_for_write(
      &mesh.corner_data, CD_ORIGSPACE_MLOOP, mesh.corners_num);
  const Span<float3> positions = mesh.vert_positions();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  int j, k;

  Vector<float2, 64> vcos_2d;

  for (const int i : faces.index_range()) {
    const IndexRange face = faces[i];
    OrigSpaceLoop *lof = lof_array + face.start();

    if (ELEM(face.size(), 3, 4)) {
      for (j = 0; j < face.size(); j++, lof++) {
        copy_v2_v2(lof->uv, default_osf[j]);
      }
    }
    else {
      float co[3];
      float mat[3][3];

      float min[2] = {FLT_MAX, FLT_MAX}, max[2] = {-FLT_MAX, -FLT_MAX};
      float translate[2], scale[2];

      const float3 p_nor = mesh::face_normal_calc(positions, corner_verts.slice(face));

      axis_dominant_v3_to_m3(mat, p_nor);

      vcos_2d.resize(face.size());
      for (j = 0; j < face.size(); j++) {
        mul_v3_m3v3(co, mat, positions[corner_verts[face[j]]]);
        copy_v2_v2(vcos_2d[j], co);

        for (k = 0; k < 2; k++) {
          if (co[k] > max[k]) {
            max[k] = co[k];
          }
          else if (co[k] < min[k]) {
            min[k] = co[k];
          }
        }
      }

      /* Brings min to (0, 0). */
      negate_v2_v2(translate, min);

      /* Scale will bring max to (1, 1). */
      sub_v2_v2v2(scale, max, min);
      if (scale[0] == 0.0f) {
        scale[0] = 1e-9f;
      }
      if (scale[1] == 0.0f) {
        scale[1] = 1e-9f;
      }
      invert_v2(scale);

      /* Finally, transform all vcos_2d into ((0, 0), (1, 1))
       * square and assign them as origspace. */
      for (j = 0; j < face.size(); j++, lof++) {
        add_v2_v2v2(lof->uv, vcos_2d[j], translate);
        mul_v2_v2(lof->uv, scale);
      }
    }
  }

  BKE_mesh_tessface_clear(&mesh);
}

}  // namespace blender::bke
