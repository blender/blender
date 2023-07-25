/* SPDX-FileCopyrightText: 2005 Blender Foundation
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

#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_math_vector_types.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_DerivedMesh.h"
#include "BKE_bvhutils.h"
#include "BKE_colorband.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_iterators.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_tangent.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_paint.h"
#include "BKE_subdiv_modifier.h"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "BKE_shrinkwrap.h"
#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "CLG_log.h"

#ifdef WITH_OPENSUBDIV
#  include "DNA_userdef_types.h"
#endif

using blender::float3;
using blender::IndexRange;
using blender::Span;
using blender::VArray;
using blender::bke::GeometryOwnershipType;
using blender::bke::GeometrySet;
using blender::bke::MeshComponent;

/* very slow! enable for testing only! */
//#define USE_MODIFIER_VALIDATE

#ifdef USE_MODIFIER_VALIDATE
#  define ASSERT_IS_VALID_MESH(mesh) \
    (BLI_assert((mesh == nullptr) || (BKE_mesh_is_valid(mesh) == true)))
#else
#  define ASSERT_IS_VALID_MESH(mesh)
#endif

static void mesh_init_origspace(Mesh *mesh);
static void editbmesh_calc_modifier_final_normals(Mesh *mesh_final,
                                                  const CustomData_MeshMasks *final_datamask);
static void editbmesh_calc_modifier_final_normals_or_defer(
    Mesh *mesh_final, const CustomData_MeshMasks *final_datamask);

/* -------------------------------------------------------------------- */

static float *dm_getVertArray(DerivedMesh *dm)
{
  float(*positions)[3] = (float(*)[3])CustomData_get_layer_named_for_write(
      &dm->vertData, CD_PROP_FLOAT3, "position", dm->getNumVerts(dm));

  if (!positions) {
    positions = (float(*)[3])CustomData_add_layer_named(
        &dm->vertData, CD_PROP_FLOAT3, CD_SET_DEFAULT, dm->getNumVerts(dm), "position");
    CustomData_set_layer_flag(&dm->vertData, CD_PROP_FLOAT3, CD_FLAG_TEMPORARY);
    dm->copyVertArray(dm, positions);
  }

  return (float *)positions;
}

static vec2i *dm_getEdgeArray(DerivedMesh *dm)
{
  vec2i *edge = (vec2i *)CustomData_get_layer_named_for_write(
      &dm->edgeData, CD_PROP_INT32_2D, ".edge_verts", dm->getNumEdges(dm));

  if (!edge) {
    edge = (vec2i *)CustomData_add_layer_named(
        &dm->edgeData, CD_PROP_INT32_2D, CD_SET_DEFAULT, dm->getNumEdges(dm), ".edge_verts");
    CustomData_set_layer_flag(&dm->edgeData, CD_PROP_INT32_2D, CD_FLAG_TEMPORARY);
    dm->copyEdgeArray(dm, edge);
  }

  return edge;
}

static int *dm_getCornerVertArray(DerivedMesh *dm)
{
  int *corner_verts = (int *)CustomData_get_layer_named_for_write(
      &dm->loopData, CD_PROP_INT32, ".corner_vert", dm->getNumLoops(dm));

  if (!corner_verts) {
    corner_verts = (int *)CustomData_add_layer_named(
        &dm->loopData, CD_PROP_INT32, CD_SET_DEFAULT, dm->getNumLoops(dm), ".corner_vert");
    dm->copyCornerVertArray(dm, corner_verts);
  }

  return corner_verts;
}

static int *dm_getCornerEdgeArray(DerivedMesh *dm)
{
  int *corner_edges = (int *)CustomData_get_layer_named(
      &dm->loopData, CD_PROP_INT32, ".corner_edge");

  if (!corner_edges) {
    corner_edges = (int *)CustomData_add_layer_named(
        &dm->loopData, CD_PROP_INT32, CD_SET_DEFAULT, dm->getNumLoops(dm), ".corner_edge");
    dm->copyCornerEdgeArray(dm, corner_edges);
  }

  return corner_edges;
}

static int *dm_getPolyArray(DerivedMesh *dm)
{
  if (!dm->face_offsets) {
    dm->face_offsets = MEM_cnew_array<int>(dm->getNumPolys(dm) + 1, __func__);
    dm->copyPolyArray(dm, dm->face_offsets);
  }
  return dm->face_offsets;
}

void DM_init_funcs(DerivedMesh *dm)
{
  /* default function implementations */
  dm->getVertArray = dm_getVertArray;
  dm->getEdgeArray = dm_getEdgeArray;
  dm->getCornerVertArray = dm_getCornerVertArray;
  dm->getCornerEdgeArray = dm_getCornerEdgeArray;
  dm->getPolyArray = dm_getPolyArray;

  dm->getVertDataArray = DM_get_vert_data_layer;
  dm->getEdgeDataArray = DM_get_edge_data_layer;
  dm->getPolyDataArray = DM_get_poly_data_layer;
  dm->getLoopDataArray = DM_get_loop_data_layer;
}

void DM_init(DerivedMesh *dm,
             DerivedMeshType type,
             int numVerts,
             int numEdges,
             int numTessFaces,
             int numLoops,
             int numPolys)
{
  dm->type = type;
  dm->numVertData = numVerts;
  dm->numEdgeData = numEdges;
  dm->numTessFaceData = numTessFaces;
  dm->numLoopData = numLoops;
  dm->numPolyData = numPolys;

  DM_init_funcs(dm);

  /* Don't use #CustomData_reset because we don't want to touch custom-data. */
  copy_vn_i(dm->vertData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->edgeData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->faceData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->loopData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->polyData.typemap, CD_NUMTYPES, -1);
}

void DM_from_template(DerivedMesh *dm,
                      DerivedMesh *source,
                      DerivedMeshType type,
                      int numVerts,
                      int numEdges,
                      int numTessFaces,
                      int numLoops,
                      int numPolys)
{
  const CustomData_MeshMasks *mask = &CD_MASK_DERIVEDMESH;
  CustomData_copy_layout(&source->vertData, &dm->vertData, mask->vmask, CD_SET_DEFAULT, numVerts);
  CustomData_copy_layout(&source->edgeData, &dm->edgeData, mask->emask, CD_SET_DEFAULT, numEdges);
  CustomData_copy_layout(
      &source->faceData, &dm->faceData, mask->fmask, CD_SET_DEFAULT, numTessFaces);
  CustomData_copy_layout(&source->loopData, &dm->loopData, mask->lmask, CD_SET_DEFAULT, numLoops);
  CustomData_copy_layout(&source->polyData, &dm->polyData, mask->pmask, CD_SET_DEFAULT, numPolys);
  dm->face_offsets = static_cast<int *>(MEM_dupallocN(source->face_offsets));

  dm->type = type;
  dm->numVertData = numVerts;
  dm->numEdgeData = numEdges;
  dm->numTessFaceData = numTessFaces;
  dm->numLoopData = numLoops;
  dm->numPolyData = numPolys;

  DM_init_funcs(dm);
}

void DM_release(DerivedMesh *dm)
{
  CustomData_free(&dm->vertData, dm->numVertData);
  CustomData_free(&dm->edgeData, dm->numEdgeData);
  CustomData_free(&dm->faceData, dm->numTessFaceData);
  CustomData_free(&dm->loopData, dm->numLoopData);
  CustomData_free(&dm->polyData, dm->numPolyData);
  MEM_SAFE_FREE(dm->face_offsets);
}

