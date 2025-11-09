/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_mesh.hh"

#include "usd_armature_utils.hh"
#include "usd_attribute_utils.hh"
#include "usd_blend_shape_utils.hh"
#include "usd_hierarchy_iterator.hh"
#include "usd_skel_convert.hh"
#include "usd_utils.hh"

#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdSkel/bindingAPI.h>

#include "BLI_array_utils.hh"
#include "BLI_assert.h"
#include "BLI_math_vector_types.hh"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_subdiv.hh"

#include "bmesh.hh"
#include "bmesh_tools.hh"

#include "DEG_depsgraph.hh"

#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace blender::io::usd {

USDGenericMeshWriter::USDGenericMeshWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx)
{
}

bool USDGenericMeshWriter::is_supported(const HierarchyContext *context) const
{
  return context->is_object_visible(usd_export_context_.export_params.evaluation_mode);
}

/* Get the last subdiv modifier, regardless of enable/disable status */
static const SubsurfModifierData *get_last_subdiv_modifier(eEvaluationMode eval_mode, Object *obj)
{
  BLI_assert(obj);

  /* Return the subdiv modifier if it is the last modifier and has
   * the required mode enabled. */

  ModifierData *md = (ModifierData *)(obj->modifiers.last);

  if (!md) {
    return nullptr;
  }

  /* Determine if the modifier is enabled for the current evaluation mode. */
  ModifierMode mod_mode = (eval_mode == DAG_EVAL_RENDER) ? eModifierMode_Render :
                                                           eModifierMode_Realtime;

  if ((md->mode & mod_mode) != mod_mode) {
    return nullptr;
  }

  if (md->type == eModifierType_Subsurf) {
    return reinterpret_cast<SubsurfModifierData *>(md);
  }

  return nullptr;
}

void USDGenericMeshWriter::do_write(HierarchyContext &context)
{
  Object *object_eval = context.object;
  bool needsfree = false;
  Mesh *mesh = get_export_mesh(object_eval, needsfree);

  if (mesh == nullptr) {
    return;
  }

  if (usd_export_context_.export_params.triangulate_meshes) {
    const bool tag_only = false;
    const int quad_method = usd_export_context_.export_params.quad_method;
    const int ngon_method = usd_export_context_.export_params.ngon_method;

    BMeshCreateParams bmesh_create_params{};
    BMeshFromMeshParams bmesh_from_mesh_params{};
    bmesh_from_mesh_params.calc_face_normal = true;
    bmesh_from_mesh_params.calc_vert_normal = true;
    BMesh *bm = BKE_mesh_to_bmesh_ex(mesh, &bmesh_create_params, &bmesh_from_mesh_params);

    BM_mesh_triangulate(bm, quad_method, ngon_method, 4, tag_only, nullptr, nullptr, nullptr);

    Mesh *triangulated_mesh = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh);
    BM_mesh_free(bm);

    if (needsfree) {
      free_export_mesh(mesh);
    }
    mesh = triangulated_mesh;
    needsfree = true;
  }

  try {
    /* Fetch the subdiv modifier, if one exists and it is the last modifier. */
    const SubsurfModifierData *subsurfData = get_last_subdiv_modifier(
        usd_export_context_.export_params.evaluation_mode, object_eval);

    write_mesh(context, mesh, subsurfData);

    auto prim = usd_export_context_.stage->GetPrimAtPath(usd_export_context_.usd_path);
    if (prim.IsValid() && object_eval) {
      prim.SetActive((object_eval->duplicator_visibility_flag & OB_DUPLI_FLAG_RENDER) != 0);
      add_to_prim_map(prim.GetPath(), &mesh->id);
      write_id_properties(prim, mesh->id, get_export_time_code());
    }

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

void USDGenericMeshWriter::write_custom_data(const Object *obj,
                                             const Mesh *mesh,
                                             const pxr::UsdGeomMesh &usd_mesh)
{
  const bke::AttributeAccessor attributes = mesh->attributes();

  const StringRef active_uvmap_name = mesh->default_uv_map_name();

  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    /* Skip "internal" Blender properties and attributes processed elsewhere.
     * Skip edge domain because USD doesn't have a good conversion for them. */
    if (iter.name[0] == '.' || bke::attribute_name_is_anonymous(iter.name) ||
        iter.domain == bke::AttrDomain::Edge ||
        ELEM(iter.name,
             "position",
             "material_index",
             "velocity",
             "crease_vert",
             "custom_normal",
             "sharp_face"))
    {
      return;
    }

    if ((usd_export_context_.export_params.export_armatures ||
         usd_export_context_.export_params.export_shapekeys) &&
        iter.name.rfind("skel:") == 0)
    {
      /* If we're exporting armatures or shape keys to UsdSkel, we skip any
       * attributes that have names with the "skel:" namespace, to avoid possible
       * conflicts. Such attribute might have been previously imported into Blender
       * from USD, but can no longer be considered valid. */
      return;
    }

    if (usd_export_context_.export_params.export_armatures &&
        is_armature_modifier_bone_name(*obj, iter.name, usd_export_context_.depsgraph))
    {
      /* This attribute is likely a vertex group for the armature modifier,
       * and it may conflict with skinning data that will be written to
       * the USD mesh, so we skip it.  Such vertex groups will instead be
       * handled in #export_deform_verts(). */
      return;
    }

    /* UV Data. */
    if (iter.domain == bke::AttrDomain::Corner && iter.data_type == bke::AttrType::Float2) {
      if (usd_export_context_.export_params.export_uvmaps) {
        this->write_uv_data(usd_mesh, iter, active_uvmap_name);
      }
    }

    else {
      this->write_generic_data(mesh, usd_mesh, iter);
    }
  });
}

