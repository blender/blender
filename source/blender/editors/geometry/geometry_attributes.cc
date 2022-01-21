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
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edgeometry
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_geometry_set.hh"
#include "BKE_object_deform.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_object.h"

#include "geometry_intern.hh"

namespace blender::ed::geometry {

using fn::CPPType;
using fn::GArray;
using fn::GVArray;

/*********************** Attribute Operators ************************/

static bool geometry_attributes_poll(bContext *C)
{
  Object *ob = ED_object_context(C);
  ID *data = (ob) ? static_cast<ID *>(ob->data) : nullptr;
  return (ob && !ID_IS_LINKED(ob) && data && !ID_IS_LINKED(data)) &&
         BKE_id_attributes_supported(data);
}

static bool geometry_attributes_remove_poll(bContext *C)
{
  if (!geometry_attributes_poll(C)) {
    return false;
  }

  Object *ob = ED_object_context(C);
  ID *data = (ob) ? static_cast<ID *>(ob->data) : nullptr;
  if (BKE_id_attributes_active_get(data) != nullptr) {
    return true;
  }

  return false;
}

static const EnumPropertyItem *geometry_attribute_domain_itemf(bContext *C,
                                                               PointerRNA *UNUSED(ptr),
                                                               PropertyRNA *UNUSED(prop),
                                                               bool *r_free)
{
  if (C == nullptr) {
    return DummyRNA_NULL_items;
  }

  Object *ob = ED_object_context(C);
  if (ob == nullptr) {
    return DummyRNA_NULL_items;
  }

  return rna_enum_attribute_domain_itemf(static_cast<ID *>(ob->data), false, r_free);
}

static int geometry_attribute_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  ID *id = static_cast<ID *>(ob->data);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "name", name);
  CustomDataType type = (CustomDataType)RNA_enum_get(op->ptr, "data_type");
  AttributeDomain domain = (AttributeDomain)RNA_enum_get(op->ptr, "domain");
  CustomDataLayer *layer = BKE_id_attribute_new(id, name, type, domain, op->reports);

  if (layer == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BKE_id_attributes_active_set(id, layer);

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  return OPERATOR_FINISHED;
}

void GEOMETRY_OT_attribute_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Geometry Attribute";
  ot->description = "Add attribute to geometry";
  ot->idname = "GEOMETRY_OT_attribute_add";

  /* api callbacks */
  ot->poll = geometry_attributes_poll;
  ot->exec = geometry_attribute_add_exec;
  ot->invoke = WM_operator_props_popup_confirm;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna, "name", "Attribute", MAX_NAME, "Name", "Name of new attribute");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "domain",
                      rna_enum_attribute_domain_items,
                      ATTR_DOMAIN_POINT,
                      "Domain",
                      "Type of element that attribute is stored on");
  RNA_def_enum_funcs(prop, geometry_attribute_domain_itemf);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "data_type",
                      rna_enum_attribute_type_items,
                      CD_PROP_FLOAT,
                      "Data Type",
                      "Type of data stored in attribute");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int geometry_attribute_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  ID *id = static_cast<ID *>(ob->data);
  CustomDataLayer *layer = BKE_id_attributes_active_get(id);

  if (layer == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (!BKE_id_attribute_remove(id, layer, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  int *active_index = BKE_id_attributes_active_index_p(id);
  if (*active_index > 0) {
    *active_index -= 1;
  }

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  return OPERATOR_FINISHED;
}

void GEOMETRY_OT_attribute_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Geometry Attribute";
  ot->description = "Remove attribute from geometry";
  ot->idname = "GEOMETRY_OT_attribute_remove";

  /* api callbacks */
  ot->exec = geometry_attribute_remove_exec;
  ot->poll = geometry_attributes_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

enum class ConvertAttributeMode {
  Generic,
  UVMap,
  VertexGroup,
  VertexColor,
};

static bool geometry_attribute_convert_poll(bContext *C)
{
  if (!geometry_attributes_poll(C)) {
    return false;
  }

  Object *ob = ED_object_context(C);
  ID *data = static_cast<ID *>(ob->data);
  if (GS(data->name) != ID_ME) {
    return false;
  }
  CustomDataLayer *layer = BKE_id_attributes_active_get(data);
  if (layer == nullptr) {
    return false;
  }
  return true;
}

static int geometry_attribute_convert_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  ID *ob_data = static_cast<ID *>(ob->data);
  CustomDataLayer *layer = BKE_id_attributes_active_get(ob_data);
  const std::string name = layer->name;

  const ConvertAttributeMode mode = static_cast<ConvertAttributeMode>(
      RNA_enum_get(op->ptr, "mode"));

  Mesh *mesh = reinterpret_cast<Mesh *>(ob_data);
  MeshComponent mesh_component;
  mesh_component.replace(mesh, GeometryOwnershipType::Editable);

  /* General conversion steps are always the same:
   * 1. Convert old data to right domain and data type.
   * 2. Copy the data into a new array so that it does not depend on the old attribute anymore.
   * 3. Delete the old attribute.
   * 4. Create a new attribute based on the previously copied data. */
  switch (mode) {
    case ConvertAttributeMode::Generic: {
      const AttributeDomain dst_domain = static_cast<AttributeDomain>(
          RNA_enum_get(op->ptr, "domain"));
      const CustomDataType dst_type = static_cast<CustomDataType>(
          RNA_enum_get(op->ptr, "data_type"));

      if (ELEM(dst_type, CD_PROP_STRING, CD_MLOOPCOL)) {
        BKE_report(op->reports, RPT_ERROR, "Cannot convert to the selected type");
        return OPERATOR_CANCELLED;
      }

      GVArray src_varray = mesh_component.attribute_get_for_read(name, dst_domain, dst_type);
      const CPPType &cpp_type = src_varray.type();
      void *new_data = MEM_malloc_arrayN(src_varray.size(), cpp_type.size(), __func__);
      src_varray.materialize_to_uninitialized(new_data);
      mesh_component.attribute_try_delete(name);
      mesh_component.attribute_try_create(name, dst_domain, dst_type, AttributeInitMove(new_data));
      break;
    }
    case ConvertAttributeMode::UVMap: {
      MLoopUV *dst_uvs = static_cast<MLoopUV *>(
          MEM_calloc_arrayN(mesh->totloop, sizeof(MLoopUV), __func__));
      VArray<float2> src_varray = mesh_component.attribute_get_for_read<float2>(
          name, ATTR_DOMAIN_CORNER, {0.0f, 0.0f});
      for (const int i : IndexRange(mesh->totloop)) {
        copy_v2_v2(dst_uvs[i].uv, src_varray[i]);
      }
      mesh_component.attribute_try_delete(name);
      CustomData_add_layer_named(
          &mesh->ldata, CD_MLOOPUV, CD_ASSIGN, dst_uvs, mesh->totloop, name.c_str());
      break;
    }
    case ConvertAttributeMode::VertexColor: {
      MLoopCol *dst_colors = static_cast<MLoopCol *>(
          MEM_calloc_arrayN(mesh->totloop, sizeof(MLoopCol), __func__));
      VArray<ColorGeometry4f> src_varray = mesh_component.attribute_get_for_read<ColorGeometry4f>(
          name, ATTR_DOMAIN_CORNER, ColorGeometry4f{0.0f, 0.0f, 0.0f, 1.0f});
      for (const int i : IndexRange(mesh->totloop)) {
        ColorGeometry4b encoded_color = src_varray[i].encode();
        copy_v4_v4_uchar(&dst_colors[i].r, &encoded_color.r);
      }
      mesh_component.attribute_try_delete(name);
      CustomData_add_layer_named(
          &mesh->ldata, CD_MLOOPCOL, CD_ASSIGN, dst_colors, mesh->totloop, name.c_str());
      break;
    }
    case ConvertAttributeMode::VertexGroup: {
      Array<float> src_weights(mesh->totvert);
      VArray<float> src_varray = mesh_component.attribute_get_for_read<float>(
          name, ATTR_DOMAIN_POINT, 0.0f);
      src_varray.materialize(src_weights);
      mesh_component.attribute_try_delete(name);

      bDeformGroup *defgroup = BKE_object_defgroup_new(ob, name.c_str());
      const int defgroup_index = BLI_findindex(BKE_id_defgroup_list_get(&mesh->id), defgroup);
      MDeformVert *dverts = BKE_object_defgroup_data_create(&mesh->id);
      for (const int i : IndexRange(mesh->totvert)) {
        const float weight = src_weights[i];
        if (weight > 0.0f) {
          BKE_defvert_add_index_notest(dverts + i, defgroup_index, weight);
        }
      }
      break;
    }
  }

  int *active_index = BKE_id_attributes_active_index_p(&mesh->id);
  if (*active_index > 0) {
    *active_index -= 1;
  }

  DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, &mesh->id);

  return OPERATOR_FINISHED;
}