void BKE_mesh_runtime_eval_to_meshkey(Mesh *me_deformed, Mesh *me, KeyBlock *kb)
{
  /* Just a shallow wrapper around #BKE_keyblock_convert_from_mesh,
   * that ensures both evaluated mesh and original one has same number of vertices. */

  const int totvert = me_deformed->totvert;

  if (totvert == 0 || me->totvert == 0 || me->totvert != totvert) {
    return;
  }

  BKE_keyblock_convert_from_mesh(me_deformed, me->key, kb);
}

void DM_set_only_copy(DerivedMesh *dm, const CustomData_MeshMasks *mask)
{
  CustomData_set_only_copy(&dm->vertData, mask->vmask);
  CustomData_set_only_copy(&dm->edgeData, mask->emask);
  CustomData_set_only_copy(&dm->faceData, mask->fmask);
  /* this wasn't in 2.63 and is disabled for 2.64 because it gives problems with
   * weight paint mode when there are modifiers applied, needs further investigation,
   * see replies to r50969, Campbell */
#if 0
  CustomData_set_only_copy(&dm->loopData, mask->lmask);
  Custom(&dm->polyData, mask->pmask);
#endif
}

static void mesh_set_only_copy(Mesh *mesh, const CustomData_MeshMasks *mask)
{
  CustomData_set_only_copy(&mesh->vdata, mask->vmask);
  CustomData_set_only_copy(&mesh->edata, mask->emask);
  CustomData_set_only_copy(&mesh->fdata_legacy, mask->fmask);
  /* this wasn't in 2.63 and is disabled for 2.64 because it gives problems with
   * weight paint mode when there are modifiers applied, needs further investigation,
   * see replies to r50969, Campbell */
#if 0
  CustomData_set_only_copy(&mesh->ldata, mask->lmask);
  CustomData_set_only_copy(&mesh->pdata, mask->pmask);
#endif
}

void *DM_get_vert_data_layer(DerivedMesh *dm, const eCustomDataType type)
{
  return CustomData_get_layer_for_write(&dm->vertData, type, dm->getNumVerts(dm));
}

void *DM_get_edge_data_layer(DerivedMesh *dm, const eCustomDataType type)
{
  return CustomData_get_layer_for_write(&dm->edgeData, type, dm->getNumEdges(dm));
}

void *DM_get_poly_data_layer(DerivedMesh *dm, const eCustomDataType type)
{
  return CustomData_get_layer_for_write(&dm->polyData, type, dm->getNumPolys(dm));
}

void *DM_get_loop_data_layer(DerivedMesh *dm, const eCustomDataType type)
{
  return CustomData_get_layer_for_write(&dm->loopData, type, dm->getNumLoops(dm));
}

void DM_copy_vert_data(
    const DerivedMesh *source, DerivedMesh *dest, int source_index, int dest_index, int count)
{
  CustomData_copy_data(&source->vertData, &dest->vertData, source_index, dest_index, count);
}

void DM_interp_vert_data(const DerivedMesh *source,
                         DerivedMesh *dest,
                         int *src_indices,
                         float *weights,
                         int count,
                         int dest_index)
{
  CustomData_interp(
      &source->vertData, &dest->vertData, src_indices, weights, nullptr, count, dest_index);
}

static float (*get_editbmesh_orco_verts(BMEditMesh *em))[3]
{
  BMIter iter;
  BMVert *eve;
  float(*orco)[3];
  int i;

  /* these may not really be the orco's, but it's only for preview.
   * could be solver better once, but isn't simple */

  orco = (float(*)[3])MEM_malloc_arrayN(em->bm->totvert, sizeof(float[3]), "BMEditMesh Orco");

  BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
    copy_v3_v3(orco[i], eve->co);
  }

  return orco;
}

/* orco custom data layer */
static float (*get_orco_coords(Object *ob, BMEditMesh *em, int layer, int *free))[3]
{
  *free = 0;

  if (layer == CD_ORCO) {
    /* get original coordinates */
    *free = 1;

    if (em) {
      return get_editbmesh_orco_verts(em);
    }
    return BKE_mesh_orco_verts_get(ob);
  }
  if (layer == CD_CLOTH_ORCO) {
    /* apply shape key for cloth, this should really be solved
     * by a more flexible customdata system, but not simple */
    if (!em) {
      ClothModifierData *clmd = (ClothModifierData *)BKE_modifiers_findby_type(
          ob, eModifierType_Cloth);
      if (clmd) {
        KeyBlock *kb = BKE_keyblock_from_key(BKE_key_from_object(ob),
                                             clmd->sim_parms->shapekey_rest);

        if (kb && kb->data) {
          return (float(*)[3])kb->data;
        }
      }
    }

    return nullptr;
  }

  return nullptr;
}

static Mesh *create_orco_mesh(Object *ob, Mesh *me, BMEditMesh *em, int layer)
{
  Mesh *mesh;
  float(*orco)[3];
  int free;

  if (em) {
    mesh = BKE_mesh_from_bmesh_for_eval_nomain(em->bm, nullptr, me);
    BKE_mesh_ensure_default_orig_index_customdata(mesh);
  }
  else {
    mesh = BKE_mesh_copy_for_eval(me);
  }

  orco = get_orco_coords(ob, em, layer, &free);

  if (orco) {
    BKE_mesh_vert_coords_apply(mesh, orco);
    if (free) {
      MEM_freeN(orco);
    }
  }

  return mesh;
}

static void add_orco_mesh(
    Object *ob, BMEditMesh *em, Mesh *mesh, Mesh *mesh_orco, const eCustomDataType layer)
{
  float(*orco)[3], (*layerorco)[3];
  int totvert, free;

  totvert = mesh->totvert;

  if (mesh_orco) {
    free = 1;

    if (mesh_orco->totvert == totvert) {
      orco = BKE_mesh_vert_coords_alloc(mesh_orco, nullptr);
    }
    else {
      orco = BKE_mesh_vert_coords_alloc(mesh, nullptr);
    }
  }
  else {
    /* TODO(sybren): totvert should potentially change here, as ob->data
     * or em may have a different number of vertices than dm. */
    orco = get_orco_coords(ob, em, layer, &free);
  }

  if (orco) {
    if (layer == CD_ORCO) {
      BKE_mesh_orco_verts_transform((Mesh *)ob->data, orco, totvert, 0);
    }

    layerorco = (float(*)[3])CustomData_get_layer_for_write(&mesh->vdata, layer, mesh->totvert);
    if (!layerorco) {
      layerorco = (float(*)[3])CustomData_add_layer(
          &mesh->vdata, eCustomDataType(layer), CD_SET_DEFAULT, mesh->totvert);
    }

    memcpy(layerorco, orco, sizeof(float[3]) * totvert);
    if (free) {
      MEM_freeN(orco);
    }
  }
}

