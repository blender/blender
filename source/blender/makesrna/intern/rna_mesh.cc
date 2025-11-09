/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute.h"
#include "BKE_editmesh.hh"
#include "BKE_mesh_types.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "rna_internal.hh"

#include "WM_types.hh"

const EnumPropertyItem rna_enum_mesh_delimit_mode_items[] = {
    {BMO_DELIM_NORMAL, "NORMAL", 0, "Normal", "Delimit by face directions"},
    {BMO_DELIM_MATERIAL, "MATERIAL", 0, "Material", "Delimit by face material"},
    {BMO_DELIM_SEAM, "SEAM", 0, "Seam", "Delimit by edge seams"},
    {BMO_DELIM_SHARP, "SHARP", 0, "Sharp", "Delimit by sharp edges"},
    {BMO_DELIM_UV, "UV", 0, "UVs", "Delimit by UV coordinates"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_mesh_remesh_mode_items[] = {
    {REMESH_VOXEL, "VOXEL", 0, "Voxel", "Use the voxel remesher"},
    {REMESH_QUAD, "QUAD", 0, "Quad", "Use the quad remesher"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "DNA_material_types.h"
#  include "DNA_scene_types.h"
#  include "DNA_world_types.h"

#  include "BLI_math_geom.h"
#  include "BLI_math_vector.h"

#  include "BKE_attribute.hh"
#  include "BKE_customdata.hh"
#  include "BKE_lib_id.hh"
#  include "BKE_main.hh"
#  include "BKE_mesh.hh"
#  include "BKE_mesh_runtime.hh"
#  include "BKE_report.hh"

#  include "DEG_depsgraph.hh"

#  include "ED_mesh.hh" /* XXX Bad level call */

#  include "WM_api.hh"

#  include "rna_mesh_utils.hh"

using blender::StringRef;

/* -------------------------------------------------------------------- */
/** \name Generic Helpers
 * \{ */

static Mesh *rna_mesh(const PointerRNA *ptr)
{
  Mesh *mesh = (Mesh *)ptr->owner_id;
  return mesh;
}

static CustomData *rna_mesh_vdata_helper(Mesh *mesh)
{
  return (mesh->runtime->edit_mesh) ? &mesh->runtime->edit_mesh->bm->vdata : &mesh->vert_data;
}

static CustomData *rna_mesh_ldata_helper(Mesh *mesh)
{
  return (mesh->runtime->edit_mesh) ? &mesh->runtime->edit_mesh->bm->ldata : &mesh->corner_data;
}

static CustomData *rna_mesh_vdata(const PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  return rna_mesh_vdata_helper(mesh);
}
static CustomData *rna_mesh_ldata(const PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  return rna_mesh_ldata_helper(mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic CustomData Layer Functions
 * \{ */

static void rna_cd_layer_name_set(CustomData *cdata, CustomDataLayer *cdl, const char *value)
{
  STRNCPY_UTF8(cdl->name, value);
  CustomData_set_layer_unique_name(cdata, cdl - cdata->layers);
}

static void rna_MeshVertexLayer_name_set(PointerRNA *ptr, const char *value)
{
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;

  if (CD_TYPE_AS_MASK(eCustomDataType(layer->type)) & CD_MASK_PROP_ALL) {
    AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
    BKE_attribute_rename(owner, layer->name, value, nullptr);
  }
  else {
    rna_cd_layer_name_set(rna_mesh_vdata(ptr), layer, value);
  }
}
#  if 0
static void rna_MeshEdgeLayer_name_set(PointerRNA *ptr, const char *value)
{
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;

  if (CD_TYPE_AS_MASK(eCustomDataType(layer->type)) & CD_MASK_PROP_ALL) {
    AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
    BKE_attribute_rename(owner, layer->name, value, nullptr);
  }
  else {
    rna_cd_layer_name_set(rna_mesh_edata(ptr), layer, value);
  }
}
#  endif
static void rna_MeshLoopLayer_name_set(PointerRNA *ptr, const char *value)
{
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;

  if (CD_TYPE_AS_MASK(eCustomDataType(layer->type)) & CD_MASK_PROP_ALL) {
    AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
    BKE_attribute_rename(owner, layer->name, value, nullptr);
  }
  else {
    rna_cd_layer_name_set(rna_mesh_ldata(ptr), layer, value);
  }
}

static bool rna_Mesh_has_custom_normals_get(PointerRNA *ptr)
{
  Mesh *mesh = static_cast<Mesh *>(ptr->data);
  return BKE_mesh_has_custom_loop_normals(mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update Callbacks
 *
 * \note Skipping meshes without users is a simple way to avoid updates on newly created meshes.
 * This speeds up importers that manipulate mesh data before linking it to an object & collection.
 *
 * \{ */

/**
 * \warning This calls `DEG_id_tag_update(id, 0)` which is something that should be phased out
 * (see #deg_graph_node_tag_zero), for now it's kept since changes to updates must be carefully
 * tested to make sure there aren't any regressions.
 *
 * This function should be replaced with more specific update flags where possible.
 */
static void rna_Mesh_update_data_legacy_deg_tag_all(Main * /*bmain*/,
                                                    Scene * /*scene*/,
                                                    PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  if (id->us <= 0) { /* See note in section heading. */
    return;
  }

  DEG_id_tag_update(id, 0);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

static void rna_Mesh_update_geom_and_params(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  if (id->us <= 0) { /* See note in section heading. */
    return;
  }

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY | ID_RECALC_PARAMETERS);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

static void rna_Mesh_update_data_edit_weight(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  BKE_mesh_batch_cache_dirty_tag(rna_mesh(ptr), BKE_MESH_BATCH_DIRTY_ALL);

  rna_Mesh_update_data_legacy_deg_tag_all(bmain, scene, ptr);
}

static void rna_Mesh_update_data_edit_active_color(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  BKE_mesh_batch_cache_dirty_tag(rna_mesh(ptr), BKE_MESH_BATCH_DIRTY_ALL);

  rna_Mesh_update_data_legacy_deg_tag_all(bmain, scene, ptr);
}
static void rna_Mesh_update_select(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  if (id->us <= 0) { /* See note in section heading. */
    return;
  }

  WM_main_add_notifier(NC_GEOM | ND_SELECT, id);
}

void rna_Mesh_update_draw(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  if (id->us <= 0) { /* See note in section heading. */
    return;
  }

  WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

static void rna_Mesh_update_bone_selection_mode(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Mesh *mesh = static_cast<Mesh *>(ptr->data);
  mesh->editflag &= ~ME_EDIT_PAINT_VERT_SEL;
  mesh->editflag &= ~ME_EDIT_PAINT_FACE_SEL;

  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);

  rna_Mesh_update_draw(bmain, scene, ptr);
}

static void rna_Mesh_update_vertmask(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Mesh *mesh = static_cast<Mesh *>(ptr->data);
  if ((mesh->editflag & ME_EDIT_PAINT_VERT_SEL) && (mesh->editflag & ME_EDIT_PAINT_FACE_SEL)) {
    mesh->editflag &= ~ME_EDIT_PAINT_FACE_SEL;
  }

  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);

  rna_Mesh_update_draw(bmain, scene, ptr);
}

static void rna_Mesh_update_facemask(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Mesh *mesh = static_cast<Mesh *>(ptr->data);
  if ((mesh->editflag & ME_EDIT_PAINT_VERT_SEL) && (mesh->editflag & ME_EDIT_PAINT_FACE_SEL)) {
    mesh->editflag &= ~ME_EDIT_PAINT_VERT_SEL;
  }

  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);

  rna_Mesh_update_draw(bmain, scene, ptr);
}

static void rna_Mesh_update_positions_tag(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Mesh_update_data_legacy_deg_tag_all(bmain, scene, ptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Property get/set Callbacks
 * \{ */

static int rna_MeshVertex_index_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const blender::float3 *position = static_cast<const blender::float3 *>(ptr->data);
  const int index = int(position - mesh->vert_positions().data());
  BLI_assert(index >= 0);
  BLI_assert(index < mesh->verts_num);
  return index;
}

static int rna_MeshEdge_index_get(PointerRNA *ptr)
{
  using namespace blender;
  const Mesh *mesh = rna_mesh(ptr);
  const blender::int2 *edge = static_cast<const blender::int2 *>(ptr->data);
  const blender::int2 *edges = mesh->edges().data();
  const int index = int(edge - edges);
  BLI_assert(index >= 0);
  BLI_assert(index < mesh->edges_num);
  return index;
}

static int rna_MeshPolygon_index_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int *face_offset = static_cast<const int *>(ptr->data);
  const int index = int(face_offset - mesh->face_offsets().data());
  BLI_assert(index >= 0);
  BLI_assert(index < mesh->faces_num);
  return index;
}

static int rna_MeshLoop_index_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int *corner_vert = static_cast<const int *>(ptr->data);
  const int index = int(corner_vert - mesh->corner_verts().data());
  BLI_assert(index >= 0);
  BLI_assert(index < mesh->corners_num);
  return index;
}

static int rna_MeshLoopTriangle_index_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const blender::int3 *tri = static_cast<const blender::int3 *>(ptr->data);
  const int index = int(tri - mesh->corner_tris().data());
  BLI_assert(index >= 0);
  BLI_assert(index < BKE_mesh_runtime_corner_tris_len(mesh));
  return index;
}

static int rna_MeshLoopTriangle_polygon_index_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshLoopTriangle_index_get(ptr);
  return mesh->corner_tri_faces()[index];
}

static void rna_Mesh_loop_triangles_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const blender::Span<blender::int3> corner_tris = mesh->corner_tris();
  rna_iterator_array_begin(iter,
                           ptr,
                           const_cast<blender::int3 *>(corner_tris.data()),
                           sizeof(blender::int3),
                           corner_tris.size(),
                           false,
                           nullptr);
}

static int rna_Mesh_loop_triangles_length(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  return BKE_mesh_runtime_corner_tris_len(mesh);
}

bool rna_Mesh_loop_triangles_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  if (index < 0 || index >= BKE_mesh_runtime_corner_tris_len(mesh)) {
    return false;
  }
  /* Casting away const is okay because this RNA type doesn't allow changing the value. */
  rna_pointer_create_with_ancestors(*ptr,
                                    &RNA_MeshLoopTriangle,
                                    const_cast<blender::int3 *>(&mesh->corner_tris()[index]),
                                    *r_ptr);
  return true;
}

static void rna_Mesh_loop_triangle_polygons_begin(CollectionPropertyIterator *iter,
                                                  PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  rna_iterator_array_begin(iter,
                           ptr,
                           const_cast<int *>(mesh->corner_tri_faces().data()),
                           sizeof(int),
                           BKE_mesh_runtime_corner_tris_len(mesh),
                           false,
                           nullptr);
}

bool rna_Mesh_loop_triangle_polygons_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  if (index < 0 || index >= BKE_mesh_runtime_corner_tris_len(mesh)) {
    return false;
  }
  /* Casting away const is okay because this RNA type doesn't allow changing the value. */
  rna_pointer_create_with_ancestors(
      *ptr, &RNA_ReadOnlyInteger, const_cast<int *>(&mesh->corner_tri_faces()[index]), *r_ptr);
  return true;
}

static void rna_MeshVertex_co_get(PointerRNA *ptr, float *value)
{
  copy_v3_v3(value, (const float *)ptr->data);
}

static void rna_MeshVertex_co_set(PointerRNA *ptr, const float *value)
{
  copy_v3_v3((float *)ptr->data, value);
  Mesh *mesh = rna_mesh(ptr);
  mesh->tag_positions_changed();
}

static void rna_MeshVertex_normal_get(PointerRNA *ptr, float *value)
{
  Mesh *mesh = rna_mesh(ptr);
  const blender::Span<blender::float3> vert_normals = mesh->vert_normals();
  const int index = rna_MeshVertex_index_get(ptr);
  copy_v3_v3(value, vert_normals[index]);
}

static bool rna_MeshVertex_hide_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const blender::VArray hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", blender::bke::AttrDomain::Point, false);
  const int index = rna_MeshVertex_index_get(ptr);
  return hide_vert[index];
}

static void rna_MeshVertex_hide_set(PointerRNA *ptr, bool value)
{
  Mesh *mesh = rna_mesh(ptr);
  blender::bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  blender::bke::AttributeWriter hide_vert = attributes.lookup_or_add_for_write<bool>(
      ".hide_vert", blender::bke::AttrDomain::Point, blender::bke::AttributeInitDefaultValue());
  const int index = rna_MeshVertex_index_get(ptr);
  hide_vert.varray.set(index, value);
  hide_vert.finish();
}

static bool rna_MeshVertex_select_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshVertex_index_get(ptr);
  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const blender::VArray select_vert = *attributes.lookup_or_default<bool>(
      ".select_vert", blender::bke::AttrDomain::Point, false);
  return select_vert[index];
}

static void rna_MeshVertex_select_set(PointerRNA *ptr, bool value)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshVertex_index_get(ptr);
  blender::bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  blender::bke::AttributeWriter select_vert = attributes.lookup_or_add_for_write<bool>(
      ".select_vert", blender::bke::AttrDomain::Point, blender::bke::AttributeInitDefaultValue());
  select_vert.varray.set(index, value);
  select_vert.finish();
}

static int rna_MeshLoop_vertex_index_get(PointerRNA *ptr)
{
  return *(int *)ptr->data;
}

static void rna_MeshLoop_vertex_index_set(PointerRNA *ptr, int value)
{
  *(int *)ptr->data = value;
}

static int rna_MeshLoop_edge_index_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshLoop_index_get(ptr);
  return mesh->corner_edges()[index];
}