static std::optional<pxr::TfToken> convert_blender_domain_to_usd(
    const bke::AttrDomain blender_domain)
{
  switch (blender_domain) {
    case bke::AttrDomain::Corner:
      return pxr::UsdGeomTokens->faceVarying;
    case bke::AttrDomain::Point:
      return pxr::UsdGeomTokens->vertex;
    case bke::AttrDomain::Face:
      return pxr::UsdGeomTokens->uniform;

    /* Notice: Edge types are not supported in USD! */
    default:
      return std::nullopt;
  }
}

void USDGenericMeshWriter::write_generic_data(const Mesh *mesh,
                                              const pxr::UsdGeomMesh &usd_mesh,
                                              const bke::AttributeIter &attr)
{
  const pxr::TfToken pv_name(
      make_safe_name(attr.name, usd_export_context_.export_params.allow_unicode));
  const bool use_color3f_type = pv_name == usdtokens::displayColor;
  const std::optional<pxr::TfToken> pv_interp = convert_blender_domain_to_usd(attr.domain);
  const std::optional<pxr::SdfValueTypeName> pv_type = convert_blender_type_to_usd(
      attr.data_type, use_color3f_type);

  if (!pv_interp || !pv_type) {
    BKE_reportf(reports(),
                RPT_WARNING,
                "Mesh '%s', Attribute '%s' (domain %d, type %d) cannot be converted to USD",
                BKE_id_name(mesh->id),
                attr.name.c_str(),
                int8_t(attr.domain),
                int(attr.data_type));
    return;
  }

  const GVArray attribute = *attr.get();
  if (attribute.is_empty()) {
    return;
  }

  const pxr::UsdTimeCode time = get_export_time_code();
  const pxr::UsdGeomPrimvarsAPI pv_api = pxr::UsdGeomPrimvarsAPI(usd_mesh);

  pxr::UsdGeomPrimvar pv_attr = pv_api.CreatePrimvar(pv_name, *pv_type, *pv_interp);

  copy_blender_attribute_to_primvar(attribute, attr.data_type, time, pv_attr, usd_value_writer_);
}