static bool mesh_has_modifier_final_normals(const Mesh *mesh_input,
                                            const CustomData_MeshMasks *final_datamask,
                                            Mesh *mesh_final)
{
  /* Test if mesh has the required loop normals, in case an additional modifier
   * evaluation from another instance or from an operator requests it but the
   * initial normals were not loop normals. */
  const bool calc_loop_normals = ((mesh_input->flag & ME_AUTOSMOOTH) != 0 ||
                                  (final_datamask->lmask & CD_MASK_NORMAL) != 0);

  return (!calc_loop_normals || CustomData_has_layer(&mesh_final->ldata, CD_NORMAL));
}

static void mesh_calc_modifier_final_normals(const Mesh *mesh_input,
                                             const CustomData_MeshMasks *final_datamask,
                                             const bool sculpt_dyntopo,
                                             Mesh *mesh_final)
{
  /* Compute normals. */
  const bool calc_loop_normals = ((mesh_input->flag & ME_AUTOSMOOTH) != 0 ||
                                  (final_datamask->lmask & CD_MASK_NORMAL) != 0);

  /* Needed as `final_datamask` is not preserved outside modifier stack evaluation. */
  SubsurfRuntimeData *subsurf_runtime_data = mesh_final->runtime->subsurf_runtime_data;
  if (subsurf_runtime_data) {
    subsurf_runtime_data->calc_loop_normals = calc_loop_normals;
  }

  if (calc_loop_normals) {
    /* Compute loop normals (NOTE: will compute face and vert normals as well, if needed!). In case
     * of deferred CPU subdivision, this will be computed when the wrapper is generated. */
    if (!subsurf_runtime_data || subsurf_runtime_data->resolution == 0) {
      BKE_mesh_calc_normals_split(mesh_final);
    }
  }
  else {
    if (sculpt_dyntopo == false) {
      /* without this, drawing ngon tri's faces will show ugly tessellated face
       * normals and will also have to calculate normals on the fly, try avoid
       * this where possible since calculating face normals isn't fast,
       * note that this isn't a problem for subsurf (only quads) or edit-mode
       * which deals with drawing differently. */
      BKE_mesh_ensure_normals_for_display(mesh_final);
    }

    /* Some modifiers, like data-transfer, may generate those data as temp layer,
     * we do not want to keep them, as they are used by display code when available
     * (i.e. even if auto-smooth is disabled). */
    if (CustomData_has_layer(&mesh_final->ldata, CD_NORMAL)) {
      CustomData_free_layers(&mesh_final->ldata, CD_NORMAL, mesh_final->totloop);
    }
  }
}

/* Does final touches to the final evaluated mesh, making sure it is perfectly usable.
 *
 * This is needed because certain information is not passed along intermediate meshes allocated
 * during stack evaluation.
 */
static void mesh_calc_finalize(const Mesh *mesh_input, Mesh *mesh_eval)
{
  /* Make sure the name is the same. This is because mesh allocation from template does not
   * take care of naming. */
  STRNCPY(mesh_eval->id.name, mesh_input->id.name);
  /* Make evaluated mesh to share same edit mesh pointer as original and copied meshes. */
  mesh_eval->edit_mesh = mesh_input->edit_mesh;
}

