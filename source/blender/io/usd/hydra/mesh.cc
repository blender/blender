/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "mesh.hh"

#include <pxr/imaging/hd/materialBindingSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/pxOsd/tokens.h>

#include "BLI_index_mask.hh"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

#include "BKE_attribute.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "populate_context.hh"
#include "util.hh"

namespace blender::io::hydra {

static const pxr::TfToken usd_st_token("st", pxr::TfToken::Immortal);

/* One per-material-slot submesh worth of geometry. */
struct SubMesh {
  pxr::VtIntArray face_vertex_counts;
  pxr::VtIntArray face_vertex_indices;
  pxr::VtVec3fArray points;
  pxr::VtVec3fArray normals;
  pxr::VtVec2fArray uvs;
  int mat_index = 0;
};

/* Build a single submesh from the given subset of triangles for a material.
 * Vertex indices are remapped to a contiguous range covering only the vertices
 * used by the subset, since Storm warns about indices into unused vertices. */
static void copy_submesh(const Mesh &mesh,
                         const Span<float3> vert_positions,
                         const Span<int> corner_verts,
                         const Span<int3> corner_tris,
                         const Span<int> tri_faces,
                         const Span<float3> face_normals,
                         const Span<float3> vert_normals,
                         const Span<float3> corner_normals,
                         const bke::MeshNormalDomain normals_domain,
                         const Span<float2> uv_map,
                         const IndexMask &triangles,
                         SubMesh &sm)
{
  resize_uninitialized(sm.face_vertex_indices, triangles.size() * 3);

  const bool copy_all_verts = triangles.size() == corner_tris.size() &&
                              mesh.verts_no_face().is_empty();

  int dst_verts_num;
  VectorSet<int> verts;
  if (copy_all_verts) {
    bke::mesh::vert_tris_from_corner_tris(
        corner_verts,
        corner_tris,
        MutableSpan(sm.face_vertex_indices.data(), sm.face_vertex_indices.size()).cast<int3>());
    dst_verts_num = vert_positions.size();
  }
  else {
    verts.reserve(triangles.size());
    triangles.foreach_index([&](const int src, const int dst) {
      const int3 &tri = corner_tris[src];
      sm.face_vertex_indices[dst * 3 + 0] = verts.index_of_or_add(corner_verts[tri[0]]);
      sm.face_vertex_indices[dst * 3 + 1] = verts.index_of_or_add(corner_verts[tri[1]]);
      sm.face_vertex_indices[dst * 3 + 2] = verts.index_of_or_add(corner_verts[tri[2]]);
    });
    dst_verts_num = verts.size();
  }

  resize_uninitialized(sm.points, dst_verts_num);
  if (copy_all_verts) {
    for (const int i : vert_positions.index_range()) {
      const float3 &v = vert_positions[i];
      sm.points[i] = pxr::GfVec3f(v.x, v.y, v.z);
    }
  }
  else {
    for (const int i : verts.index_range()) {
      const float3 &v = vert_positions[verts[i]];
      sm.points[i] = pxr::GfVec3f(v.x, v.y, v.z);
    }
  }

  resize_uninitialized(sm.face_vertex_counts, triangles.size());
  std::fill(sm.face_vertex_counts.begin(), sm.face_vertex_counts.end(), 3);

  /* Per-corner normals. */
  resize_uninitialized(sm.normals, triangles.size() * 3);
  switch (normals_domain) {
    case bke::MeshNormalDomain::Face:
      triangles.foreach_index([&](const int src, const int dst) {
        const float3 &n = face_normals[tri_faces[src]];
        for (int c = 0; c < 3; c++) {
          sm.normals[dst * 3 + c] = pxr::GfVec3f(n.x, n.y, n.z);
        }
      });
      break;
    case bke::MeshNormalDomain::Point:
      triangles.foreach_index([&](const int src, const int dst) {
        const int3 &tri = corner_tris[src];
        for (int c = 0; c < 3; c++) {
          const float3 &n = vert_normals[corner_verts[tri[c]]];
          sm.normals[dst * 3 + c] = pxr::GfVec3f(n.x, n.y, n.z);
        }
      });
      break;
    case bke::MeshNormalDomain::Corner:
      triangles.foreach_index([&](const int src, const int dst) {
        const int3 &tri = corner_tris[src];
        for (int c = 0; c < 3; c++) {
          const float3 &n = corner_normals[tri[c]];
          sm.normals[dst * 3 + c] = pxr::GfVec3f(n.x, n.y, n.z);
        }
      });
      break;
  }

  if (!uv_map.is_empty()) {
    resize_uninitialized(sm.uvs, triangles.size() * 3);
    triangles.foreach_index([&](const int src, const int dst) {
      const int3 &tri = corner_tris[src];
      for (int c = 0; c < 3; c++) {
        const float2 &uv = uv_map[tri[c]];
        sm.uvs[dst * 3 + c] = pxr::GfVec2f(uv.x, uv.y);
      }
    });
  }
}

static Vector<SubMesh> build_submeshes(const Object *object, const Mesh &mesh)
{
  const int mat_count = BKE_object_material_count_eval(object);
  Vector<SubMesh> submeshes(mat_count > 0 ? mat_count : 1);
  for (const int i : submeshes.index_range()) {
    submeshes[i].mat_index = i;
  }

  const Span<float3> vert_positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int3> corner_tris = mesh.corner_tris();
  const Span<int> tri_faces = mesh.corner_tri_faces();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const StringRef active_uv = mesh.active_uv_map_name();
  const VArraySpan<float2> uv_map = *attributes.lookup<float2>(active_uv, bke::AttrDomain::Corner);
  const VArraySpan<int> material_indices = *attributes.lookup<int>("material_index",
                                                                   bke::AttrDomain::Face);

  const bke::MeshNormalDomain normals_domain = mesh.normals_domain();
  const Span<float3> face_normals = (normals_domain == bke::MeshNormalDomain::Face) ?
                                        mesh.face_normals() :
                                        Span<float3>();
  const Span<float3> vert_normals = (normals_domain == bke::MeshNormalDomain::Point) ?
                                        mesh.vert_normals() :
                                        Span<float3>();
  const Span<float3> corner_normals = (normals_domain == bke::MeshNormalDomain::Corner) ?
                                          mesh.corner_normals() :
                                          Span<float3>();

  if (material_indices.is_empty()) {
    copy_submesh(mesh,
                 vert_positions,
                 corner_verts,
                 corner_tris,
                 tri_faces,
                 face_normals,
                 vert_normals,
                 corner_normals,
                 normals_domain,
                 uv_map,
                 corner_tris.index_range(),
                 submeshes.first());
  }
  else {
    IndexMaskMemory memory;
    Array<IndexMask> triangles_by_material(submeshes.size());
    const int max_index = std::max(mat_count - 1, 0);
    IndexMask::from_groups<int>(
        corner_tris.index_range(),
        memory,
        [&](const int i) { return std::clamp(material_indices[tri_faces[i]], 0, max_index); },
        triangles_by_material);

    threading::parallel_for(submeshes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        copy_submesh(mesh,
                     vert_positions,
                     corner_verts,
                     corner_tris,
                     tri_faces,
                     face_normals,
                     vert_normals,
                     corner_normals,
                     normals_domain,
                     uv_map,
                     triangles_by_material[i],
                     submeshes[i]);
      }
    });
  }

  /* Remove submeshes without faces. */
  submeshes.remove_if([](const SubMesh &submesh) { return submesh.face_vertex_counts.empty(); });

  return submeshes;
}