static void geometry_attribute_convert_ui(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;

  uiItemR(layout, op->ptr, "mode", 0, nullptr, ICON_NONE);

  const ConvertAttributeMode mode = static_cast<ConvertAttributeMode>(
      RNA_enum_get(op->ptr, "mode"));

  if (mode == ConvertAttributeMode::Generic) {
    uiItemR(layout, op->ptr, "domain", 0, nullptr, ICON_NONE);
    uiItemR(layout, op->ptr, "data_type", 0, nullptr, ICON_NONE);
  }
}

static int geometry_attribute_convert_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent *UNUSED(event))
{
  return WM_operator_props_dialog_popup(C, op, 300);
}

void GEOMETRY_OT_attribute_convert(wmOperatorType *ot)
{
  ot->name = "Convert Attribute";
  ot->description = "Change how the attribute is stored";
  ot->idname = "GEOMETRY_OT_attribute_convert";

  ot->invoke = geometry_attribute_convert_invoke;
  ot->exec = geometry_attribute_convert_exec;
  ot->poll = geometry_attribute_convert_poll;
  ot->ui = geometry_attribute_convert_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  static EnumPropertyItem mode_items[] = {
      {int(ConvertAttributeMode::Generic), "GENERIC", 0, "Generic", ""},
      {int(ConvertAttributeMode::UVMap), "UV_MAP", 0, "UV Map", ""},
      {int(ConvertAttributeMode::VertexGroup), "VERTEX_GROUP", 0, "Vertex Group", ""},
      {int(ConvertAttributeMode::VertexColor), "VERTEX_COLOR", 0, "Vertex Color", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_enum(
      ot->srna, "mode", mode_items, static_cast<int>(ConvertAttributeMode::Generic), "Mode", "");

  prop = RNA_def_enum(ot->srna,
                      "domain",
                      rna_enum_attribute_domain_items,
                      ATTR_DOMAIN_POINT,
                      "Domain",
                      "Which geometry element to move the attribute to");
  RNA_def_enum_funcs(prop, geometry_attribute_domain_itemf);

  RNA_def_enum(
      ot->srna, "data_type", rna_enum_attribute_type_items, CD_PROP_FLOAT, "Data Type", "");
}

}  // namespace blender::ed::geometry
