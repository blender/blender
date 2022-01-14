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
 */

#include "BLI_virtual_array.hh"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_volume.h"

#include "DNA_ID.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "DEG_depsgraph_query.h"

#include "ED_spreadsheet.h"

#include "NOD_geometry_nodes_eval_log.hh"

#include "BLT_translation.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "FN_field_cpp_type.hh"

#include "bmesh.h"

#include "spreadsheet_data_source_geometry.hh"
#include "spreadsheet_intern.hh"

namespace geo_log = blender::nodes::geometry_nodes_eval_log;
using blender::fn::GField;

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
  const fn::GSpan *values = columns_.lookup_ptr(column_id.name);
  if (values == nullptr) {
    return {};
  }
  return std::make_unique<ColumnValues>(column_id.name, fn::GVArray::ForSpan(*values));
}

void GeometryDataSource::foreach_default_column_ids(
    FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> fn) const
{
  if (component_->attribute_domain_size(domain_) == 0) {
    return;
  }

  if (component_->type() == GEO_COMPONENT_TYPE_INSTANCES) {
    fn({(char *)"Name"}, false);
  }

  extra_columns_.foreach_default_column_ids(fn);
  component_->attribute_foreach(
      [&](const bke::AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        if (meta_data.domain != domain_) {
          return true;
        }
        if (attribute_id.is_anonymous()) {
          return true;
        }
        SpreadsheetColumnID column_id;
        column_id.name = (char *)attribute_id.name().data();
        fn(column_id, false);
        return true;
      });

  if (component_->type() == GEO_COMPONENT_TYPE_INSTANCES) {
    fn({(char *)"Rotation"}, false);
    fn({(char *)"Scale"}, false);
  }
  else if (G.debug_value == 4001 && component_->type() == GEO_COMPONENT_TYPE_MESH) {
    if (domain_ == ATTR_DOMAIN_EDGE) {
      fn({(char *)"Vertex 1"}, false);
      fn({(char *)"Vertex 2"}, false);
    }
    else if (domain_ == ATTR_DOMAIN_FACE) {
      fn({(char *)"Corner Start"}, false);
      fn({(char *)"Corner Size"}, false);
    }
    else if (domain_ == ATTR_DOMAIN_CORNER) {
      fn({(char *)"Vertex"}, false);
      fn({(char *)"Edge"}, false);
    }
  }
}