void BKE_mesh_wrapper_deferred_finalize_mdata(Mesh *me_eval,
                                              const CustomData_MeshMasks *cd_mask_finalize)
{
  if (me_eval->runtime->wrapper_type_finalize & (1 << ME_WRAPPER_TYPE_BMESH)) {
    editbmesh_calc_modifier_final_normals(me_eval, cd_mask_finalize);
    me_eval->runtime->wrapper_type_finalize = eMeshWrapperType(
        me_eval->runtime->wrapper_type_finalize & ~(1 << ME_WRAPPER_TYPE_BMESH));
  }
  BLI_assert(me_eval->runtime->wrapper_type_finalize == 0);
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
  if (mti->modifyGeometrySet == nullptr) {
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
    mti->modifyGeometrySet(md, &mectx, &geometry_set);

    /* Release the mesh from the geometry set again. */
    if (geometry_set.has<MeshComponent>()) {
      MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
      if (mesh_component.get_for_read() != input_mesh) {
        /* Make sure the mesh component actually owns the mesh before taking over ownership. */
        mesh_component.ensure_owns_direct_data();
      }
      mesh_output = mesh_component.release();
    }

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
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const AttributeReader positions = attributes.lookup<float3>("position");
  attributes.remove("rest_position");
  if (positions) {
    if (positions.sharing_info && positions.varray.is_span()) {
      attributes.add<float3>("rest_position",
                             ATTR_DOMAIN_POINT,
                             AttributeInitShared(positions.varray.get_internal_span().data(),
                                                 *positions.sharing_info));
    }
    else {
      attributes.add<float3>(
          "rest_position", ATTR_DOMAIN_POINT, AttributeInitVArray(positions.varray));
    }
  }
}

static void mesh_calc_modifiers(Depsgraph *depsgraph,
                                const Scene *scene,
                                Object *ob,
                                const bool use_deform,
                                const bool need_mapping,
                                const CustomData_MeshMasks *dataMask,
                                const bool use_cache,
                                const bool allow_shared_mesh,
                                /* return args */
                                Mesh **r_deform,
                                Mesh **r_final,
                                GeometrySet **r_geometry_set)
{
  using namespace blender::bke;
  /* Input mesh shouldn't be modified. */
  Mesh *mesh_input = (Mesh *)ob->data;
  /* The final mesh is the result of calculating all enabled modifiers. */
  Mesh *mesh_final = nullptr;
  /* The result of calculating all leading deform modifiers. */
  Mesh *mesh_deform = nullptr;
  /* This geometry set contains the non-mesh data that might be generated by modifiers. */
  GeometrySet geometry_set_final;

  BLI_assert((mesh_input->id.tag & LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT) == 0);

  /* Mesh with constructive modifiers but no deformation applied. Tracked
   * along with final mesh if undeformed / orco coordinates are requested
   * for texturing. */
  Mesh *mesh_orco = nullptr;
  Mesh *mesh_orco_cloth = nullptr;

  /* Modifier evaluation modes. */
  const bool use_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const int required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;

  /* Sculpt can skip certain modifiers. */
  const bool has_multires = BKE_sculpt_multires_active(scene, ob) != nullptr;
  bool multires_applied = false;
  const bool sculpt_mode = ob->mode & OB_MODE_SCULPT && ob->sculpt && !use_render;
  const bool sculpt_dyntopo = (sculpt_mode && ob->sculpt->bm) && !use_render;

  /* Modifier evaluation contexts for different types of modifiers. */
  ModifierApplyFlag apply_render = use_render ? MOD_APPLY_RENDER : ModifierApplyFlag(0);
  ModifierApplyFlag apply_cache = use_cache ? MOD_APPLY_USECACHE : ModifierApplyFlag(0);
  const ModifierEvalContext mectx = {depsgraph, ob, apply_render | apply_cache};
  const ModifierEvalContext mectx_orco = {depsgraph, ob, apply_render | MOD_APPLY_ORCO};

  /* Get effective list of modifiers to execute. Some effects like shape keys
   * are added as virtual modifiers before the user created modifiers. */
  VirtualModifierData virtualModifierData;
  ModifierData *firstmd = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
  ModifierData *md = firstmd;

  /* Preview colors by modifiers such as dynamic paint, to show the results
   * even if the resulting data is not used in a material. Only in object mode.
   * TODO: this is broken, not drawn by the drawn manager. */
  const bool do_mod_mcol = (ob->mode == OB_MODE_OBJECT);
  ModifierData *previewmd = nullptr;
  CustomData_MeshMasks previewmask = {0};
  if (do_mod_mcol) {
    /* Find the last active modifier generating a preview, or nullptr if none. */
    /* XXX Currently, DPaint modifier just ignores this.
     *     Needs a stupid hack...
     *     The whole "modifier preview" thing has to be (re?)designed, anyway! */
    previewmd = BKE_modifier_get_last_preview(scene, md, required_mode);
  }

  /* Compute accumulated datamasks needed by each modifier. It helps to do
   * this fine grained so that for example vertex groups are preserved up to
   * an armature modifier, but not through a following subsurf modifier where
   * subdividing them is expensive. */
  CustomData_MeshMasks final_datamask = *dataMask;
  CDMaskLink *datamasks = BKE_modifier_calc_data_masks(
      scene, md, &final_datamask, required_mode, previewmd, &previewmask);
  CDMaskLink *md_datamask = datamasks;
  /* XXX Always copying POLYINDEX, else tessellated data are no more valid! */
  CustomData_MeshMasks append_mask = CD_MASK_BAREMESH_ORIGINDEX;

  /* Clear errors before evaluation. */
  BKE_modifiers_clear_errors(ob);

  if (ob->modifier_flag & OB_MODIFIER_FLAG_ADD_REST_POSITION) {
    if (mesh_final == nullptr) {
      mesh_final = BKE_mesh_copy_for_eval(mesh_input);
      ASSERT_IS_VALID_MESH(mesh_final);
    }
    set_rest_position(*mesh_final);
  }

  /* Apply all leading deform modifiers. */
  if (use_deform) {
    for (; md; md = md->next, md_datamask = md_datamask->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

      if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
        continue;
      }

      if (mti->type == eModifierTypeType_OnlyDeform && !sculpt_dyntopo) {
        blender::bke::ScopedModifierTimer modifier_timer{*md};
        if (!mesh_final) {
          mesh_final = BKE_mesh_copy_for_eval(mesh_input);
          ASSERT_IS_VALID_MESH(mesh_final);
        }
        BKE_modifier_deform_verts(
            md,
            &mectx,
            mesh_final,
            reinterpret_cast<float(*)[3]>(mesh_final->vert_positions_for_write().data()),
            mesh_final->totvert);
      }
      else {
        break;
      }
    }

    /* Result of all leading deforming modifiers is cached for
     * places that wish to use the original mesh but with deformed
     * coordinates (like vertex paint). */
    if (r_deform) {
      mesh_deform = BKE_mesh_copy_for_eval(mesh_final ? mesh_final : mesh_input);
    }
  }

  /* Apply all remaining constructive and deforming modifiers. */
  bool have_non_onlydeform_modifiers_applied = false;
  for (; md; md = md->next, md_datamask = md_datamask->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }

    if (mti->type == eModifierTypeType_OnlyDeform && !use_deform) {
      continue;
    }

    if ((mti->flags & eModifierTypeFlag_RequiresOriginalData) &&
        have_non_onlydeform_modifiers_applied) {
      BKE_modifier_set_error(ob, md, "Modifier requires original data, bad stack position");
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

      if (scene->toolsettings->sculpt->flags & SCULPT_ONLY_DEFORM) {
        unsupported |= (mti->type != eModifierTypeType_OnlyDeform);
      }

      unsupported |= multires_applied;

      if (unsupported) {
        if (sculpt_dyntopo) {
          BKE_modifier_set_error(ob, md, "Not supported in dyntopo");
        }
        else {
          BKE_modifier_set_error(ob, md, "Not supported in sculpt mode");
        }
        continue;
      }
    }

    if (need_mapping && !BKE_modifier_supports_mapping(md)) {
      continue;
    }

    blender::bke::ScopedModifierTimer modifier_timer{*md};

    /* Add orco mesh as layer if needed by this modifier. */
    if (mesh_final && mesh_orco && mti->requiredDataMask) {
      CustomData_MeshMasks mask = {0};
      mti->requiredDataMask(md, &mask);
      if (mask.vmask & CD_MASK_ORCO) {
        add_orco_mesh(ob, nullptr, mesh_final, mesh_orco, CD_ORCO);
      }
    }

    if (mti->type == eModifierTypeType_OnlyDeform) {
      if (!mesh_final) {
        mesh_final = BKE_mesh_copy_for_eval(mesh_input);
        ASSERT_IS_VALID_MESH(mesh_final);
      }
      BKE_modifier_deform_verts(
          md,
          &mectx,
          mesh_final,
          reinterpret_cast<float(*)[3]>(mesh_final->vert_positions_for_write().data()),
          mesh_final->totvert);
    }
    else {
      bool check_for_needs_mapping = false;
      if (mesh_final != nullptr) {
        if (have_non_onlydeform_modifiers_applied == false) {
          /* If we only deformed, we won't have initialized #CD_ORIGINDEX.
           * as this is the only part of the function that initializes mapping. */
          check_for_needs_mapping = true;
        }
      }
      else {
        mesh_final = BKE_mesh_copy_for_eval(mesh_input);
        ASSERT_IS_VALID_MESH(mesh_final);
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
            ((nextmask.vmask | nextmask.emask | nextmask.pmask) & CD_MASK_ORIGINDEX)) {
          /* calc */
          CustomData_add_layer(
              &mesh_final->vdata, CD_ORIGINDEX, CD_CONSTRUCT, mesh_final->totvert);
          CustomData_add_layer(
              &mesh_final->edata, CD_ORIGINDEX, CD_CONSTRUCT, mesh_final->totedge);
          CustomData_add_layer(
              &mesh_final->pdata, CD_ORIGINDEX, CD_CONSTRUCT, mesh_final->faces_num);

          /* Not worth parallelizing this,
           * gives less than 0.1% overall speedup in best of best cases... */
          range_vn_i((int *)CustomData_get_layer_for_write(
                         &mesh_final->vdata, CD_ORIGINDEX, mesh_final->totvert),
                     mesh_final->totvert,
                     0);
          range_vn_i((int *)CustomData_get_layer_for_write(
                         &mesh_final->edata, CD_ORIGINDEX, mesh_final->totedge),
                     mesh_final->totedge,
                     0);
          range_vn_i((int *)CustomData_get_layer_for_write(
                         &mesh_final->pdata, CD_ORIGINDEX, mesh_final->faces_num),
                     mesh_final->faces_num,
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
      mesh_set_only_copy(mesh_final, &mask);

      /* add cloth rest shape key if needed */
      if (mask.vmask & CD_MASK_CLOTH_ORCO) {
        add_orco_mesh(ob, nullptr, mesh_final, mesh_orco, CD_CLOTH_ORCO);
      }

      /* add an origspace layer if needed */
      if ((md_datamask->mask.lmask) & CD_MASK_ORIGSPACE_MLOOP) {
        if (!CustomData_has_layer(&mesh_final->ldata, CD_ORIGSPACE_MLOOP)) {
          CustomData_add_layer(
              &mesh_final->ldata, CD_ORIGSPACE_MLOOP, CD_SET_DEFAULT, mesh_final->totloop);
          mesh_init_origspace(mesh_final);
        }
      }

      Mesh *mesh_next = modifier_modify_mesh_and_geometry_set(
          md, mectx, mesh_final, geometry_set_final);
      ASSERT_IS_VALID_MESH(mesh_next);

      if (mesh_next) {
        /* if the modifier returned a new mesh, release the old one */
        if (mesh_final != mesh_next) {
          BLI_assert(mesh_final != mesh_input);
          BKE_id_free(nullptr, mesh_final);
        }
        mesh_final = mesh_next;
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

        if (mti->requiredDataMask != nullptr) {
          mti->requiredDataMask(md, &temp_cddata_masks);
        }
        CustomData_MeshMasks_update(&temp_cddata_masks, &nextmask);
        mesh_set_only_copy(mesh_orco, &temp_cddata_masks);

        mesh_next = BKE_modifier_modify_mesh(md, &mectx_orco, mesh_orco);
        ASSERT_IS_VALID_MESH(mesh_next);

        if (mesh_next) {
          /* if the modifier returned a new mesh, release the old one */
          if (mesh_orco != mesh_next) {
            BLI_assert(mesh_orco != mesh_input);
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
            BLI_assert(mesh_orco != mesh_input);
            BKE_id_free(nullptr, mesh_orco_cloth);
          }

          mesh_orco_cloth = mesh_next;
        }
      }

      /* in case of dynamic paint, make sure preview mask remains for following modifiers */
      /* XXX Temp and hackish solution! */
      if (md->type == eModifierType_DynamicPaint) {
        append_mask.lmask |= CD_MASK_PREVIEW_MLOOPCOL;
      }

      mesh_final->runtime->deformed_only = false;
    }

    if (sculpt_mode && md->type == eModifierType_Multires) {
      multires_applied = true;
    }
  }

  BLI_linklist_free((LinkNode *)datamasks, nullptr);

  for (md = firstmd; md; md = md->next) {
    BKE_modifier_free_temporary_data(md);
  }

  if (mesh_final == nullptr) {
    if (allow_shared_mesh) {
      mesh_final = mesh_input;
    }
    else {
      mesh_final = BKE_mesh_copy_for_eval(mesh_input);
    }
  }

  /* Denotes whether the object which the modifier stack came from owns the mesh or whether the
   * mesh is shared across multiple objects since there are no effective modifiers. */
  const bool is_own_mesh = (mesh_final != mesh_input);

  /* Add orco coordinates to final and deformed mesh if requested. */
  if (final_datamask.vmask & CD_MASK_ORCO) {
    /* No need in ORCO layer if the mesh was not deformed or modified: undeformed mesh in this case
     * matches input mesh. */
    if (is_own_mesh) {
      add_orco_mesh(ob, nullptr, mesh_final, mesh_orco, CD_ORCO);
    }

    if (mesh_deform) {
      add_orco_mesh(ob, nullptr, mesh_deform, nullptr, CD_ORCO);
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
  CustomData_free_layers(&mesh_final->vdata, CD_CLOTH_ORCO, mesh_final->totvert);

  /* Compute normals. */
  if (is_own_mesh) {
    mesh_calc_modifier_final_normals(mesh_input, &final_datamask, sculpt_dyntopo, mesh_final);
    mesh_calc_finalize(mesh_input, mesh_final);
  }
  else {
    blender::bke::MeshRuntime *runtime = mesh_input->runtime;
    if (runtime->mesh_eval == nullptr) {
      std::lock_guard lock{mesh_input->runtime->eval_mutex};
      if (runtime->mesh_eval == nullptr) {
        /* Not yet finalized by any instance, do it now
         * Isolate since computing normals is multithreaded and we are holding a lock. */
        blender::threading::isolate_task([&] {
          mesh_final = BKE_mesh_copy_for_eval(mesh_input);
          mesh_calc_modifier_final_normals(
              mesh_input, &final_datamask, sculpt_dyntopo, mesh_final);
          mesh_calc_finalize(mesh_input, mesh_final);
          runtime->mesh_eval = mesh_final;
        });
      }
      else {
        /* Already finalized by another instance, reuse. */
        mesh_final = runtime->mesh_eval;
      }
    }
    else if (!mesh_has_modifier_final_normals(mesh_input, &final_datamask, runtime->mesh_eval)) {
      /* Modifier stack was (re-)evaluated with a request for additional normals
       * different than the instanced mesh, can't instance anymore now. */
      mesh_final = BKE_mesh_copy_for_eval(mesh_input);
      mesh_calc_modifier_final_normals(mesh_input, &final_datamask, sculpt_dyntopo, mesh_final);
      mesh_calc_finalize(mesh_input, mesh_final);
    }
    else {
      /* Already finalized by another instance, reuse. */
      mesh_final = runtime->mesh_eval;
    }
  }

  /* Return final mesh */
  *r_final = mesh_final;
  if (r_deform) {
    *r_deform = mesh_deform;
  }
  if (r_geometry_set) {
    *r_geometry_set = new GeometrySet(std::move(geometry_set_final));
  }
}

static blender::Array<float3> editbmesh_vert_coords_alloc(const BMEditMesh *em)
{
  blender::Array<float3> cos(em->bm->totvert);
  BMIter iter;
  BMVert *eve;
  int i;
  BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
    cos[i] = eve->co;
  }
  return cos;
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

static void editbmesh_calc_modifier_final_normals(Mesh *mesh_final,
                                                  const CustomData_MeshMasks *final_datamask)
{
  const bool calc_loop_normals = ((mesh_final->flag & ME_AUTOSMOOTH) != 0 ||
                                  (final_datamask->lmask & CD_MASK_NORMAL) != 0);

  SubsurfRuntimeData *subsurf_runtime_data = mesh_final->runtime->subsurf_runtime_data;
  if (subsurf_runtime_data) {
    subsurf_runtime_data->calc_loop_normals = calc_loop_normals;
  }

  if (calc_loop_normals) {
    /* Compute loop normals. In case of deferred CPU subdivision, this will be computed when the
     * wrapper is generated. */
    if (!subsurf_runtime_data || subsurf_runtime_data->resolution == 0) {
      BKE_mesh_calc_normals_split(mesh_final);
    }
  }
  else {
    /* Same as #mesh_calc_modifiers.
     * If using loop normals, face normals have already been computed. */
    BKE_mesh_ensure_normals_for_display(mesh_final);

    /* Some modifiers, like data-transfer, may generate those data, we do not want to keep them,
     * as they are used by display code when available (i.e. even if auto-smooth is disabled). */
    if (CustomData_has_layer(&mesh_final->ldata, CD_NORMAL)) {
      CustomData_free_layers(&mesh_final->ldata, CD_NORMAL, mesh_final->totloop);
    }
  }
}

static void editbmesh_calc_modifier_final_normals_or_defer(
    Mesh *mesh_final, const CustomData_MeshMasks *final_datamask)
{
  if (mesh_final->runtime->wrapper_type != ME_WRAPPER_TYPE_MDATA) {
    /* Generated at draw time. */
    mesh_final->runtime->wrapper_type_finalize = eMeshWrapperType(
        1 << mesh_final->runtime->wrapper_type);
    return;
  }

  editbmesh_calc_modifier_final_normals(mesh_final, final_datamask);
}

static blender::MutableSpan<float3> mesh_wrapper_vert_coords_ensure_for_write(Mesh *mesh)
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      if (mesh->runtime->edit_data->vertexCos.is_empty()) {
        mesh->runtime->edit_data->vertexCos = editbmesh_vert_coords_alloc(mesh->edit_mesh);
      }
      return mesh->runtime->edit_data->vertexCos;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return mesh->vert_positions_for_write();
  }
  BLI_assert_unreachable();
  return {};
}

static void editbmesh_calc_modifiers(Depsgraph *depsgraph,
                                     const Scene *scene,
                                     Object *ob,
                                     BMEditMesh *em_input,
                                     const CustomData_MeshMasks *dataMask,
                                     /* return args */
                                     Mesh **r_cage,
                                     Mesh **r_final,
                                     GeometrySet **r_geometry_set)
{
  Mesh *mesh_input = (Mesh *)ob->data;
  Mesh *mesh_cage = nullptr;
  /* This geometry set contains the non-mesh data that might be generated by modifiers. */
  GeometrySet geometry_set_final;

  /* Mesh with constructive modifiers but no deformation applied. Tracked
   * along with final mesh if undeformed / orco coordinates are requested
   * for texturing. */
  Mesh *mesh_orco = nullptr;

  /* Modifier evaluation modes. */
  const int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;

  const bool use_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  /* Modifier evaluation contexts for different types of modifiers. */
  ModifierApplyFlag apply_render = use_render ? MOD_APPLY_RENDER : ModifierApplyFlag(0);
  const ModifierEvalContext mectx = {depsgraph, ob, MOD_APPLY_USECACHE | apply_render};
  const ModifierEvalContext mectx_orco = {depsgraph, ob, MOD_APPLY_ORCO};

  /* Get effective list of modifiers to execute. Some effects like shape keys
   * are added as virtual modifiers before the user created modifiers. */
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);

  /* Compute accumulated datamasks needed by each modifier. It helps to do
   * this fine grained so that for example vertex groups are preserved up to
   * an armature modifier, but not through a following subsurf modifier where
   * subdividing them is expensive. */
  CustomData_MeshMasks final_datamask = *dataMask;
  CDMaskLink *datamasks = BKE_modifier_calc_data_masks(
      scene, md, &final_datamask, required_mode, nullptr, nullptr);
  CDMaskLink *md_datamask = datamasks;
  CustomData_MeshMasks append_mask = CD_MASK_BAREMESH;

  Mesh *mesh_final = BKE_mesh_wrapper_from_editmesh(em_input, &final_datamask, mesh_input);

  int cageIndex = BKE_modifiers_get_cage_index(scene, ob, nullptr, true);
  if (r_cage && cageIndex == -1) {
    mesh_cage = mesh_final;
  }

  /* The mesh from edit mode should not have any original index layers already, since those
   * are added during evaluation when necessary and are redundant on an original mesh. */
  BLI_assert(CustomData_get_layer(&em_input->bm->pdata, CD_ORIGINDEX) == nullptr &&
             CustomData_get_layer(&em_input->bm->edata, CD_ORIGINDEX) == nullptr &&
             CustomData_get_layer(&em_input->bm->pdata, CD_ORIGINDEX) == nullptr);

  /* Clear errors before evaluation. */
  BKE_modifiers_clear_errors(ob);

  if (ob->modifier_flag & OB_MODIFIER_FLAG_ADD_REST_POSITION) {
    BKE_mesh_wrapper_ensure_mdata(mesh_final);
    set_rest_position(*mesh_final);
  }

  bool non_deform_modifier_applied = false;
  for (int i = 0; md; i++, md = md->next, md_datamask = md_datamask->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);
    if (!editbmesh_modifier_is_enabled(scene, ob, md, non_deform_modifier_applied)) {
      continue;
    }

    blender::bke::ScopedModifierTimer modifier_timer{*md};

    /* Add an orco mesh as layer if needed by this modifier. */
    if (mesh_orco && mti->requiredDataMask) {
      CustomData_MeshMasks mask = {0};
      mti->requiredDataMask(md, &mask);
      if (mask.vmask & CD_MASK_ORCO) {
        add_orco_mesh(ob, em_input, mesh_final, mesh_orco, CD_ORCO);
      }
    }

    if (mesh_final == mesh_cage) {
      /* If the cage mesh has already been assigned, we have passed the cage index in the modifier
       * list. If the cage and final meshes are still the same, duplicate the final mesh so the
       * cage mesh isn't modified anymore. */
      mesh_final = BKE_mesh_copy_for_eval(mesh_final);
      if (mesh_cage->edit_mesh) {
        mesh_final->edit_mesh = static_cast<BMEditMesh *>(MEM_dupallocN(mesh_cage->edit_mesh));
        mesh_final->edit_mesh->is_shallow_copy = true;
        mesh_final->runtime->is_original_bmesh = true;
        BKE_mesh_runtime_ensure_edit_data(mesh_final);
      }
    }

    if (mti->type == eModifierTypeType_OnlyDeform) {
      if (mti->deformVertsEM) {
        BKE_modifier_deform_vertsEM(
            md,
            &mectx,
            em_input,
            mesh_final,
            reinterpret_cast<float(*)[3]>(
                mesh_wrapper_vert_coords_ensure_for_write(mesh_final).data()),
            BKE_mesh_wrapper_vert_len(mesh_final));
        BKE_mesh_wrapper_tag_positions_changed(mesh_final);
      }
      else {
        BKE_mesh_wrapper_ensure_mdata(mesh_final);
        BKE_modifier_deform_verts(md,
                                  &mectx,
                                  mesh_final,
                                  BKE_mesh_vert_positions_for_write(mesh_final),
                                  mesh_final->totvert);
        BKE_mesh_tag_positions_changed(mesh_final);
      }
    }
    else {
      non_deform_modifier_applied = true;

      /* create an orco derivedmesh in parallel */
      CustomData_MeshMasks mask = md_datamask->mask;
      if (mask.vmask & CD_MASK_ORCO) {
        if (!mesh_orco) {
          mesh_orco = create_orco_mesh(ob, mesh_input, em_input, CD_ORCO);
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

      mesh_set_only_copy(mesh_final, &mask);

      if (mask.lmask & CD_MASK_ORIGSPACE_MLOOP) {
        if (!CustomData_has_layer(&mesh_final->ldata, CD_ORIGSPACE_MLOOP)) {
          CustomData_add_layer(
              &mesh_final->ldata, CD_ORIGSPACE_MLOOP, CD_SET_DEFAULT, mesh_final->totloop);
          mesh_init_origspace(mesh_final);
        }
      }

      Mesh *mesh_next = modifier_modify_mesh_and_geometry_set(
          md, mectx, mesh_final, geometry_set_final);
      ASSERT_IS_VALID_MESH(mesh_next);

      if (mesh_next) {
        if (mesh_final != mesh_next) {
          BKE_id_free(nullptr, mesh_final);
        }
        mesh_final = mesh_next;
      }
      mesh_final->runtime->deformed_only = false;
    }

    if (r_cage && i == cageIndex) {
      mesh_cage = mesh_final;
    }
  }

  BLI_linklist_free((LinkNode *)datamasks, nullptr);

  /* Add orco coordinates to final and deformed mesh if requested. */
  if (final_datamask.vmask & CD_MASK_ORCO) {
    /* FIXME(@ideasman42): avoid the need to convert to mesh data just to add an orco layer. */
    BKE_mesh_wrapper_ensure_mdata(mesh_final);

    add_orco_mesh(ob, em_input, mesh_final, mesh_orco, CD_ORCO);
  }

  if (mesh_orco) {
    BKE_id_free(nullptr, mesh_orco);
  }

  /* Compute normals. */
  editbmesh_calc_modifier_final_normals_or_defer(mesh_final, &final_datamask);
  if (mesh_cage && (mesh_cage != mesh_final)) {
    editbmesh_calc_modifier_final_normals_or_defer(mesh_cage, &final_datamask);
  }

  /* Return final mesh. */
  *r_final = mesh_final;
  if (r_cage) {
    *r_cage = mesh_cage;
  }
  if (r_geometry_set) {
    *r_geometry_set = new GeometrySet(std::move(geometry_set_final));
  }
}

static void mesh_build_extra_data(Depsgraph *depsgraph, Object *ob, Mesh *mesh_eval)
{
  uint32_t eval_flags = DEG_get_eval_flags_for_id(depsgraph, &ob->id);

  if (eval_flags & DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY) {
    BKE_shrinkwrap_compute_boundary_data(mesh_eval);
  }
}

static void mesh_build_data(Depsgraph *depsgraph,
                            const Scene *scene,
                            Object *ob,
                            const CustomData_MeshMasks *dataMask,
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
  Mesh *mesh = (Mesh *)ob->data;
  const bool is_mesh_eval_owned = (mesh_eval != mesh->runtime->mesh_eval);
  BKE_object_eval_assign_data(ob, &mesh_eval->id, is_mesh_eval_owned);

  /* Add the final mesh as a non-owning component to the geometry set. */
  MeshComponent &mesh_component = geometry_set_eval->get_component_for_write<MeshComponent>();
  mesh_component.replace(mesh_eval, GeometryOwnershipType::Editable);
  ob->runtime.geometry_set_eval = geometry_set_eval;

  ob->runtime.mesh_deform_eval = mesh_deform_eval;
  ob->runtime.last_data_mask = *dataMask;
  ob->runtime.last_need_mapping = need_mapping;

  BKE_object_boundbox_calc_from_mesh(ob, mesh_eval);

  /* Make sure that drivers can target shapekey properties.
   * Note that this causes a potential inconsistency, as the shapekey may have a
   * different topology than the evaluated mesh. */
  BLI_assert(mesh->key == nullptr || DEG_is_evaluated_id(&mesh->key->id));
  mesh_eval->key = mesh->key;

  if ((ob->mode & OB_MODE_ALL_SCULPT) && ob->sculpt) {
    if (DEG_is_active(depsgraph)) {
      BKE_sculpt_update_object_after_eval(depsgraph, ob);
    }
  }

  mesh_build_extra_data(depsgraph, ob, mesh_eval);
}

static void editbmesh_build_data(Depsgraph *depsgraph,
                                 const Scene *scene,
                                 Object *obedit,
                                 BMEditMesh *em,
                                 CustomData_MeshMasks *dataMask)
{
  Mesh *mesh = static_cast<Mesh *>(obedit->data);
  Mesh *me_cage;
  Mesh *me_final;
  GeometrySet *non_mesh_components;

  editbmesh_calc_modifiers(
      depsgraph, scene, obedit, em, dataMask, &me_cage, &me_final, &non_mesh_components);

  /* The modifier stack result is expected to share edit mesh pointer with the input.
   * This is similar `mesh_calc_finalize()`. */
  BKE_mesh_free_editmesh(me_final);
  BKE_mesh_free_editmesh(me_cage);
  me_final->edit_mesh = me_cage->edit_mesh = em;

  /* Object has edit_mesh but is not in edit mode (object shares mesh datablock with another object
   * with is in edit mode).
   * Convert edit mesh to mesh until the draw manager can draw mesh wrapper which is not in the
   * edit mode. */
  if (!(obedit->mode & OB_MODE_EDIT)) {
    BKE_mesh_wrapper_ensure_mdata(me_final);
    if (me_final != me_cage) {
      BKE_mesh_wrapper_ensure_mdata(me_cage);
    }
  }

  const bool is_mesh_eval_owned = (me_final != mesh->runtime->mesh_eval);
  BKE_object_eval_assign_data(obedit, &me_final->id, is_mesh_eval_owned);

  /* Make sure that drivers can target shapekey properties.
   * Note that this causes a potential inconsistency, as the shapekey may have a
   * different topology than the evaluated mesh. */
  BLI_assert(mesh->key == nullptr || DEG_is_evaluated_id(&mesh->key->id));
  me_final->key = mesh->key;

  obedit->runtime.editmesh_eval_cage = me_cage;

  obedit->runtime.geometry_set_eval = non_mesh_components;

  BKE_object_boundbox_calc_from_mesh(obedit, me_final);

  obedit->runtime.last_data_mask = *dataMask;
}

static void object_get_datamask(const Depsgraph *depsgraph,
                                Object *ob,
                                CustomData_MeshMasks *r_mask,
                                bool *r_need_mapping)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

  DEG_get_customdata_mask_for_object(depsgraph, ob, r_mask);

  if (r_need_mapping) {
    *r_need_mapping = false;
  }

  /* Must never access original objects when dependency graph is not active: it might be already
   * freed. */
  if (!DEG_is_active(depsgraph)) {
    return;
  }

  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *actob = BKE_view_layer_active_object_get(view_layer);
  if (actob) {
    actob = DEG_get_original_object(actob);
  }
  if (DEG_get_original_object(ob) == actob) {
    bool editing = BKE_paint_select_face_test(actob);

    /* weight paint and face select need original indices because of selection buffer drawing */
    if (r_need_mapping) {
      *r_need_mapping = (editing || (ob->mode & (OB_MODE_WEIGHT_PAINT | OB_MODE_VERTEX_PAINT)));
    }

    /* check if we need tfaces & mcols due to face select or texture paint */
    if ((ob->mode & OB_MODE_TEXTURE_PAINT) || editing) {
      r_mask->lmask |= CD_MASK_PROP_FLOAT2 | CD_MASK_PROP_BYTE_COLOR;
      r_mask->fmask |= CD_MASK_MTFACE;
    }

    /* check if we need mcols due to vertex paint or weightpaint */
    if (ob->mode & OB_MODE_VERTEX_PAINT) {
      r_mask->lmask |= CD_MASK_PROP_BYTE_COLOR;
    }

    if (ob->mode & OB_MODE_WEIGHT_PAINT) {
      r_mask->vmask |= CD_MASK_MDEFORMVERT;
    }

    if (ob->mode & OB_MODE_EDIT) {
      r_mask->vmask |= CD_MASK_MVERT_SKIN;
    }
  }
}

