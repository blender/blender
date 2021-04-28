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

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"

#include "DNA_ID.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "DEG_depsgraph_query.h"

#include "ED_spreadsheet.h"

#include "bmesh.h"

#include "spreadsheet_data_source_geometry.hh"
#include "spreadsheet_intern.hh"

namespace blender::ed::spreadsheet {

void GeometryDataSource::foreach_default_column_ids(
    FunctionRef<void(const SpreadsheetColumnID &)> fn) const
{
  component_->attribute_foreach([&](StringRefNull name, const AttributeMetaData &meta_data) {
    if (meta_data.domain != domain_) {
      return true;
    }
    SpreadsheetColumnID column_id;
    column_id.name = (char *)name.c_str();
    fn(column_id);
    return true;
  });
}

std::unique_ptr<ColumnValues> GeometryDataSource::get_column_values(
    const SpreadsheetColumnID &column_id) const
{
  std::lock_guard lock{mutex_};

  bke::ReadAttributeLookup attribute = component_->attribute_try_get_for_read(column_id.name);
  if (!attribute) {
    return {};
  }
  const fn::GVArray *varray = scope_.add(std::move(attribute.varray), __func__);
  if (attribute.domain != domain_) {
    return {};
  }
  int domain_size = varray->size();
  const CustomDataType type = bke::cpp_type_to_custom_data_type(varray->type());
  switch (type) {
    case CD_PROP_FLOAT:
      return column_values_from_function(
          column_id.name, domain_size, [varray](int index, CellValue &r_cell_value) {
            float value;
            varray->get(index, &value);
            r_cell_value.value_float = value;
          });
    case CD_PROP_INT32:
      return column_values_from_function(
          column_id.name, domain_size, [varray](int index, CellValue &r_cell_value) {
            int value;
            varray->get(index, &value);
            r_cell_value.value_int = value;
          });
    case CD_PROP_BOOL:
      return column_values_from_function(
          column_id.name, domain_size, [varray](int index, CellValue &r_cell_value) {
            bool value;
            varray->get(index, &value);
            r_cell_value.value_bool = value;
          });
    case CD_PROP_FLOAT2: {
      return column_values_from_function(
          column_id.name,
          domain_size,
          [varray](int index, CellValue &r_cell_value) {
            float2 value;
            varray->get(index, &value);
            r_cell_value.value_float2 = value;
          },
          default_float2_column_width);
    }
    case CD_PROP_FLOAT3: {
      return column_values_from_function(
          column_id.name,
          domain_size,
          [varray](int index, CellValue &r_cell_value) {
            float3 value;
            varray->get(index, &value);
            r_cell_value.value_float3 = value;
          },
          default_float3_column_width);
    }
    case CD_PROP_COLOR: {
      return column_values_from_function(
          column_id.name,
          domain_size,
          [varray](int index, CellValue &r_cell_value) {
            Color4f value;
            varray->get(index, &value);
            r_cell_value.value_color = value;
          },
          default_color_column_width);
    }
    default:
      break;
  }
  return {};
}

int GeometryDataSource::tot_rows() const
{
  return component_->attribute_domain_size(domain_);
}

using IsVertexSelectedFn = FunctionRef<bool(int vertex_index)>;

static void get_selected_vertex_indices(const Mesh &mesh,
                                        const IsVertexSelectedFn is_vertex_selected_fn,
                                        Vector<int64_t> &r_vertex_indices)
{
  for (const int i : IndexRange(mesh.totvert)) {
    if (is_vertex_selected_fn(i)) {
      r_vertex_indices.append(i);
    }
  }
}

static void get_selected_corner_indices(const Mesh &mesh,
                                        const IsVertexSelectedFn is_vertex_selected_fn,
                                        Vector<int64_t> &r_corner_indices)
{
  for (const int i : IndexRange(mesh.totloop)) {
    const MLoop &loop = mesh.mloop[i];
    if (is_vertex_selected_fn(loop.v)) {
      r_corner_indices.append(i);
    }
  }
}

static void get_selected_face_indices(const Mesh &mesh,
                                      const IsVertexSelectedFn is_vertex_selected_fn,
                                      Vector<int64_t> &r_face_indices)
{
  for (const int poly_index : IndexRange(mesh.totpoly)) {
    const MPoly &poly = mesh.mpoly[poly_index];
    bool is_selected = true;
    for (const int loop_index : IndexRange(poly.loopstart, poly.totloop)) {
      const MLoop &loop = mesh.mloop[loop_index];
      if (!is_vertex_selected_fn(loop.v)) {
        is_selected = false;
        break;
      }
    }
    if (is_selected) {
      r_face_indices.append(poly_index);
    }
  }
}

static void get_selected_edge_indices(const Mesh &mesh,
                                      const IsVertexSelectedFn is_vertex_selected_fn,
                                      Vector<int64_t> &r_edge_indices)
{
  for (const int i : IndexRange(mesh.totedge)) {
    const MEdge &edge = mesh.medge[i];
    if (is_vertex_selected_fn(edge.v1) && is_vertex_selected_fn(edge.v2)) {
      r_edge_indices.append(i);
    }
  }
}

static void get_selected_indices_on_domain(const Mesh &mesh,
                                           const AttributeDomain domain,
                                           const IsVertexSelectedFn is_vertex_selected_fn,
                                           Vector<int64_t> &r_indices)
{
  switch (domain) {
    case ATTR_DOMAIN_POINT:
      return get_selected_vertex_indices(mesh, is_vertex_selected_fn, r_indices);
    case ATTR_DOMAIN_FACE:
      return get_selected_face_indices(mesh, is_vertex_selected_fn, r_indices);
    case ATTR_DOMAIN_CORNER:
      return get_selected_corner_indices(mesh, is_vertex_selected_fn, r_indices);
    case ATTR_DOMAIN_EDGE:
      return get_selected_edge_indices(mesh, is_vertex_selected_fn, r_indices);
    default:
      return;
  }
}

Span<int64_t> GeometryDataSource::get_selected_element_indices() const
{
  std::lock_guard lock{mutex_};

  BLI_assert(object_eval_->mode == OB_MODE_EDIT);
  BLI_assert(component_->type() == GEO_COMPONENT_TYPE_MESH);
  Object *object_orig = DEG_get_original_object(object_eval_);
  Vector<int64_t> &indices = scope_.construct<Vector<int64_t>>(__func__);
  const MeshComponent *mesh_component = static_cast<const MeshComponent *>(component_);
  const Mesh *mesh_eval = mesh_component->get_for_read();
  Mesh *mesh_orig = (Mesh *)object_orig->data;
  BMesh *bm = mesh_orig->edit_mesh->bm;
  BM_mesh_elem_table_ensure(bm, BM_VERT);

  int *orig_indices = (int *)CustomData_get_layer(&mesh_eval->vdata, CD_ORIGINDEX);
  if (orig_indices != nullptr) {
    /* Use CD_ORIGINDEX layer if it exists. */
    auto is_vertex_selected = [&](int vertex_index) -> bool {
      const int i_orig = orig_indices[vertex_index];
      if (i_orig < 0) {
        return false;
      }
      if (i_orig >= bm->totvert) {
        return false;
      }
      BMVert *vert = bm->vtable[i_orig];
      return BM_elem_flag_test(vert, BM_ELEM_SELECT);
    };
    get_selected_indices_on_domain(*mesh_eval, domain_, is_vertex_selected, indices);
  }
  else if (mesh_eval->totvert == bm->totvert) {
    /* Use a simple heuristic to match original vertices to evaluated ones. */
    auto is_vertex_selected = [&](int vertex_index) -> bool {
      BMVert *vert = bm->vtable[vertex_index];
      return BM_elem_flag_test(vert, BM_ELEM_SELECT);
    };
    get_selected_indices_on_domain(*mesh_eval, domain_, is_vertex_selected, indices);
  }

  return indices;
}

void InstancesDataSource::foreach_default_column_ids(
    FunctionRef<void(const SpreadsheetColumnID &)> fn) const
{
  if (component_->instances_amount() == 0) {
    return;
  }

  SpreadsheetColumnID column_id;
  column_id.name = (char *)"Name";
  fn(column_id);
  for (const char *name : {"Position", "Rotation", "Scale", "ID"}) {
    column_id.name = (char *)name;
    fn(column_id);
  }
}

std::unique_ptr<ColumnValues> InstancesDataSource::get_column_values(
    const SpreadsheetColumnID &column_id) const
{
  if (component_->instances_amount() == 0) {
    return {};
  }

  const int size = this->tot_rows();
  if (STREQ(column_id.name, "Name")) {
    Span<InstancedData> instance_data = component_->instanced_data();
    std::unique_ptr<ColumnValues> values = column_values_from_function(
        "Name", size, [instance_data](int index, CellValue &r_cell_value) {
          const InstancedData &data = instance_data[index];
          if (data.type == INSTANCE_DATA_TYPE_OBJECT) {
            if (data.data.object != nullptr) {
              r_cell_value.value_object = ObjectCellValue{data.data.object};
            }
          }
          else if (data.type == INSTANCE_DATA_TYPE_COLLECTION) {
            if (data.data.collection != nullptr) {
              r_cell_value.value_collection = CollectionCellValue{data.data.collection};
            }
          }
        });
    values->default_width = 8.0f;
    return values;
  }
  Span<float4x4> transforms = component_->transforms();
  if (STREQ(column_id.name, "Position")) {
    return column_values_from_function(
        column_id.name,
        size,
        [transforms](int index, CellValue &r_cell_value) {
          r_cell_value.value_float3 = transforms[index].translation();
        },
        default_float3_column_width);
  }
  if (STREQ(column_id.name, "Rotation")) {
    return column_values_from_function(
        column_id.name,
        size,
        [transforms](int index, CellValue &r_cell_value) {
          r_cell_value.value_float3 = transforms[index].to_euler();
        },
        default_float3_column_width);
  }
  if (STREQ(column_id.name, "Scale")) {
    return column_values_from_function(
        column_id.name,
        size,
        [transforms](int index, CellValue &r_cell_value) {
          r_cell_value.value_float3 = transforms[index].scale();
        },
        default_float3_column_width);
  }
  Span<int> ids = component_->ids();
  if (STREQ(column_id.name, "ID")) {
    /* Make the column a bit wider by default, since the IDs tend to be large numbers. */
    return column_values_from_function(
        column_id.name,
        size,
        [ids](int index, CellValue &r_cell_value) { r_cell_value.value_int = ids[index]; },
        5.5f);
  }
  return {};
}

int InstancesDataSource::tot_rows() const
{
  return component_->instances_amount();
}

static GeometrySet get_display_geometry_set(SpaceSpreadsheet *sspreadsheet,
                                            Object *object_eval,
                                            const GeometryComponentType used_component_type)
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
      mesh_component.copy_vertex_group_names_from_object(*object_orig);
    }
    else if (object_orig->type == OB_POINTCLOUD) {
      PointCloud *pointcloud = (PointCloud *)object_orig->data;
      PointCloudComponent &pointcloud_component =
          geometry_set.get_component_for_write<PointCloudComponent>();
      pointcloud_component.replace(pointcloud, GeometryOwnershipType::ReadOnly);
    }
  }
  else if (sspreadsheet->object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_EVALUATED) {
    if (used_component_type == GEO_COMPONENT_TYPE_MESH && object_eval->mode == OB_MODE_EDIT) {
      Mesh *mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(object_eval, false);
      if (mesh == nullptr) {
        return geometry_set;
      }
      BKE_mesh_wrapper_ensure_mdata(mesh);
      MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
      mesh_component.replace(mesh, GeometryOwnershipType::ReadOnly);
      mesh_component.copy_vertex_group_names_from_object(*object_eval);
    }
    else {
      if (BLI_listbase_count(&sspreadsheet->context_path) == 1) {
        /* Use final evaluated object. */
        if (object_eval->runtime.geometry_set_eval != nullptr) {
          geometry_set = *object_eval->runtime.geometry_set_eval;
        }
      }
      else {
        if (object_eval->runtime.geometry_set_previews != nullptr) {
          GHash *ghash = (GHash *)object_eval->runtime.geometry_set_previews;
          const uint64_t key = ED_spreadsheet_context_path_hash(sspreadsheet);
          GeometrySet *geometry_set_preview = (GeometrySet *)BLI_ghash_lookup_default(
              ghash, POINTER_FROM_UINT(key), nullptr);
          if (geometry_set_preview != nullptr) {
            geometry_set = *geometry_set_preview;
          }
        }
      }
    }
  }
  return geometry_set;
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

std::unique_ptr<DataSource> data_source_from_geometry(const bContext *C, Object *object_eval)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  const AttributeDomain domain = (AttributeDomain)sspreadsheet->attribute_domain;
  const GeometryComponentType component_type = get_display_component_type(C, object_eval);
  GeometrySet geometry_set = get_display_geometry_set(sspreadsheet, object_eval, component_type);

  if (!geometry_set.has(component_type)) {
    return {};
  }

  if (component_type == GEO_COMPONENT_TYPE_INSTANCES) {
    return std::make_unique<InstancesDataSource>(geometry_set);
  }
  return std::make_unique<GeometryDataSource>(object_eval, geometry_set, component_type, domain);
}

}  // namespace blender::ed::spreadsheet
