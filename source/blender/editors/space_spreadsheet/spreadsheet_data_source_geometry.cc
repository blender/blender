/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLI_listbase.h"
#include "BLI_math_matrix.hh"
#include "BLI_virtual_array.hh"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_editmesh.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"
#include "BKE_object_types.hh"
#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"

#include "DNA_pointcloud_types.h"
#include "DNA_space_types.h"

#include "DEG_depsgraph_query.hh"

#include "ED_curves.hh"
#include "ED_outliner.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_closure.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_geometry_nodes_log.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "UI_resources.hh"

#include "bmesh.hh"

#include "spreadsheet_data_source_geometry.hh"
#include "spreadsheet_intern.hh"

uint64_t SpreadsheetInstanceID::hash() const
{
  return blender::get_default_hash(this->reference_index);
}

bool operator==(const SpreadsheetInstanceID &a, const SpreadsheetInstanceID &b)
{
  return a.reference_index == b.reference_index;
}

bool operator!=(const SpreadsheetInstanceID &a, const SpreadsheetInstanceID &b)
{
  return !(a == b);
}

bool operator==(const SpreadsheetBundlePathElem &a, const SpreadsheetBundlePathElem &b)
{
  return STREQ(a.identifier, b.identifier);
}

bool operator!=(const SpreadsheetBundlePathElem &a, const SpreadsheetBundlePathElem &b)
{
  return !(a == b);
}

namespace blender::ed::spreadsheet {

static void add_mesh_debug_column_names(
    const Mesh &mesh,
    const bke::AttrDomain domain,
    FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      if (CustomData_has_layer(&mesh.vert_data, CD_ORIGINDEX)) {
        fn({(char *)"Original Index"}, false);
      }
      break;
    case bke::AttrDomain::Edge:
      if (CustomData_has_layer(&mesh.edge_data, CD_ORIGINDEX)) {
        fn({(char *)"Original Index"}, false);
      }
      break;
    case bke::AttrDomain::Face:
      if (CustomData_has_layer(&mesh.face_data, CD_ORIGINDEX)) {
        fn({(char *)"Original Index"}, false);
      }
      fn({(char *)"Corner Start"}, false);
      fn({(char *)"Corner Size"}, false);
      break;
    case bke::AttrDomain::Corner:
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

static std::unique_ptr<ColumnValues> build_mesh_debug_columns(const Mesh &mesh,
                                                              const bke::AttrDomain domain,
                                                              const StringRef name)
{
  switch (domain) {
    case bke::AttrDomain::Point: {
      if (name == "Original Index") {
        const int *data = static_cast<const int *>(
            CustomData_get_layer(&mesh.vert_data, CD_ORIGINDEX));
        if (data) {
          return std::make_unique<ColumnValues>(name,
                                                VArray<int>::from_span({data, mesh.verts_num}));
        }
      }
      return {};
    }
    case bke::AttrDomain::Edge: {
      if (name == "Original Index") {
        const int *data = static_cast<const int *>(
            CustomData_get_layer(&mesh.edge_data, CD_ORIGINDEX));
        if (data) {
          return std::make_unique<ColumnValues>(name,
                                                VArray<int>::from_span({data, mesh.edges_num}));
        }
      }
      return {};
    }
    case bke::AttrDomain::Face: {
      if (name == "Original Index") {
        const int *data = static_cast<const int *>(
            CustomData_get_layer(&mesh.face_data, CD_ORIGINDEX));
        if (data) {
          return std::make_unique<ColumnValues>(name,
                                                VArray<int>::from_span({data, mesh.faces_num}));
        }
      }
      if (name == "Corner Start") {
        return std::make_unique<ColumnValues>(
            name, VArray<int>::from_span(mesh.face_offsets().drop_back(1)));
      }
      if (name == "Corner Size") {
        const OffsetIndices faces = mesh.faces();
        return std::make_unique<ColumnValues>(
            name, VArray<int>::from_std_func(faces.size(), [faces](int64_t index) {
              return faces[index].size();
            }));
      }
      return {};
    }
    case bke::AttrDomain::Corner: {
      return {};
    }
    default:
      BLI_assert_unreachable();
      return {};
  }
}

bool GeometryDataSource::display_attribute(const StringRef name,
                                           const bke::AttrDomain domain) const
{
  if (bke::attribute_name_is_anonymous(name)) {
    return false;
  }
  if (!show_internal_attributes_) {
    if (!bke::allow_procedural_attribute_access(name)) {
      return false;
    }
    if (domain == bke::AttrDomain::Instance && name == "instance_transform") {
      /* Don't display the instance transform attribute, since matrix visualization in the
       * spreadsheet isn't helpful. */
      return false;
    }
  }
  return true;
}

void GeometryDataSource::foreach_default_column_ids(
    FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const
{
  std::optional<const bke::AttributeAccessor> attributes = this->get_component_attributes();
  if (!attributes.has_value()) {
    return;
  }
  if (attributes->domain_size(domain_) == 0) {
    return;
  }

  if (component_->type() == bke::GeometryComponent::Type::Instance) {
    fn({(char *)"Name"}, false);
  }

  if (component_->type() == bke::GeometryComponent::Type::GreasePencil) {
    fn({(char *)"Name"}, false);
  }

  attributes->foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != domain_) {
      return;
    }
    if (!display_attribute(iter.name, iter.domain)) {
      return;
    }
    SpreadsheetColumnID column_id;
    column_id.name = (char *)iter.name.data();
    const bool is_front = iter.name == ".viewer";
    fn(column_id, is_front);
  });