static void rna_MeshLoop_edge_index_set(PointerRNA *ptr, int value)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshLoop_index_get(ptr);
  mesh->corner_edges_for_write()[index] = value;
}

static void rna_MeshLoop_normal_get(PointerRNA *ptr, float *values)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshLoop_index_get(ptr);
  copy_v3_v3(values, mesh->corner_normals()[index]);
}

static void rna_MeshLoop_tangent_get(PointerRNA *ptr, float *values)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshLoop_index_get(ptr);
  const blender::float4 *layer = static_cast<const blender::float4 *>(
      CustomData_get_layer(&mesh->corner_data, CD_MLOOPTANGENT));

  if (!layer) {
    zero_v3(values);
  }
  else {
    copy_v3_v3(values, (const float *)(layer + index));
  }
}

static float rna_MeshLoop_bitangent_sign_get(PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshLoop_index_get(ptr);
  const blender::float4 *vec = static_cast<const blender::float4 *>(
      CustomData_get_layer(&mesh->corner_data, CD_MLOOPTANGENT));

  return (vec) ? vec[index][3] : 0.0f;
}

static void rna_MeshLoop_bitangent_get(PointerRNA *ptr, float *values)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshLoop_index_get(ptr);
  const blender::float4 *vec = static_cast<const blender::float4 *>(
      CustomData_get_layer(&mesh->corner_data, CD_MLOOPTANGENT));

  if (vec) {
    cross_v3_v3v3(values, mesh->corner_normals()[index], vec[index]);
    mul_v3_fl(values, vec[index][3]);
  }
  else {
    zero_v3(values);
  }
}

static void rna_MeshPolygon_normal_get(PointerRNA *ptr, float *values)
{
  using namespace blender;
  Mesh *mesh = rna_mesh(ptr);
  const int poly_start = *((const int *)ptr->data);
  const int poly_size = *(((const int *)ptr->data) + 1) - poly_start;
  const Span<int> face_verts = mesh->corner_verts().slice(poly_start, poly_size);
  const float3 result = bke::mesh::face_normal_calc(mesh->vert_positions(), face_verts);
  copy_v3_v3(values, result);
}

static bool rna_MeshPolygon_hide_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshPolygon_index_get(ptr);
  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const blender::VArray hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", blender::bke::AttrDomain::Face, false);
  return hide_poly[index];
}

static void rna_MeshPolygon_hide_set(PointerRNA *ptr, bool value)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshPolygon_index_get(ptr);
  blender::bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  blender::bke::AttributeWriter hide_poly = attributes.lookup_or_add_for_write<bool>(
      ".hide_poly", blender::bke::AttrDomain::Face, blender::bke::AttributeInitDefaultValue());
  hide_poly.varray.set(index, value);
  hide_poly.finish();
}

static bool rna_MeshPolygon_use_smooth_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshPolygon_index_get(ptr);
  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const blender::VArray sharp_face = *attributes.lookup_or_default<bool>(
      "sharp_face", blender::bke::AttrDomain::Face, false);
  return !sharp_face[index];
}

static void rna_MeshPolygon_use_smooth_set(PointerRNA *ptr, bool value)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshPolygon_index_get(ptr);
  blender::bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  blender::bke::AttributeWriter sharp_face = attributes.lookup_or_add_for_write<bool>(
      "sharp_face", blender::bke::AttrDomain::Face, blender::bke::AttributeInitDefaultValue());
  sharp_face.varray.set(index, !value);
  sharp_face.finish();
}

static bool rna_MeshPolygon_select_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshPolygon_index_get(ptr);
  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const blender::VArray select_poly = *attributes.lookup_or_default<bool>(
      ".select_poly", blender::bke::AttrDomain::Face, false);
  return select_poly[index];
}

static void rna_MeshPolygon_select_set(PointerRNA *ptr, bool value)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshPolygon_index_get(ptr);
  blender::bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  blender::bke::AttributeWriter select_poly = attributes.lookup_or_add_for_write<bool>(
      ".select_poly", blender::bke::AttrDomain::Face, blender::bke::AttributeInitDefaultValue());
  select_poly.varray.set(index, value);
  select_poly.finish();
}

static int rna_MeshPolygon_material_index_get(PointerRNA *ptr)
{
  using namespace blender;
  const Mesh *mesh = rna_mesh(ptr);
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArray material_index = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Face, 0);
  return material_index[rna_MeshPolygon_index_get(ptr)];
}

static void rna_MeshPolygon_material_index_set(PointerRNA *ptr, int value)
{
  using namespace blender;
  Mesh *mesh = rna_mesh(ptr);
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::AttributeWriter material_index = attributes.lookup_or_add_for_write<int>(
      "material_index", bke::AttrDomain::Face);
  material_index.varray.set(rna_MeshPolygon_index_get(ptr), max_ii(0, value));
  material_index.finish();
}

static void rna_MeshPolygon_center_get(PointerRNA *ptr, float *values)
{
  using namespace blender;
  Mesh *mesh = rna_mesh(ptr);
  const int poly_start = *((const int *)ptr->data);
  const int poly_size = *(((const int *)ptr->data) + 1) - poly_start;
  const Span<int> face_verts = mesh->corner_verts().slice(poly_start, poly_size);
  const float3 result = bke::mesh::face_center_calc(mesh->vert_positions(), face_verts);
  copy_v3_v3(values, result);
}

static float rna_MeshPolygon_area_get(PointerRNA *ptr)
{
  using namespace blender;
  Mesh *mesh = (Mesh *)ptr->owner_id;
  const int poly_start = *((const int *)ptr->data);
  const int poly_size = *(((const int *)ptr->data) + 1) - poly_start;
  const Span<int> face_verts = mesh->corner_verts().slice(poly_start, poly_size);
  return bke::mesh::face_area_calc(mesh->vert_positions(), face_verts);
}

static void rna_MeshPolygon_flip(ID *id, MIntProperty *poly_offset_p)
{
  using namespace blender;
  Mesh *mesh = (Mesh *)id;
  const int index = reinterpret_cast<int *>(poly_offset_p) - mesh->faces().data().data();
  bke::mesh_flip_faces(*mesh, IndexMask(IndexRange(index, 1)));
  BKE_mesh_tessface_clear(mesh);
  BKE_mesh_runtime_clear_geometry(mesh);
}

static void rna_MeshLoopTriangle_verts_get(PointerRNA *ptr, int *values)
{
  Mesh *mesh = rna_mesh(ptr);
  const blender::Span<int> corner_verts = mesh->corner_verts();
  blender::int3 tri = *(blender::int3 *)ptr->data;
  values[0] = corner_verts[tri[0]];
  values[1] = corner_verts[tri[1]];
  values[2] = corner_verts[tri[2]];
}

static void rna_MeshLoopTriangle_normal_get(PointerRNA *ptr, float *values)
{
  Mesh *mesh = rna_mesh(ptr);
  blender::int3 tri = *(blender::int3 *)ptr->data;
  const blender::Span<blender::float3> positions = mesh->vert_positions();
  const blender::Span<int> corner_verts = mesh->corner_verts();
  const int v1 = corner_verts[tri[0]];
  const int v2 = corner_verts[tri[1]];
  const int v3 = corner_verts[tri[2]];

  normal_tri_v3(values, positions[v1], positions[v2], positions[v3]);
}

static void rna_MeshLoopTriangle_split_normals_get(PointerRNA *ptr, float *values)
{
  Mesh *mesh = rna_mesh(ptr);
  const blender::Span<blender::float3> corner_normals = mesh->corner_normals();
  const blender::int3 tri = *(const blender::int3 *)ptr->data;
  copy_v3_v3(values + 0, corner_normals[tri[0]]);
  copy_v3_v3(values + 3, corner_normals[tri[1]]);
  copy_v3_v3(values + 6, corner_normals[tri[2]]);
}

static float rna_MeshLoopTriangle_area_get(PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  blender::int3 tri = *(blender::int3 *)ptr->data;
  const blender::Span<blender::float3> positions = mesh->vert_positions();
  const blender::Span<int> corner_verts = mesh->corner_verts();
  const int v1 = corner_verts[tri[0]];
  const int v2 = corner_verts[tri[1]];
  const int v3 = corner_verts[tri[2]];
  return area_tri_v3(positions[v1], positions[v2], positions[v3]);
}

static void rna_MeshLoopColor_color_get(PointerRNA *ptr, float *values)
{
  MLoopCol *mlcol = (MLoopCol *)ptr->data;

  values[0] = mlcol->r / 255.0f;
  values[1] = mlcol->g / 255.0f;
  values[2] = mlcol->b / 255.0f;
  values[3] = mlcol->a / 255.0f;
}

static void rna_MeshLoopColor_color_set(PointerRNA *ptr, const float *values)
{
  MLoopCol *mlcol = (MLoopCol *)ptr->data;

  mlcol->r = round_fl_to_uchar_clamp(values[0] * 255.0f);
  mlcol->g = round_fl_to_uchar_clamp(values[1] * 255.0f);
  mlcol->b = round_fl_to_uchar_clamp(values[2] * 255.0f);
  mlcol->a = round_fl_to_uchar_clamp(values[3] * 255.0f);
}

static int rna_Mesh_texspace_editable(const PointerRNA *ptr, const char ** /*r_info*/)
{
  Mesh *mesh = (Mesh *)ptr->data;
  return (mesh->texspace_flag & ME_TEXSPACE_FLAG_AUTO) ? PropertyFlag(0) : PROP_EDITABLE;
}

static void rna_Mesh_texspace_size_get(PointerRNA *ptr, float values[3])
{
  Mesh *mesh = (Mesh *)ptr->data;

  BKE_mesh_texspace_ensure(mesh);

  copy_v3_v3(values, mesh->texspace_size);
}

static void rna_Mesh_texspace_location_get(PointerRNA *ptr, float values[3])
{
  Mesh *mesh = (Mesh *)ptr->data;

  BKE_mesh_texspace_ensure(mesh);

  copy_v3_v3(values, mesh->texspace_location);
}

static void rna_MeshVertex_groups_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  MDeformVert *dverts = mesh->deform_verts_for_write().data();
  if (dverts) {
    const int index = rna_MeshVertex_index_get(ptr);
    MDeformVert *dvert = &dverts[index];

    rna_iterator_array_begin(
        iter, ptr, dvert->dw, sizeof(MDeformWeight), dvert->totweight, 0, nullptr);
  }
  else {
    rna_iterator_array_begin(iter, ptr, nullptr, 0, 0, 0, nullptr);
  }
}