/* Build the geometry-only schema sources for one submesh. */
static EmittedGeometryPrim build_submesh_geometry(const SubMesh &sm,
                                                  const pxr::SdfPath &material_path,
                                                  const bool double_sided)
{
  EmittedGeometryPrim out;
  out.schema_token = pxr::HdMeshSchema::GetSchemaToken();
  /* Topology. */
  pxr::HdContainerDataSourceHandle topology =
      pxr::HdMeshTopologySchema::Builder()
          .SetFaceVertexCounts(
              pxr::HdRetainedTypedSampledDataSource<pxr::VtIntArray>::New(sm.face_vertex_counts))
          .SetFaceVertexIndices(
              pxr::HdRetainedTypedSampledDataSource<pxr::VtIntArray>::New(sm.face_vertex_indices))
          .SetOrientation(pxr::HdMeshTopologySchema::BuildOrientationDataSource(
              pxr::HdMeshTopologySchemaTokens->rightHanded))
          .Build();

  out.geometry =
      pxr::HdMeshSchema::Builder()
          .SetTopology(topology)
          .SetSubdivisionScheme(pxr::HdRetainedTypedSampledDataSource<pxr::TfToken>::New(
              pxr::PxOsdOpenSubdivTokens->none))
          .SetDoubleSided(pxr::HdRetainedTypedSampledDataSource<bool>::New(double_sided))
          .Build();

  /* Primvars. */
  HdContainerBuilder primvars;

  primvars.add(
      pxr::HdPrimvarsSchemaTokens->points,
      pxr::HdPrimvarSchema::Builder()
          .SetPrimvarValue(
              pxr::HdRetainedTypedSampledDataSource<pxr::VtVec3fArray>::New(sm.points))
          .SetInterpolation(pxr::HdPrimvarSchema::BuildInterpolationDataSource(
              pxr::HdPrimvarSchemaTokens->vertex))
          .SetRole(pxr::HdPrimvarSchema::BuildRoleDataSource(pxr::HdPrimvarSchemaTokens->point))
          .Build());

  if (!sm.normals.empty()) {
    primvars.add(
        pxr::HdPrimvarsSchemaTokens->normals,
        pxr::HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                pxr::HdRetainedTypedSampledDataSource<pxr::VtVec3fArray>::New(sm.normals))
            .SetInterpolation(pxr::HdPrimvarSchema::BuildInterpolationDataSource(
                pxr::HdPrimvarSchemaTokens->faceVarying))
            .SetRole(pxr::HdPrimvarSchema::BuildRoleDataSource(pxr::HdPrimvarSchemaTokens->normal))
            .Build());
  }

  if (!sm.uvs.empty()) {
    primvars.add(
        usd_st_token,
        pxr::HdPrimvarSchema::Builder()
            .SetPrimvarValue(pxr::HdRetainedTypedSampledDataSource<pxr::VtVec2fArray>::New(sm.uvs))
            .SetInterpolation(pxr::HdPrimvarSchema::BuildInterpolationDataSource(
                pxr::HdPrimvarSchemaTokens->faceVarying))
            .SetRole(pxr::HdPrimvarSchema::BuildRoleDataSource(
                pxr::HdPrimvarSchemaTokens->textureCoordinate))
            .Build());
  }

  out.primvars = primvars.build();

  /* Material binding. */
  if (!material_path.IsEmpty()) {
    pxr::HdContainerDataSourceHandle binding =
        pxr::HdMaterialBindingSchema::Builder()
            .SetPath(pxr::HdRetainedTypedSampledDataSource<pxr::SdfPath>::New(material_path))
            .Build();
    out.bindings = pxr::HdRetainedContainerDataSource::New(
        pxr::HdMaterialBindingsSchemaTokens->allPurpose, binding);
  }
  return out;
}

