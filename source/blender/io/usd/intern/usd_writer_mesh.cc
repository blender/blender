/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_mesh.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include "BLI_assert.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

#include "BKE_attribute.h"
#include "BKE_customdata.h"
#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"

#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_particle_types.h"

#include "WM_api.h"

#include <iostream>

namespace blender::io::usd {

USDGenericMeshWriter::USDGenericMeshWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx)
{
}

bool USDGenericMeshWriter::is_supported(const HierarchyContext *context) const
{
  if (usd_export_context_.export_params.visible_objects_only) {
    return context->is_object_visible(usd_export_context_.export_params.evaluation_mode);
  }
  return true;
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

void USDGenericMeshWriter::write_custom_data(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh)
{
  const bke::AttributeAccessor attributes = mesh->attributes();

  attributes.for_all(
      [&](const bke::AttributeIDRef &attribute_id, const bke::AttributeMetaData &meta_data) {
        /* Color data. */
        if (ELEM(meta_data.domain, ATTR_DOMAIN_CORNER, ATTR_DOMAIN_POINT) &&
            ELEM(meta_data.data_type, CD_PROP_BYTE_COLOR, CD_PROP_COLOR))
        {
          write_color_data(mesh, usd_mesh, attribute_id, meta_data);
        }

        return true;
      });
}

void USDGenericMeshWriter::write_color_data(const Mesh *mesh,
                                            pxr::UsdGeomMesh usd_mesh,
                                            const bke::AttributeIDRef &attribute_id,
                                            const bke::AttributeMetaData &meta_data)
{
  pxr::UsdTimeCode timecode = get_export_time_code();
  const std::string name = attribute_id.name();
  pxr::TfToken primvar_name(pxr::TfMakeValidIdentifier(name));
  const pxr::UsdGeomPrimvarsAPI pvApi = pxr::UsdGeomPrimvarsAPI(usd_mesh);

  /* Varying type depends on original domain. */
  const pxr::TfToken prim_varying = meta_data.domain == ATTR_DOMAIN_CORNER ?
                                        pxr::UsdGeomTokens->faceVarying :
                                        pxr::UsdGeomTokens->vertex;

  pxr::UsdGeomPrimvar colors_pv = pvApi.CreatePrimvar(
      primvar_name, pxr::SdfValueTypeNames->Color3fArray, prim_varying);

  const VArray<ColorGeometry4f> attribute = *mesh->attributes().lookup_or_default<ColorGeometry4f>(
      attribute_id, meta_data.domain, {0.0f, 0.0f, 0.0f, 1.0f});

  pxr::VtArray<pxr::GfVec3f> colors_data;

  /* TODO: Thread the copy, like the obj exporter. */
  switch (meta_data.domain) {
    case ATTR_DOMAIN_CORNER:
      for (size_t loop_idx = 0; loop_idx < mesh->totloop; loop_idx++) {
        const ColorGeometry4f color = attribute.get(loop_idx);
        colors_data.push_back(pxr::GfVec3f(color.r, color.g, color.b));
      }
      break;

    case ATTR_DOMAIN_POINT:
      for (const int point_index : attribute.index_range()) {
        const ColorGeometry4f color = attribute.get(point_index);
        colors_data.push_back(pxr::GfVec3f(color.r, color.g, color.b));
      }
      break;

    default:
      BLI_assert_msg(0, "Invalid domain for mesh color data.");
      return;
  }

  colors_pv.Set(colors_data, timecode);

  const pxr::UsdAttribute &prim_colors_attr = colors_pv.GetAttr();
  usd_value_writer_.SetAttribute(prim_colors_attr, pxr::VtValue(colors_data), timecode);
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
   * the number of elements in this array will be either 'len(creaseLengths)' or the sum over all X
   * of '(creaseLengths[X] - 1)'. Note that while the RI spec allows each crease to have either a
   * single sharpness or a value per-edge, USD will encode either a single sharpness per crease on
   * a mesh, or sharpness's for all edges making up the creases on a mesh. */
  pxr::VtFloatArray crease_sharpnesses;

  /* The lengths of this array specifies the number of sharp corners (or vertex crease) on the
   * surface. Each value is the index of a vertex in the mesh's vertex list. */
  pxr::VtIntArray corner_indices;
  /* The per-vertex sharpnesses. The lengths of this array must match that of `corner_indices`. */
  pxr::VtFloatArray corner_sharpnesses;
};

void USDGenericMeshWriter::write_uv_maps(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh)
{
  pxr::UsdTimeCode timecode = get_export_time_code();

  pxr::UsdGeomPrimvarsAPI primvarsAPI(usd_mesh.GetPrim());

  const CustomData *ldata = &mesh->ldata;
  for (int layer_idx = 0; layer_idx < ldata->totlayer; layer_idx++) {
    const CustomDataLayer *layer = &ldata->layers[layer_idx];
    if (layer->type != CD_PROP_FLOAT2) {
      continue;
    }

    /* UV coordinates are stored in a Primvar on the Mesh, and can be referenced from materials.
     * The primvar name is the same as the UV Map name. This is to allow the standard name "st"
     * for texture coordinates by naming the UV Map as such, without having to guess which UV Map
     * is the "standard" one. */
    pxr::TfToken primvar_name(pxr::TfMakeValidIdentifier(layer->name));
    pxr::UsdGeomPrimvar uv_coords_primvar = primvarsAPI.CreatePrimvar(
        primvar_name, pxr::SdfValueTypeNames->TexCoord2fArray, pxr::UsdGeomTokens->faceVarying);

    const float2 *mloopuv = static_cast<const float2 *>(layer->data);
    pxr::VtArray<pxr::GfVec2f> uv_coords;
    for (int loop_idx = 0; loop_idx < mesh->totloop; loop_idx++) {
      uv_coords.push_back(pxr::GfVec2f(mloopuv[loop_idx].x, mloopuv[loop_idx].y));
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
  write_visibility(context, timecode, usd_mesh);

  USDMeshData usd_mesh_data;
  /* Ensure data exists if currently in edit mode. */
  BKE_mesh_wrapper_ensure_mdata(mesh);
  get_geometry_data(mesh, usd_mesh_data);

  if (usd_export_context_.export_params.use_instancing && context.is_instance()) {
    if (!mark_as_instance(context, usd_mesh.GetPrim())) {
      return;
    }

    /* The material path will be of the form </_materials/{material name}>, which is outside the
     * sub-tree pointed to by ref_path. As a result, the referenced data is not allowed to point
     * out of its own sub-tree. It does work when we override the material with exactly the same
     * path, though. */
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
    /* Provide the initial value as default. This makes USD write the value as constant if they
     * don't change over time. */
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

  if (!usd_mesh_data.corner_indices.empty() &&
      usd_mesh_data.corner_indices.size() == usd_mesh_data.corner_sharpnesses.size())
  {
    pxr::UsdAttribute attr_corner_indices = usd_mesh.CreateCornerIndicesAttr(pxr::VtValue(), true);
    pxr::UsdAttribute attr_corner_sharpnesses = usd_mesh.CreateCornerSharpnessesAttr(
        pxr::VtValue(), true);

    if (!attr_corner_indices.HasValue()) {
      attr_corner_indices.Set(usd_mesh_data.corner_indices, defaultTime);
      attr_corner_sharpnesses.Set(usd_mesh_data.corner_sharpnesses, defaultTime);
    }

    usd_value_writer_.SetAttribute(
        attr_corner_indices, pxr::VtValue(usd_mesh_data.corner_indices), timecode);
    usd_value_writer_.SetAttribute(
        attr_corner_sharpnesses, pxr::VtValue(usd_mesh_data.crease_sharpnesses), timecode);
  }

  if (usd_export_context_.export_params.export_uvmaps) {
    write_uv_maps(mesh, usd_mesh);
  }

  write_custom_data(mesh, usd_mesh);

  if (usd_export_context_.export_params.export_normals) {
    write_normals(mesh, usd_mesh);
  }
  write_surface_velocity(mesh, usd_mesh);

  /* TODO(Sybren): figure out what happens when the face groups change. */
  if (frame_has_been_written_) {
    return;
  }

  usd_mesh.CreateSubdivisionSchemeAttr().Set(pxr::UsdGeomTokens->none);

  if (usd_export_context_.export_params.export_materials) {
    assign_materials(context, usd_mesh, usd_mesh_data.face_groups);
  }

  /* Blender grows its bounds cache to cover animated meshes, so only author once. */
  float bound_min[3];
  float bound_max[3];
  INIT_MINMAX(bound_min, bound_max);
  BKE_mesh_minmax(mesh, bound_min, bound_max);
  pxr::VtArray<pxr::GfVec3f> extent{pxr::GfVec3f{bound_min[0], bound_min[1], bound_min[2]},
                                    pxr::GfVec3f{bound_max[0], bound_max[1], bound_max[2]}};
  usd_mesh.CreateExtentAttr().Set(extent);
}

static void get_vertices(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  usd_mesh_data.points.reserve(mesh->totvert);

  const Span<float3> positions = mesh->vert_positions();
  for (const int i : positions.index_range()) {
    const float3 &position = positions[i];
    usd_mesh_data.points.push_back(pxr::GfVec3f(position.x, position.y, position.z));
  }
}

static void get_loops_polys(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  /* Only construct face groups (a.k.a. geometry subsets) when we need them for material
   * assignments. */
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", ATTR_DOMAIN_FACE, 0);
  if (!material_indices.is_single() && mesh->totcol > 1) {
    const VArraySpan<int> indices_span(material_indices);
    for (const int i : indices_span.index_range()) {
      usd_mesh_data.face_groups[indices_span[i]].push_back(i);
    }
  }

  usd_mesh_data.face_vertex_counts.reserve(mesh->totpoly);
  usd_mesh_data.face_indices.reserve(mesh->totloop);

  const OffsetIndices polys = mesh->polys();
  const Span<int> corner_verts = mesh->corner_verts();

  for (const int i : polys.index_range()) {
    const IndexRange poly = polys[i];
    usd_mesh_data.face_vertex_counts.push_back(poly.size());
    for (const int vert : corner_verts.slice(poly)) {
      usd_mesh_data.face_indices.push_back(vert);
    }
  }
}

static void get_edge_creases(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  const bke::AttributeAccessor attributes = mesh->attributes();
  const bke::AttributeReader attribute = attributes.lookup<float>("crease_edge", ATTR_DOMAIN_EDGE);
  if (!attribute) {
    return;
  }
  const VArraySpan creases(*attribute);
  const Span<int2> edges = mesh->edges();
  for (const int i : edges.index_range()) {
    const float crease = creases[i];
    if (crease == 0.0f) {
      continue;
    }

    const float sharpness = crease >= 1.0f ? pxr::UsdGeomMesh::SHARPNESS_INFINITE : crease;

    usd_mesh_data.crease_vertex_indices.push_back(edges[i][0]);
    usd_mesh_data.crease_vertex_indices.push_back(edges[i][1]);
    usd_mesh_data.crease_lengths.push_back(2);
    usd_mesh_data.crease_sharpnesses.push_back(sharpness);
  }
}

static void get_vert_creases(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  const bke::AttributeAccessor attributes = mesh->attributes();
  const bke::AttributeReader attribute = attributes.lookup<float>("crease_vert",
                                                                  ATTR_DOMAIN_POINT);
  if (!attribute) {
    return;
  }
  const VArraySpan creases(*attribute);
  for (const int i : creases.index_range()) {
    const float sharpness = creases[i];

    if (sharpness != 0.0f) {
      usd_mesh_data.corner_indices.push_back(i);
      usd_mesh_data.corner_sharpnesses.push_back(sharpness);
    }
  }
}

void USDGenericMeshWriter::get_geometry_data(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  get_vertices(mesh, usd_mesh_data);
  get_loops_polys(mesh, usd_mesh_data);
  get_edge_creases(mesh, usd_mesh_data);
  get_vert_creases(mesh, usd_mesh_data);
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
  auto mesh_prim = usd_mesh.GetPrim();
  pxr::UsdShadeMaterialBindingAPI material_binding_api(mesh_prim);
  for (int mat_num = 0; mat_num < context.object->totcol; mat_num++) {
    Material *material = BKE_object_material_get(context.object, mat_num + 1);
    if (material == nullptr) {
      continue;
    }

    pxr::UsdShadeMaterial usd_material = ensure_usd_material(context, material);
    material_binding_api.Bind(usd_material);

    /* USD seems to support neither per-material nor per-face-group double-sidedness, so we just
     * use the flag from the first non-empty material slot. */
    usd_mesh.CreateDoubleSidedAttr(
        pxr::VtValue((material->blend_flag & MA_BL_CULL_BACKFACE) == 0));

    mesh_material_bound = true;
    break;
  }

  if (mesh_material_bound) {
    /* USD will require that prims with material bindings have the #MaterialBindingAPI applied
     * schema. While Bind() above will create the binding attribute, Apply() needs to be called as
     * well to add the #MaterialBindingAPI schema to the prim itself. */
    material_binding_api.Apply(mesh_prim);
  }
  else {
    /* Blender defaults to double-sided, but USD to single-sided. */
    usd_mesh.CreateDoubleSidedAttr(pxr::VtValue(true));
  }

  if (!mesh_material_bound || usd_face_groups.size() < 2) {
    /* Either all material slots were empty or there is only one material in use. As geometry
     * subsets are only written when actually used to assign a material, and the mesh already has
     * the material assigned, there is no need to continue. */
    return;
  }

  /* Define a geometry subset per material. */
  for (const MaterialFaceGroups::value_type &face_group : usd_face_groups) {
    short material_number = face_group.first;
    const pxr::VtIntArray &face_indices = face_group.second;

    Material *material = BKE_object_material_get(context.object, material_number + 1);
    if (material == nullptr) {
      continue;
    }

    pxr::UsdShadeMaterial usd_material = ensure_usd_material(context, material);
    pxr::TfToken material_name = usd_material.GetPath().GetNameToken();

    pxr::UsdGeomSubset usd_face_subset = material_binding_api.CreateMaterialBindSubset(
        material_name, face_indices);
    auto subset_prim = usd_face_subset.GetPrim();
    auto subset_material_api = pxr::UsdShadeMaterialBindingAPI(subset_prim);
    subset_material_api.Bind(usd_material);
    /* Apply the #MaterialBindingAPI applied schema, as required by USD. */
    subset_material_api.Apply(subset_prim);
  }
}

void USDGenericMeshWriter::write_normals(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh)
{
  pxr::UsdTimeCode timecode = get_export_time_code();
  const float(*lnors)[3] = static_cast<const float(*)[3]>(
      CustomData_get_layer(&mesh->ldata, CD_NORMAL));
  const OffsetIndices polys = mesh->polys();
  const Span<int> corner_verts = mesh->corner_verts();

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
    bke::AttributeAccessor attributes = mesh->attributes();
    const Span<float3> vert_normals = mesh->vert_normals();
    const Span<float3> poly_normals = mesh->poly_normals();
    const VArray<bool> sharp_faces = *attributes.lookup_or_default<bool>(
        "sharp_face", ATTR_DOMAIN_FACE, false);
    for (const int i : polys.index_range()) {
      const IndexRange poly = polys[i];
      if (sharp_faces[i]) {
        /* Flat shaded, use common normal for all verts. */
        pxr::GfVec3f pxr_normal(&poly_normals[i].x);
        for (int loop_idx = 0; loop_idx < poly.size(); ++loop_idx) {
          loop_normals.push_back(pxr_normal);
        }
      }
      else {
        /* Smooth shaded, use individual vert normals. */
        for (const int vert : corner_verts.slice(poly)) {
          loop_normals.push_back(pxr::GfVec3f(&vert_normals[vert].x));
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

void USDGenericMeshWriter::write_surface_velocity(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh)
{
  /* Export velocity attribute output by fluid sim, sequence cache modifier
   * and geometry nodes. */
  CustomDataLayer *velocity_layer = BKE_id_attribute_find(
      &mesh->id, "velocity", CD_PROP_FLOAT3, ATTR_DOMAIN_POINT);

  if (velocity_layer == nullptr) {
    return;
  }

  const float(*velocities)[3] = reinterpret_cast<float(*)[3]>(velocity_layer->data);

  /* Export per-vertex velocity vectors. */
  pxr::VtVec3fArray usd_velocities;
  usd_velocities.reserve(mesh->totvert);

  for (int vertex_idx = 0, totvert = mesh->totvert; vertex_idx < totvert; ++vertex_idx) {
    usd_velocities.push_back(pxr::GfVec3f(velocities[vertex_idx]));
  }

  pxr::UsdTimeCode timecode = get_export_time_code();
  usd_mesh.CreateVelocitiesAttr().Set(usd_velocities, timecode);
}

USDMeshWriter::USDMeshWriter(const USDExporterContext &ctx) : USDGenericMeshWriter(ctx) {}

Mesh *USDMeshWriter::get_export_mesh(Object *object_eval, bool & /*r_needsfree*/)
{
  return BKE_object_get_evaluated_mesh(object_eval);
}

}  // namespace blender::io::usd