void makeDerivedMesh(Depsgraph *depsgraph,
                     const Scene *scene,
                     Object *ob,
                     const CustomData_MeshMasks *dataMask)
{
  BLI_assert(ob->type == OB_MESH);

  /* Evaluated meshes aren't supposed to be created on original instances. If you do,
   * they aren't cleaned up properly on mode switch, causing crashes, e.g #58150. */
  BLI_assert(ob->id.tag & LIB_TAG_COPIED_ON_WRITE);

  BKE_object_free_derived_caches(ob);
  if (DEG_is_active(depsgraph)) {
    BKE_sculpt_update_object_before_eval(ob);
  }

  /* NOTE: Access the `edit_mesh` after freeing the derived caches, so that `ob->data` is restored
   * to the pre-evaluated state. This is because the evaluated state is not necessarily sharing the
   * `edit_mesh` pointer with the input. For example, if the object is first evaluated in the
   * object mode, and then user in another scene moves object to edit mode. */
  BMEditMesh *em = ((Mesh *)ob->data)->edit_mesh;

  bool need_mapping;
  CustomData_MeshMasks cddata_masks = *dataMask;
  object_get_datamask(depsgraph, ob, &cddata_masks, &need_mapping);

  if (em) {
    editbmesh_build_data(depsgraph, scene, ob, em, &cddata_masks);
  }
  else {
    mesh_build_data(depsgraph, scene, ob, &cddata_masks, need_mapping);
  }
}