static const EmittedGeometry &get_or_build_emitted_mesh(PopulateContext &ctx,
                                                        const BObjectInfo &info,
                                                        const EmittedGeometryKey &key)
{
  ctx.used_emitted_geometry.add(key);

  /* Record geometry set to object mapping to handle updates. */
  if (!info.is_real_object_data() && info.object_data) {
    ctx.instance_geometries_by_object.lookup_or_add_default(info.real_object)
        .append_non_duplicates(info.object_data);
    ctx.used_instance_sources.add(info.real_object);
  }

  /* Already built in this populate? */
  if (EmittedGeometry *cached = ctx.emitted_geometry.lookup_ptr(key)) {
    for (const Material *mat : cached->materials) {
      ctx.get_or_create_material(mat);
    }
    return *cached;
  }

  EmittedGeometry &entry = ctx.emitted_geometry.lookup_or_add_default(key);

  /* Get proper mesh to use. */
  Mesh *mesh = nullptr;
  bool needs_clear = false;
  if (info.is_real_object_data()) {
    mesh = BKE_object_to_mesh(nullptr, info.real_object, false);
    needs_clear = (mesh != nullptr);
  }
  else if (info.object_data && GS(info.object_data->name) == ID_ME) {
    mesh = id_cast<Mesh *>(info.object_data);
  }
  if (!mesh) {
    return entry;
  }

  /* Build and emit submesh per material. */
  const Vector<SubMesh> submeshes = build_submeshes(info.real_object, *mesh);
  if (needs_clear) {
    BKE_object_to_mesh_clear(info.real_object);
  }

  entry.prims.reserve(submeshes.size());
  entry.materials.reserve(submeshes.size());
  for (const SubMesh &sm : submeshes) {
    const Material *material = BKE_object_material_get_eval(info.real_object, sm.mat_index + 1);
    const EmittedMaterial *mat_entry = ctx.get_or_create_material(material);
    const pxr::SdfPath material_path = mat_entry ? mat_entry->path : pxr::SdfPath();
    const bool double_sided = mat_entry ? mat_entry->double_sided : true;

    entry.prims.append(build_submesh_geometry(sm, material_path, double_sided));
    entry.materials.append(material);
  }

  return entry;
}

