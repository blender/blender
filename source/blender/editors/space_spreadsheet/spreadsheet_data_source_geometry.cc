/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_virtual_array.hh"

#include "BKE_attribute.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_editmesh.h"
#include "BKE_geometry_fields.hh"
#include "BKE_global.h"
#include "BKE_instances.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.h"
#include "BKE_volume.h"
#include "BKE_volume_openvdb.hh"

#include "DNA_ID.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "DEG_depsgraph_query.h"

#include "ED_curves.hh"
#include "ED_spreadsheet.hh"

#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_geometry_nodes_log.hh"

#include "BLT_translation.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "FN_field_cpp_type.hh"

#include "bmesh.h"

/* Uncomment to view sculpt attributes in the spreadsheet. */
//#define DEBUG_SCULPT_BM_ATTRS

#ifdef DEBUG_SCULPT_BM_ATTRS
#  include "bmesh_varray.hh"
#endif

#include "spreadsheet_data_source_geometry.hh"
#include "spreadsheet_intern.hh"

using blender::nodes::geo_eval_log::ViewerNodeLog;

namespace blender::ed::spreadsheet {

static BMesh *get_object_bmesh(Object *object_eval)
{
#ifdef DEBUG_SCULPT_BM_ATTRS
  if (object_eval->mode == OB_MODE_SCULPT) {
    return object_eval->sculpt->bm;
  }
  else {
#endif
    Object *object_orig = DEG_get_original_object(object_eval);
    Mesh *mesh_orig = (Mesh *)object_orig->data;

    return mesh_orig->edit_mesh->bm;
#ifdef DEBUG_SCULPT_BM_ATTRS
  }
#endif
}

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
    const eAttrDomain domain,
    FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn)
{
  switch (domain) {
    case ATTR_DOMAIN_POINT:
      if (CustomData_has_layer(&mesh.vert_data, CD_ORIGINDEX)) {
        fn({(char *)"Original Index"}, false);
      }
      break;
    case ATTR_DOMAIN_EDGE:
      if (CustomData_has_layer(&mesh.edge_data, CD_ORIGINDEX)) {
        fn({(char *)"Original Index"}, false);
      }
      fn({(char *)"Vertices"}, false);
      break;
    case ATTR_DOMAIN_FACE:
      if (CustomData_has_layer(&mesh.face_data, CD_ORIGINDEX)) {
        fn({(char *)"Original Index"}, false);
      }
      fn({(char *)"Corner Start"}, false);
      fn({(char *)"Corner Size"}, false);
      break;
    case ATTR_DOMAIN_CORNER:
      fn({(char *)"Vertex"}, false);
      fn({(char *)"Edge"}, false);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

static std::unique_ptr<ColumnValues> build_mesh_debug_columns(const Mesh &mesh,
                                                              const eAttrDomain domain,
                                                              const StringRef name)
{
  switch (domain) {
    case ATTR_DOMAIN_POINT: {
      if (name == "Original Index") {
        const int *data = static_cast<const int *>(
            CustomData_get_layer(&mesh.vert_data, CD_ORIGINDEX));
        if (data) {
          return std::make_unique<ColumnValues>(name, VArray<int>::ForSpan({data, mesh.totvert}));
        }
      }
      return {};
    }
    case ATTR_DOMAIN_EDGE: {
      if (name == "Original Index") {
        const int *data = static_cast<const int *>(
            CustomData_get_layer(&mesh.edge_data, CD_ORIGINDEX));
        if (data) {
          return std::make_unique<ColumnValues>(name, VArray<int>::ForSpan({data, mesh.totedge}));
        }
      }
      if (name == "Vertices") {
        return std::make_unique<ColumnValues>(name, VArray<int2>::ForSpan(mesh.edges()));
      }
      return {};
    }
    case ATTR_DOMAIN_FACE: {
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
    case ATTR_DOMAIN_CORNER: {
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
#ifdef DEBUG_SCULPT_BM_ATTRS
  if (object_eval_->mode == OB_MODE_SCULPT && object_eval_->sculpt && object_eval_->sculpt->bm) {
    CustomData *cdata = nullptr;
    BMesh *bm = object_eval_->sculpt->bm;

    switch (domain_) {
      case ATTR_DOMAIN_POINT:
        cdata = &bm->vdata;
        break;
      case ATTR_DOMAIN_EDGE:
        cdata = &bm->edata;
        break;
      case ATTR_DOMAIN_FACE:
        cdata = &bm->pdata;
        break;
    }

    if (cdata) {
      const eCustomDataMask typemask = CD_MASK_PROP_FLOAT | CD_MASK_PROP_FLOAT2 |
                                       CD_MASK_PROP_FLOAT3 | CD_MASK_PROP_COLOR |
                                       CD_MASK_PROP_BYTE_COLOR | CD_MASK_PROP_INT32 |
                                       CD_MASK_PROP_INT8 | CD_MASK_ORIGINDEX;

      for (int i = 0; i < cdata->totlayer; i++) {
        CustomDataLayer *layer = &cdata->layers[i];

        if (!bke::allow_procedural_attribute_access(layer->name)) {
          continue;
        }

        if (CD_TYPE_AS_MASK(layer->type) & typemask) {
          fn({layer->name}, false);
        }
      }
      return;
    }
  }
#endif

  if (!component_->attributes().has_value()) {
    return;
  }
  const bke::AttributeAccessor attributes = *component_->attributes();

  if (attributes.domain_size(domain_) == 0) {
    return;
  }

  if (component_->type() == bke::GeometryComponent::Type::Instance) {
    fn({(char *)"Name"}, false);
  }

  extra_columns_.foreach_default_column_ids(fn);

  attributes.for_all(
      [&](const bke::AttributeIDRef &attribute_id, const bke::AttributeMetaData &meta_data) {
        if (meta_data.domain != domain_) {
          return true;
        }
        if (attribute_id.is_anonymous()) {
          return true;
        }
        if (!bke::allow_procedural_attribute_access(attribute_id.name())) {
          return true;
        }
        SpreadsheetColumnID column_id;
        column_id.name = (char *)attribute_id.name().data();
        const bool is_front = attribute_id.name() == ".viewer";
        fn(column_id, is_front);
        return true;
      });

  if (component_->type() == bke::GeometryComponent::Type::Instance) {
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
  if (!component_->attributes().has_value()) {
    return {};
  }
  const bke::AttributeAccessor attributes = *component_->attributes();
  const int domain_num = attributes.domain_size(domain_);
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
      if (STREQ(column_id.name, "Rotation")) {
        return std::make_unique<ColumnValues>(
            column_id.name, VArray<float3>::ForFunc(domain_num, [transforms](int64_t index) {
              return float3(math::to_euler(math::normalize(transforms[index])));
            }));
      }
      if (STREQ(column_id.name, "Scale")) {
        return std::make_unique<ColumnValues>(
            column_id.name, VArray<float3>::ForFunc(domain_num, [transforms](int64_t index) {
              return math::to_scale(transforms[index]);
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

#ifdef DEBUG_SCULPT_BM_ATTRS
  if (object_eval_->mode == OB_MODE_SCULPT && object_eval_->sculpt && object_eval_->sculpt->bm) {
    char htype = 0;
    BMesh *bm = object_eval_->sculpt->bm;

    CustomData *cdata = &bm->vdata;
    for (int i = 0; !htype && i < 4; i++, cdata++) {
      for (int j = 0; j < cdata->totlayer; j++) {
        CustomDataLayer *layer = &cdata->layers[j];

        if (STREQ(layer->name, column_id.name)) {
          htype = 1 << i;
          break;
        }
      }
    }

    if (htype) {
      BM_mesh_elem_table_ensure(object_eval_->sculpt->bm, htype);

      GVArray bmarray = blender::bmesh::bmesh_attr_gvarray(
          object_eval_->sculpt->bm, htype, column_id.name);

      if (bmarray.size() > 0) {
        printf("%s: bmesh adaptor: %s\n", __func__, column_id.name);
        StringRefNull column_display_name = column_id.name;
        return std::make_unique<ColumnValues>(column_display_name, std::move(bmarray));
      }
      else {
        printf("%s: unknown bmesh attribute %s\n", __func__, column_id.name);
      }
    }
  }
#endif

  bke::GAttributeReader attribute = attributes.lookup(column_id.name);
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
  if (!component_->attributes().has_value()) {
    return {};
  }
  const bke::AttributeAccessor attributes = *component_->attributes();
  return attributes.domain_size(domain_);
}

bool GeometryDataSource::has_selection_filter() const
{
  Object *object_orig = DEG_get_original_object(object_eval_);
  switch (component_->type()) {
    case bke::GeometryComponent::Type::Mesh: {
      if (object_orig->type != OB_MESH) {
        return false;
      }
#ifdef DEBUG_SCULPT_BM_ATTRS
      if (!ELEM(object_orig->mode, OB_MODE_EDIT, OB_MODE_SCULPT)) {
#else
      if (object_orig->mode != OB_MODE_EDIT) {
#endif
        return false;
      }
      return true;
    }
    case bke::GeometryComponent::Type::Curve: {
      if (object_orig->type != OB_CURVES) {
        return false;
      }
      if (!ELEM(object_orig->mode, OB_MODE_SCULPT_CURVES, OB_MODE_EDIT)) {
        return false;
      }
      return true;
    }
    case bke::GeometryComponent::Type::PointCloud: {
      if (object_orig->type != OB_POINTCLOUD) {
        return false;
      }
      if (object_orig->mode != OB_MODE_EDIT) {
        return false;
      }
      return true;
    }
    default:
      return false;
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
      BLI_assert(object_eval_->type == OB_MESH);
      // BLI_assert(object_eval_->mode == OB_MODE_EDIT);
      Object *object_orig = DEG_get_original_object(object_eval_);
      const Mesh *mesh_eval = geometry_set_.get_mesh();
      const bke::AttributeAccessor attributes_eval = mesh_eval->attributes();
      Mesh *mesh_orig = (Mesh *)object_orig->data;
      BMesh *bm = get_object_bmesh(object_eval_);
      BM_mesh_elem_table_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

      const int *orig_indices = (const int *)CustomData_get_layer(&mesh_eval->vert_data,
                                                                  CD_ORIGINDEX);
      if (orig_indices != nullptr) {
        /* Use CD_ORIGINDEX layer if it exists. */
        VArray<bool> selection = attributes_eval.adapt_domain<bool>(
            VArray<bool>::ForFunc(mesh_eval->totvert,
                                  [bm, orig_indices](int vertex_index) -> bool {
                                    const int i_orig = orig_indices[vertex_index];
                                    if (i_orig < 0) {
                                      return false;
                                    }
                                    if (i_orig >= bm->totvert) {
                                      return false;
                                    }
                                    const BMVert *vert = BM_vert_at_index(bm, i_orig);
                                    return BM_elem_flag_test(vert, BM_ELEM_SELECT);
                                  }),
            ATTR_DOMAIN_POINT,
            domain_);
        return IndexMask::from_bools(selection, memory);
      }

      if (mesh_eval->totvert == bm->totvert) {
        /* Use a simple heuristic to match original vertices to evaluated ones. */
        VArray<bool> selection = attributes_eval.adapt_domain<bool>(
            VArray<bool>::ForFunc(mesh_eval->totvert,
                                  [bm](int vertex_index) -> bool {
                                    const BMVert *vert = BM_vert_at_index(bm, vertex_index);
                                    return BM_elem_flag_test(vert, BM_ELEM_SELECT);
                                  }),
            ATTR_DOMAIN_POINT,
            domain_);
        return IndexMask::from_bools(selection, memory);
      }

      return full_range;
    }
    case bke::GeometryComponent::Type::Curve: {
      BLI_assert(object_eval_->type == OB_CURVES);
      const bke::CurveComponent &component = static_cast<const bke::CurveComponent &>(*component_);
      const Curves &curves_id = *component.get();
      switch (domain_) {
        case ATTR_DOMAIN_POINT:
          return curves::retrieve_selected_points(curves_id, memory);
        case ATTR_DOMAIN_CURVE:
          return curves::retrieve_selected_curves(curves_id, memory);
        default:
          BLI_assert_unreachable();
      }
      return full_range;
    }
    case bke::GeometryComponent::Type::PointCloud: {
      BLI_assert(object_eval_->type == OB_POINTCLOUD);
      const bke::AttributeAccessor attributes = *component_->attributes();
      const VArray<bool> &selection = *attributes.lookup_or_default(
          ".selection", ATTR_DOMAIN_POINT, false);
      return IndexMask::from_bools(selection, memory);
    }
    default:
      return full_range;
  }
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
          const VolumeGrid *volume_grid = BKE_volume_grid_get_for_read(volume, index);
          return BKE_volume_grid_name(volume_grid);
        }));
  }
  if (STREQ(column_id.name, "Data Type")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Data Type"), VArray<std::string>::ForFunc(size, [volume](int64_t index) {
          const VolumeGrid *volume_grid = BKE_volume_grid_get_for_read(volume, index);
          const VolumeGridType type = BKE_volume_grid_type(volume_grid);
          const char *name = nullptr;
          RNA_enum_name_from_value(rna_enum_volume_grid_data_type_items, type, &name);
          return IFACE_(name);
        }));
  }
  if (STREQ(column_id.name, "Class")) {
    return std::make_unique<ColumnValues>(
        IFACE_("Class"), VArray<std::string>::ForFunc(size, [volume](int64_t index) {
          const VolumeGrid *volume_grid = BKE_volume_grid_get_for_read(volume, index);
          openvdb::GridBase::ConstPtr grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);
          openvdb::GridClass grid_class = grid->getGridClass();
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

bke::GeometrySet spreadsheet_get_display_geometry_set(const SpaceSpreadsheet *sspreadsheet,
                                                      Object *object_eval)
{
  bke::GeometrySet geometry_set;
  if (sspreadsheet->object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL) {
    const Object *object_orig = DEG_get_original_object(object_eval);
    if (object_orig->type == OB_MESH) {
      const Mesh *mesh = static_cast<const Mesh *>(object_orig->data);
      if (object_orig->mode == OB_MODE_EDIT) {
        if (const BMEditMesh *em = mesh->edit_mesh) {
          Mesh *new_mesh = (Mesh *)BKE_id_new_nomain(ID_ME, nullptr);
          /* This is a potentially heavy operation to do on every redraw. The best solution here is
           * to display the data directly from the bmesh without a conversion, which can be
           * implemented a bit later. */
          BM_mesh_bm_to_me_for_eval(em->bm, new_mesh, nullptr);
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
  }
  else {
    if (BLI_listbase_is_single(&sspreadsheet->viewer_path.path)) {
      if (const bke::GeometrySet *geometry_eval = object_eval->runtime.geometry_set_eval) {
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

std::unique_ptr<DataSource> data_source_from_geometry(const bContext *C, Object *object_eval)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  const eAttrDomain domain = (eAttrDomain)sspreadsheet->attribute_domain;
  const auto component_type = bke::GeometryComponent::Type(sspreadsheet->geometry_component_type);
  bke::GeometrySet geometry_set = spreadsheet_get_display_geometry_set(sspreadsheet, object_eval);
  if (!geometry_set.has(component_type)) {
    return {};
  }

  if (component_type == bke::GeometryComponent::Type::Volume) {
    return std::make_unique<VolumeDataSource>(std::move(geometry_set));
  }
  return std::make_unique<GeometryDataSource>(
      object_eval, std::move(geometry_set), component_type, domain);
}

}  // namespace blender::ed::spreadsheet