/***/

Mesh *mesh_get_eval_deform(Depsgraph *depsgraph,
                           const Scene *scene,
                           Object *ob,
                           const CustomData_MeshMasks *dataMask)
{
  BMEditMesh *em = ((Mesh *)ob->data)->edit_mesh;
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
  BLI_assert(ob->id.tag & LIB_TAG_COPIED_ON_WRITE);

  /* if there's no derived mesh or the last data mask used doesn't include
   * the data we need, rebuild the derived mesh
   */
  bool need_mapping;

  CustomData_MeshMasks cddata_masks = *dataMask;
  object_get_datamask(depsgraph, ob, &cddata_masks, &need_mapping);

  if (!ob->runtime.mesh_deform_eval ||
      !CustomData_MeshMasks_are_matching(&(ob->runtime.last_data_mask), &cddata_masks) ||
      (need_mapping && !ob->runtime.last_need_mapping))
  {
    CustomData_MeshMasks_update(&cddata_masks, &ob->runtime.last_data_mask);
    mesh_build_data(
        depsgraph, scene, ob, &cddata_masks, need_mapping || ob->runtime.last_need_mapping);
  }

  return ob->runtime.mesh_deform_eval;
}

Mesh *mesh_create_eval_final(Depsgraph *depsgraph,
                             const Scene *scene,
                             Object *ob,
                             const CustomData_MeshMasks *dataMask)
{
  Mesh *result;
  mesh_calc_modifiers(
      depsgraph, scene, ob, true, false, dataMask, false, false, nullptr, &result, nullptr);
  return result;
}