std::unique_ptr<ColumnValues> GeometryDataSource::get_column_values(
    const SpreadsheetColumnID &column_id) const
{
  const int domain_size = component_->attribute_domain_size(domain_);
  if (domain_size == 0) {
    return {};
  }

  std::lock_guard lock{mutex_};

  std::unique_ptr<ColumnValues> extra_column_values = extra_columns_.get_column_values(column_id);
  if (extra_column_values) {
    return extra_column_values;
  }

  if (component_->type() == GEO_COMPONENT_TYPE_INSTANCES) {
    const InstancesComponent &instances = static_cast<const InstancesComponent &>(*component_);
    if (STREQ(column_id.name, "Name")) {
      Span<int> reference_handles = instances.instance_reference_handles();
      Span<InstanceReference> references = instances.references();
      return std::make_unique<ColumnValues>(
          column_id.name,
          VArray<InstanceReference>::ForFunc(domain_size,
                                             [reference_handles, references](int64_t index) {
                                               return references[reference_handles[index]];
                                             }));
    }
    Span<float4x4> transforms = instances.instance_transforms();
    if (STREQ(column_id.name, "Rotation")) {
      return std::make_unique<ColumnValues>(
          column_id.name, VArray<float3>::ForFunc(domain_size, [transforms](int64_t index) {
            return transforms[index].to_euler();
          }));
    }
    if (STREQ(column_id.name, "Scale")) {
      return std::make_unique<ColumnValues>(
          column_id.name, VArray<float3>::ForFunc(domain_size, [transforms](int64_t index) {
            return transforms[index].scale();
          }));
    }
  }
  else if (G.debug_value == 4001 && component_->type() == GEO_COMPONENT_TYPE_MESH) {
    const MeshComponent &component = static_cast<const MeshComponent &>(*component_);
    if (const Mesh *mesh = component.get_for_read()) {
      if (domain_ == ATTR_DOMAIN_EDGE) {
        if (STREQ(column_id.name, "Vertex 1")) {
          return std::make_unique<ColumnValues>(
              column_id.name, VArray<int>::ForFunc(mesh->totedge, [mesh](int64_t index) {
                return mesh->medge[index].v1;
              }));
        }
        if (STREQ(column_id.name, "Vertex 2")) {
          return std::make_unique<ColumnValues>(
              column_id.name, VArray<int>::ForFunc(mesh->totedge, [mesh](int64_t index) {
                return mesh->medge[index].v2;
              }));
        }
      }
      else if (domain_ == ATTR_DOMAIN_FACE) {
        if (STREQ(column_id.name, "Corner Start")) {
          return std::make_unique<ColumnValues>(
              column_id.name, VArray<int>::ForFunc(mesh->totpoly, [mesh](int64_t index) {
                return mesh->mpoly[index].loopstart;
              }));
        }
        if (STREQ(column_id.name, "Corner Size")) {
          return std::make_unique<ColumnValues>(
              column_id.name, VArray<int>::ForFunc(mesh->totpoly, [mesh](int64_t index) {
                return mesh->mpoly[index].totloop;
              }));
        }
      }
      else if (domain_ == ATTR_DOMAIN_CORNER) {
        if (STREQ(column_id.name, "Vertex")) {
          return std::make_unique<ColumnValues>(
              column_id.name, VArray<int>::ForFunc(mesh->totloop, [mesh](int64_t index) {
                return mesh->mloop[index].v;
              }));
        }
        if (STREQ(column_id.name, "Edge")) {
          return std::make_unique<ColumnValues>(
              column_id.name, VArray<int>::ForFunc(mesh->totloop, [mesh](int64_t index) {
                return mesh->mloop[index].e;
              }));
        }
      }
    }
  }

  bke::ReadAttributeLookup attribute = component_->attribute_try_get_for_read(column_id.name);
  if (!attribute) {
    return {};
  }
  fn::GVArray varray = std::move(attribute.varray);
  if (attribute.domain != domain_) {
    return {};
  }

  return std::make_unique<ColumnValues>(column_id.name, std::move(varray));
}

int GeometryDataSource::tot_rows() const
{
  return component_->attribute_domain_size(domain_);
}

/**
 * Only data sets corresponding to mesh objects in edit mode currently support selection filtering.
 */
bool GeometryDataSource::has_selection_filter() const
{
  Object *object_orig = DEG_get_original_object(object_eval_);
  if (object_orig->type != OB_MESH) {
    return false;
  }
  if (object_orig->mode != OB_MODE_EDIT) {
    return false;
  }
  if (component_->type() != GEO_COMPONENT_TYPE_MESH) {
    return false;
  }

  return true;
}

static IndexMask index_mask_from_bool_array(const VArray<bool> &selection,
                                            Vector<int64_t> &indices)
{
  for (const int i : selection.index_range()) {
    if (selection[i]) {
      indices.append(i);
    }
  }
  return IndexMask(indices);
}

IndexMask GeometryDataSource::apply_selection_filter(Vector<int64_t> &indices) const
{
  std::lock_guard lock{mutex_};

  BLI_assert(object_eval_->mode == OB_MODE_EDIT);
  BLI_assert(component_->type() == GEO_COMPONENT_TYPE_MESH);
  Object *object_orig = DEG_get_original_object(object_eval_);
  const MeshComponent *mesh_component = static_cast<const MeshComponent *>(component_);
  const Mesh *mesh_eval = mesh_component->get_for_read();
  Mesh *mesh_orig = (Mesh *)object_orig->data;
  BMesh *bm = mesh_orig->edit_mesh->bm;
  BM_mesh_elem_table_ensure(bm, BM_VERT);

  int *orig_indices = (int *)CustomData_get_layer(&mesh_eval->vdata, CD_ORIGINDEX);
  if (orig_indices != nullptr) {
    /* Use CD_ORIGINDEX layer if it exists. */
    VArray<bool> selection = mesh_component->attribute_try_adapt_domain<bool>(
        VArray<bool>::ForFunc(mesh_eval->totvert,
                              [bm, orig_indices](int vertex_index) -> bool {
                                const int i_orig = orig_indices[vertex_index];
                                if (i_orig < 0) {
                                  return false;
                                }
                                if (i_orig >= bm->totvert) {
                                  return false;
                                }
                                BMVert *vert = bm->vtable[i_orig];
                                return BM_elem_flag_test(vert, BM_ELEM_SELECT);
                              }),
        ATTR_DOMAIN_POINT,
        domain_);
    return index_mask_from_bool_array(selection, indices);
  }

  if (mesh_eval->totvert == bm->totvert) {
    /* Use a simple heuristic to match original vertices to evaluated ones. */
    VArray<bool> selection = mesh_component->attribute_try_adapt_domain<bool>(
        VArray<bool>::ForFunc(mesh_eval->totvert,
                              [bm](int vertex_index) -> bool {
                                BMVert *vert = bm->vtable[vertex_index];
                                return BM_elem_flag_test(vert, BM_ELEM_SELECT);
                              }),
        ATTR_DOMAIN_POINT,
        domain_);
    return index_mask_from_bool_array(selection, indices);
  }

  return IndexMask(mesh_eval->totvert);
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
  const Volume *volume = component_->get_for_read();
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
  const Volume *volume = component_->get_for_read();
  if (volume == nullptr) {
    return 0;
  }
  return BKE_volume_num_grids(volume);
}