static void rna_MeshVertex_undeformed_co_get(PointerRNA *ptr, float values[3])
{
  Mesh *mesh = rna_mesh(ptr);
  const float *position = (const float *)ptr->data;
  const blender::float3 *orco = static_cast<const blender::float3 *>(
      CustomData_get_layer(&mesh->vert_data, CD_ORCO));

  if (orco) {
    const int index = rna_MeshVertex_index_get(ptr);
    /* orco is normalized to 0..1, we do inverse to match the vertex position */
    float texspace_location[3], texspace_size[3];

    BKE_mesh_texspace_get(
        mesh->texcomesh ? mesh->texcomesh : mesh, texspace_location, texspace_size);
    madd_v3_v3v3v3(values, texspace_location, orco[index], texspace_size);
  }
  else {
    copy_v3_v3(values, position);
  }
}

static int rna_CustomDataLayer_active_get(PointerRNA *ptr, CustomData *data, int type, bool render)
{
  int n = ((CustomDataLayer *)ptr->data) - data->layers;

  if (render) {
    return (n == CustomData_get_render_layer_index(data, eCustomDataType(type)));
  }
  else {
    return (n == CustomData_get_active_layer_index(data, eCustomDataType(type)));
  }
}

static int rna_CustomDataLayer_clone_get(PointerRNA *ptr, CustomData *data, int type)
{
  int n = ((CustomDataLayer *)ptr->data) - data->layers;

  return (n == CustomData_get_clone_layer_index(data, eCustomDataType(type)));
}

static void rna_CustomDataLayer_active_set(
    PointerRNA *ptr, CustomData *data, int value, int type, int render)
{
  Mesh *mesh = (Mesh *)ptr->owner_id;
  int n = (((CustomDataLayer *)ptr->data) - data->layers) -
          CustomData_get_layer_index(data, eCustomDataType(type));

  if (value == 0) {
    return;
  }

  if (render) {
    CustomData_set_layer_render(data, eCustomDataType(type), n);
  }
  else {
    CustomData_set_layer_active(data, eCustomDataType(type), n);
  }

  BKE_mesh_tessface_clear(mesh);
}

static void rna_CustomDataLayer_clone_set(PointerRNA *ptr, CustomData *data, int value, int type)
{
  int n = ((CustomDataLayer *)ptr->data) - data->layers;

  if (value == 0) {
    return;
  }

  CustomData_set_layer_clone_index(data, eCustomDataType(type), n);
}

/* uv_layers */

DEFINE_CUSTOMDATA_LAYER_COLLECTION(uv_layer, ldata, CD_PROP_FLOAT2)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(
    uv_layer, ldata, CD_PROP_FLOAT2, active, MeshUVLoopLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(
    uv_layer, ldata, CD_PROP_FLOAT2, clone, MeshUVLoopLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(
    uv_layer, ldata, CD_PROP_FLOAT2, stencil, MeshUVLoopLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(
    uv_layer, ldata, CD_PROP_FLOAT2, render, MeshUVLoopLayer)

/* MeshUVLoopLayer */

static std::optional<std::string> rna_MeshUVLoopLayer_path(const PointerRNA *ptr)
{
  const CustomDataLayer *cdl = static_cast<const CustomDataLayer *>(ptr->data);
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return fmt::format("uv_layers[\"{}\"]", name_esc);
}

static void rna_MeshUVLoopLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  const int length = (mesh->runtime->edit_mesh) ? 0 : mesh->corners_num;
  CustomData_ensure_data_is_mutable(layer, length);
  rna_iterator_array_begin(iter, ptr, layer->data, sizeof(float[2]), length, 0, nullptr);
}

static int rna_MeshUVLoopLayer_data_length(PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  return (mesh->runtime->edit_mesh) ? 0 : mesh->corners_num;
}

static void bool_layer_begin(CollectionPropertyIterator *iter,
                             PointerRNA *ptr,
                             StringRef (*layername_func)(const StringRef uv_name, char *buffer))
{
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  Mesh *mesh = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  const StringRef name = layername_func(layer->name, buffer);
  if (mesh->runtime->edit_mesh) {
    rna_iterator_array_begin(iter, ptr, nullptr, sizeof(MBoolProperty), 0, 0, nullptr);
    return;
  }
  MBoolProperty *data = (MBoolProperty *)CustomData_get_layer_named_for_write(
      &mesh->corner_data, CD_PROP_BOOL, name, mesh->corners_num);
  if (!data) {
    rna_iterator_array_begin(iter, ptr, nullptr, sizeof(MBoolProperty), 0, 0, nullptr);
    return;
  }
  rna_iterator_array_begin(iter, ptr, data, sizeof(MBoolProperty), mesh->corners_num, 0, nullptr);
}

static bool bool_layer_lookup_int(PointerRNA *ptr,
                                  int index,
                                  PointerRNA *r_ptr,
                                  StringRef (*layername_func)(const StringRef uv_name,
                                                              char *buffer))
{
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  Mesh *mesh = rna_mesh(ptr);
  if (mesh->runtime->edit_mesh || index < 0 || index >= mesh->corners_num) {
    return 0;
  }
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  const StringRef name = layername_func(layer->name, buffer);
  MBoolProperty *data = (MBoolProperty *)CustomData_get_layer_named_for_write(
      &mesh->corner_data, CD_PROP_BOOL, name, mesh->corners_num);
  if (!data) {
    return false;
  }
  rna_pointer_create_with_ancestors(*ptr, &RNA_BoolAttributeValue, data + index, *r_ptr);
  return 1;
}

static int bool_layer_length(PointerRNA *ptr,
                             StringRef (*layername_func)(const StringRef uv_name, char *buffer))
{
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  Mesh *mesh = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  const StringRef name = layername_func(layer->name, buffer);
  if (mesh->runtime->edit_mesh) {
    return 0;
  }
  MBoolProperty *data = (MBoolProperty *)CustomData_get_layer_named_for_write(
      &mesh->corner_data, CD_PROP_BOOL, name, mesh->corners_num);
  if (!data) {
    return 0;
  }
  return mesh->corners_num;
}

static PointerRNA bool_layer_ensure(PointerRNA *ptr,
                                    StringRef (*layername_func)(const StringRef uv_name,
                                                                char *buffer))
{
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  Mesh *mesh = rna_mesh(ptr);
  if (mesh->runtime->edit_mesh) {
    return {};
  }
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  const StringRef name = layername_func(layer->name, buffer);
  int index = CustomData_get_named_layer_index(&mesh->corner_data, CD_PROP_BOOL, name);
  if (index == -1) {
    CustomData_add_layer_named(
        &mesh->corner_data, CD_PROP_BOOL, CD_SET_DEFAULT, mesh->corners_num, name);
    index = CustomData_get_named_layer_index(&mesh->corner_data, CD_PROP_BOOL, name);
  }
  if (index == -1) {
    return {};
  }
  CustomDataLayer *bool_layer = &mesh->corner_data.layers[index];
  return RNA_pointer_create_discrete(&mesh->id, &RNA_BoolAttribute, bool_layer);
}

/* Collection accessors for pin. */
static void rna_MeshUVLoopLayer_pin_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  bool_layer_begin(iter, ptr, BKE_uv_map_pin_name_get);
}

static bool rna_MeshUVLoopLayer_pin_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  return bool_layer_lookup_int(ptr, index, r_ptr, BKE_uv_map_pin_name_get);
}

static int rna_MeshUVLoopLayer_pin_length(PointerRNA *ptr)
{
  return bool_layer_length(ptr, BKE_uv_map_pin_name_get);
}

static PointerRNA rna_MeshUVLoopLayer_pin_ensure(PointerRNA ptr)
{
  return bool_layer_ensure(&ptr, BKE_uv_map_pin_name_get);
}

static void rna_MeshUVLoopLayer_uv_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;

  rna_iterator_array_begin(iter,
                           ptr,
                           layer->data,
                           sizeof(float[2]),
                           (mesh->runtime->edit_mesh) ? 0 : mesh->corners_num,
                           0,
                           nullptr);
}

bool rna_MeshUVLoopLayer_uv_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  if (mesh->runtime->edit_mesh || index < 0 || index >= mesh->corners_num) {
    return 0;
  }
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_pointer_create_with_ancestors(
      *ptr, &RNA_Float2AttributeValue, (float *)layer->data + 2 * index, *r_ptr);
  return 1;
}

static bool rna_MeshUVLoopLayer_active_render_get(PointerRNA *ptr)
{
  return rna_CustomDataLayer_active_get(ptr, rna_mesh_ldata(ptr), CD_PROP_FLOAT2, 1);
}

static bool rna_MeshUVLoopLayer_active_get(PointerRNA *ptr)
{
  return rna_CustomDataLayer_active_get(ptr, rna_mesh_ldata(ptr), CD_PROP_FLOAT2, 0);
}

static bool rna_MeshUVLoopLayer_clone_get(PointerRNA *ptr)
{
  return rna_CustomDataLayer_clone_get(ptr, rna_mesh_ldata(ptr), CD_PROP_FLOAT2);
}

static void rna_MeshUVLoopLayer_active_render_set(PointerRNA *ptr, bool value)
{
  rna_CustomDataLayer_active_set(ptr, rna_mesh_ldata(ptr), value, CD_PROP_FLOAT2, 1);
}

static void rna_MeshUVLoopLayer_active_set(PointerRNA *ptr, bool value)
{
  rna_CustomDataLayer_active_set(ptr, rna_mesh_ldata(ptr), value, CD_PROP_FLOAT2, 0);
}

static void rna_MeshUVLoopLayer_clone_set(PointerRNA *ptr, bool value)
{
  rna_CustomDataLayer_clone_set(ptr, rna_mesh_ldata(ptr), value, CD_PROP_FLOAT2);
}

/* vertex_color_layers */

DEFINE_CUSTOMDATA_LAYER_COLLECTION(vertex_color, ldata, CD_PROP_BYTE_COLOR)

static PointerRNA rna_Mesh_vertex_color_active_get(PointerRNA *ptr)
{
  Mesh *mesh = (Mesh *)ptr->data;
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  CustomDataLayer *layer = BKE_attribute_search_for_write(
      owner, mesh->active_color_attribute, CD_MASK_PROP_BYTE_COLOR, ATTR_DOMAIN_MASK_CORNER);
  return RNA_pointer_create_with_parent(*ptr, &RNA_MeshLoopColorLayer, layer);
}

static void rna_Mesh_vertex_color_active_set(PointerRNA *ptr,
                                             const PointerRNA value,
                                             ReportList * /*reports*/)
{
  Mesh *mesh = (Mesh *)ptr->data;
  CustomDataLayer *layer = (CustomDataLayer *)value.data;

  if (!layer) {
    return;
  }

  BKE_id_attributes_active_color_set(&mesh->id, layer->name);
}

static int rna_Mesh_vertex_color_active_index_get(PointerRNA *ptr)
{
  Mesh *mesh = (Mesh *)ptr->data;
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  const CustomDataLayer *layer = BKE_attribute_search(
      owner, mesh->active_color_attribute, CD_MASK_PROP_BYTE_COLOR, ATTR_DOMAIN_MASK_CORNER);
  if (!layer) {
    return 0;
  }
  CustomData *ldata = rna_mesh_ldata(ptr);
  return layer - ldata->layers + CustomData_get_layer_index(ldata, CD_PROP_BYTE_COLOR);
}

static void rna_Mesh_vertex_color_active_index_set(PointerRNA *ptr, int value)
{
  Mesh *mesh = (Mesh *)ptr->data;
  CustomData *ldata = rna_mesh_ldata(ptr);

  if (value < 0 || value >= CustomData_number_of_layers(ldata, CD_PROP_BYTE_COLOR)) {
    fprintf(stderr, "Invalid loop byte attribute index %d\n", value);
    return;
  }

  CustomDataLayer *layer = ldata->layers + CustomData_get_layer_index(ldata, CD_PROP_BYTE_COLOR) +
                           value;

  BKE_id_attributes_active_color_set(&mesh->id, layer->name);
}