  if (component_->type() == bke::GeometryComponent::Type::Instance) {
    fn({(char *)"Position"}, false);
    fn({(char *)"Rotation"}, false);
    fn({(char *)"Scale"}, false);
  }
  else if (G.debug_value == 4001 && component_->type() == bke::GeometryComponent::Type::Mesh) {
    const bke::MeshComponent &component = static_cast<const bke::MeshComponent &>(*component_);
    if (const Mesh *mesh = component.get()) {
      add_mesh_debug_column_names(*mesh, domain_, fn);
    }
  }
}

std::unique_ptr<ColumnValues> GeometryDataSource::get_column_values(
    const SpreadsheetColumnID &column_id) const
{
  std::optional<const bke::AttributeAccessor> attributes = this->get_component_attributes();
  if (!attributes.has_value()) {
    return {};
  }
  const int domain_num = attributes->domain_size(domain_);
  if (domain_num == 0) {
    return {};
  }
  if (!display_attribute(column_id.name, domain_)) {
    return {};
  }

  std::lock_guard lock{mutex_};

  if (component_->type() == bke::GeometryComponent::Type::Instance) {
    if (const bke::Instances *instances =
            static_cast<const bke::InstancesComponent &>(*component_).get())
    {
      if (STREQ(column_id.name, "Name")) {
        Span<int> reference_handles = instances->reference_handles();
        Span<bke::InstanceReference> references = instances->references();
        return std::make_unique<ColumnValues>(
            column_id.name,
            VArray<bke::InstanceReference>::from_std_func(
                domain_num, [reference_handles, references](int64_t index) {
                  return references[reference_handles[index]];
                }));
      }
      Span<float4x4> transforms = instances->transforms();
      if (STREQ(column_id.name, "Position")) {
        return std::make_unique<ColumnValues>(
            column_id.name, VArray<float3>::from_std_func(domain_num, [transforms](int64_t index) {
              return transforms[index].location();
            }));
      }
      if (STREQ(column_id.name, "Rotation")) {
        return std::make_unique<ColumnValues>(
            column_id.name, VArray<float3>::from_std_func(domain_num, [transforms](int64_t index) {
              return float3(math::to_euler(math::normalize(transforms[index])));
            }));
      }
      if (STREQ(column_id.name, "Scale")) {
        return std::make_unique<ColumnValues>(
            column_id.name, VArray<float3>::from_std_func(domain_num, [transforms](int64_t index) {
              return math::to_scale<true>(transforms[index]);
            }));
      }
    }
  }
  else if (component_->type() == bke::GeometryComponent::Type::GreasePencil) {
    if (const GreasePencil *grease_pencil =
            static_cast<const bke::GreasePencilComponent &>(*component_).get())
    {
      if (domain_ == bke::AttrDomain::Layer && STREQ(column_id.name, "Name")) {
        const Span<const bke::greasepencil::Layer *> layers = grease_pencil->layers();
        return std::make_unique<ColumnValues>(
            column_id.name,
            VArray<std::string>::from_std_func(domain_num, [layers](int64_t index) {
              StringRefNull name = layers[index]->name();
              if (name.is_empty()) {
                name = IFACE_("(Layer)");
              }
              return std::string(name);
            }));
      }
    }
  }
  else if (G.debug_value == 4001 && component_->type() == bke::GeometryComponent::Type::Mesh) {
    const bke::MeshComponent &component = static_cast<const bke::MeshComponent &>(*component_);
    if (const Mesh *mesh = component.get()) {
      if (std::unique_ptr<ColumnValues> values = build_mesh_debug_columns(
              *mesh, domain_, column_id.name))
      {
        return values;
      }
    }
  }

  bke::GAttributeReader attribute = attributes->lookup(column_id.name);
  if (!attribute) {
    return {};
  }
  GVArray varray = std::move(attribute.varray);
  if (attribute.domain != domain_) {
    return {};
  }

  StringRefNull column_display_name = column_id.name;
  if (column_display_name == ".viewer") {
    column_display_name = "Viewer";
  }

  return std::make_unique<ColumnValues>(column_display_name, std::move(varray));
}

int GeometryDataSource::tot_rows() const
{
  std::optional<const bke::AttributeAccessor> attributes = this->get_component_attributes();
  if (!attributes.has_value()) {
    return {};
  }
  return attributes->domain_size(domain_);
}

bool GeometryDataSource::has_selection_filter() const
{
  if (!object_orig_) {
    return false;
  }
  switch (component_->type()) {
    case bke::GeometryComponent::Type::Mesh: {
      if (object_orig_->type != OB_MESH) {
        return false;
      }
      if (object_orig_->mode != OB_MODE_EDIT) {
        return false;
      }
      return true;
    }
    case bke::GeometryComponent::Type::Curve: {
      if (object_orig_->type != OB_CURVES) {
        return false;
      }
      if (!ELEM(object_orig_->mode, OB_MODE_SCULPT_CURVES, OB_MODE_EDIT)) {
        return false;
      }
      return true;
    }
    case bke::GeometryComponent::Type::PointCloud: {
      if (object_orig_->type != OB_POINTCLOUD) {
        return false;
      }
      if (object_orig_->mode != OB_MODE_EDIT) {
        return false;
      }
      return true;
    }
    default:
      return false;
  }
}

