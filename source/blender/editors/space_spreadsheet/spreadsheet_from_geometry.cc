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

#include <iomanip>
#include <sstream>

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLF_api.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "DEG_depsgraph_query.h"

#include "bmesh.h"

#include "spreadsheet_from_geometry.hh"

namespace blender::ed::spreadsheet {

using blender::bke::ReadAttribute;
using blender::bke::ReadAttributePtr;

class AttributeColumn {
 public:
  std::string name;
  int width;

  AttributeColumn(std::string column_name) : name(std::move(column_name))
  {
    /* Compute the column width based on its name. */
    const int fontid = UI_style_get()->widget.uifont_id;
    const int header_name_padding = UI_UNIT_X;
    const int minimum_column_width = 3 * UI_UNIT_X;
    /* Use a consistent font size for the width calculation. */
    BLF_size(fontid, 11 * U.pixelsize, U.dpi);
    const int text_width = BLF_width(fontid, name.data(), name.size());
    width = std::max(text_width + header_name_padding, minimum_column_width);
  }

  virtual ~AttributeColumn() = default;
  virtual void draw(const int index, const CellDrawParams &params) const = 0;
};

class GeometryAttributeSpreadsheetDrawer : public SpreadsheetDrawer {
 private:
  /* Contains resources that are used during drawing. They will be freed automatically. */
  std::unique_ptr<ResourceCollector> resources_;
  /* Information about how to draw the individual columns. */
  Vector<std::unique_ptr<AttributeColumn>> columns_;
  /* This is used to filter the selected rows. The referenced data lives at least as long as the
   * resource collector above. */
  Span<int64_t> visible_rows_;

 public:
  GeometryAttributeSpreadsheetDrawer(std::unique_ptr<ResourceCollector> resources,
                                     Vector<std::unique_ptr<AttributeColumn>> columns,
                                     Span<int64_t> visible_rows,
                                     const int domain_size)
      : resources_(std::move(resources)), columns_(std::move(columns)), visible_rows_(visible_rows)
  {
    tot_columns = columns_.size();
    tot_rows = visible_rows.size();

    /* Compute index column width based on number of digits. */
    const int fontid = UI_style_get()->widget.uifont_id;
    left_column_width = std::to_string(std::max(domain_size - 1, 0)).size() *
                            BLF_width(fontid, "0", 1) +
                        UI_UNIT_X * 0.75;
  }

  void draw_top_row_cell(int column_index, const CellDrawParams &params) const final
  {
    uiBut *but = uiDefIconTextBut(params.block,
                                  UI_BTYPE_LABEL,
                                  0,
                                  ICON_NONE,
                                  columns_[column_index]->name.c_str(),
                                  params.xmin,
                                  params.ymin,
                                  params.width,
                                  params.height,
                                  nullptr,
                                  0,
                                  0,
                                  0,
                                  0,
                                  nullptr);
    /* Center-align column headers. */
    UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
    UI_but_drawflag_disable(but, UI_BUT_TEXT_RIGHT);
  }

  void draw_left_column_cell(int row_index, const CellDrawParams &params) const final
  {
    const int real_index = visible_rows_[row_index];
    std::string index_str = std::to_string(real_index);
    uiBut *but = uiDefIconTextBut(params.block,
                                  UI_BTYPE_LABEL,
                                  0,
                                  ICON_NONE,
                                  index_str.c_str(),
                                  params.xmin,
                                  params.ymin,
                                  params.width,
                                  params.height,
                                  nullptr,
                                  0,
                                  0,
                                  0,
                                  0,
                                  nullptr);
    /* Right-align indices. */
    UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
    UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  }

  void draw_content_cell(int row_index, int column_index, const CellDrawParams &params) const final
  {
    const int real_index = visible_rows_[row_index];
    columns_[column_index]->draw(real_index, params);
  }