void USDGenericMeshWriter::write_uv_data(const pxr::UsdGeomMesh &usd_mesh,
                                         const bke::AttributeIter &attr,
                                         const StringRef active_uvmap_name)
{
  const VArray<float2> buffer = *attr.get<float2>(bke::AttrDomain::Corner);
  if (buffer.is_empty()) {
    return;
  }

  /* Optionally rename active UV map to "st", to follow USD conventions
   * and better work with MaterialX shader nodes. */
  const StringRef name = usd_export_context_.export_params.rename_uvmaps &&
                                 active_uvmap_name == attr.name ?
                             "st" :
                             attr.name;

  const pxr::UsdTimeCode time = get_export_time_code();
  const pxr::TfToken pv_name(
      make_safe_name(name, usd_export_context_.export_params.allow_unicode));
  const pxr::UsdGeomPrimvarsAPI pv_api = pxr::UsdGeomPrimvarsAPI(usd_mesh);

  pxr::UsdGeomPrimvar pv_uv = pv_api.CreatePrimvar(
      pv_name, pxr::SdfValueTypeNames->TexCoord2fArray, pxr::UsdGeomTokens->faceVarying);

  copy_blender_buffer_to_primvar<float2, pxr::GfVec2f>(buffer, time, pv_uv, usd_value_writer_);
}

void USDGenericMeshWriter::free_export_mesh(Mesh *mesh)
{
  BKE_id_free(nullptr, mesh);
}

struct USDMeshData {
  pxr::VtArray<pxr::GfVec3f> points;
  pxr::VtIntArray face_vertex_counts;
  pxr::VtIntArray face_indices;
  MaterialFaceGroups face_groups;

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
   * the number of elements in this array will be either `len(creaseLengths)` or the sum over all X
   * of `(creaseLengths[X] - 1)`. Note that while the RI spec allows each crease to have either a
   * single sharpness or a value per-edge, USD will encode either a single sharpness per crease on
   * a mesh, or sharpness's for all edges making up the creases on a mesh. */
  pxr::VtFloatArray crease_sharpnesses;

  /* The lengths of this array specifies the number of sharp corners (or vertex crease) on the
   * surface. Each value is the index of a vertex in the mesh's vertex list. */
  pxr::VtIntArray corner_indices;
  /* The per-vertex sharpnesses. The lengths of this array must match that of `corner_indices`. */
  pxr::VtFloatArray corner_sharpnesses;
};

