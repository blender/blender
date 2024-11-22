/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_virtual_array.hh"

#include "BKE_attribute.hh"
#include "BKE_compute_contexts.hh"
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
#include "BKE_node_socket_value.hh"
#include "BKE_object_types.hh"
#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"

#include "DNA_ID.h"
#include "DNA_pointcloud_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "DEG_depsgraph_query.hh"

#include "ED_curves.hh"
#include "ED_outliner.hh"
#include "ED_spreadsheet.hh"

#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_geometry_nodes_log.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "UI_resources.hh"

#include "bmesh.hh"

#include "spreadsheet_data_source_geometry.hh"
#include "spreadsheet_intern.hh"

using blender::nodes::geo_eval_log::ViewerNodeLog;

namespace blender::ed::spreadsheet {

void ExtraColumns::foreach_default_column_ids(
    FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const
{
  for (const auto item : columns_.items()) {
    SpreadsheetColumnID column_id;
    column_id.name = (char *)item.key.c_str();
    fn(column_id, true);
  }
}

std::unique_ptr<ColumnValues> ExtraColumns::get_column_values(
    const SpreadsheetColumnID &column_id) const
{
  const GSpan *values = columns_.lookup_ptr(column_id.name);
  if (values == nullptr) {
    return {};
  }
  return std::make_unique<ColumnValues>(column_id.name, GVArray::ForSpan(*values));
}

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
      fn({(char *)"Vertices"}, false);
      break;
    case bke::AttrDomain::Face:
      if (CustomData_has_layer(&mesh.face_data, CD_ORIGINDEX)) {
        fn({(char *)"Original Index"}, false);
      }
      fn({(char *)"Corner Start"}, false);
      fn({(char *)"Corner Size"}, false);
      break;
    case bke::AttrDomain::Corner:
      fn({(char *)"Vertex"}, false);
      fn({(char *)"Edge"}, false);
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
                                                VArray<int>::ForSpan({data, mesh.verts_num}));
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
                                                VArray<int>::ForSpan({data, mesh.edges_num}));
        }
      }
      if (name == "Vertices") {
        return std::make_unique<ColumnValues>(name, VArray<int2>::ForSpan(mesh.edges()));
      }
      return {};
    }
    case bke::AttrDomain::Face: {
      if (name == "Original Index") {
        const int *data = static_cast<const int *>(
            CustomData_get_layer(&mesh.face_data, CD_ORIGINDEX));
        if (data) {
          return std::make_unique<ColumnValues>(name,
                                                VArray<int>::ForSpan({data, mesh.faces_num}));
        }
      }
      if (name == "Corner Start") {
        return std::make_unique<ColumnValues>(
            name, VArray<int>::ForSpan(mesh.face_offsets().drop_back(1)));
      }
      if (name == "Corner Size") {
        const OffsetIndices faces = mesh.faces();
        return std::make_unique<ColumnValues>(
            name, VArray<int>::ForFunc(faces.size(), [faces](int64_t index) {
              return faces[index].size();
            }));
      }
      return {};
    }
    case bke::AttrDomain::Corner: {
      if (name == "Vertex") {
        return std::make_unique<ColumnValues>(name, VArray<int>::ForSpan(mesh.corner_verts()));
      }
      if (name == "Edge") {
        return std::make_unique<ColumnValues>(name, VArray<int>::ForSpan(mesh.corner_edges()));
      }
      return {};
    }
    default:
      BLI_assert_unreachable();
      return {};
  }
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

  extra_columns_.foreach_default_column_ids(fn);

  attributes->foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != domain_) {
      return;
    }
    if (bke::attribute_name_is_anonymous(iter.name)) {
      return;
    }
    if (!bke::allow_procedural_attribute_access(iter.name)) {
      return;
    }
    if (iter.domain == bke::AttrDomain::Instance && iter.name == "instance_transform") {
      /* Don't display the instance transform attribute, since matrix visualization in the
       * spreadsheet isn't helpful. */
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

  std::lock_guard lock{mutex_};

  std::unique_ptr<ColumnValues> extra_column_values = extra_columns_.get_column_values(column_id);
  if (extra_column_values) {
    return extra_column_values;
  }

  if (component_->type() == bke::GeometryComponent::Type::Instance) {
    if (const bke::Instances *instances =
            static_cast<const bke::InstancesComponent &>(*component_).get())
    {
      if (STREQ(column_id.name, "Name")) {
        Span<int> reference_handles = instances->reference_handles();
        Span<bke::InstanceReference> references = instances->references();
        return std::make_unique<ColumnValues>(
            column_id.name,
            VArray<bke::InstanceReference>::ForFunc(
                domain_num, [reference_handles, references](int64_t index) {
                  return references[reference_handles[index]];
                }));
      }
      Span<float4x4> transforms = instances->transforms();
      if (STREQ(column_id.name, "Position")) {
        return std::make_unique<ColumnValues>(
            column_id.name, VArray<float3>::ForFunc(domain_num, [transforms](int64_t index) {
              return transforms[index].location();
            }));
      }
      if (STREQ(column_id.name, "Rotation")) {
        return std::make_unique<ColumnValues>(
            column_id.name, VArray<float3>::ForFunc(domain_num, [transforms](int64_t index) {
              return float3(math::to_euler(math::normalize(transforms[index])));
            }));
      }
      if (STREQ(column_id.name, "Scale")) {
        return std::make_unique<ColumnValues>(
            column_id.name, VArray<float3>::ForFunc(domain_num, [transforms](int64_t index) {
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
            column_id.name, VArray<std::string>::ForFunc(domain_num, [layers](int64_t index) {
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
          VArray<bool>::ForSpan(face_selection), bke::AttrDomain::Face, bke::AttrDomain::Corner);
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
          ".selection", bke::AttrDomain::Point, false);
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

  for (const char *name : {"Grid Name", "Data Type", "Class"}) {
    SpreadsheetColumnID column_id{(char *)name};
    fn(column_id, false);
  }
}

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
        IFACE_("Grid Name"), VArray<std::string>::ForFunc(size, [volume](int64_t index) {
          const bke::VolumeGridData *volume_grid = BKE_volume_grid_get(volume, index);
          return volume_grid->name();
        }));
  }
  if (STREQ(column_id.name, "Data Type")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Data Type"), VArray<std::string>::ForFunc(size, [volume](int64_t index) {
          const bke::VolumeGridData *volume_grid = BKE_volume_grid_get(volume, index);
          const VolumeGridType type = volume_grid->grid_type();
          const char *name = nullptr;
          RNA_enum_name_from_value(rna_enum_volume_grid_data_type_items, type, &name);
          return IFACE_(name);
        }));
  }
  if (STREQ(column_id.name, "Class")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Class"), VArray<std::string>::ForFunc(size, [volume](int64_t index) {
          const bke::VolumeGridData *volume_grid = BKE_volume_grid_get(volume, index);
          openvdb::GridClass grid_class = volume_grid->grid_class();
          if (grid_class == openvdb::GridClass::GRID_FOG_VOLUME) {
            return IFACE_("Fog Volume");
          }
          if (grid_class == openvdb::GridClass::GRID_LEVEL_SET) {
            return IFACE_("Level Set");
          }
          return IFACE_("Unknown");
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

bke::GeometrySet spreadsheet_get_display_geometry_set(const SpaceSpreadsheet *sspreadsheet,
                                                      Object *object_eval)
{
  bke::GeometrySet geometry_set;
  if (sspreadsheet->object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL) {
    const Object *object_orig = DEG_get_original_object(object_eval);
    if (object_orig->type == OB_MESH) {
      const Mesh *mesh = static_cast<const Mesh *>(object_orig->data);
      if (object_orig->mode == OB_MODE_EDIT) {
        if (const BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
          Mesh *new_mesh = (Mesh *)BKE_id_new_nomain(ID_ME, nullptr);
          /* This is a potentially heavy operation to do on every redraw. The best solution here is
           * to display the data directly from the bmesh without a conversion, which can be
           * implemented a bit later. */
          BM_mesh_bm_to_me_for_eval(*em->bm, *new_mesh, nullptr);
          geometry_set.replace_mesh(new_mesh, bke::GeometryOwnershipType::Owned);
        }
      }
      else {
        geometry_set.replace_mesh(const_cast<Mesh *>(mesh), bke::GeometryOwnershipType::ReadOnly);
      }
    }
    else if (object_orig->type == OB_POINTCLOUD) {
      const PointCloud *pointcloud = static_cast<const PointCloud *>(object_orig->data);
      geometry_set.replace_pointcloud(const_cast<PointCloud *>(pointcloud),
                                      bke::GeometryOwnershipType::ReadOnly);
    }
    else if (object_orig->type == OB_CURVES) {
      const Curves &curves_id = *static_cast<const Curves *>(object_orig->data);
      geometry_set.replace_curves(&const_cast<Curves &>(curves_id),
                                  bke::GeometryOwnershipType::ReadOnly);
    }
    else if (object_orig->type == OB_GREASE_PENCIL) {
      const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(object_orig->data);
      geometry_set.replace_grease_pencil(&const_cast<GreasePencil &>(grease_pencil),
                                         bke::GeometryOwnershipType::ReadOnly);
    }
  }
  else {
    if (BLI_listbase_is_single(&sspreadsheet->viewer_path.path)) {
      if (const bke::GeometrySet *geometry_eval = object_eval->runtime->geometry_set_eval) {
        geometry_set = *geometry_eval;
      }

      if (object_eval->mode == OB_MODE_EDIT && object_eval->type == OB_MESH) {
        if (Mesh *mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(object_eval)) {
          BKE_mesh_wrapper_ensure_mdata(mesh);
          geometry_set.replace_mesh(mesh, bke::GeometryOwnershipType::ReadOnly);
        }
      }
    }
    else {
      if (const ViewerNodeLog *viewer_log =
              nodes::geo_eval_log::GeoModifierLog::find_viewer_node_log_for_path(
                  sspreadsheet->viewer_path))
      {
        geometry_set = viewer_log->geometry;
      }
    }
  }
  return geometry_set;
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

  const bke::GeometrySet root_geometry_set = spreadsheet_get_display_geometry_set(sspreadsheet,
                                                                                  object_eval);
  const bke::GeometrySet geometry_set = get_geometry_set_for_instance_ids(
      root_geometry_set, Span{sspreadsheet->instance_ids, sspreadsheet->instance_ids_num});

  const bke::AttrDomain domain = (bke::AttrDomain)sspreadsheet->attribute_domain;
  const auto component_type = bke::GeometryComponent::Type(sspreadsheet->geometry_component_type);
  const int active_layer_index = sspreadsheet->active_layer_index;
  if (!geometry_set.has(component_type)) {
    return {};
  }

  if (component_type == bke::GeometryComponent::Type::Volume) {
    return std::make_unique<VolumeDataSource>(std::move(geometry_set));
  }
  Object *object_orig = sspreadsheet->instance_ids_num == 0 ?
                            DEG_get_original_object(object_eval) :
                            nullptr;
  return std::make_unique<GeometryDataSource>(
      object_orig, std::move(geometry_set), component_type, domain, active_layer_index);
}

}  // namespace blender::ed::spreadsheet