GeometrySet spreadsheet_get_display_geometry_set(const SpaceSpreadsheet *sspreadsheet,
                                                 Object *object_eval)
{
  GeometrySet geometry_set;
  if (sspreadsheet->object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL) {
    Object *object_orig = DEG_get_original_object(object_eval);
    if (object_orig->type == OB_MESH) {
      MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
      if (object_orig->mode == OB_MODE_EDIT) {
        Mesh *mesh = (Mesh *)object_orig->data;
        BMEditMesh *em = mesh->edit_mesh;
        if (em != nullptr) {
          Mesh *new_mesh = (Mesh *)BKE_id_new_nomain(ID_ME, nullptr);
          /* This is a potentially heavy operation to do on every redraw. The best solution here is
           * to display the data directly from the bmesh without a conversion, which can be
           * implemented a bit later. */
          BM_mesh_bm_to_me_for_eval(em->bm, new_mesh, nullptr);
          mesh_component.replace(new_mesh, GeometryOwnershipType::Owned);
        }
      }
      else {
        Mesh *mesh = (Mesh *)object_orig->data;
        mesh_component.replace(mesh, GeometryOwnershipType::ReadOnly);
      }
    }
    else if (object_orig->type == OB_POINTCLOUD) {
      PointCloud *pointcloud = (PointCloud *)object_orig->data;
      PointCloudComponent &pointcloud_component =
          geometry_set.get_component_for_write<PointCloudComponent>();
      pointcloud_component.replace(pointcloud, GeometryOwnershipType::ReadOnly);
    }
  }
  else {
    if (object_eval->mode == OB_MODE_EDIT && object_eval->type == OB_MESH) {
      Mesh *mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(object_eval, false);
      if (mesh == nullptr) {
        return geometry_set;
      }
      BKE_mesh_wrapper_ensure_mdata(mesh);
      MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
      mesh_component.replace(mesh, GeometryOwnershipType::ReadOnly);
    }
    else {
      if (BLI_listbase_count(&sspreadsheet->context_path) == 1) {
        /* Use final evaluated object. */
        if (object_eval->runtime.geometry_set_eval != nullptr) {
          geometry_set = *object_eval->runtime.geometry_set_eval;
        }
      }
      else {
        const geo_log::NodeLog *node_log =
            geo_log::ModifierLog::find_node_by_spreadsheet_editor_context(*sspreadsheet);
        if (node_log != nullptr) {
          for (const geo_log::SocketLog &input_log : node_log->input_logs()) {
            if (const geo_log::GeometryValueLog *geo_value_log =
                    dynamic_cast<const geo_log::GeometryValueLog *>(input_log.value())) {
              const GeometrySet *full_geometry = geo_value_log->full_geometry();
              if (full_geometry != nullptr) {
                geometry_set = *full_geometry;
                break;
              }
            }
          }
        }
      }
    }
  }
  return geometry_set;
}