static IndexMask calc_mesh_selection_mask_faces(const Mesh &mesh_eval,
                                                const Mesh &mesh_orig,
                                                IndexMaskMemory &memory)
{
  const bke::AttributeAccessor attributes_eval = mesh_eval.attributes();
  const IndexRange range(attributes_eval.domain_size(bke::AttrDomain::Face));
  BMesh *bm = mesh_orig.runtime->edit_mesh->bm;

  BM_mesh_elem_table_ensure(bm, BM_FACE);
  if (mesh_eval.faces_num == bm->totface) {
    return IndexMask::from_predicate(range, GrainSize(4096), memory, [&](const int i) {
      const BMFace *face = BM_face_at_index(bm, i);
      return BM_elem_flag_test_bool(face, BM_ELEM_SELECT);
    });
  }
  if (const int *orig_indices = static_cast<const int *>(
          CustomData_get_layer(&mesh_eval.face_data, CD_ORIGINDEX)))
  {
    return IndexMask::from_predicate(range, GrainSize(2048), memory, [&](const int i) {
      const int orig = orig_indices[i];
      if (orig == -1) {
        return false;
      }
      const BMFace *face = BM_face_at_index(bm, orig);
      return BM_elem_flag_test_bool(face, BM_ELEM_SELECT);
    });
  }
  return range;
}

static IndexMask calc_mesh_selection_mask(const Mesh &mesh_eval,
                                          const Mesh &mesh_orig,
                                          const bke::AttrDomain domain,
                                          IndexMaskMemory &memory)
{
  const bke::AttributeAccessor attributes_eval = mesh_eval.attributes();
  const IndexRange range(attributes_eval.domain_size(domain));
  BMesh *bm = mesh_orig.runtime->edit_mesh->bm;

  switch (domain) {
    case bke::AttrDomain::Point: {
      BM_mesh_elem_table_ensure(bm, BM_VERT);
      if (mesh_eval.verts_num == bm->totvert) {
        return IndexMask::from_predicate(range, GrainSize(4096), memory, [&](const int i) {
          const BMVert *vert = BM_vert_at_index(bm, i);
          return BM_elem_flag_test_bool(vert, BM_ELEM_SELECT);
        });
      }
      if (const int *orig_indices = static_cast<const int *>(
              CustomData_get_layer(&mesh_eval.vert_data, CD_ORIGINDEX)))
      {
        return IndexMask::from_predicate(range, GrainSize(2048), memory, [&](const int i) {
          const int orig = orig_indices[i];
          if (orig == -1) {
            return false;
          }
          const BMVert *vert = BM_vert_at_index(bm, orig);
          return BM_elem_flag_test_bool(vert, BM_ELEM_SELECT);
        });
      }
      return range;
    }
    case bke::AttrDomain::Edge: {
      BM_mesh_elem_table_ensure(bm, BM_EDGE);
      if (mesh_eval.edges_num == bm->totedge) {
        return IndexMask::from_predicate(range, GrainSize(4096), memory, [&](const int i) {
          const BMEdge *edge = BM_edge_at_index(bm, i);
          return BM_elem_flag_test_bool(edge, BM_ELEM_SELECT);
        });
      }
      if (const int *orig_indices = static_cast<const int *>(
              CustomData_get_layer(&mesh_eval.edge_data, CD_ORIGINDEX)))
      {
        return IndexMask::from_predicate(range, GrainSize(2048), memory, [&](const int i) {
          const int orig = orig_indices[i];
          if (orig == -1) {
            return false;
          }
          const BMEdge *edge = BM_edge_at_index(bm, orig);
          return BM_elem_flag_test_bool(edge, BM_ELEM_SELECT);
        });
      }
      return range;
    }
    case bke::AttrDomain::Face: {
      return calc_mesh_selection_mask_faces(mesh_eval, mesh_orig, memory);
    }
    case bke::AttrDomain::Corner: {
      IndexMaskMemory face_memory;
      const IndexMask face_mask = calc_mesh_selection_mask_faces(
          mesh_eval, mesh_orig, face_memory);
      if (face_mask.is_empty()) {
        return {};
      }
      if (face_mask.size() == range.size()) {
        return range;
      }

      Array<bool> face_selection(range.size(), false);
      face_mask.to_bools(face_selection);

      const VArray<bool> corner_selection = attributes_eval.adapt_domain<bool>(
          VArray<bool>::from_span(face_selection), bke::AttrDomain::Face, bke::AttrDomain::Corner);
      return IndexMask::from_bools(corner_selection, memory);
    }
    default:
      BLI_assert_unreachable();
      return range;
  }
}