static void rna_MeshLoopColorLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(iter,
                           ptr,
                           layer->data,
                           sizeof(MLoopCol),
                           (mesh->runtime->edit_mesh) ? 0 : mesh->corners_num,
                           0,
                           nullptr);
}

static int rna_MeshLoopColorLayer_data_length(PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  return (mesh->runtime->edit_mesh) ? 0 : mesh->corners_num;
}

static bool rna_mesh_color_active_render_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const CustomDataLayer *layer = (const CustomDataLayer *)ptr->data;
  return mesh->default_color_attribute && STREQ(mesh->default_color_attribute, layer->name);
}

static bool rna_mesh_color_active_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const CustomDataLayer *layer = (const CustomDataLayer *)ptr->data;
  return mesh->active_color_attribute && STREQ(mesh->active_color_attribute, layer->name);
}

static void rna_mesh_color_active_render_set(PointerRNA *ptr, bool value)
{
  if (value == false) {
    return;
  }
  Mesh *mesh = (Mesh *)ptr->owner_id;
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  BKE_id_attributes_default_color_set(&mesh->id, layer->name);
}

static void rna_mesh_color_active_set(PointerRNA *ptr, bool value)
{
  if (value == false) {
    return;
  }
  Mesh *mesh = (Mesh *)ptr->owner_id;
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;

  BKE_id_attributes_active_color_set(&mesh->id, layer->name);
}

/* Skin vertices */
DEFINE_CUSTOMDATA_LAYER_COLLECTION(skin_vertice, vdata, CD_MVERT_SKIN)

static std::optional<std::string> rna_MeshSkinVertexLayer_path(const PointerRNA *ptr)
{
  const CustomDataLayer *cdl = static_cast<const CustomDataLayer *>(ptr->data);
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return fmt::format("skin_vertices[\"{}\"]", name_esc);
}

static std::optional<std::string> rna_VertCustomData_data_path(const PointerRNA *ptr,
                                                               const char *collection,
                                                               int type);
static std::optional<std::string> rna_MeshSkinVertex_path(const PointerRNA *ptr)
{
  return rna_VertCustomData_data_path(ptr, "skin_vertices", CD_MVERT_SKIN);
}

static void rna_MeshSkinVertexLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(iter,
                           ptr,
                           layer->data,
                           sizeof(MVertSkin),
                           (mesh->runtime->edit_mesh) ? 0 : mesh->verts_num,
                           0,
                           nullptr);
}

static int rna_MeshSkinVertexLayer_data_length(PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  return (mesh->runtime->edit_mesh) ? 0 : mesh->verts_num;
}

/* End skin vertices */

/* poly.vertices - this is faked loop access for convenience */
static int rna_MeshPoly_vertices_get_length(const PointerRNA *ptr,
                                            int length[RNA_MAX_ARRAY_DIMENSION])
{
  const int *poly_offset_p = static_cast<const int *>(ptr->data);
  const int poly_start = *poly_offset_p;
  const int poly_size = *(poly_offset_p + 1) - poly_start;
  /* NOTE: raw access uses dummy item, this _could_ crash,
   * watch out for this, #MFace uses it but it can't work here. */
  return (length[0] = poly_size);
}

static void rna_MeshPoly_vertices_get(PointerRNA *ptr, int *values)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int *poly_offset_p = static_cast<const int *>(ptr->data);
  const int poly_start = *poly_offset_p;
  const int poly_size = *(poly_offset_p + 1) - poly_start;
  memcpy(values, &mesh->corner_verts()[poly_start], sizeof(int) * poly_size);
}

static int rna_MeshPolygon_loop_total_get(PointerRNA *ptr)
{
  const int *data = static_cast<const int *>(ptr->data);
  return *(data + 1) - *data;
}

static void rna_MeshPoly_vertices_set(PointerRNA *ptr, const int *values)
{
  Mesh *mesh = rna_mesh(ptr);
  const int *poly_offset_p = static_cast<const int *>(ptr->data);
  const int poly_start = *poly_offset_p;
  const int poly_size = *(poly_offset_p + 1) - poly_start;
  memcpy(&mesh->corner_verts_for_write()[poly_start], values, sizeof(int) * poly_size);
}

/* disabling, some importers don't know the total material count when assigning materials */
#  if 0
static void rna_MeshPoly_material_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  Mesh *mesh = rna_mesh(ptr);
  *min = 0;
  *max = max_ii(0, mesh->totcol - 1);
}
#  endif

static bool rna_MeshEdge_hide_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshEdge_index_get(ptr);
  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const blender::VArray hide_edge = *attributes.lookup_or_default<bool>(
      ".hide_edge", blender::bke::AttrDomain::Edge, false);
  return hide_edge[index];
}

static void rna_MeshEdge_hide_set(PointerRNA *ptr, bool value)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshEdge_index_get(ptr);
  blender::bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  blender::bke::AttributeWriter hide_edge = attributes.lookup_or_add_for_write<bool>(
      ".hide_edge", blender::bke::AttrDomain::Edge, blender::bke::AttributeInitDefaultValue());
  hide_edge.varray.set(index, value);
  hide_edge.finish();
}

static bool rna_MeshEdge_select_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshEdge_index_get(ptr);
  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const blender::VArray select_edge = *attributes.lookup_or_default<bool>(
      ".select_edge", blender::bke::AttrDomain::Edge, false);
  return select_edge[index];
}

static void rna_MeshEdge_select_set(PointerRNA *ptr, bool value)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshEdge_index_get(ptr);
  blender::bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  blender::bke::AttributeWriter select_edge = attributes.lookup_or_add_for_write<bool>(
      ".select_edge", blender::bke::AttrDomain::Edge, blender::bke::AttributeInitDefaultValue());
  select_edge.varray.set(index, value);
  select_edge.finish();
}

static bool rna_MeshEdge_use_edge_sharp_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshEdge_index_get(ptr);
  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const blender::VArray sharp_edge = *attributes.lookup_or_default<bool>(
      "sharp_edge", blender::bke::AttrDomain::Edge, false);
  return sharp_edge[index];
}

static void rna_MeshEdge_use_edge_sharp_set(PointerRNA *ptr, bool value)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshEdge_index_get(ptr);
  blender::bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  blender::bke::AttributeWriter sharp_edge = attributes.lookup_or_add_for_write<bool>(
      "sharp_edge", blender::bke::AttrDomain::Edge, blender::bke::AttributeInitDefaultValue());
  sharp_edge.varray.set(index, value);
  sharp_edge.finish();
}

static bool rna_MeshEdge_use_seam_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshEdge_index_get(ptr);
  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const blender::VArray uv_seam = *attributes.lookup_or_default<bool>(
      "uv_seam", blender::bke::AttrDomain::Edge, false);
  return uv_seam[index];
}

static void rna_MeshEdge_use_seam_set(PointerRNA *ptr, bool value)
{
  Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshEdge_index_get(ptr);
  blender::bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  blender::bke::AttributeWriter uv_seam = attributes.lookup_or_add_for_write<bool>(
      "uv_seam", blender::bke::AttrDomain::Edge, blender::bke::AttributeInitDefaultValue());
  uv_seam.varray.set(index, value);
  uv_seam.finish();
}

static bool rna_MeshEdge_is_loose_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const int index = rna_MeshEdge_index_get(ptr);
  const blender::bke::LooseEdgeCache &loose_edges = mesh->loose_edges();
  return loose_edges.count > 0 && loose_edges.is_loose_bits[index];
}

static int rna_MeshLoopTriangle_material_index_get(PointerRNA *ptr)
{
  using namespace blender;
  const Mesh *mesh = rna_mesh(ptr);
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArray material_indices = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Face, 0);
  return material_indices[rna_MeshLoopTriangle_polygon_index_get(ptr)];
}

static bool rna_MeshLoopTriangle_use_smooth_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const blender::VArray sharp_face = *attributes.lookup_or_default<bool>(
      "sharp_face", blender::bke::AttrDomain::Face, false);
  return !sharp_face[rna_MeshLoopTriangle_polygon_index_get(ptr)];
}

/* path construction */

static std::optional<std::string> rna_VertexGroupElement_path(const PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr); /* XXX not always! */
  const MDeformWeight *dw = (MDeformWeight *)ptr->data;
  const MDeformVert *dvert = mesh->deform_verts().data();
  int a, b;

  for (a = 0; a < mesh->verts_num; a++, dvert++) {
    for (b = 0; b < dvert->totweight; b++) {
      if (dw == &dvert->dw[b]) {
        return fmt::format("vertices[{}].groups[{}]", a, b);
      }
    }
  }

  return std::nullopt;
}

static std::optional<std::string> rna_MeshPolygon_path(const PointerRNA *ptr)
{
  return fmt::format("polygons[{}]", rna_MeshPolygon_index_get(const_cast<PointerRNA *>(ptr)));
}

static std::optional<std::string> rna_MeshLoopTriangle_path(const PointerRNA *ptr)
{
  const int index = rna_MeshLoopTriangle_index_get(const_cast<PointerRNA *>(ptr));
  return fmt::format("loop_triangles[{}]", index);
}

static std::optional<std::string> rna_MeshEdge_path(const PointerRNA *ptr)
{
  return fmt::format("edges[{}]", rna_MeshEdge_index_get(const_cast<PointerRNA *>(ptr)));
}

static std::optional<std::string> rna_MeshLoop_path(const PointerRNA *ptr)
{
  return fmt::format("loops[{}]", rna_MeshLoop_index_get(const_cast<PointerRNA *>(ptr)));
}

static std::optional<std::string> rna_MeshVertex_path(const PointerRNA *ptr)
{
  return fmt::format("vertices[{}]", rna_MeshVertex_index_get(const_cast<PointerRNA *>(ptr)));
}

static std::optional<std::string> rna_VertCustomData_data_path(const PointerRNA *ptr,
                                                               const char *collection,
                                                               int type)
{
  const CustomDataLayer *cdl;
  const Mesh *mesh = rna_mesh(ptr);
  const CustomData *vdata = rna_mesh_vdata(ptr);
  int a, b, totvert = (mesh->runtime->edit_mesh) ? 0 : mesh->verts_num;

  for (cdl = vdata->layers, a = 0; a < vdata->totlayer; cdl++, a++) {
    if (cdl->type == type) {
      b = ((char *)ptr->data - ((char *)cdl->data)) / CustomData_sizeof(eCustomDataType(type));
      if (b >= 0 && b < totvert) {
        char name_esc[sizeof(cdl->name) * 2];
        BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
        return fmt::format("{}[\"{}\"].data[{}]", collection, name_esc, b);
      }
    }
  }

  return std::nullopt;
}

static std::optional<std::string> rna_LoopCustomData_data_path(const PointerRNA *ptr,
                                                               const char *collection,
                                                               int type)
{
  const CustomDataLayer *cdl;
  const Mesh *mesh = rna_mesh(ptr);
  const CustomData *ldata = rna_mesh_ldata(ptr);
  int a, b, totloop = (mesh->runtime->edit_mesh) ? 0 : mesh->corners_num;

  for (cdl = ldata->layers, a = 0; a < ldata->totlayer; cdl++, a++) {
    if (cdl->type == type) {
      b = ((char *)ptr->data - ((char *)cdl->data)) / CustomData_sizeof(eCustomDataType(type));
      if (b >= 0 && b < totloop) {
        char name_esc[sizeof(cdl->name) * 2];
        BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
        return fmt::format("{}[\"{}\"].data[{}]", collection, name_esc, b);
      }
    }
  }

  return std::nullopt;
}