void USDGenericMeshWriter::write_mesh(HierarchyContext &context,
                                      Mesh *mesh,
                                      const SubsurfModifierData *subsurfData)
{
  pxr::UsdTimeCode time = get_export_time_code();
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  const pxr::SdfPath &usd_path = usd_export_context_.usd_path;

  pxr::UsdGeomMesh usd_mesh = pxr::UsdGeomMesh::Define(stage, usd_path);
  write_visibility(context, time, usd_mesh);

  USDMeshData usd_mesh_data;
  /* Ensure data exists if currently in edit mode. */
  BKE_mesh_wrapper_ensure_mdata(mesh);
  get_geometry_data(mesh, usd_mesh_data);

  pxr::UsdAttribute attr_points = usd_mesh.CreatePointsAttr(pxr::VtValue(), true);
  pxr::UsdAttribute attr_face_vertex_counts = usd_mesh.CreateFaceVertexCountsAttr(pxr::VtValue(),
                                                                                  true);
  pxr::UsdAttribute attr_face_vertex_indices = usd_mesh.CreateFaceVertexIndicesAttr(pxr::VtValue(),
                                                                                    true);

  if (!attr_points.HasValue()) {
    /* Provide the initial value as default. This makes USD write the value as constant if they
     * don't change over time. */
    attr_points.Set(usd_mesh_data.points, pxr::UsdTimeCode::Default());
    attr_face_vertex_counts.Set(usd_mesh_data.face_vertex_counts, pxr::UsdTimeCode::Default());
    attr_face_vertex_indices.Set(usd_mesh_data.face_indices, pxr::UsdTimeCode::Default());
  }

  usd_value_writer_.SetAttribute(attr_points, pxr::VtValue(usd_mesh_data.points), time);
  usd_value_writer_.SetAttribute(
      attr_face_vertex_counts, pxr::VtValue(usd_mesh_data.face_vertex_counts), time);
  usd_value_writer_.SetAttribute(
      attr_face_vertex_indices, pxr::VtValue(usd_mesh_data.face_indices), time);

  if (!usd_mesh_data.crease_lengths.empty()) {
    pxr::UsdAttribute attr_crease_lengths = usd_mesh.CreateCreaseLengthsAttr(pxr::VtValue(), true);
    pxr::UsdAttribute attr_crease_indices = usd_mesh.CreateCreaseIndicesAttr(pxr::VtValue(), true);
    pxr::UsdAttribute attr_crease_sharpness = usd_mesh.CreateCreaseSharpnessesAttr(pxr::VtValue(),
                                                                                   true);

    if (!attr_crease_lengths.HasValue()) {
      attr_crease_lengths.Set(usd_mesh_data.crease_lengths, pxr::UsdTimeCode::Default());
      attr_crease_indices.Set(usd_mesh_data.crease_vertex_indices, pxr::UsdTimeCode::Default());
      attr_crease_sharpness.Set(usd_mesh_data.crease_sharpnesses, pxr::UsdTimeCode::Default());
    }

    usd_value_writer_.SetAttribute(
        attr_crease_lengths, pxr::VtValue(usd_mesh_data.crease_lengths), time);
    usd_value_writer_.SetAttribute(
        attr_crease_indices, pxr::VtValue(usd_mesh_data.crease_vertex_indices), time);
    usd_value_writer_.SetAttribute(
        attr_crease_sharpness, pxr::VtValue(usd_mesh_data.crease_sharpnesses), time);
  }

  if (!usd_mesh_data.corner_indices.empty() &&
      usd_mesh_data.corner_indices.size() == usd_mesh_data.corner_sharpnesses.size())
  {
    pxr::UsdAttribute attr_corner_indices = usd_mesh.CreateCornerIndicesAttr(pxr::VtValue(), true);
    pxr::UsdAttribute attr_corner_sharpnesses = usd_mesh.CreateCornerSharpnessesAttr(
        pxr::VtValue(), true);

    if (!attr_corner_indices.HasValue()) {
      attr_corner_indices.Set(usd_mesh_data.corner_indices, pxr::UsdTimeCode::Default());
      attr_corner_sharpnesses.Set(usd_mesh_data.corner_sharpnesses, pxr::UsdTimeCode::Default());
    }

    usd_value_writer_.SetAttribute(
        attr_corner_indices, pxr::VtValue(usd_mesh_data.corner_indices), time);
    usd_value_writer_.SetAttribute(
        attr_corner_sharpnesses, pxr::VtValue(usd_mesh_data.corner_sharpnesses), time);
  }

  write_custom_data(context.object, mesh, usd_mesh);
  write_surface_velocity(mesh, usd_mesh);

  const pxr::TfToken subdiv_scheme = get_subdiv_scheme(subsurfData);

  /* Normals can be animated, so ensure these are written for each frame,
   * unless a subdiv modifier is used, in which case normals are computed,
   * not stored with the mesh. */
  if (usd_export_context_.export_params.export_normals &&
      subdiv_scheme == pxr::UsdGeomTokens->none)
  {
    write_normals(mesh, usd_mesh);
  }

  this->author_extent(usd_mesh, mesh->bounds_min_max(), time);

  /* TODO(Sybren): figure out what happens when the face groups change. */
  if (frame_has_been_written_) {
    return;
  }

  /* The subdivision scheme is a uniform according to spec,
   * so this value cannot be animated. */
  write_subdiv(subdiv_scheme, usd_mesh, subsurfData);

  if (usd_export_context_.export_params.export_materials) {
    assign_materials(context, usd_mesh, usd_mesh_data.face_groups);
  }
}