IndexMask GeometryDataSource::apply_selection_filter(IndexMaskMemory &memory) const
{
  std::lock_guard lock{mutex_};
  const IndexMask full_range(this->tot_rows());
  if (full_range.is_empty()) {
    return full_range;
  }

  switch (component_->type()) {
    case bke::GeometryComponent::Type::Mesh: {
      BLI_assert(object_orig_->type == OB_MESH);
      BLI_assert(object_orig_->mode == OB_MODE_EDIT);
      const Mesh *mesh_eval = geometry_set_.get_mesh();
      const Mesh *mesh_orig = static_cast<const Mesh *>(object_orig_->data);
      return calc_mesh_selection_mask(*mesh_eval, *mesh_orig, domain_, memory);
    }
    case bke::GeometryComponent::Type::Curve: {
      BLI_assert(object_orig_->type == OB_CURVES);
      const bke::CurveComponent &component = static_cast<const bke::CurveComponent &>(*component_);
      const Curves &curves_id = *component.get();
      switch (domain_) {
        case bke::AttrDomain::Point:
          return curves::retrieve_selected_points(curves_id, memory);
        case bke::AttrDomain::Curve:
          return curves::retrieve_selected_curves(curves_id, memory);
        default:
          BLI_assert_unreachable();
      }
      return full_range;
    }
    case bke::GeometryComponent::Type::PointCloud: {
      BLI_assert(object_orig_->type == OB_POINTCLOUD);
      const bke::AttributeAccessor attributes = *component_->attributes();
      const VArray<bool> selection = *attributes.lookup_or_default(
          ".selection", bke::AttrDomain::Point, true);
      return IndexMask::from_bools(selection, memory);
    }
    default:
      return full_range;
  }
}

std::optional<const bke::AttributeAccessor> GeometryDataSource::get_component_attributes() const
{
  if (component_->type() != bke::GeometryComponent::Type::GreasePencil) {
    return component_->attributes();
  }
  const GreasePencil *grease_pencil = geometry_set_.get_grease_pencil();
  if (!grease_pencil) {
    return {};
  }
  if (domain_ == bke::AttrDomain::Layer) {
    return grease_pencil->attributes();
  }
  if (layer_index_ >= 0 && layer_index_ < grease_pencil->layers().size()) {
    if (const bke::greasepencil::Drawing *drawing = grease_pencil->get_eval_drawing(
            grease_pencil->layer(layer_index_)))
    {
      return drawing->strokes().attributes();
    }
  }
  return {};
}

void VolumeDataSource::foreach_default_column_ids(
    FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const
{
  if (component_->is_empty()) {
    return;
  }

  for (const char *name :
       {"Grid Name", "Data Type", "Class", "Extent", "Voxels", "Leaf Voxels", "Tiles", "Size"})
  {
    SpreadsheetColumnID column_id{(char *)name};
    fn(column_id, false);
  }
}

#ifdef WITH_OPENVDB
static StringRef grid_class_name(const bke::VolumeGridData &grid_data)
{
  openvdb::GridClass grid_class = grid_data.grid_class();
  if (grid_class == openvdb::GridClass::GRID_FOG_VOLUME) {
    return IFACE_("Fog Volume");
  }
  if (grid_class == openvdb::GridClass::GRID_LEVEL_SET) {
    return IFACE_("Level Set");
  }
  return IFACE_("Unknown");
}
#endif

std::unique_ptr<ColumnValues> VolumeDataSource::get_column_values(
    const SpreadsheetColumnID &column_id) const
{
  const Volume *volume = component_->get();
  if (volume == nullptr) {
    return {};
  }

#ifdef WITH_OPENVDB
  const int size = this->tot_rows();
  if (STREQ(column_id.name, "Grid Name")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Grid Name"), VArray<std::string>::from_std_func(size, [volume](int64_t index) {
          const bke::VolumeGridData *volume_grid = BKE_volume_grid_get(volume, index);
          return volume_grid->name();
        }));
  }
  if (STREQ(column_id.name, "Data Type")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Data Type"), VArray<std::string>::from_std_func(size, [volume](int64_t index) {
          const bke::VolumeGridData *volume_grid = BKE_volume_grid_get(volume, index);
          const VolumeGridType type = volume_grid->grid_type();
          const char *name = nullptr;
          RNA_enum_name_from_value(rna_enum_volume_grid_data_type_items, type, &name);
          return IFACE_(name);
        }));
  }
  if (STREQ(column_id.name, "Class")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Class"), VArray<std::string>::from_std_func(size, [volume](int64_t index) {
          return grid_class_name(*BKE_volume_grid_get(volume, index));
        }));
  }
  if (STREQ(column_id.name, "Voxels")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Voxels"), VArray<int64_t>::from_std_func(size, [volume](const int64_t index) {
          return BKE_volume_grid_get(volume, index)->active_voxels();
        }));
  }
  if (STREQ(column_id.name, "Leaf Voxels")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Leaf Voxels"), VArray<int64_t>::from_std_func(size, [volume](const int64_t index) {
          return BKE_volume_grid_get(volume, index)->active_leaf_voxels();
        }));
  }
  if (STREQ(column_id.name, "Tiles")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Tiles"), VArray<int64_t>::from_std_func(size, [volume](const int64_t index) {
          return BKE_volume_grid_get(volume, index)->active_tiles();
        }));
  }
  if (STREQ(column_id.name, "Size")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Size"),
        VArray<int64_t>::from_std_func(
            size,
            [volume](const int64_t index) {
              return BKE_volume_grid_get(volume, index)->size_in_bytes();
            }),
        ColumnValueDisplayHint::Bytes);
  }
  if (STREQ(column_id.name, "Extent")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Extent"), VArray<int3>::from_std_func(size, [volume](const int64_t index) {
          return int3(BKE_volume_grid_get(volume, index)->active_bounds().dim().asPointer());
        }));
  }