static void find_fields_to_evaluate(const SpaceSpreadsheet *sspreadsheet,
                                    Map<std::string, GField> &r_fields)
{
  if (sspreadsheet->object_eval_state != SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE) {
    return;
  }
  if (BLI_listbase_count(&sspreadsheet->context_path) <= 1) {
    /* No viewer is currently referenced by the context path. */
    return;
  }
  const geo_log::NodeLog *node_log = geo_log::ModifierLog::find_node_by_spreadsheet_editor_context(
      *sspreadsheet);
  if (node_log == nullptr) {
    return;
  }
  for (const geo_log::SocketLog &socket_log : node_log->input_logs()) {
    const geo_log::ValueLog *value_log = socket_log.value();
    if (value_log == nullptr) {
      continue;
    }
    if (const geo_log::GFieldValueLog *field_value_log =
            dynamic_cast<const geo_log::GFieldValueLog *>(value_log)) {
      const GField &field = field_value_log->field();
      if (field) {
        r_fields.add("Viewer", std::move(field));
      }
    }
  }
}

static GeometryComponentType get_display_component_type(const bContext *C, Object *object_eval)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  if (sspreadsheet->object_eval_state != SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL) {
    return (GeometryComponentType)sspreadsheet->geometry_component_type;
  }
  if (object_eval->type == OB_POINTCLOUD) {
    return GEO_COMPONENT_TYPE_POINT_CLOUD;
  }
  return GEO_COMPONENT_TYPE_MESH;
}

class GeometryComponentCacheKey : public SpreadsheetCache::Key {
 public:
  /* Use the pointer to the geometry component as a key to detect when the geometry changed. */
  const GeometryComponent *component;

  GeometryComponentCacheKey(const GeometryComponent &component) : component(&component)
  {
  }

  uint64_t hash() const override
  {
    return get_default_hash(this->component);
  }

  bool is_equal_to(const Key &other) const override
  {
    if (const GeometryComponentCacheKey *other_geo =
            dynamic_cast<const GeometryComponentCacheKey *>(&other)) {
      return this->component == other_geo->component;
    }
    return false;
  }
};

class GeometryComponentCacheValue : public SpreadsheetCache::Value {
 public:
  /* Stores the result of fields evaluated on a geometry component. Without this, fields would have
   * to be reevaluated on every redraw. */
  Map<std::pair<AttributeDomain, GField>, fn::GArray<>> arrays;
};

static void add_fields_as_extra_columns(SpaceSpreadsheet *sspreadsheet,
                                        const GeometryComponent &component,
                                        ExtraColumns &r_extra_columns)
{
  Map<std::string, GField> fields_to_show;
  find_fields_to_evaluate(sspreadsheet, fields_to_show);

  GeometryComponentCacheValue &cache =
      sspreadsheet->runtime->cache.lookup_or_add<GeometryComponentCacheValue>(
          std::make_unique<GeometryComponentCacheKey>(component));

  const AttributeDomain domain = (AttributeDomain)sspreadsheet->attribute_domain;
  const int domain_size = component.attribute_domain_size(domain);
  for (const auto item : fields_to_show.items()) {
    StringRef name = item.key;
    const GField &field = item.value;

    /* Use the cached evaluated array if it exists, otherwise evaluate the field now. */
    fn::GArray<> &evaluated_array = cache.arrays.lookup_or_add_cb({domain, field}, [&]() {
      fn::GArray<> evaluated_array(field.cpp_type(), domain_size);

      bke::GeometryComponentFieldContext field_context{component, domain};
      fn::FieldEvaluator field_evaluator{field_context, domain_size};
      field_evaluator.add_with_destination(field, evaluated_array);
      field_evaluator.evaluate();
      return evaluated_array;
    });

    r_extra_columns.add(std::move(name), evaluated_array.as_span());
  }
}

std::unique_ptr<DataSource> data_source_from_geometry(const bContext *C, Object *object_eval)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  const AttributeDomain domain = (AttributeDomain)sspreadsheet->attribute_domain;
  const GeometryComponentType component_type = get_display_component_type(C, object_eval);
  GeometrySet geometry_set = spreadsheet_get_display_geometry_set(sspreadsheet, object_eval);

  if (!geometry_set.has(component_type)) {
    return {};
  }

  const GeometryComponent &component = *geometry_set.get_component_for_read(component_type);
  ExtraColumns extra_columns;
  add_fields_as_extra_columns(sspreadsheet, component, extra_columns);

  if (component_type == GEO_COMPONENT_TYPE_VOLUME) {
    return std::make_unique<VolumeDataSource>(geometry_set);
  }
  return std::make_unique<GeometryDataSource>(
      object_eval, geometry_set, component_type, domain, std::move(extra_columns));
}

}  // namespace blender::ed::spreadsheet