Mesh *mesh_create_eval_no_deform(Depsgraph *depsgraph,
                                 const Scene *scene,
                                 Object *ob,
                                 const CustomData_MeshMasks *dataMask)
{
  Mesh *result;
  mesh_calc_modifiers(
      depsgraph, scene, ob, false, false, dataMask, false, false, nullptr, &result, nullptr);
  return result;
}

Mesh *mesh_create_eval_no_deform_render(Depsgraph *depsgraph,
                                        const Scene *scene,
                                        Object *ob,
                                        const CustomData_MeshMasks *dataMask)
{
  Mesh *result;
  mesh_calc_modifiers(
      depsgraph, scene, ob, false, false, dataMask, false, false, nullptr, &result, nullptr);
  return result;
}

/***/

Mesh *editbmesh_get_eval_cage(Depsgraph *depsgraph,
                              const Scene *scene,
                              Object *obedit,
                              BMEditMesh *em,
                              const CustomData_MeshMasks *dataMask)
{
  CustomData_MeshMasks cddata_masks = *dataMask;

  /* if there's no derived mesh or the last data mask used doesn't include
   * the data we need, rebuild the derived mesh
   */
  object_get_datamask(depsgraph, obedit, &cddata_masks, nullptr);

  if (!obedit->runtime.editmesh_eval_cage ||
      !CustomData_MeshMasks_are_matching(&(obedit->runtime.last_data_mask), &cddata_masks))
  {
    editbmesh_build_data(depsgraph, scene, obedit, em, &cddata_masks);
  }

  return obedit->runtime.editmesh_eval_cage;
}