#else
  UNUSED_VARS(column_id);
#endif

  return {};
}

int VolumeDataSource::tot_rows() const
{
  const Volume *volume = component_->get();
  if (volume == nullptr) {
    return 0;
  }
  return BKE_volume_num_grids(volume);
}

#ifdef WITH_OPENVDB

VolumeGridDataSource::VolumeGridDataSource(const bke::GVolumeGrid &grid)
    : grid_(std::make_unique<bke::GVolumeGrid>(grid))
{
}

void VolumeGridDataSource::foreach_default_column_ids(
    FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const
{
  if (!grid_) {
    return;
  }

  for (const char *name :
       {"Data Type", "Class", "Extent", "Voxels", "Leaf Voxels", "Tiles", "Size"})
  {
    SpreadsheetColumnID column_id{(char *)name};
    fn(column_id, false);
  }
}

std::unique_ptr<ColumnValues> VolumeGridDataSource::get_column_values(
    const SpreadsheetColumnID &column_id) const
{
  const bke::VolumeGridData &grid = grid_->get();
  if (STREQ(column_id.name, "Data Type")) {
    const VolumeGridType type = grid.grid_type();
    const char *name = nullptr;
    RNA_enum_name_from_value(rna_enum_volume_grid_data_type_items, type, &name);
    return std::make_unique<ColumnValues>(IFACE_("Data Type"),
                                          VArray<std::string>::from_single(name, 1));
  }
  if (STREQ(column_id.name, "Class")) {
    const StringRef name = grid_class_name(grid_->get());
    return std::make_unique<ColumnValues>(IFACE_("Class"),
                                          VArray<std::string>::from_single(name, 1));
  }
  if (STREQ(column_id.name, "Voxels")) {
    const int64_t active_voxels = grid.active_voxels();
    return std::make_unique<ColumnValues>(IFACE_("Voxels"),
                                          VArray<int64_t>::from_single(active_voxels, 1));
  }
  if (STREQ(column_id.name, "Leaf Voxels")) {
    const int64_t active_leaf_voxels = grid.active_leaf_voxels();
    return std::make_unique<ColumnValues>(IFACE_("Leaf Voxels"),
                                          VArray<int64_t>::from_single(active_leaf_voxels, 1));
  }
  if (STREQ(column_id.name, "Tiles")) {
    const int64_t active_tiles = grid.active_tiles();
    return std::make_unique<ColumnValues>(IFACE_("Tiles"),
                                          VArray<int64_t>::from_single(active_tiles, 1));
  }
  if (STREQ(column_id.name, "Size")) {
    const int64_t size = grid.size_in_bytes();
    return std::make_unique<ColumnValues>(
        IFACE_("Size"), VArray<int64_t>::from_single(size, 1), ColumnValueDisplayHint::Bytes);
  }
  if (STREQ(column_id.name, "Extent")) {
    const int3 extent = int3(grid.active_bounds().dim().asPointer());
    return std::make_unique<ColumnValues>(IFACE_("Extent"), VArray<int3>::from_single(extent, 1));
  }
  return {};
}

int VolumeGridDataSource::tot_rows() const
{
  return 1;
}

#endif

ListDataSource::ListDataSource(nodes::ListPtr list) : list_(std::move(list)) {}

void ListDataSource::foreach_default_column_ids(
    FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const
{
  if (list_->size() == 0) {
    return;
  }

  for (const char *name : {"Value"}) {
    SpreadsheetColumnID column_id{(char *)name};
    fn(column_id, false);
  }
}

std::unique_ptr<ColumnValues> ListDataSource::get_column_values(
    const SpreadsheetColumnID &column_id) const
{
  if (STREQ(column_id.name, "Value")) {
    return std::make_unique<ColumnValues>(IFACE_("Value"), list_->varray());
  }
  return {};
}

int ListDataSource::tot_rows() const
{
  return list_->size();
}

BundleDataSource::BundleDataSource(nodes::BundlePtr bundle) : bundle_(std::move(bundle))
{
  this->collect_flat_items(*bundle_, "");
}

void BundleDataSource::foreach_default_column_ids(
    FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const
{
  if (bundle_->is_empty()) {
    return;
  }

  for (const char *name : {"Identifier", "Type", "Value"}) {
    SpreadsheetColumnID column_id{(char *)name};
    fn(column_id, false);
  }
}

std::unique_ptr<ColumnValues> BundleDataSource::get_column_values(
    const SpreadsheetColumnID &column_id) const
{
  if (STREQ(column_id.name, "Identifier")) {
    return std::make_unique<ColumnValues>(IFACE_("Identifier"),
                                          VArray<std::string>::from_span(flat_item_keys_));
  }
  if (STREQ(column_id.name, "Type")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Type"),
        VArray<std::string>::from_func(
            flat_items_.size(), [items = flat_items_](int64_t index) -> std::string {
              const nodes::BundleItemValue &value = *items[index];
              if (const auto *socket_value = std::get_if<nodes::BundleItemSocketValue>(
                      &value.value)) {
                const bke::SocketValueVariant &value_variant = socket_value->value;
                const StringRef type_name = IFACE_(socket_value->type->label);
                if (value_variant.is_single()) {
                  return type_name;
                }
                if (value_variant.is_context_dependent_field()) {
                  return fmt::format("{} {}", type_name, IFACE_("Field"));
                }
                if (value_variant.is_volume_grid()) {
                  return fmt::format("{} {}", type_name, IFACE_("Grid"));
                }
                if (value_variant.is_list()) {
                  return fmt::format("{} {}", type_name, IFACE_("List"));
                }
                return type_name;
              }
              if (const auto *internal_value = std::get_if<nodes::BundleItemInternalValue>(
                      &value.value)) {
                return internal_value->value->type_name();
              }
              return "";
            }));
  }
  if (STREQ(column_id.name, "Value")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Value"),
        VArray<nodes::BundleItemValue>::from_func(
            flat_items_.size(),
            [items = flat_items_](const int64_t index) -> nodes::BundleItemValue {
              return *items[index];
            }));
  }
  return {};
}

int BundleDataSource::tot_rows() const
{
  return flat_item_keys_.size();
}

void BundleDataSource::collect_flat_items(const nodes::Bundle &bundle, const StringRef parent_path)
{
  const Span<nodes::Bundle::StoredItem> items = bundle.items();
  for (const nodes::Bundle::StoredItem &item : items) {
    const std::string path = parent_path.is_empty() ?
                                 item.key :
                                 nodes::Bundle::combine_path({parent_path, item.key});
    flat_item_keys_.append(path);
    flat_items_.append(&item.value);
    if (const auto *value = std::get_if<nodes::BundleItemSocketValue>(&item.value.value)) {
      if (value->value.is_single()) {
        const GPointer ptr = value->value.get_single_ptr();
        if (ptr.is_type<nodes::BundlePtr>()) {
          const nodes::BundlePtr child_bundle = *ptr.get<nodes::BundlePtr>();
          if (child_bundle) {
            this->collect_flat_items(*child_bundle, path);
          }
        }
      }
    }
  }
}

ClosureSignatureDataSource::ClosureSignatureDataSource(nodes::ClosurePtr closure,
                                                       const SpreadsheetClosureInputOutput in_out)
    : closure_(std::move(closure)), in_out_(in_out)
{
}

void ClosureSignatureDataSource::foreach_default_column_ids(
    FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const
{
  Vector<StringRefNull> columns_names;
  if (in_out_ == SPREADSHEET_CLOSURE_NONE) {
    columns_names.append("Interface");
  }
  columns_names.extend({"Identifier", "Type"});

  for (const StringRefNull name : columns_names) {
    SpreadsheetColumnID column_id{(char *)name.c_str()};
    fn(column_id, false);
  }
}

std::unique_ptr<ColumnValues> ClosureSignatureDataSource::get_column_values(
    const SpreadsheetColumnID &column_id) const
{
  const Span<nodes::ClosureSignature::Item> input_items = closure_->signature().inputs;
  const Span<nodes::ClosureSignature::Item> output_items = closure_->signature().outputs;

  switch (in_out_) {
    case SPREADSHEET_CLOSURE_NONE: {
      const int items_sum = input_items.size() + output_items.size();
      if (STREQ(column_id.name, "Identifier")) {
        return std::make_unique<ColumnValues>(
            IFACE_("Identifier"),
            VArray<std::string>::from_func(items_sum,
                                           [input_items, output_items](const int64_t index) {
                                             if (index < input_items.size()) {
                                               return input_items[index].key;
                                             }
                                             return output_items[index - input_items.size()].key;
                                           }));
      }
      if (STREQ(column_id.name, "Type")) {
        return std::make_unique<ColumnValues>(
            IFACE_("Type"),
            VArray<std::string>::from_func(
                items_sum, [input_items, output_items](const int64_t index) {
                  if (index < input_items.size()) {
                    return input_items[index].type->label;
                  }
                  return output_items[index - input_items.size()].type->label;
                }));
      }
      if (STREQ(column_id.name, "Interface")) {
        return std::make_unique<ColumnValues>(
            IFACE_("Interface"),
            VArray<std::string>::from_func(items_sum,
                                           [inputs_num = input_items.size()](const int64_t index) {
                                             if (index < inputs_num) {
                                               return IFACE_("Input");
                                             }
                                             return IFACE_("Output");
                                           }));
      }
      break;
    }
    case SPREADSHEET_CLOSURE_INPUT:
    case SPREADSHEET_CLOSURE_OUTPUT: {
      const Span<nodes::ClosureSignature::Item> items = in_out_ == SPREADSHEET_CLOSURE_INPUT ?
                                                            input_items :
                                                            output_items;
      if (STREQ(column_id.name, "Identifier")) {
        return std::make_unique<ColumnValues>(
            IFACE_("Identifier"),
            VArray<std::string>::from_func(items.size(),
                                           [items](int64_t index) { return items[index].key; }));
      }
      if (STREQ(column_id.name, "Type")) {
        return std::make_unique<ColumnValues>(
            IFACE_("Type"), VArray<std::string>::from_func(items.size(), [items](int64_t index) {
              return items[index].type->label;
            }));
      }
      break;
    }
  }
  return {};
}

int ClosureSignatureDataSource::tot_rows() const
{
  const int inputs_num = closure_->signature().inputs.size();
  const int outputs_num = closure_->signature().outputs.size();
  switch (in_out_) {
    case SPREADSHEET_CLOSURE_NONE:
      return inputs_num + outputs_num;
    case SPREADSHEET_CLOSURE_INPUT:
      return inputs_num;
    case SPREADSHEET_CLOSURE_OUTPUT:
      return outputs_num;
  }
  return 0;
}

SingleValueDataSource::SingleValueDataSource(const GPointer value)
    : value_gvarray_(GVArray::from_single(*value.type(), 1, value.get()))
{
}

void SingleValueDataSource::foreach_default_column_ids(
    FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const
{
  for (const char *name : {"Value"}) {
    SpreadsheetColumnID column_id{(char *)name};
    fn(column_id, false);
  }
}

std::unique_ptr<ColumnValues> SingleValueDataSource::get_column_values(
    const SpreadsheetColumnID &column_id) const
{
  if (STREQ(column_id.name, "Value")) {
    return std::make_unique<ColumnValues>(IFACE_("Value"), value_gvarray_);
  }
  return {};
}

int SingleValueDataSource::tot_rows() const
{
  return 1;
}

int get_instance_reference_icon(const bke::InstanceReference &reference)
{
  switch (reference.type()) {
    case bke::InstanceReference::Type::Object: {
      const Object &object = reference.object();
      return ED_outliner_icon_from_id(object.id);
    }
    case bke::InstanceReference::Type::Collection: {
      return ICON_OUTLINER_COLLECTION;
    }
    case bke::InstanceReference::Type::GeometrySet: {
      return ICON_GEOMETRY_SET;
    }
    case bke::InstanceReference::Type::None: {
      break;
    }
  }
  return ICON_NONE;
}

const nodes::geo_eval_log::ViewerNodeLog *viewer_node_log_lookup(
    const SpaceSpreadsheet &sspreadsheet)
{
  return nodes::geo_eval_log::GeoNodesLog::find_viewer_node_log_for_path(
      sspreadsheet.geometry_id.viewer_path);
}

bke::SocketValueVariant geometry_display_data_get(const SpaceSpreadsheet *sspreadsheet,
                                                  Object *object_eval)
{
  if (sspreadsheet->geometry_id.object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL) {
    const Object *object_orig = DEG_get_original(object_eval);
    if (object_orig->type == OB_MESH) {
      const Mesh *mesh = static_cast<const Mesh *>(object_orig->data);
      if (object_orig->mode == OB_MODE_EDIT) {
        if (const BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
          Mesh *new_mesh = BKE_id_new_nomain<Mesh>(nullptr);
          /* This is a potentially heavy operation to do on every redraw. The best solution here
           * is to display the data directly from the bmesh without a conversion, which can be
           * implemented a bit later. */
          BM_mesh_bm_to_me_for_eval(*em->bm, *new_mesh, nullptr);
          return bke::SocketValueVariant::From(bke::GeometrySet::from_mesh(new_mesh));
        }
      }
      else {
        return bke::SocketValueVariant::From(bke::GeometrySet::from_mesh(
            const_cast<Mesh *>(mesh), bke::GeometryOwnershipType::ReadOnly));
      }
    }
    else if (object_orig->type == OB_POINTCLOUD) {
      const PointCloud *pointcloud = static_cast<const PointCloud *>(object_orig->data);
      return bke::SocketValueVariant::From(bke::GeometrySet::from_pointcloud(
          const_cast<PointCloud *>(pointcloud), bke::GeometryOwnershipType::ReadOnly));
    }
    else if (object_orig->type == OB_CURVES) {
      const Curves &curves_id = *static_cast<const Curves *>(object_orig->data);
      return bke::SocketValueVariant::From(bke::GeometrySet::from_curves(
          &const_cast<Curves &>(curves_id), bke::GeometryOwnershipType::ReadOnly));
    }
    else if (object_orig->type == OB_GREASE_PENCIL) {
      const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(object_orig->data);
      return bke::SocketValueVariant::From(bke::GeometrySet::from_grease_pencil(
          &const_cast<GreasePencil &>(grease_pencil), bke::GeometryOwnershipType::ReadOnly));
    }
    return {};
  }

  if (BLI_listbase_is_single(&sspreadsheet->geometry_id.viewer_path.path)) {
    return bke::SocketValueVariant::From(bke::object_get_evaluated_geometry_set(*object_eval));
  }

  const nodes::geo_eval_log::ViewerNodeLog *viewer_log =
      nodes::geo_eval_log::GeoNodesLog::find_viewer_node_log_for_path(
          sspreadsheet->geometry_id.viewer_path);
  if (!viewer_log) {
    return {};
  }

  const SpreadsheetTableIDGeometry &table_id = sspreadsheet->geometry_id;
  const int item_index = viewer_log->items.index_of_try_as(table_id.viewer_item_identifier);
  if (item_index == -1) {
    return {};
  }

  bke::SocketValueVariant value = viewer_log->items[item_index].value;

  /* Try to display the previous geometry instead of the value is a field (it will have been
   * evaluated on that geometry). */
  if (value.is_context_dependent_field()) {
    for (int i = item_index - 1; i >= 0; i--) {
      const bke::SocketValueVariant &prev_value = viewer_log->items[i].value;
      if (!prev_value.is_single()) {
        continue;
      }
      const GPointer ptr = prev_value.get_single_ptr();
      if (!ptr.is_type<bke::GeometrySet>()) {
        continue;
      }
      return prev_value;
    }
    return {};
  }

  for (const SpreadsheetBundlePathElem &bundle_path_elem :
       Span(table_id.bundle_path, table_id.bundle_path_num))
  {
    if (!value.is_single()) {
      return {};
    }
    const GPointer ptr = value.get_single_ptr();
    if (!ptr.is_type<nodes::BundlePtr>()) {
      return {};
    }
    const nodes::BundlePtr &bundle = *ptr.get<nodes::BundlePtr>();
    const nodes::BundleItemValue *item = bundle->lookup(bundle_path_elem.identifier);
    if (!item) {
      return {};
    }
    const auto *stored_value = std::get_if<nodes::BundleItemSocketValue>(&item->value);
    if (!stored_value) {
      return {};
    }
    value = stored_value->value;
  }

  return value;
}

std::optional<bke::GeometrySet> root_geometry_set_get(const SpaceSpreadsheet *sspreadsheet,
                                                      Object *object_eval)
{
  bke::SocketValueVariant display_data = geometry_display_data_get(sspreadsheet, object_eval);
  if (!display_data.is_single()) {
    return std::nullopt;
  }
  const GPointer ptr = display_data.get_single_ptr();
  if (!ptr.is_type<bke::GeometrySet>()) {
    return std::nullopt;
  }
  return display_data.extract<bke::GeometrySet>();
}

bke::GeometrySet get_geometry_set_for_instance_ids(const bke::GeometrySet &root_geometry,
                                                   const Span<SpreadsheetInstanceID> instance_ids)
{
  bke::GeometrySet geometry = root_geometry;
  for (const SpreadsheetInstanceID &instance_id : instance_ids) {
    const bke::Instances *instances = geometry.get_instances();
    if (!instances) {
      /* Return the best available geometry. */
      return geometry;
    }
    const Span<bke::InstanceReference> references = instances->references();
    if (instance_id.reference_index < 0 || instance_id.reference_index >= references.size()) {
      /* Return the best available geometry. */
      return geometry;
    }
    const bke::InstanceReference &reference = references[instance_id.reference_index];
    bke::GeometrySet reference_geometry;
    reference.to_geometry_set(reference_geometry);
    geometry = reference_geometry;
  }
  return geometry;
}

std::unique_ptr<DataSource> data_source_from_geometry(const bContext *C, Object *object_eval)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);

  bke::SocketValueVariant display_data = geometry_display_data_get(sspreadsheet, object_eval);
  if (display_data.is_context_dependent_field()) {
    return {};
  }
  if (display_data.is_volume_grid()) {
#ifdef WITH_OPENVDB
    return std::make_unique<VolumeGridDataSource>(display_data.get<bke::GVolumeGrid>());
#else
    return {};
#endif
  }
  if (display_data.is_list()) {
    return std::make_unique<ListDataSource>(display_data.extract<nodes::ListPtr>());
  }
  if (!display_data.is_single()) {
    return {};
  }
  const GPointer ptr = display_data.get_single_ptr();
  if (ptr.is_type<bke::GeometrySet>()) {
    const bke::GeometrySet root_geometry_set = display_data.extract<bke::GeometrySet>();
    const bke::GeometrySet geometry_set = get_geometry_set_for_instance_ids(
        root_geometry_set,
        Span{sspreadsheet->geometry_id.instance_ids, sspreadsheet->geometry_id.instance_ids_num});

    const bke::AttrDomain domain = (bke::AttrDomain)sspreadsheet->geometry_id.attribute_domain;
    const auto component_type = bke::GeometryComponent::Type(
        sspreadsheet->geometry_id.geometry_component_type);
    const int layer_index = sspreadsheet->geometry_id.layer_index;
    if (!geometry_set.has(component_type)) {
      return {};
    }

    if (component_type == bke::GeometryComponent::Type::Volume) {
      return std::make_unique<VolumeDataSource>(std::move(geometry_set));
    }
    Object *object_orig = sspreadsheet->geometry_id.instance_ids_num == 0 ?
                              DEG_get_original(object_eval) :
                              nullptr;
    return std::make_unique<GeometryDataSource>(object_orig,
                                                std::move(geometry_set),
                                                component_type,
                                                domain,
                                                sspreadsheet->flag &
                                                    SPREADSHEET_FLAG_SHOW_INTERNAL_ATTRIBUTES,
                                                layer_index);
  }
  if (ptr.is_type<nodes::BundlePtr>()) {
    const nodes::BundlePtr bundle_ptr = display_data.extract<nodes::BundlePtr>();
    if (bundle_ptr) {
      return std::make_unique<BundleDataSource>(bundle_ptr);
    }
    return {};
  }
  if (ptr.is_type<nodes::ClosurePtr>()) {
    const auto in_out = SpreadsheetClosureInputOutput(
        sspreadsheet->geometry_id.closure_input_output);
    const nodes::ClosurePtr closure_ptr = display_data.extract<nodes::ClosurePtr>();
    if (closure_ptr) {
      return std::make_unique<ClosureSignatureDataSource>(closure_ptr, in_out);
    }
    return {};
  }
  const eSpreadsheetColumnValueType column_type = cpp_type_to_column_type(*ptr.type());
  if (column_type == SPREADSHEET_VALUE_TYPE_UNKNOWN) {
    return {};
  }
  return std::make_unique<SingleValueDataSource>(ptr);
}

}  // namespace blender::ed::spreadsheet