pxr::TfToken USDGenericMeshWriter::get_subdiv_scheme(const SubsurfModifierData *subsurfData)
{
  /* Default to setting the subdivision scheme to None. */
  pxr::TfToken subdiv_scheme = pxr::UsdGeomTokens->none;

  if (subsurfData) {
    if (subsurfData->subdivType == SUBSURF_TYPE_CATMULL_CLARK) {
      if (usd_export_context_.export_params.export_subdiv == USD_SUBDIV_BEST_MATCH) {
        /* If a subdivision modifier exists, and it uses Catmull-Clark, then apply Catmull-Clark
         * SubD scheme. */
        subdiv_scheme = pxr::UsdGeomTokens->catmullClark;
      }
    }
    else {
      /* "Simple" is currently the only other subdivision type provided by Blender, */
      /* and we do not yet provide a corresponding representation for USD export. */
      BKE_reportf(reports(),
                  RPT_WARNING,
                  "USD export: Simple subdivision not supported, exporting subdivided mesh");
    }
  }

  return subdiv_scheme;
}

void USDGenericMeshWriter::write_subdiv(const pxr::TfToken &subdiv_scheme,
                                        const pxr::UsdGeomMesh &usd_mesh,
                                        const SubsurfModifierData *subsurfData)
{
  usd_mesh.CreateSubdivisionSchemeAttr().Set(subdiv_scheme);
  if (subdiv_scheme == pxr::UsdGeomTokens->catmullClark) {
    /* For Catmull-Clark, also consider the various interpolation modes. */
    /* For reference, see
     * https://graphics.pixar.com/opensubdiv/docs/subdivision_surfaces.html#face-varying-interpolation-rules
     */
    switch (subsurfData->uv_smooth) {
      case SUBSURF_UV_SMOOTH_NONE:
        usd_mesh.CreateFaceVaryingLinearInterpolationAttr().Set(pxr::UsdGeomTokens->all);
        break;
      case SUBSURF_UV_SMOOTH_PRESERVE_CORNERS:
        usd_mesh.CreateFaceVaryingLinearInterpolationAttr().Set(pxr::UsdGeomTokens->cornersOnly);
        break;
      case SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_AND_JUNCTIONS:
        usd_mesh.CreateFaceVaryingLinearInterpolationAttr().Set(pxr::UsdGeomTokens->cornersPlus1);
        break;
      case SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_JUNCTIONS_AND_CONCAVE:
        usd_mesh.CreateFaceVaryingLinearInterpolationAttr().Set(pxr::UsdGeomTokens->cornersPlus2);
        break;
      case SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES:
        usd_mesh.CreateFaceVaryingLinearInterpolationAttr().Set(pxr::UsdGeomTokens->boundaries);
        break;
      case SUBSURF_UV_SMOOTH_ALL:
        usd_mesh.CreateFaceVaryingLinearInterpolationAttr().Set(pxr::UsdGeomTokens->none);
        break;
      default:
        BLI_assert_msg(0, "Unsupported UV smoothing mode.");
    }

    /* For reference, see
     * https://graphics.pixar.com/opensubdiv/docs/subdivision_surfaces.html#boundary-interpolation-rules
     */
    switch (subsurfData->boundary_smooth) {
      case SUBSURF_BOUNDARY_SMOOTH_ALL:
        usd_mesh.CreateInterpolateBoundaryAttr().Set(pxr::UsdGeomTokens->edgeOnly);
        break;
      case SUBSURF_BOUNDARY_SMOOTH_PRESERVE_CORNERS:
        usd_mesh.CreateInterpolateBoundaryAttr().Set(pxr::UsdGeomTokens->edgeAndCorner);
        break;
      default:
        BLI_assert_msg(0, "Unsupported boundary smoothing mode.");
    }
  }
}

static void get_positions(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  const Span<pxr::GfVec3f> positions = mesh->vert_positions().cast<pxr::GfVec3f>();
  usd_mesh_data.points = pxr::VtArray<pxr::GfVec3f>(positions.begin(), positions.end());
}