static void rna_Mesh_vertices_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  rna_iterator_array_begin(iter,
                           ptr,
                           mesh->vert_positions_for_write().data(),
                           sizeof(blender::float3),
                           mesh->verts_num,
                           false,
                           nullptr);
}
static int rna_Mesh_vertices_length(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  return mesh->verts_num;
}
bool rna_Mesh_vertices_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  if (index < 0 || index >= mesh->verts_num) {
    return false;
  }
  rna_pointer_create_with_ancestors(
      *ptr, &RNA_MeshVertex, &mesh->vert_positions_for_write()[index], *r_ptr);
  return true;
}

static void rna_Mesh_edges_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  using namespace blender;
  Mesh *mesh = rna_mesh(ptr);
  blender::MutableSpan<blender::int2> edges = mesh->edges_for_write();
  rna_iterator_array_begin(
      iter, ptr, edges.data(), sizeof(blender::int2), edges.size(), false, nullptr);
}
static int rna_Mesh_edges_length(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  return mesh->edges_num;
}
bool rna_Mesh_edges_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  using namespace blender;
  Mesh *mesh = rna_mesh(ptr);
  if (index < 0 || index >= mesh->edges_num) {
    return false;
  }
  blender::MutableSpan<blender::int2> edges = mesh->edges_for_write();
  rna_pointer_create_with_ancestors(*ptr, &RNA_MeshEdge, &edges[index], *r_ptr);
  return true;
}

static void rna_Mesh_polygons_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  rna_iterator_array_begin(iter,
                           ptr,
                           mesh->face_offsets_for_write().data(),
                           sizeof(int),
                           mesh->faces_num,
                           false,
                           nullptr);
}
static int rna_Mesh_polygons_length(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  return mesh->faces_num;
}
bool rna_Mesh_polygons_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  if (index < 0 || index >= mesh->faces_num) {
    return false;
  }
  rna_pointer_create_with_ancestors(
      *ptr, &RNA_MeshPolygon, &mesh->face_offsets_for_write()[index], *r_ptr);
  return true;
}

static void rna_Mesh_loops_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  rna_iterator_array_begin(iter,
                           ptr,
                           mesh->corner_verts_for_write().data(),
                           sizeof(int),
                           mesh->corners_num,
                           false,
                           nullptr);
}
static int rna_Mesh_loops_length(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  return mesh->corners_num;
}
bool rna_Mesh_loops_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  if (index < 0 || index >= mesh->corners_num) {
    return false;
  }
  rna_pointer_create_with_ancestors(
      *ptr, &RNA_MeshLoop, &mesh->corner_verts_for_write()[index], *r_ptr);
  return true;
}

static int rna_Mesh_normals_domain_get(PointerRNA *ptr)
{
  return int(rna_mesh(ptr)->normals_domain());
}

static void rna_Mesh_vertex_normals_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const blender::Span<blender::float3> normals = mesh->vert_normals();
  rna_iterator_array_begin(iter,
                           ptr,
                           const_cast<blender::float3 *>(normals.data()),
                           sizeof(blender::float3),
                           normals.size(),
                           false,
                           nullptr);
}

static int rna_Mesh_vertex_normals_length(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  return mesh->verts_num;
}

bool rna_Mesh_vertex_normals_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  if (index < 0 || index >= mesh->verts_num) {
    return false;
  }
  /* Casting away const is okay because this RNA type doesn't allow changing the value. */
  rna_pointer_create_with_ancestors(*ptr,
                                    &RNA_MeshNormalValue,
                                    const_cast<blender::float3 *>(&mesh->vert_normals()[index]),
                                    *r_ptr);
  return true;
}

static void rna_Mesh_poly_normals_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const blender::Span<blender::float3> normals = mesh->face_normals();
  rna_iterator_array_begin(iter,
                           ptr,
                           const_cast<blender::float3 *>(normals.data()),
                           sizeof(blender::float3),
                           normals.size(),
                           false,
                           nullptr);
}

static int rna_Mesh_poly_normals_length(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  return mesh->faces_num;
}

bool rna_Mesh_poly_normals_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  if (index < 0 || index >= mesh->faces_num) {
    return false;
  }
  /* Casting away const is okay because this RNA type doesn't allow changing the value. */
  rna_pointer_create_with_ancestors(*ptr,
                                    &RNA_MeshNormalValue,
                                    const_cast<blender::float3 *>(&mesh->face_normals()[index]),
                                    *r_ptr);
  return true;
}

static void rna_Mesh_corner_normals_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const blender::Span<blender::float3> normals = mesh->corner_normals();
  if (normals.is_empty()) {
    iter->valid = false;
    return;
  }
  rna_iterator_array_begin(
      iter, ptr, (void *)normals.data(), sizeof(float[3]), mesh->corners_num, false, nullptr);
}

static int rna_Mesh_corner_normals_length(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  return mesh->corners_num;
}

bool rna_Mesh_corner_normals_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  const blender::Span<blender::float3> normals = mesh->corner_normals();
  if (index < 0 || index >= mesh->corners_num || normals.is_empty()) {
    return false;
  }
  /* Casting away const is okay because this RNA type doesn't allow changing the value. */
  rna_pointer_create_with_ancestors(
      *ptr, &RNA_MeshNormalValue, const_cast<blender::float3 *>(&normals[index]), *r_ptr);
  return true;
}

static std::optional<std::string> rna_MeshUVLoop_path(const PointerRNA *ptr)
{
  return rna_LoopCustomData_data_path(ptr, "uv_layers", CD_PROP_FLOAT2);
}
/**
 * The `rna_MeshUVLoop_*_get/set()` functions get passed a pointer to
 * the (float2) uv attribute. This is for historical reasons because
 * the API used to wrap `MLoopUV`, which contained the UV and all the selection
 * pin states in a single struct. But since that struct no longer exists and
 * we still can use only a single pointer to access these, we need to look up
 * the original attribute layer and the index of the UV in it to be able to
 * find the associated bool layers. So we scan the available #float2 layers
 * to find into which layer the pointer we got passed points.
 */
static bool get_uv_index_and_layer(const PointerRNA *ptr,
                                   int *r_uv_map_index,
                                   int *r_index_in_attribute)
{
  const Mesh *mesh = rna_mesh(ptr);
  const blender::float2 *uv_coord = static_cast<const blender::float2 *>(ptr->data);

  /* We don't know from which attribute the RNA pointer is from, so we need to scan them all. */
  const int uv_layers_num = CustomData_number_of_layers(&mesh->corner_data, CD_PROP_FLOAT2);
  for (int layer_i = 0; layer_i < uv_layers_num; layer_i++) {
    const blender::float2 *layer_data = static_cast<const blender::float2 *>(
        CustomData_get_layer_n(&mesh->corner_data, CD_PROP_FLOAT2, layer_i));
    const ptrdiff_t index = uv_coord - layer_data;
    if (index >= 0 && index < mesh->corners_num) {
      *r_uv_map_index = layer_i;
      *r_index_in_attribute = index;
      return true;
    }
  }
  /* This can happen if the Customdata arrays were re-allocated between obtaining the
   * Python object and accessing it. */
  return false;
}

static bool rna_MeshUVLoop_pin_uv_get(PointerRNA *ptr)
{
  const Mesh *mesh = rna_mesh(ptr);
  int uv_map_index;
  int loop_index;
  blender::VArray<bool> pin_uv;
  if (get_uv_index_and_layer(ptr, &uv_map_index, &loop_index)) {
    pin_uv = ED_mesh_uv_map_pin_layer_get(mesh, uv_map_index);
  }
  return pin_uv ? pin_uv[loop_index] : false;
}

static void rna_MeshUVLoop_pin_uv_set(PointerRNA *ptr, const bool value)
{
  Mesh *mesh = rna_mesh(ptr);
  int uv_map_index;
  int loop_index;
  if (get_uv_index_and_layer(ptr, &uv_map_index, &loop_index)) {
    blender::bke::AttributeWriter<bool> pin_uv = ED_mesh_uv_map_pin_layer_ensure(mesh,
                                                                                 uv_map_index);
    pin_uv.varray.set(loop_index, value);
    pin_uv.finish();
  }
}

static void rna_MeshUVLoop_uv_get(PointerRNA *ptr, float *value)
{
  copy_v2_v2(value, static_cast<const float *>(ptr->data));
}

static void rna_MeshUVLoop_uv_set(PointerRNA *ptr, const float *value)
{
  copy_v2_v2(static_cast<float *>(ptr->data), value);
}

static std::optional<std::string> rna_MeshLoopColorLayer_path(const PointerRNA *ptr)
{
  const CustomDataLayer *cdl = static_cast<const CustomDataLayer *>(ptr->data);
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return fmt::format("vertex_colors[\"{}\"]", name_esc);
}

static std::optional<std::string> rna_MeshColor_path(const PointerRNA *ptr)
{
  return rna_LoopCustomData_data_path(ptr, "vertex_colors", CD_PROP_BYTE_COLOR);
}

/***************************************/

static int rna_Mesh_tot_vert_get(PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  return mesh->runtime->edit_mesh ? mesh->runtime->edit_mesh->bm->totvertsel : 0;
}
static int rna_Mesh_tot_edge_get(PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  return mesh->runtime->edit_mesh ? mesh->runtime->edit_mesh->bm->totedgesel : 0;
}
static int rna_Mesh_tot_face_get(PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  return mesh->runtime->edit_mesh ? mesh->runtime->edit_mesh->bm->totfacesel : 0;
}

static PointerRNA rna_Mesh_vertex_color_new(Mesh *mesh,
                                            ReportList *reports,
                                            const char *name,
                                            const bool do_init)
{
  std::string new_name = ED_mesh_color_add(mesh, name, false, do_init, reports);
  if (new_name.empty()) {
    return {};
  }

  if (!mesh->active_color_attribute) {
    mesh->active_color_attribute = BLI_strdup(new_name.c_str());
  }
  if (!mesh->default_color_attribute) {
    mesh->default_color_attribute = BLI_strdup(new_name.c_str());
  }
  CustomData *ldata = rna_mesh_ldata_helper(mesh);
  const int layer_index = CustomData_get_named_layer_index(ldata, CD_PROP_BYTE_COLOR, new_name);
  CustomDataLayer *cdl = &ldata->layers[layer_index];
  return RNA_pointer_create_discrete(&mesh->id, &RNA_MeshLoopColorLayer, cdl);
}

static void rna_Mesh_vertex_color_remove(Mesh *mesh, ReportList *reports, CustomDataLayer *layer)
{
  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  BKE_attribute_remove(owner, layer->name, reports);
}

static PointerRNA rna_Mesh_uv_layers_new(Mesh *mesh,
                                         ReportList *reports,
                                         const char *name,
                                         const bool do_init)
{
  CustomData *ldata;
  CustomDataLayer *cdl = nullptr;
  int index = ED_mesh_uv_add(mesh, name, false, do_init, reports);

  if (index != -1) {
    ldata = rna_mesh_ldata_helper(mesh);
    cdl = &ldata->layers[CustomData_get_layer_index_n(ldata, CD_PROP_FLOAT2, index)];
  }

  PointerRNA ptr = RNA_pointer_create_discrete(&mesh->id, &RNA_MeshUVLoopLayer, cdl);
  return ptr;
}

static void rna_Mesh_uv_layers_remove(Mesh *mesh, ReportList *reports, CustomDataLayer *layer)
{
  using namespace blender;
  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  if (mesh->runtime->edit_mesh) {
    BMesh &bm = *mesh->runtime->edit_mesh->bm;
    if (!CustomData_has_layer_named(&bm.ldata, CD_PROP_FLOAT2, layer->name)) {
      BKE_reportf(reports, RPT_ERROR, "UV map '%s' not found", layer->name);
      return;
    }
  }
  else {
    bke::AttributeAccessor attributes = *owner.get_accessor();
    if (!attributes.contains(layer->name)) {
      BKE_reportf(reports, RPT_ERROR, "UV map '%s' not found", layer->name);
      return;
    }
  }
  BKE_attribute_remove(owner, layer->name, reports);
}

static bool rna_Mesh_is_editmode_get(PointerRNA *ptr)
{
  Mesh *mesh = rna_mesh(ptr);
  return (mesh->runtime->edit_mesh != nullptr);
}