Mesh *editbmesh_get_eval_cage_from_orig(Depsgraph *depsgraph,
                                        const Scene *scene,
                                        Object *obedit,
                                        const CustomData_MeshMasks *dataMask)
{
  BLI_assert((obedit->id.tag & LIB_TAG_COPIED_ON_WRITE) == 0);
  const Scene *scene_eval = (const Scene *)DEG_get_evaluated_id(depsgraph, (ID *)&scene->id);
  Object *obedit_eval = (Object *)DEG_get_evaluated_id(depsgraph, &obedit->id);
  BMEditMesh *em_eval = BKE_editmesh_from_object(obedit_eval);
  return editbmesh_get_eval_cage(depsgraph, scene_eval, obedit_eval, em_eval, dataMask);
}

/***/

/* same as above but for vert coords */
struct MappedUserData {
  float (*vertexcos)[3];
  BLI_bitmap *vertex_visit;
};

static void make_vertexcos__mapFunc(void *userData,
                                    int index,
                                    const float co[3],
                                    const float /*no*/[3])
{
  MappedUserData *mappedData = (MappedUserData *)userData;

  if (BLI_BITMAP_TEST(mappedData->vertex_visit, index) == 0) {
    /* we need coord from prototype vertex, not from copies,
     * assume they stored in the beginning of vertex array stored in DM
     * (mirror modifier for eg does this) */
    copy_v3_v3(mappedData->vertexcos[index], co);
    BLI_BITMAP_ENABLE(mappedData->vertex_visit, index);
  }
}

void mesh_get_mapped_verts_coords(Mesh *me_eval, float (*r_cos)[3], const int totcos)
{
  if (me_eval->runtime->deformed_only == false) {
    MappedUserData userData;
    memset(r_cos, 0, sizeof(*r_cos) * totcos);
    userData.vertexcos = r_cos;
    userData.vertex_visit = BLI_BITMAP_NEW(totcos, "vertexcos flags");
    BKE_mesh_foreach_mapped_vert(me_eval, make_vertexcos__mapFunc, &userData, MESH_FOREACH_NOP);
    MEM_freeN(userData.vertex_visit);
  }
  else {
    const Span<float3> positions = me_eval->vert_positions();
    for (int i = 0; i < totcos; i++) {
      copy_v3_v3(r_cos[i], positions[i]);
    }
  }
}

static void mesh_init_origspace(Mesh *mesh)
{
  const float default_osf[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

  OrigSpaceLoop *lof_array = (OrigSpaceLoop *)CustomData_get_layer_for_write(
      &mesh->ldata, CD_ORIGSPACE_MLOOP, mesh->totloop);
  const Span<float3> positions = mesh->vert_positions();
  const blender::OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  int j, k;

  blender::Vector<blender::float2, 64> vcos_2d;

  for (const int i : faces.index_range()) {
    const blender::IndexRange face = faces[i];
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

      const float3 p_nor = blender::bke::mesh::face_normal_calc(positions,
                                                                corner_verts.slice(face));

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

  BKE_mesh_tessface_clear(mesh);
}