static void get_loops_polys(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  /* Only construct face groups (a.k.a. geometry subsets) when we need them for material
   * assignments. */
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Face, 0);
  if (!material_indices.is_single() && mesh->totcol > 1) {
    const VArraySpan<int> indices_span(material_indices);
    for (const int i : indices_span.index_range()) {
      usd_mesh_data.face_groups.lookup_or_add_default(indices_span[i]).push_back(i);
    }
  }

  usd_mesh_data.face_vertex_counts.resize(mesh->faces_num);
  const OffsetIndices faces = mesh->faces();
  offset_indices::copy_group_sizes(
      faces,
      faces.index_range(),
      MutableSpan(usd_mesh_data.face_vertex_counts.data(), mesh->faces_num));

  const Span<int> corner_verts = mesh->corner_verts();
  usd_mesh_data.face_indices = pxr::VtIntArray(corner_verts.begin(), corner_verts.end());
}

static void get_edge_creases(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  const bke::AttributeAccessor attributes = mesh->attributes();
  const bke::AttributeReader attribute = attributes.lookup<float>("crease_edge",
                                                                  bke::AttrDomain::Edge);
  if (!attribute) {
    return;
  }
  const VArraySpan creases(*attribute);
  const Span<int2> edges = mesh->edges();
  for (const int i : edges.index_range()) {
    const float crease = std::clamp(creases[i], 0.0f, 1.0f);

    if (crease != 0.0f) {
      usd_mesh_data.crease_vertex_indices.push_back(edges[i][0]);
      usd_mesh_data.crease_vertex_indices.push_back(edges[i][1]);
      usd_mesh_data.crease_lengths.push_back(2);
      usd_mesh_data.crease_sharpnesses.push_back(bke::subdiv::crease_to_sharpness(crease));
    }
  }
}

static void get_vert_creases(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  const bke::AttributeAccessor attributes = mesh->attributes();
  const bke::AttributeReader attribute = attributes.lookup<float>("crease_vert",
                                                                  bke::AttrDomain::Point);
  if (!attribute) {
    return;
  }
  const VArraySpan creases(*attribute);
  for (const int i : creases.index_range()) {
    const float crease = std::clamp(creases[i], 0.0f, 1.0f);

    if (crease != 0.0f) {
      usd_mesh_data.corner_indices.push_back(i);
      usd_mesh_data.corner_sharpnesses.push_back(bke::subdiv::crease_to_sharpness(crease));
    }
  }
}

void USDGenericMeshWriter::get_geometry_data(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  get_positions(mesh, usd_mesh_data);
  get_loops_polys(mesh, usd_mesh_data);
  get_edge_creases(mesh, usd_mesh_data);
  get_vert_creases(mesh, usd_mesh_data);
}

void USDGenericMeshWriter::assign_materials(const HierarchyContext &context,
                                            const pxr::UsdGeomMesh &usd_mesh,
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
    pxr::UsdShadeMaterialBindingAPI::Apply(mesh_prim);
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
  for (const MaterialFaceGroups::Item &face_group : usd_face_groups.items()) {
    short material_number = face_group.key;
    const pxr::VtIntArray &face_indices = face_group.value;

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
    pxr::UsdShadeMaterialBindingAPI::Apply(subset_prim);
  }
}

void USDGenericMeshWriter::write_normals(const Mesh *mesh, pxr::UsdGeomMesh &usd_mesh)
{
  pxr::UsdTimeCode time = get_export_time_code();

  pxr::VtVec3fArray loop_normals;
  loop_normals.resize(mesh->corners_num);

  MutableSpan dst_normals(reinterpret_cast<float3 *>(loop_normals.data()), loop_normals.size());

  switch (mesh->normals_domain()) {
    case bke::MeshNormalDomain::Point: {
      array_utils::gather(mesh->vert_normals(), mesh->corner_verts(), dst_normals);
      break;
    }
    case bke::MeshNormalDomain::Face: {
      const OffsetIndices faces = mesh->faces();
      const Span<float3> face_normals = mesh->face_normals();
      for (const int i : faces.index_range()) {
        dst_normals.slice(faces[i]).fill(face_normals[i]);
      }
      break;
    }
    case bke::MeshNormalDomain::Corner: {
      array_utils::copy(mesh->corner_normals(), dst_normals);
      break;
    }
  }

  pxr::UsdAttribute attr_normals = usd_mesh.CreateNormalsAttr(pxr::VtValue(), true);
  if (!attr_normals.HasValue()) {
    attr_normals.Set(loop_normals, pxr::UsdTimeCode::Default());
  }
  usd_value_writer_.SetAttribute(attr_normals, pxr::VtValue(loop_normals), time);
  usd_mesh.SetNormalsInterpolation(pxr::UsdGeomTokens->faceVarying);
}