static bool rna_Mesh_materials_override_apply(Main *bmain,
                                              RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  PointerRNA *ptr_item_dst = &rnaapply_ctx.ptr_item_dst;
  PointerRNA *ptr_item_src = &rnaapply_ctx.ptr_item_src;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  BLI_assert_msg(opop->operation == LIBOVERRIDE_OP_REPLACE,
                 "Unsupported RNA override operation on collections' objects");
  UNUSED_VARS_NDEBUG(opop);

  Mesh *mesh_dst = (Mesh *)ptr_dst->owner_id;

  if (ptr_item_dst->type == nullptr || ptr_item_src->type == nullptr) {
    // BLI_assert_msg(0, "invalid source or destination material.");
    return false;
  }

  Material *mat_dst = static_cast<Material *>(ptr_item_dst->data);
  Material *mat_src = static_cast<Material *>(ptr_item_src->data);

  if (mat_src == mat_dst) {
    return true;
  }

  bool is_modified = false;
  for (int i = 0; i < mesh_dst->totcol; i++) {
    if (mesh_dst->mat[i] == mat_dst) {
      id_us_min(&mat_dst->id);
      mesh_dst->mat[i] = mat_src;
      id_us_plus(&mat_src->id);
      is_modified = true;
    }
  }

  if (is_modified) {
    RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
  }

  return true;
}

/** \} */

#else

/* -------------------------------------------------------------------- */
/** \name RNA Mesh Definition
 * \{ */

static void rna_def_mvert_group(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "VertexGroupElement", nullptr);
  RNA_def_struct_sdna(srna, "MDeformWeight");
  RNA_def_struct_path_func(srna, "rna_VertexGroupElement_path");
  RNA_def_struct_ui_text(
      srna, "Vertex Group Element", "Weight value of a vertex in a vertex group");
  RNA_def_struct_ui_icon(srna, ICON_GROUP_VERTEX);

  /* we can't point to actual group, it is in the object and so
   * there is no unique group to point to, hence the index */
  prop = RNA_def_property(srna, "group", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "def_nr");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Group Index", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Weight", "Vertex Weight");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_edit_weight");
}

static void rna_def_mvert(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshVertex", nullptr);
  RNA_def_struct_ui_text(srna, "Mesh Vertex", "Vertex in a Mesh data-block");
  RNA_def_struct_path_func(srna, "rna_MeshVertex_path");
  RNA_def_struct_ui_icon(srna, ICON_VERTEXSEL);

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop, "rna_MeshVertex_co_get", "rna_MeshVertex_co_set", nullptr);
  RNA_def_property_ui_text(prop, "Position", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_positions_tag");

  prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshVertex_normal_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Normal", "Vertex Normal");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_MeshVertex_select_get", "rna_MeshVertex_select_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Select", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Hide", "");
  RNA_def_property_boolean_funcs(prop, "rna_MeshVertex_hide_get", "rna_MeshVertex_hide_set");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

  prop = RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshVertex_groups_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "VertexGroupElement");
  RNA_def_property_ui_text(
      prop, "Groups", "Weights for the vertex groups this vertex is member of");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MeshVertex_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Index", "Index of this vertex");

  prop = RNA_def_property(srna, "undeformed_co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop,
      "Undeformed Location",
      "For meshes with modifiers applied, the coordinate of the vertex with no deforming "
      "modifiers applied, as used for generated texture coordinates");
  RNA_def_property_float_funcs(prop, "rna_MeshVertex_undeformed_co_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_medge(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshEdge", nullptr);
  RNA_def_struct_sdna(srna, "vec2i");
  RNA_def_struct_ui_text(srna, "Mesh Edge", "Edge in a Mesh data-block");
  RNA_def_struct_path_func(srna, "rna_MeshEdge_path");
  RNA_def_struct_ui_icon(srna, ICON_EDGESEL);

  prop = RNA_def_property(srna, "vertices", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 2);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Vertices", "Vertex indices");
  /* XXX allows creating invalid meshes */

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_MeshEdge_select_get", "rna_MeshEdge_select_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Select", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Hide", "");
  RNA_def_property_boolean_funcs(prop, "rna_MeshEdge_hide_get", "rna_MeshEdge_hide_set");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

  prop = RNA_def_property(srna, "use_seam", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_MeshEdge_use_seam_get", "rna_MeshEdge_use_seam_set");
  RNA_def_property_ui_text(prop, "Seam", "Seam edge for UV unwrapping");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

  prop = RNA_def_property(srna, "use_edge_sharp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_MeshEdge_use_edge_sharp_get", "rna_MeshEdge_use_edge_sharp_set");
  RNA_def_property_ui_text(prop, "Sharp", "Sharp edge for shading");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "is_loose", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_MeshEdge_is_loose_get", nullptr);
  RNA_def_property_ui_text(prop, "Loose", "Edge is not connected to any faces");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MeshEdge_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Index", "Index of this edge");
}

static void rna_def_mlooptri(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  const int splitnor_dim[] = {3, 3};

  srna = RNA_def_struct(brna, "MeshLoopTriangle", nullptr);
  RNA_def_struct_sdna(srna, "vec3i");
  RNA_def_struct_ui_text(srna, "Mesh Loop Triangle", "Tessellated triangle in a Mesh data-block");
  RNA_def_struct_path_func(srna, "rna_MeshLoopTriangle_path");
  RNA_def_struct_ui_icon(srna, ICON_FACESEL);

  prop = RNA_def_property(srna, "vertices", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_array(prop, 3);
  RNA_def_property_int_funcs(prop, "rna_MeshLoopTriangle_verts_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Vertices", "Indices of triangle vertices");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "loops", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Loops", "Indices of mesh loops that make up the triangle");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "polygon_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop, "rna_MeshLoopTriangle_polygon_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Polygon", "Index of mesh face that the triangle is a part of");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoopTriangle_normal_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Triangle Normal", "Local space unit length normal vector for this triangle");

  prop = RNA_def_property(srna, "split_normals", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_multi_array(prop, 2, splitnor_dim);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoopTriangle_split_normals_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop,
      "Custom Normals",
      "Local space unit length custom normal vectors of the face corners of this triangle");

  prop = RNA_def_property(srna, "area", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoopTriangle_area_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Triangle Area", "Area of this triangle");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MeshLoopTriangle_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Index", "Index of this loop triangle");

  prop = RNA_def_property(srna, "material_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MeshLoopTriangle_material_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Material Index", "Material slot index of this triangle");

  prop = RNA_def_property(srna, "use_smooth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_MeshLoopTriangle_use_smooth_get", nullptr);
  RNA_def_property_ui_text(prop, "Smooth", "");
}

static void rna_def_mloop(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshLoop", nullptr);
  RNA_def_struct_ui_text(srna, "Mesh Loop", "Loop in a Mesh data-block");
  RNA_def_struct_path_func(srna, "rna_MeshLoop_path");
  RNA_def_struct_ui_icon(srna, ICON_EDGESEL);

  prop = RNA_def_property(srna, "vertex_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(
      prop, "rna_MeshLoop_vertex_index_get", "rna_MeshLoop_vertex_index_set", nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Vertex", "Vertex index");

  prop = RNA_def_property(srna, "edge_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(
      prop, "rna_MeshLoop_edge_index_get", "rna_MeshLoop_edge_index_set", nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Edge", "Edge index");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MeshLoop_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Index", "Index of this loop");

  prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_float_funcs(prop, "rna_MeshLoop_normal_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop,
                           "Normal",
                           "The normal direction of the face corner, taking into account sharp "
                           "faces, sharp edges, and custom normal data");

  prop = RNA_def_property(srna, "tangent", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoop_tangent_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop,
                           "Tangent",
                           "Local space unit length tangent vector of this vertex for this face "
                           "(must be computed beforehand using calc_tangents)");

  prop = RNA_def_property(srna, "bitangent_sign", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoop_bitangent_sign_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop,
      "Bitangent Sign",
      "Sign of the bitangent vector of this vertex for this face (must be computed "
      "beforehand using calc_tangents, bitangent = bitangent_sign * cross(normal, tangent))");

  prop = RNA_def_property(srna, "bitangent", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoop_bitangent_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop,
      "Bitangent",
      "Bitangent vector of this vertex for this face (must be computed beforehand using "
      "calc_tangents, use it only if really needed, slower access than bitangent_sign)");
}

static void rna_def_mpolygon(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "MeshPolygon", nullptr);
  RNA_def_struct_sdna(srna, "MIntProperty");
  RNA_def_struct_ui_text(srna, "Mesh Polygon", "Polygon in a Mesh data-block");
  RNA_def_struct_path_func(srna, "rna_MeshPolygon_path");
  RNA_def_struct_ui_icon(srna, ICON_FACESEL);

  /* Faked, actually access to loop vertex values, don't this way because manually setting up
   * vertex/edge per loop is very low level.
   * Instead we setup face sizes, assign indices, then calc edges automatic when creating
   * meshes from rna/py. */
  prop = RNA_def_property(srna, "vertices", PROP_INT, PROP_UNSIGNED);
  /* Eek, this is still used in some cases but in fact we don't want to use it at all here. */
  RNA_def_property_array(prop, 3);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_dynamic_array_funcs(prop, "rna_MeshPoly_vertices_get_length");
  RNA_def_property_int_funcs(
      prop, "rna_MeshPoly_vertices_get", "rna_MeshPoly_vertices_set", nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Vertices", "Vertex indices");

  /* these are both very low level access */
  prop = RNA_def_property(srna, "loop_start", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "i");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Loop Start", "Index of the first loop of this face");
  /* also low level */
  prop = RNA_def_property(srna, "loop_total", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MeshPolygon_loop_total_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Loop Total", "Number of loops used by this face");

  prop = RNA_def_property(srna, "material_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(
      prop, "rna_MeshPolygon_material_index_get", "rna_MeshPolygon_material_index_set", nullptr);
  RNA_def_property_ui_text(prop, "Material Index", "Material slot index of this face");
#  if 0
  RNA_def_property_int_funcs(prop, nullptr, nullptr, "rna_MeshPoly_material_index_range");
#  endif
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_MeshPolygon_select_get", "rna_MeshPolygon_select_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Select", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Hide", "");
  RNA_def_property_boolean_funcs(prop, "rna_MeshPolygon_hide_get", "rna_MeshPolygon_hide_set");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

  prop = RNA_def_property(srna, "use_smooth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_MeshPolygon_use_smooth_get", "rna_MeshPolygon_use_smooth_set");
  RNA_def_property_ui_text(prop, "Smooth", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshPolygon_normal_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Polygon Normal", "Local space unit length normal vector for this face");

  prop = RNA_def_property(srna, "center", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshPolygon_center_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Polygon Center", "Center of this face");

  prop = RNA_def_property(srna, "area", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshPolygon_area_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Polygon Area", "Read only area of this face");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MeshPolygon_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Index", "Index of this face");

  func = RNA_def_function(srna, "flip", "rna_MeshPolygon_flip");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Invert winding of this face (flip its normal)");
}

/* mesh.loop_uvs */
static void rna_def_mloopuv(BlenderRNA *brna)
{
  FunctionRNA *func;
  StructRNA *srna;
  PropertyRNA *prop, *parm;

  srna = RNA_def_struct(brna, "MeshUVLoopLayer", nullptr);
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_path_func(srna, "rna_MeshUVLoopLayer_path");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshUVLoop");
  RNA_def_property_ui_text(
      prop,
      "MeshUVLoop (Deprecated)",
      "Deprecated, use 'uv', 'vertex_select', 'edge_select' or 'pin' properties instead");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshUVLoopLayer_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_MeshUVLoopLayer_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_MeshLoopLayer_name_set");
  RNA_def_property_string_maxlength(prop, MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX);
  RNA_def_property_ui_text(prop, "Name", "Name of UV map");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_MeshUVLoopLayer_active_get", "rna_MeshUVLoopLayer_active_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Active", "Set the map as active for display and editing");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "active_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "active_rnd", 0);
  RNA_def_property_boolean_funcs(
      prop, "rna_MeshUVLoopLayer_active_render_get", "rna_MeshUVLoopLayer_active_render_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Active Render", "Set the UV map as active for rendering");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "active_clone", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "active_clone", 0);
  RNA_def_property_boolean_funcs(
      prop, "rna_MeshUVLoopLayer_clone_get", "rna_MeshUVLoopLayer_clone_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Active Clone", "Set the map as active for cloning");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "uv", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Float2AttributeValue");
  RNA_def_property_ui_text(prop, "UV", "UV coordinates on face corners");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshUVLoopLayer_uv_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_MeshUVLoopLayer_data_length",
                                    "rna_MeshUVLoopLayer_uv_lookup_int",
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "pin", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "BoolAttributeValue");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "UV Pin", "UV pinned state in the UV editor");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshUVLoopLayer_pin_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_MeshUVLoopLayer_pin_length",
                                    "rna_MeshUVLoopLayer_pin_lookup_int",
                                    nullptr,
                                    nullptr);

  func = RNA_def_function(srna, "pin_ensure", "rna_MeshUVLoopLayer_pin_ensure");
  RNA_def_function_flag(func, FUNC_SELF_AS_RNA);
  parm = RNA_def_pointer(func, "layer", "BoolAttribute", "", "The boolean attribute");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  srna = RNA_def_struct(brna, "MeshUVLoop", nullptr);
  RNA_def_struct_ui_text(
      srna, "Mesh UV Layer", "(Deprecated) Layer of UV coordinates in a Mesh data-block");
  RNA_def_struct_path_func(srna, "rna_MeshUVLoop_path");

  prop = RNA_def_property(srna, "uv", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_funcs(prop, "rna_MeshUVLoop_uv_get", "rna_MeshUVLoop_uv_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "pin_uv", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_MeshUVLoop_pin_uv_get", "rna_MeshUVLoop_pin_uv_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "UV Pinned", "");
}

static void rna_def_mloopcol(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshLoopColorLayer", nullptr);
  RNA_def_struct_ui_text(
      srna, "Mesh Vertex Color Layer", "Layer of vertex colors in a Mesh data-block");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_path_func(srna, "rna_MeshLoopColorLayer_path");
  RNA_def_struct_ui_icon(srna, ICON_GROUP_VCOL);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_MeshLoopLayer_name_set");
  RNA_def_property_string_maxlength(prop, MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX);
  RNA_def_property_ui_text(prop, "Name", "Name of Vertex color layer");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_mesh_color_active_get", "rna_mesh_color_active_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Active", "Sets the layer as active for display and editing");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "active_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "active_rnd", 0);
  RNA_def_property_boolean_funcs(
      prop, "rna_mesh_color_active_render_get", "rna_mesh_color_active_render_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Active Render", "Sets the layer as active for rendering");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshLoopColor");
  RNA_def_property_ui_text(prop, "Data", "");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshLoopColorLayer_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_MeshLoopColorLayer_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);

  srna = RNA_def_struct(brna, "MeshLoopColor", nullptr);
  RNA_def_struct_sdna(srna, "MLoopCol");
  RNA_def_struct_ui_text(srna, "Mesh Vertex Color", "Vertex loop colors in a Mesh");
  RNA_def_struct_path_func(srna, "rna_MeshColor_path");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_funcs(
      prop, "rna_MeshLoopColor_color_get", "rna_MeshLoopColor_color_set", nullptr);
  RNA_def_property_ui_text(prop, "Color", "Color in sRGB color space");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");
}

