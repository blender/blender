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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */
#include "usd_writer_mesh.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include "BLI_assert.h"
#include "BLI_math_vector.h"

#include "BKE_customdata.h"
#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"

#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_particle_types.h"

namespace blender {
namespace io {
namespace usd {

USDGenericMeshWriter::USDGenericMeshWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx)
{
}

bool USDGenericMeshWriter::is_supported(const HierarchyContext *context) const
{
  Object *object = context->object;
  bool is_dupli = context->duplicator != nullptr;
  int base_flag;

  if (is_dupli) {
    /* Construct the object's base flags from its dupli-parent, just like is done in
     * deg_objects_dupli_iterator_next(). Without this, the visibility check below will fail. Doing
     * this here, instead of a more suitable location in AbstractHierarchyIterator, prevents
     * copying the Object for every dupli. */
    base_flag = object->base_flag;
    object->base_flag = context->duplicator->base_flag | BASE_FROM_DUPLI;
  }

  int visibility = BKE_object_visibility(object,
                                         usd_export_context_.export_params.evaluation_mode);

  if (is_dupli) {
    object->base_flag = base_flag;
  }

  return (visibility & OB_VISIBLE_SELF) != 0;
}

void USDGenericMeshWriter::do_write(HierarchyContext &context)
{
  Object *object_eval = context.object;
  bool needsfree = false;
  Mesh *mesh = get_export_mesh(object_eval, needsfree);

  if (mesh == nullptr) {
    return;
  }

  try {
    write_mesh(context, mesh);

    if (needsfree) {
      free_export_mesh(mesh);
    }
  }
  catch (...) {
    if (needsfree) {
      free_export_mesh(mesh);
    }
    throw;
  }
}

void USDGenericMeshWriter::free_export_mesh(Mesh *mesh)
{
  BKE_id_free(nullptr, mesh);
}

struct USDMeshData {
  pxr::VtArray<pxr::GfVec3f> points;
  pxr::VtIntArray face_vertex_counts;
  pxr::VtIntArray face_indices;
  std::map<short, pxr::VtIntArray> face_groups;