void USDGenericMeshWriter::write_surface_velocity(const Mesh *mesh,
                                                  const pxr::UsdGeomMesh &usd_mesh)
{
  /* Export velocity attribute output by fluid sim, sequence cache modifier
   * and geometry nodes. */
  const VArraySpan velocity = *mesh->attributes().lookup<float3>("velocity",
                                                                 blender::bke::AttrDomain::Point);
  if (velocity.is_empty()) {
    return;
  }

  /* Export per-vertex velocity vectors. */
  Span<pxr::GfVec3f> data = velocity.cast<pxr::GfVec3f>();
  pxr::VtVec3fArray usd_velocities;
  usd_velocities.assign(data.begin(), data.end());

  pxr::UsdTimeCode time = get_export_time_code();
  pxr::UsdAttribute attr_vel = usd_mesh.CreateVelocitiesAttr(pxr::VtValue(), true);
  if (!attr_vel.HasValue()) {
    attr_vel.Set(usd_velocities, pxr::UsdTimeCode::Default());
  }

  usd_value_writer_.SetAttribute(attr_vel, usd_velocities, time);
}

USDMeshWriter::USDMeshWriter(const USDExporterContext &ctx)
    : USDGenericMeshWriter(ctx), write_skinned_mesh_(false), write_blend_shapes_(false)
{
}

void USDMeshWriter::set_skel_export_flags(const HierarchyContext &context)
{
  write_skinned_mesh_ = false;
  write_blend_shapes_ = false;

  const USDExportParams &params = usd_export_context_.export_params;

  /* We can write a skinned mesh if exporting armatures is enabled and the object has an armature
   * modifier. */
  write_skinned_mesh_ = params.export_armatures &&
                        can_export_skinned_mesh(*context.object, usd_export_context_.depsgraph);

  /* We can write blend shapes if exporting shape keys is enabled and the object has shape keys. */
  write_blend_shapes_ = params.export_shapekeys && is_mesh_with_shape_keys(context.object);
}