void rna_def_texmat_common(StructRNA *srna, const char *texspace_editable)
{
  PropertyRNA *prop;

  /* texture space */
  prop = RNA_def_property(srna, "auto_texspace", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "texspace_flag", ME_TEXSPACE_FLAG_AUTO);
  RNA_def_property_ui_text(
      prop,
      "Auto Texture Space",
      "Adjust active object's texture space automatically when transforming object");

  prop = RNA_def_property(srna, "texspace_location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "texspace_location");
  RNA_def_property_ui_text(prop, "Texture Space Location", "Texture space location");
  RNA_def_property_float_funcs(prop, "rna_Mesh_texspace_location_get", nullptr, nullptr);
  RNA_def_property_editable_func(prop, texspace_editable);
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "texspace_size", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "texspace_size");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_ui_text(prop, "Texture Space Size", "Texture space size");
  RNA_def_property_float_funcs(prop, "rna_Mesh_texspace_size_get", nullptr, nullptr);
  RNA_def_property_editable_func(prop, texspace_editable);
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  /* materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "mat", "totcol");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.cc */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_override_funcs(prop, nullptr, nullptr, "rna_Mesh_materials_override_apply");
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_IDMaterials_assign_int");
}

/* scene.objects */
/* mesh.vertices */
static void rna_def_mesh_vertices(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  // PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MeshVertices");
  srna = RNA_def_struct(brna, "MeshVertices", nullptr);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Mesh Vertices", "Collection of mesh vertices");

  func = RNA_def_function(srna, "add", "ED_mesh_verts_add");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "count", 0, 0, INT_MAX, "Count", "Number of vertices to add", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
#  if 0 /* BMESH_TODO Remove until BMesh merge */
  func = RNA_def_function(srna, "remove", "ED_mesh_verts_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of vertices to remove", 0, INT_MAX);
#  endif
}

/* mesh.edges */
static void rna_def_mesh_edges(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  // PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MeshEdges");
  srna = RNA_def_struct(brna, "MeshEdges", nullptr);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Mesh Edges", "Collection of mesh edges");

  func = RNA_def_function(srna, "add", "ED_mesh_edges_add");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of edges to add", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
#  if 0 /* BMESH_TODO Remove until BMesh merge */
  func = RNA_def_function(srna, "remove", "ED_mesh_edges_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of edges to remove", 0, INT_MAX);
#  endif
}

/* mesh.loop_triangles */
static void rna_def_mesh_looptris(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  RNA_def_property_srna(cprop, "MeshLoopTriangles");
  srna = RNA_def_struct(brna, "MeshLoopTriangles", nullptr);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(
      srna, "Mesh Loop Triangles", "Tessellation of mesh polygons into triangles");
}

/* mesh.loops */
static void rna_def_mesh_loops(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  // PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MeshLoops");
  srna = RNA_def_struct(brna, "MeshLoops", nullptr);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Mesh Loops", "Collection of mesh loops");

  func = RNA_def_function(srna, "add", "ED_mesh_loops_add");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of loops to add", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

/* mesh.polygons */
static void rna_def_mesh_polygons(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MeshPolygons");
  srna = RNA_def_struct(brna, "MeshPolygons", nullptr);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Mesh Polygons", "Collection of mesh polygons");

  prop = RNA_def_property(srna, "active", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "act_face");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Active Polygon", "The active face for this mesh");

  func = RNA_def_function(srna, "add", "ED_mesh_faces_add");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "count", 0, 0, INT_MAX, "Count", "Number of polygons to add", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

/* Defines a read-only vector type since normals can not be modified manually. */
static void rna_def_normal_layer_value(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "MeshNormalValue", nullptr);
  RNA_def_struct_sdna(srna, "vec3f");
  RNA_def_struct_ui_text(srna, "Mesh Normal Vector", "Vector in a mesh normal array");

  PropertyRNA *prop = RNA_def_property(srna, "vector", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_ui_text(prop, "Vector", "3D vector");
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_loop_colors(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "LoopColors");
  srna = RNA_def_struct(brna, "LoopColors", nullptr);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Loop Colors", "Collection of vertex colors");

  func = RNA_def_function(srna, "new", "rna_Mesh_vertex_color_new");
  RNA_def_function_ui_description(func, "Add a vertex color layer to Mesh");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_string(func, "name", "Col", 0, "", "Vertex color name");
  RNA_def_boolean(func,
                  "do_init",
                  true,
                  "",
                  "Whether new layer's data should be initialized by copying current active one");
  parm = RNA_def_pointer(func, "layer", "MeshLoopColorLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Mesh_vertex_color_remove");
  RNA_def_function_ui_description(func, "Remove a vertex color layer");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "MeshLoopColorLayer", "", "The layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshLoopColorLayer");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Mesh_vertex_color_active_get",
                                 "rna_Mesh_vertex_color_active_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Vertex Color Layer", "Active vertex color layer");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_edit_active_color");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_Mesh_vertex_color_active_index_get",
                             "rna_Mesh_vertex_color_active_index_set",
                             "rna_Mesh_vertex_color_index_range");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Active Vertex Color Index", "Active vertex color index");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_edit_active_color");
}

static void rna_def_uv_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "UVLoopLayers");
  srna = RNA_def_struct(brna, "UVLoopLayers", nullptr);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "UV Map Layers", "Collection of UV map layers");

  func = RNA_def_function(srna, "new", "rna_Mesh_uv_layers_new");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a UV map layer to Mesh");
  RNA_def_string(func, "name", "UVMap", 0, "", "UV map name");
  RNA_def_boolean(func,
                  "do_init",
                  true,
                  "",
                  "Whether new layer's data should be initialized by copying current active one, "
                  "or if none is active, with a default UVmap");
  parm = RNA_def_pointer(func, "layer", "MeshUVLoopLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Mesh_uv_layers_remove");
  RNA_def_function_ui_description(func, "Remove a UV map layer");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "MeshUVLoopLayer", "", "The layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_Mesh_uv_layer_active_get", "rna_Mesh_uv_layer_active_set", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active UV Map Layer", "Active UV Map layer");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_Mesh_uv_layer_active_index_get",
                             "rna_Mesh_uv_layer_active_index_set",
                             "rna_Mesh_uv_layer_index_range");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Active UV Map Index", "Active UV map index");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");
}