void emit_mesh_object(PopulateContext &ctx, const BObjectInfo &info, EmittedObject &emitted)
{
  const EmittedGeometryKey key = ctx.emitted_geometry_key(info, pxr::HdPrimTypeTokens->mesh);
  const EmittedGeometry &cached = get_or_build_emitted_mesh(ctx, info, key);
  emitted.geometry_keys.append_non_duplicates(key);
  for (const Material *m : cached.materials) {
    emitted.materials.append_non_duplicates(m);
  }

  Object *object = info.iter_object;
  const pxr::GfMatrix4d transform = gf_matrix_from_transform(object->object_to_world().ptr());

  for (const int sm_index : cached.prims.index_range()) {
    const pxr::SdfPath path = ctx.submesh_prim_id(object, sm_index);
    pxr::HdContainerDataSourceHandle prim_ds = compose_gprim_data_source(
        cached.prims[sm_index], transform, true);
    ctx.emit_object_prim(emitted, path, pxr::HdPrimTypeTokens->mesh, prim_ds);
  }
}

void emit_mesh_proto(PopulateContext &ctx, const BObjectInfo &info)
{
  const EmittedGeometryKey key = ctx.emitted_geometry_key(info, pxr::HdPrimTypeTokens->mesh);
  const EmittedGeometry &cached = get_or_build_emitted_mesh(ctx, info, key);

  Object *source = info.real_object;
  const pxr::HdContainerDataSourceHandle instanced_by_ds = ctx.proto_instanced_by(source);

  for (const int sm_index : cached.prims.index_range()) {
    const pxr::SdfPath proto_path = ctx.instancer_proto_submesh(source, sm_index);
    pxr::HdContainerDataSourceHandle prim_ds = compose_gprim_data_source(
        cached.prims[sm_index], pxr::GfMatrix4d(1.0), true, instanced_by_ds);

    ctx.emit_prim(proto_path, pxr::HdPrimTypeTokens->mesh, prim_ds);
    ctx.all_proto_paths.append(proto_path);
    ctx.per_proto_indices.append(pxr::VtIntArray());
  }
}

}  // namespace blender::io::hydra