  int column_width(int column_index) const final
  {
    return columns_[column_index]->width;
  }
};

/* Utility to make writing column drawing code more concise. */
template<typename DrawF> class CustomAttributeColumn : public AttributeColumn {
 private:
  DrawF draw_;

 public:
  CustomAttributeColumn(std::string attribute_name, DrawF draw)
      : AttributeColumn(std::move(attribute_name)), draw_(std::move(draw))
  {
  }

  void draw(const int index, const CellDrawParams &params) const final
  {
    draw_(index, params);
  }
};

template<typename DrawF>
std::unique_ptr<CustomAttributeColumn<DrawF>> create_attribute_column(std::string attribute_name,
                                                                      DrawF draw)
{
  return std::make_unique<CustomAttributeColumn<DrawF>>(std::move(attribute_name),
                                                        std::move(draw));
}

static Vector<std::string> get_sorted_attribute_names_to_display(
    const GeometryComponent &component, const AttributeDomain domain)
{
  Vector<std::string> attribute_names;
  component.attribute_foreach(
      [&](const StringRef attribute_name, const AttributeMetaData &meta_data) {
        if (meta_data.domain == domain) {
          attribute_names.append(attribute_name);
        }
        return true;
      });
  std::sort(attribute_names.begin(),
            attribute_names.end(),
            [](const std::string &a, const std::string &b) {
              return BLI_strcasecmp_natural(a.c_str(), b.c_str()) < 0;
            });
  return attribute_names;
}

static void draw_float_in_cell(const CellDrawParams &params, const float value)
{
  std::stringstream ss;
  ss << std::fixed << std::setprecision(3) << value;
  const std::string value_str = ss.str();
  uiDefIconTextBut(params.block,
                   UI_BTYPE_LABEL,
                   0,
                   ICON_NONE,
                   value_str.c_str(),
                   params.xmin,
                   params.ymin,
                   params.width,
                   params.height,
                   nullptr,
                   0,
                   0,
                   0,
                   0,
                   nullptr);
}

static void draw_int_in_cell(const CellDrawParams &params, const int value)
{
  const std::string value_str = std::to_string(value);
  uiDefIconTextBut(params.block,
                   UI_BTYPE_LABEL,
                   0,
                   ICON_NONE,
                   value_str.c_str(),
                   params.xmin,
                   params.ymin,
                   params.width,
                   params.height,
                   nullptr,
                   0,
                   0,
                   0,
                   0,
                   nullptr);
}

static void draw_bool_in_cell(const CellDrawParams &params, const bool value)
{
  const int icon = value ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
  uiDefIconTextBut(params.block,
                   UI_BTYPE_LABEL,
                   0,
                   icon,
                   "",
                   params.xmin,
                   params.ymin,
                   params.width,
                   params.height,
                   nullptr,
                   0,
                   0,
                   0,
                   0,
                   nullptr);
}

static void add_columns_for_attribute(const ReadAttribute *attribute,
                                      const StringRefNull attribute_name,
                                      Vector<std::unique_ptr<AttributeColumn>> &columns)
{
  const CustomDataType data_type = attribute->custom_data_type();
  switch (data_type) {
    case CD_PROP_FLOAT: {
      columns.append(create_attribute_column(attribute_name,
                                             [attribute](int index, const CellDrawParams &params) {
                                               float value;
                                               attribute->get(index, &value);
                                               draw_float_in_cell(params, value);
                                             }));
      break;
    }
    case CD_PROP_FLOAT2: {
      static std::array<char, 2> axis_char = {'X', 'Y'};
      for (const int i : {0, 1}) {
        std::string name = attribute_name + " " + axis_char[i];
        columns.append(
            create_attribute_column(name, [attribute, i](int index, const CellDrawParams &params) {
              float2 value;
              attribute->get(index, &value);
              draw_float_in_cell(params, value[i]);
            }));
      }
      break;
    }
    case CD_PROP_FLOAT3: {
      static std::array<char, 3> axis_char = {'X', 'Y', 'Z'};
      for (const int i : {0, 1, 2}) {
        std::string name = attribute_name + " " + axis_char[i];
        columns.append(
            create_attribute_column(name, [attribute, i](int index, const CellDrawParams &params) {
              float3 value;
              attribute->get(index, &value);
              draw_float_in_cell(params, value[i]);
            }));
      }
      break;
    }
    case CD_PROP_COLOR: {
      static std::array<char, 4> axis_char = {'R', 'G', 'B', 'A'};
      for (const int i : {0, 1, 2, 3}) {
        std::string name = attribute_name + " " + axis_char[i];
        columns.append(
            create_attribute_column(name, [attribute, i](int index, const CellDrawParams &params) {
              Color4f value;
              attribute->get(index, &value);
              draw_float_in_cell(params, value[i]);
            }));
      }
      break;
    }
    case CD_PROP_INT32: {
      columns.append(create_attribute_column(attribute_name,
                                             [attribute](int index, const CellDrawParams &params) {
                                               int value;
                                               attribute->get(index, &value);
                                               draw_int_in_cell(params, value);
                                             }));
      break;
    }
    case CD_PROP_BOOL: {
      columns.append(create_attribute_column(attribute_name,
                                             [attribute](int index, const CellDrawParams &params) {
                                               bool value;
                                               attribute->get(index, &value);
                                               draw_bool_in_cell(params, value);
                                             }));
      break;
    }
    default:
      break;
  }
}

static GeometrySet get_display_geometry_set(Object *object_eval,
                                            const GeometryComponentType used_component_type)
{
  GeometrySet geometry_set;
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
    if (object_eval->runtime.geometry_set_eval != nullptr) {
      /* This does not copy the geometry data itself. */
      geometry_set = *object_eval->runtime.geometry_set_eval;
    }
  }
  return geometry_set;
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

static void get_selected_polygon_indices(const Mesh &mesh,
                                         const IsVertexSelectedFn is_vertex_selected_fn,
                                         Vector<int64_t> &r_polygon_indices)
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
      r_polygon_indices.append(poly_index);
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
    case ATTR_DOMAIN_POLYGON:
      return get_selected_polygon_indices(mesh, is_vertex_selected_fn, r_indices);
    case ATTR_DOMAIN_CORNER:
      return get_selected_corner_indices(mesh, is_vertex_selected_fn, r_indices);
    case ATTR_DOMAIN_EDGE:
      return get_selected_edge_indices(mesh, is_vertex_selected_fn, r_indices);
    default:
      return;
  }
}