static void rna_def_skin_vertices(BlenderRNA *brna, PropertyRNA * /*cprop*/)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshSkinVertexLayer", nullptr);
  RNA_def_struct_ui_text(
      srna, "Mesh Skin Vertex Layer", "Per-vertex skin data for use with the Skin modifier");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_path_func(srna, "rna_MeshSkinVertexLayer_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_MeshVertexLayer_name_set");
  RNA_def_property_string_maxlength(prop, MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX);
  RNA_def_property_ui_text(prop, "Name", "Name of skin layer");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshSkinVertex");
  RNA_def_property_ui_text(prop, "Data", "");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshSkinVertexLayer_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_MeshSkinVertexLayer_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);

  /* SkinVertex struct */
  srna = RNA_def_struct(brna, "MeshSkinVertex", nullptr);
  RNA_def_struct_sdna(srna, "MVertSkin");
  RNA_def_struct_ui_text(
      srna, "Skin Vertex", "Per-vertex skin data for use with the Skin modifier");
  RNA_def_struct_path_func(srna, "rna_MeshSkinVertex_path");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_range(prop, 0.001, 100.0, 1, 3);
  RNA_def_property_ui_text(prop, "Radius", "Radius of the skin");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  /* Flags */

  prop = RNA_def_property(srna, "use_root", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", MVERT_SKIN_ROOT);
  RNA_def_property_ui_text(prop,
                           "Root",
                           "Vertex is a root for rotation calculations and armature generation, "
                           "setting this flag does not clear other roots in the same mesh island");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  prop = RNA_def_property(srna, "use_loose", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", MVERT_SKIN_LOOSE);
  RNA_def_property_ui_text(
      prop, "Loose", "If vertex has multiple adjacent edges, it is hulled to them directly");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");
}

static void rna_def_looptri_poly_value(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "ReadOnlyInteger", nullptr);
  RNA_def_struct_sdna(srna, "MIntProperty");
  RNA_def_struct_ui_text(srna, "Read-only Integer", "");

  PropertyRNA *prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Value", "");
  RNA_def_property_int_sdna(prop, nullptr, "i");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_mesh(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Mesh", "ID");
  RNA_def_struct_ui_text(srna, "Mesh", "Mesh data-block defining geometric surfaces");
  RNA_def_struct_ui_icon(srna, ICON_MESH_DATA);

  prop = RNA_def_property(srna, "vertices", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_vertices_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Mesh_vertices_length",
                                    "rna_Mesh_vertices_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MeshVertex");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Vertices", "Vertices of the mesh");
  rna_def_mesh_vertices(brna, prop);

  prop = RNA_def_property(srna, "edges", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_edges_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Mesh_edges_length",
                                    "rna_Mesh_edges_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MeshEdge");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Edges", "Edges of the mesh");
  rna_def_mesh_edges(brna, prop);

  prop = RNA_def_property(srna, "loops", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_loops_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Mesh_loops_length",
                                    "rna_Mesh_loops_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MeshLoop");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Loops", "Loops of the mesh (face corners)");
  rna_def_mesh_loops(brna, prop);

  prop = RNA_def_property(srna, "polygons", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_polygons_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Mesh_polygons_length",
                                    "rna_Mesh_polygons_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MeshPolygon");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Polygons", "Polygons of the mesh");
  rna_def_mesh_polygons(brna, prop);

  rna_def_normal_layer_value(brna);

  static const EnumPropertyItem normal_domain_items[] = {
      {int(blender::bke::MeshNormalDomain::Point), "POINT", 0, "Point", ""},
      {int(blender::bke::MeshNormalDomain::Face), "FACE", 0, "Face", ""},
      {int(blender::bke::MeshNormalDomain::Corner), "CORNER", 0, "Corner", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "normals_domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, normal_domain_items);
  RNA_def_property_ui_text(
      prop,
      "Normal Domain",
      "The attribute domain that gives enough information to represent the mesh's normals");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_funcs(prop, "rna_Mesh_normals_domain_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "vertex_normals", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshNormalValue");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop,
                           "Vertex Normals",
                           "The normal direction of each vertex, defined as the average of the "
                           "surrounding face normals");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_vertex_normals_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Mesh_vertex_normals_length",
                                    "rna_Mesh_vertex_normals_lookup_int",
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "polygon_normals", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshNormalValue");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop,
                           "Polygon Normals",
                           "The normal direction of each face, defined by the winding order "
                           "and position of its vertices");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_poly_normals_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Mesh_poly_normals_length",
                                    "rna_Mesh_poly_normals_lookup_int",
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "corner_normals", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshNormalValue");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(
      prop,
      "Corner Normals",
      "The \"slit\" normal direction of each face corner, influenced by vertex normals, "
      "sharp faces, sharp edges, and custom normals. May be empty.");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_corner_normals_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Mesh_corner_normals_length",
                                    "rna_Mesh_corner_normals_lookup_int",
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "loop_triangles", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_loop_triangles_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Mesh_loop_triangles_length",
                                    "rna_Mesh_loop_triangles_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MeshLoopTriangle");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Loop Triangles", "Tessellation of mesh polygons into triangles");
  rna_def_mesh_looptris(brna, prop);

  rna_def_looptri_poly_value(brna);

  prop = RNA_def_property(srna, "loop_triangle_polygons", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_loop_triangle_polygons_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Mesh_loop_triangles_length",
                                    "rna_Mesh_loop_triangle_polygons_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "ReadOnlyInteger");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Triangle Faces", "The face index for each loop triangle");

  /* TODO: should this be allowed to be itself? */
  prop = RNA_def_property(srna, "texture_mesh", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "texcomesh");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop,
      "Texture Mesh",
      "Use another mesh for texture indices (vertex indices must be aligned)");

  /* UV loop layers */
  prop = RNA_def_property(srna, "uv_layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "corner_data.layers", "corner_data.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_uv_layers_begin",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_Mesh_uv_layers_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "UV Loop Layers", "All UV loop layers");
  rna_def_uv_layers(brna, prop);

  prop = RNA_def_property(srna, "uv_layer_clone", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_Mesh_uv_layer_clone_get", "rna_Mesh_uv_layer_clone_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(
      prop, "Clone UV Loop Layer", "UV loop layer to be used as cloning source");

  prop = RNA_def_property(srna, "uv_layer_clone_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_Mesh_uv_layer_clone_index_get",
                             "rna_Mesh_uv_layer_clone_index_set",
                             "rna_Mesh_uv_layer_index_range");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Clone UV Loop Layer Index", "Clone UV loop layer index");

  prop = RNA_def_property(srna, "uv_layer_stencil", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_Mesh_uv_layer_stencil_get", "rna_Mesh_uv_layer_stencil_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Mask UV Loop Layer", "UV loop layer to mask the painted area");

  prop = RNA_def_property(srna, "uv_layer_stencil_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_Mesh_uv_layer_stencil_index_get",
                             "rna_Mesh_uv_layer_stencil_index_set",
                             "rna_Mesh_uv_layer_index_range");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Mask UV Loop Layer Index", "Mask UV loop layer index");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_legacy_deg_tag_all");

  /* Vertex colors */

  prop = RNA_def_property(srna, "vertex_colors", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "corner_data.layers", "corner_data.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_vertex_colors_begin",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_Mesh_vertex_colors_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MeshLoopColorLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(
      prop,
      "Vertex Colors",
      "Legacy vertex color layers. Deprecated, use color attributes instead.");
  rna_def_loop_colors(brna, prop);

  /* Skin vertices */
  prop = RNA_def_property(srna, "skin_vertices", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "vert_data.layers", "vert_data.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_skin_vertices_begin",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_Mesh_skin_vertices_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MeshSkinVertexLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Skin Vertices", "All skin vertices");
  rna_def_skin_vertices(brna, prop);
  /* End skin vertices */

  /* Attributes */
  rna_def_attributes_common(srna, AttributeOwnerType::Mesh);

  /* Remesh */
  prop = RNA_def_property(srna, "remesh_voxel_size", PROP_FLOAT, PROP_DISTANCE);
  /* NOTE: allow zero (which skips computation), to avoid zero clamping
   * to a small value which is likely to run out of memory, see: #130526. */
  RNA_def_property_float_sdna(prop, nullptr, "remesh_voxel_size");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0001f, FLT_MAX, 0.01, 4);
  RNA_def_property_ui_text(prop,
                           "Voxel Size",
                           "Size of the voxel in object space used for volume evaluation. Lower "
                           "values preserve finer details.");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);

  prop = RNA_def_property(srna, "remesh_voxel_adaptivity", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "remesh_voxel_adaptivity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 4);
  RNA_def_property_ui_text(
      prop,
      "Adaptivity",
      "Reduces the final face count by simplifying geometry where detail is not needed, "
      "generating triangles. A value greater than 0 disables Fix Poles.");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);

  prop = RNA_def_property(srna, "use_remesh_fix_poles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ME_REMESH_FIX_POLES);
  RNA_def_property_ui_text(prop, "Fix Poles", "Produces fewer poles and a better topology flow");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);

  prop = RNA_def_property(srna, "use_remesh_preserve_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ME_REMESH_REPROJECT_VOLUME);
  RNA_def_property_ui_text(
      prop,
      "Preserve Volume",
      "Projects the mesh to preserve the volume and details of the original mesh");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);

  prop = RNA_def_property(srna, "use_remesh_preserve_attributes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ME_REMESH_REPROJECT_ATTRIBUTES);
  RNA_def_property_ui_text(prop, "Preserve Attributes", "Transfer all attributes to the new mesh");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);

  prop = RNA_def_property(srna, "remesh_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "remesh_mode");
  RNA_def_property_enum_items(prop, rna_enum_mesh_remesh_mode_items);
  RNA_def_property_ui_text(prop, "Remesh Mode", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);

  /* End remesh */

  /* Symmetry */
  prop = RNA_def_property(srna, "use_mirror_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry", ME_SYMMETRY_X);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "X", "Enable symmetry in the X axis");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_mirror_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry", ME_SYMMETRY_Y);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Y", "Enable symmetry in the Y axis");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_mirror_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry", ME_SYMMETRY_Z);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Z", "Enable symmetry in the Z axis");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_mirror_vertex_groups", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "editflag", ME_EDIT_MIRROR_VERTEX_GROUPS);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Mirror Vertex Groups",
                           "Mirror the left/right vertex groups when painting. The symmetry axis "
                           "is determined by the symmetry settings.");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "radial_symmetry", PROP_INT, PROP_XYZ);
  RNA_def_property_int_sdna(prop, nullptr, "radial_symmetry");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_default(prop, 1);
  RNA_def_property_range(prop, 1, 64);
  RNA_def_property_ui_range(prop, 1, 32, 1, 1);
  RNA_def_property_ui_text(
      prop, "Radial Symmetry Count", "Number of mirrored regions around a central axis");
  /* End Symmetry */

  RNA_define_verify_sdna(false);
  prop = RNA_def_property(srna, "has_custom_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "", 0);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Has Custom Normals", "True if there is custom normal data for this mesh");
  RNA_def_property_boolean_funcs(prop, "rna_Mesh_has_custom_normals_get", nullptr);
  RNA_define_verify_sdna(true);

  prop = RNA_def_property(srna, "texco_mesh", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "texcomesh");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Texture Space Mesh", "Derive texture coordinates from another mesh");

  prop = RNA_def_property(srna, "shape_keys", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "key");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_clear_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_ui_text(prop, "Shape Keys", "");

  /* texture space */
  prop = RNA_def_property(srna, "use_auto_texspace", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "texspace_flag", ME_TEXSPACE_FLAG_AUTO);
  RNA_def_property_ui_text(
      prop,
      "Auto Texture Space",
      "Adjust active object's texture space automatically when transforming object");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_geom_and_params");

#  if 0
  prop = RNA_def_property(srna, "texspace_location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Texture Space Location", "Texture space location");
  RNA_def_property_editable_func(prop, "rna_Mesh_texspace_editable");
  RNA_def_property_float_funcs(
      prop, "rna_Mesh_texspace_location_get", "rna_Mesh_texspace_location_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");
#  endif

  /* editflag */
  prop = RNA_def_property(srna, "use_mirror_topology", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "editflag", ME_EDIT_MIRROR_TOPO);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Topology Mirror",
                           "Use topology based mirroring "
                           "(for when both sides of mesh have matching, unique topology)");

  prop = RNA_def_property(srna, "use_paint_bone_selection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "editflag", ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Bone Selection", "Bone selection during painting");
  RNA_def_property_ui_icon(prop, ICON_BONE_DATA, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Mesh_update_bone_selection_mode");

  prop = RNA_def_property(srna, "use_paint_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "editflag", ME_EDIT_PAINT_FACE_SEL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Paint Mask", "Face selection masking for painting");
  RNA_def_property_ui_icon(prop, ICON_FACESEL, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Mesh_update_facemask");

  prop = RNA_def_property(srna, "use_paint_mask_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "editflag", ME_EDIT_PAINT_VERT_SEL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Vertex Selection", "Vertex selection masking for painting");
  RNA_def_property_ui_icon(prop, ICON_VERTEXSEL, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Mesh_update_vertmask");

  /* readonly editmesh info - use for extrude menu */
  prop = RNA_def_property(srna, "total_vert_sel", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop, "rna_Mesh_tot_vert_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Selected Vertex Total", "Selected vertex count in editmode");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "total_edge_sel", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop, "rna_Mesh_tot_edge_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Selected Edge Total", "Selected edge count in editmode");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "total_face_sel", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop, "rna_Mesh_tot_face_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Selected Face Total", "Selected face count in editmode");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_editmode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Mesh_is_editmode_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Is Editmode", "True when used in editmode");

  /* pointers */
  rna_def_animdata_common(srna);
  rna_def_texmat_common(srna, "rna_Mesh_texspace_editable");

  RNA_api_mesh(srna);
}

void RNA_def_mesh(BlenderRNA *brna)
{
  rna_def_mesh(brna);
  rna_def_mvert(brna);
  rna_def_mvert_group(brna);
  rna_def_medge(brna);
  rna_def_mlooptri(brna);
  rna_def_mloop(brna);
  rna_def_mpolygon(brna);
  rna_def_mloopuv(brna);
  rna_def_mloopcol(brna);
}

#endif

/** \} */