  /* The length of this array specifies the number of creases on the surface. Each element gives
   * the number of (must be adjacent) vertices in each crease, whose indices are linearly laid out
   * in the 'creaseIndices' attribute. Since each crease must be at least one edge long, each
   * element of this array should be greater than one. */
  pxr::VtIntArray crease_lengths;
  /* The indices of all vertices forming creased edges. The size of this array must be equal to the
   * sum of all elements of the 'creaseLengths' attribute. */
  pxr::VtIntArray crease_vertex_indices;
  /* The per-crease or per-edge sharpness for all creases (Usd.Mesh.SHARPNESS_INFINITE for a
   * perfectly sharp crease). Since 'creaseLengths' encodes the number of vertices in each crease,
   * the number of elements in this array will be either len(creaseLengths) or the sum over all X
   * of (creaseLengths[X] - 1). Note that while the RI spec allows each crease to have either a
   * single sharpness or a value per-edge, USD will encode either a single sharpness per crease on
   * a mesh, or sharpnesses for all edges making up the creases on a mesh. */
  pxr::VtFloatArray crease_sharpnesses;
};

void USDGenericMeshWriter::write_uv_maps(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh)
{
  pxr::UsdTimeCode timecode = get_export_time_code();

  const CustomData *ldata = &mesh->ldata;
  for (int layer_idx = 0; layer_idx < ldata->totlayer; layer_idx++) {
    const CustomDataLayer *layer = &ldata->layers[layer_idx];
    if (layer->type != CD_MLOOPUV) {
      continue;
    }

    /* UV coordinates are stored in a Primvar on the Mesh, and can be referenced from materials.
     * The primvar name is the same as the UV Map name. This is to allow the standard name "st"
     * for texture coordinates by naming the UV Map as such, without having to guess which UV Map
     * is the "standard" one. */
    pxr::TfToken primvar_name(pxr::TfMakeValidIdentifier(layer->name));
    pxr::UsdGeomPrimvar uv_coords_primvar = usd_mesh.CreatePrimvar(
        primvar_name, pxr::SdfValueTypeNames->TexCoord2fArray, pxr::UsdGeomTokens->faceVarying);

    MLoopUV *mloopuv = static_cast<MLoopUV *>(layer->data);
    pxr::VtArray<pxr::GfVec2f> uv_coords;
    for (int loop_idx = 0; loop_idx < mesh->totloop; loop_idx++) {
      uv_coords.push_back(pxr::GfVec2f(mloopuv[loop_idx].uv));
    }

    if (!uv_coords_primvar.HasValue()) {
      uv_coords_primvar.Set(uv_coords, pxr::UsdTimeCode::Default());
    }
    const pxr::UsdAttribute &uv_coords_attr = uv_coords_primvar.GetAttr();
    usd_value_writer_.SetAttribute(uv_coords_attr, pxr::VtValue(uv_coords), timecode);
  }
}

void USDGenericMeshWriter::write_mesh(HierarchyContext &context, Mesh *mesh)
{
  pxr::UsdTimeCode timecode = get_export_time_code();
  pxr::UsdTimeCode defaultTime = pxr::UsdTimeCode::Default();
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  const pxr::SdfPath &usd_path = usd_export_context_.usd_path;

  pxr::UsdGeomMesh usd_mesh = pxr::UsdGeomMesh::Define(stage, usd_path);
  USDMeshData usd_mesh_data;
  get_geometry_data(mesh, usd_mesh_data);

  if (usd_export_context_.export_params.use_instancing && context.is_instance()) {
    // This object data is instanced, just reference the original instead of writing a copy.
    if (context.export_path == context.original_export_path) {
      printf("USD ref error: export path is reference path: %s\n", context.export_path.c_str());
      BLI_assert(!"USD reference error");
      return;
    }
    pxr::SdfPath ref_path(context.original_export_path);
    if (!usd_mesh.GetPrim().GetReferences().AddInternalReference(ref_path)) {
      /* See this URL for a description fo why referencing may fail"
       * https://graphics.pixar.com/usd/docs/api/class_usd_references.html#Usd_Failing_References
       */
      printf("USD Export warning: unable to add reference from %s to %s, not instancing object\n",
             context.export_path.c_str(),
             context.original_export_path.c_str());
      return;
    }
    /* The material path will be of the form </_materials/{material name}>, which is outside the
    subtree pointed to by ref_path. As a result, the referenced data is not allowed to point out
    of its own subtree. It does work when we override the material with exactly the same path,
    though.*/
    if (usd_export_context_.export_params.export_materials) {
      assign_materials(context, usd_mesh, usd_mesh_data.face_groups);
    }
    return;
  }

  pxr::UsdAttribute attr_points = usd_mesh.CreatePointsAttr(pxr::VtValue(), true);
  pxr::UsdAttribute attr_face_vertex_counts = usd_mesh.CreateFaceVertexCountsAttr(pxr::VtValue(),
                                                                                  true);
  pxr::UsdAttribute attr_face_vertex_indices = usd_mesh.CreateFaceVertexIndicesAttr(pxr::VtValue(),
                                                                                    true);

  if (!attr_points.HasValue()) {
    // Provide the initial value as default. This makes USD write the value as constant if they
    // don't change over time.
    attr_points.Set(usd_mesh_data.points, defaultTime);
    attr_face_vertex_counts.Set(usd_mesh_data.face_vertex_counts, defaultTime);
    attr_face_vertex_indices.Set(usd_mesh_data.face_indices, defaultTime);
  }

  usd_value_writer_.SetAttribute(attr_points, pxr::VtValue(usd_mesh_data.points), timecode);
  usd_value_writer_.SetAttribute(
      attr_face_vertex_counts, pxr::VtValue(usd_mesh_data.face_vertex_counts), timecode);
  usd_value_writer_.SetAttribute(
      attr_face_vertex_indices, pxr::VtValue(usd_mesh_data.face_indices), timecode);

  if (!usd_mesh_data.crease_lengths.empty()) {
    pxr::UsdAttribute attr_crease_lengths = usd_mesh.CreateCreaseLengthsAttr(pxr::VtValue(), true);
    pxr::UsdAttribute attr_crease_indices = usd_mesh.CreateCreaseIndicesAttr(pxr::VtValue(), true);
    pxr::UsdAttribute attr_crease_sharpness = usd_mesh.CreateCreaseSharpnessesAttr(pxr::VtValue(),
                                                                                   true);

    if (!attr_crease_lengths.HasValue()) {
      attr_crease_lengths.Set(usd_mesh_data.crease_lengths, defaultTime);
      attr_crease_indices.Set(usd_mesh_data.crease_vertex_indices, defaultTime);
      attr_crease_sharpness.Set(usd_mesh_data.crease_sharpnesses, defaultTime);
    }

    usd_value_writer_.SetAttribute(
        attr_crease_lengths, pxr::VtValue(usd_mesh_data.crease_lengths), timecode);
    usd_value_writer_.SetAttribute(
        attr_crease_indices, pxr::VtValue(usd_mesh_data.crease_vertex_indices), timecode);
    usd_value_writer_.SetAttribute(
        attr_crease_sharpness, pxr::VtValue(usd_mesh_data.crease_sharpnesses), timecode);
  }

  if (usd_export_context_.export_params.export_uvmaps) {
    write_uv_maps(mesh, usd_mesh);
  }
  if (usd_export_context_.export_params.export_normals) {
    write_normals(mesh, usd_mesh);
  }
  write_surface_velocity(context.object, mesh, usd_mesh);

  // TODO(Sybren): figure out what happens when the face groups change.
  if (frame_has_been_written_) {
    return;
  }

  usd_mesh.CreateSubdivisionSchemeAttr().Set(pxr::UsdGeomTokens->none);

  if (usd_export_context_.export_params.export_materials) {
    assign_materials(context, usd_mesh, usd_mesh_data.face_groups);
  }
}

static void get_vertices(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  usd_mesh_data.points.reserve(mesh->totvert);

  const MVert *verts = mesh->mvert;
  for (int i = 0; i < mesh->totvert; ++i) {
    usd_mesh_data.points.push_back(pxr::GfVec3f(verts[i].co));
  }
}

static void get_loops_polys(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  /* Only construct face groups (a.k.a. geometry subsets) when we need them for material
   * assignments. */
  bool construct_face_groups = mesh->totcol > 1;

  usd_mesh_data.face_vertex_counts.reserve(mesh->totpoly);
  usd_mesh_data.face_indices.reserve(mesh->totloop);

  MLoop *mloop = mesh->mloop;
  MPoly *mpoly = mesh->mpoly;
  for (int i = 0; i < mesh->totpoly; ++i, ++mpoly) {
    MLoop *loop = mloop + mpoly->loopstart;
    usd_mesh_data.face_vertex_counts.push_back(mpoly->totloop);
    for (int j = 0; j < mpoly->totloop; ++j, ++loop) {
      usd_mesh_data.face_indices.push_back(loop->v);
    }

    if (construct_face_groups) {
      usd_mesh_data.face_groups[mpoly->mat_nr].push_back(i);
    }
  }
}

static void get_creases(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  const float factor = 1.0f / 255.0f;

  MEdge *edge = mesh->medge;
  float sharpness;
  for (int edge_idx = 0, totedge = mesh->totedge; edge_idx < totedge; ++edge_idx, ++edge) {
    if (edge->crease == 0) {
      continue;
    }

    if (edge->crease == 255) {
      sharpness = pxr::UsdGeomMesh::SHARPNESS_INFINITE;
    }
    else {
      sharpness = static_cast<float>(edge->crease) * factor;
    }

    usd_mesh_data.crease_vertex_indices.push_back(edge->v1);
    usd_mesh_data.crease_vertex_indices.push_back(edge->v2);
    usd_mesh_data.crease_lengths.push_back(2);
    usd_mesh_data.crease_sharpnesses.push_back(sharpness);
  }
}

void USDGenericMeshWriter::get_geometry_data(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  get_vertices(mesh, usd_mesh_data);
  get_loops_polys(mesh, usd_mesh_data);
  get_creases(mesh, usd_mesh_data);
}

void USDGenericMeshWriter::assign_materials(const HierarchyContext &context,
                                            pxr::UsdGeomMesh usd_mesh,
                                            const MaterialFaceGroups &usd_face_groups)
{
  if (context.object->totcol == 0) {
    return;
  }

  /* Binding a material to a geometry subset isn't supported by the Hydra GL viewport yet,
   * which is why we always bind the first material to the entire mesh. See
   * https://github.com/PixarAnimationStudios/USD/issues/542 for more info. */
  bool mesh_material_bound = false;
  pxr::UsdShadeMaterialBindingAPI material_binding_api(usd_mesh.GetPrim());
  for (short mat_num = 0; mat_num < context.object->totcol; mat_num++) {
    Material *material = BKE_object_material_get(context.object, mat_num + 1);
    if (material == nullptr) {
      continue;
    }

    pxr::UsdShadeMaterial usd_material = ensure_usd_material(material);
    material_binding_api.Bind(usd_material);

    /* USD seems to support neither per-material nor per-face-group double-sidedness, so we just
     * use the flag from the first non-empty material slot. */
    usd_mesh.CreateDoubleSidedAttr(
        pxr::VtValue((material->blend_flag & MA_BL_CULL_BACKFACE) == 0));

    mesh_material_bound = true;
    break;
  }

  if (!mesh_material_bound) {
    /* Blender defaults to double-sided, but USD to single-sided. */
    usd_mesh.CreateDoubleSidedAttr(pxr::VtValue(true));
  }

  if (!mesh_material_bound || usd_face_groups.size() < 2) {
    /* Either all material slots were empty or there is only one material in use. As geometry
     * subsets are only written when actually used to assign a material, and the mesh already has
     * the material assigned, there is no need to continue. */
    return;
  }

  // Define a geometry subset per material.
  for (const MaterialFaceGroups::value_type &face_group : usd_face_groups) {
    short material_number = face_group.first;
    const pxr::VtIntArray &face_indices = face_group.second;

    Material *material = BKE_object_material_get(context.object, material_number + 1);
    if (material == nullptr) {
      continue;
    }

    pxr::UsdShadeMaterial usd_material = ensure_usd_material(material);
    pxr::TfToken material_name = usd_material.GetPath().GetNameToken();

    pxr::UsdGeomSubset usd_face_subset = material_binding_api.CreateMaterialBindSubset(
        material_name, face_indices);
    pxr::UsdShadeMaterialBindingAPI(usd_face_subset.GetPrim()).Bind(usd_material);
  }
}

void USDGenericMeshWriter::write_normals(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh)
{
  pxr::UsdTimeCode timecode = get_export_time_code();
  const float(*lnors)[3] = static_cast<float(*)[3]>(CustomData_get_layer(&mesh->ldata, CD_NORMAL));

  pxr::VtVec3fArray loop_normals;
  loop_normals.reserve(mesh->totloop);

  if (lnors != nullptr) {
    /* Export custom loop normals. */
    for (int loop_idx = 0, totloop = mesh->totloop; loop_idx < totloop; ++loop_idx) {
      loop_normals.push_back(pxr::GfVec3f(lnors[loop_idx]));
    }
  }
  else {
    /* Compute the loop normals based on the 'smooth' flag. */
    float normal[3];
    MPoly *mpoly = mesh->mpoly;
    const MVert *mvert = mesh->mvert;
    for (int poly_idx = 0, totpoly = mesh->totpoly; poly_idx < totpoly; ++poly_idx, ++mpoly) {
      MLoop *mloop = mesh->mloop + mpoly->loopstart;

      if ((mpoly->flag & ME_SMOOTH) == 0) {
        /* Flat shaded, use common normal for all verts. */
        BKE_mesh_calc_poly_normal(mpoly, mloop, mvert, normal);
        pxr::GfVec3f pxr_normal(normal);
        for (int loop_idx = 0; loop_idx < mpoly->totloop; ++loop_idx) {
          loop_normals.push_back(pxr_normal);
        }
      }
      else {
        /* Smooth shaded, use individual vert normals. */
        for (int loop_idx = 0; loop_idx < mpoly->totloop; ++loop_idx, ++mloop) {
          normal_short_to_float_v3(normal, mvert[mloop->v].no);
          loop_normals.push_back(pxr::GfVec3f(normal));
        }
      }
    }
  }

  pxr::UsdAttribute attr_normals = usd_mesh.CreateNormalsAttr(pxr::VtValue(), true);
  if (!attr_normals.HasValue()) {
    attr_normals.Set(loop_normals, pxr::UsdTimeCode::Default());
  }
  usd_value_writer_.SetAttribute(attr_normals, pxr::VtValue(loop_normals), timecode);
  usd_mesh.SetNormalsInterpolation(pxr::UsdGeomTokens->faceVarying);
}

void USDGenericMeshWriter::write_surface_velocity(Object *object,
                                                  const Mesh *mesh,
                                                  pxr::UsdGeomMesh usd_mesh)
{
  /* Only velocities from the fluid simulation are exported. This is the most important case,
   * though, as the baked mesh changes topology all the time, and thus computing the velocities
   * at import time in a post-processing step is hard. */
  ModifierData *md = BKE_modifiers_findby_type(object, eModifierType_Fluidsim);
  if (md == nullptr) {
    return;
  }

  /* Check that the fluid sim modifier is enabled and has useful data. */
  const bool use_render = (DEG_get_mode(usd_export_context_.depsgraph) == DAG_EVAL_RENDER);
  const ModifierMode required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;
  const Scene *scene = DEG_get_evaluated_scene(usd_export_context_.depsgraph);
  if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
    return;
  }
  FluidsimModifierData *fsmd = reinterpret_cast<FluidsimModifierData *>(md);
  if (!fsmd->fss || fsmd->fss->type != OB_FLUIDSIM_DOMAIN) {
    return;
  }
  FluidsimSettings *fss = fsmd->fss;
  if (!fss->meshVelocities) {
    return;
  }

  /* Export per-vertex velocity vectors. */
  pxr::VtVec3fArray usd_velocities;
  usd_velocities.reserve(mesh->totvert);

  FluidVertexVelocity *mesh_velocities = fss->meshVelocities;
  for (int vertex_idx = 0, totvert = mesh->totvert; vertex_idx < totvert;
       ++vertex_idx, ++mesh_velocities) {
    usd_velocities.push_back(pxr::GfVec3f(mesh_velocities->vel));
  }

  pxr::UsdTimeCode timecode = get_export_time_code();
  usd_mesh.CreateVelocitiesAttr().Set(usd_velocities, timecode);
}

USDMeshWriter::USDMeshWriter(const USDExporterContext &ctx) : USDGenericMeshWriter(ctx)
{
}

Mesh *USDMeshWriter::get_export_mesh(Object *object_eval, bool & /*r_needsfree*/)
{
  return BKE_object_get_evaluated_mesh(object_eval);
}

}  // namespace usd
}  // namespace io
}  // namespace blender