static Span<int64_t> filter_mesh_elements_by_selection(const bContext *C,
                                                       Object *object_eval,
                                                       const MeshComponent *component,
                                                       const AttributeDomain domain,
                                                       ResourceCollector &resources)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  const bool show_only_selected = sspreadsheet->filter_flag & SPREADSHEET_FILTER_SELECTED_ONLY;
  if (object_eval->mode == OB_MODE_EDIT && show_only_selected) {
    Object *object_orig = DEG_get_original_object(object_eval);
    Vector<int64_t> &visible_rows = resources.construct<Vector<int64_t>>("visible rows");
    const Mesh *mesh_eval = component->get_for_read();
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
      get_selected_indices_on_domain(*mesh_eval, domain, is_vertex_selected, visible_rows);
    }
    else if (mesh_eval->totvert == bm->totvert) {
      /* Use a simple heuristic to match original vertices to evaluated ones. */
      auto is_vertex_selected = [&](int vertex_index) -> bool {
        BMVert *vert = bm->vtable[vertex_index];
        return BM_elem_flag_test(vert, BM_ELEM_SELECT);
      };
      get_selected_indices_on_domain(*mesh_eval, domain, is_vertex_selected, visible_rows);
    }
    /* This is safe, because the vector lives in the resource collector. */
    return visible_rows.as_span();
  }
  /* No filter is used. */
  const int domain_size = component->attribute_domain_size(domain);
  return IndexRange(domain_size).as_span();
}

std::unique_ptr<SpreadsheetDrawer> spreadsheet_drawer_from_geometry_attributes(const bContext *C,
                                                                               Object *object_eval)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  const AttributeDomain domain = (AttributeDomain)sspreadsheet->attribute_domain;
  const GeometryComponentType component_type = (GeometryComponentType)
                                                   sspreadsheet->geometry_component_type;

  /* Create a resource collector that owns stuff that needs to live until drawing is done. */
  std::unique_ptr<ResourceCollector> resources = std::make_unique<ResourceCollector>();
  GeometrySet &geometry_set = resources->add_value(
      get_display_geometry_set(object_eval, component_type), "geometry set");

  const GeometryComponent *component = geometry_set.get_component_for_read(component_type);
  if (component == nullptr) {
    return {};
  }
  if (!component->attribute_domain_supported(domain)) {
    return {};
  }

  Vector<std::string> attribute_names = get_sorted_attribute_names_to_display(*component, domain);

  Vector<std::unique_ptr<AttributeColumn>> columns;
  for (StringRefNull attribute_name : attribute_names) {
    ReadAttributePtr attribute_ptr = component->attribute_try_get_for_read(attribute_name);
    ReadAttribute &attribute = *attribute_ptr;
    resources->add(std::move(attribute_ptr), "attribute");
    add_columns_for_attribute(&attribute, attribute_name, columns);
  }

  /* The filter below only works for mesh vertices currently. */
  Span<int64_t> visible_rows;
  if (component_type == GEO_COMPONENT_TYPE_MESH) {
    visible_rows = filter_mesh_elements_by_selection(
        C, object_eval, static_cast<const MeshComponent *>(component), domain, *resources);
  }
  else {
    visible_rows = IndexRange(component->attribute_domain_size(domain)).as_span();
  }

  const int domain_size = component->attribute_domain_size(domain);
  return std::make_unique<GeometryAttributeSpreadsheetDrawer>(
      std::move(resources), std::move(columns), visible_rows, domain_size);
}

}  // namespace blender::ed::spreadsheet