void USDMeshWriter::init_skinned_mesh(const HierarchyContext &context)
{
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;

  pxr::UsdPrim mesh_prim = stage->GetPrimAtPath(usd_export_context_.usd_path);

  if (!mesh_prim.IsValid()) {
    CLOG_WARN(&LOG,
              "%s: couldn't get valid mesh prim for mesh %s",
              __func__,
              usd_export_context_.usd_path.GetAsString().c_str());
    return;
  }

  pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI::Apply(mesh_prim);

  if (!skel_api) {
    CLOG_WARN(&LOG,
              "Couldn't apply UsdSkelBindingAPI to mesh prim %s",
              usd_export_context_.usd_path.GetAsString().c_str());
    return;
  }

  const Object *arm_obj = get_armature_modifier_obj(*context.object,
                                                    usd_export_context_.depsgraph);

  if (!arm_obj) {
    CLOG_WARN(&LOG,
              "Couldn't get armature modifier object for skinned mesh %s",
              usd_export_context_.usd_path.GetAsString().c_str());
    return;
  }

  Vector<StringRef> bone_names;
  get_armature_bone_names(
      arm_obj, usd_export_context_.export_params.only_deform_bones, bone_names);

  if (bone_names.is_empty()) {
    CLOG_WARN(&LOG,
              "No armature bones for skinned mesh %s",
              usd_export_context_.usd_path.GetAsString().c_str());
    return;
  }

  bool needsfree = false;
  Mesh *mesh = get_export_mesh(context.object, needsfree);

  if (mesh == nullptr) {
    return;
  }

  try {
    export_deform_verts(mesh, skel_api, bone_names);

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

void USDMeshWriter::init_blend_shapes(const HierarchyContext &context)
{
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;

  pxr::UsdPrim mesh_prim = stage->GetPrimAtPath(usd_export_context_.usd_path);

  if (!mesh_prim.IsValid()) {
    CLOG_WARN(&LOG,
              "Couldn't get valid mesh prim for mesh %s",
              mesh_prim.GetPath().GetAsString().c_str());
    return;
  }

  create_blend_shapes(this->usd_export_context_.stage,
                      context.object,
                      mesh_prim,
                      usd_export_context_.export_params.allow_unicode);
}

void USDMeshWriter::do_write(HierarchyContext &context)
{
  set_skel_export_flags(context);

  if (frame_has_been_written_ && (write_skinned_mesh_ || write_blend_shapes_)) {
    /* When writing skinned meshes or blend shapes, we only write the rest mesh once,
     * so we return early after the first frame has been written. However, we still
     * update blend shape weights if needed. */
    if (write_blend_shapes_) {
      add_shape_key_weights_sample(context.object);
    }
    return;
  }

  USDGenericMeshWriter::do_write(context);

  if (write_skinned_mesh_) {
    init_skinned_mesh(context);
  }

  if (write_blend_shapes_) {
    init_blend_shapes(context);
    add_shape_key_weights_sample(context.object);
  }
}

Mesh *USDMeshWriter::get_export_mesh(Object *object_eval, bool &r_needsfree)
{
  if (write_blend_shapes_) {
    r_needsfree = true;
    /* We return the pre-modified mesh with the verts in the shape key
     * basis positions. */
    return get_shape_key_basis_mesh(object_eval);
  }

  if (write_skinned_mesh_) {
    r_needsfree = false;
    /* We must export the skinned mesh in its rest pose.  We therefore
     * return the pre-modified mesh, so that the armature modifier isn't
     * applied. */
    /* TODO: Store the "needs free" mesh in a separate variable. */
    return const_cast<Mesh *>(BKE_object_get_pre_modified_mesh(object_eval));
  }

  /* Return the fully evaluated mesh. */
  r_needsfree = false;
  return BKE_object_get_evaluated_mesh(object_eval);
}

void USDMeshWriter::add_shape_key_weights_sample(const Object *obj)
{
  if (!obj) {
    return;
  }

  const Key *key = get_mesh_shape_key(obj);
  if (!key) {
    return;
  }

  pxr::UsdStageRefPtr stage = usd_export_context_.stage;

  pxr::UsdPrim mesh_prim = stage->GetPrimAtPath(usd_export_context_.usd_path);

  if (!mesh_prim.IsValid()) {
    CLOG_WARN(&LOG,
              "Couldn't get valid mesh prim for mesh %s",
              usd_export_context_.usd_path.GetAsString().c_str());
    return;
  }

  pxr::VtFloatArray weights = get_blendshape_weights(key);
  pxr::UsdTimeCode time = get_export_time_code();

  /* Save the weights samples to a temporary privar which will be copied to
   * a skeleton animation later. */
  pxr::UsdAttribute temp_weights_attr = pxr::UsdGeomPrimvarsAPI(mesh_prim).CreatePrimvar(
      TempBlendShapeWeightsPrimvarName, pxr::SdfValueTypeNames->FloatArray);

  if (!temp_weights_attr) {
    CLOG_WARN(&LOG,
              "Couldn't create primvar %s on prim %s",
              TempBlendShapeWeightsPrimvarName.GetText(),
              mesh_prim.GetPath().GetAsString().c_str());
    return;
  }

  temp_weights_attr.Set(weights, time);
}

}  // namespace blender::io::usd
