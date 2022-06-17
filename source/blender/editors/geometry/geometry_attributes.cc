/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

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
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_object_deform.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_geometry.h"
#include "ED_object.h"

#include "geometry_intern.hh"

namespace blender::ed::geometry {

/*********************** Attribute Operators ************************/

static bool geometry_attributes_poll(bContext *C)
{
  const Object *ob = ED_object_context(C);
  const Main *bmain = CTX_data_main(C);
  const ID *data = (ob) ? static_cast<ID *>(ob->data) : nullptr;
  return (ob && BKE_id_is_editable(bmain, &ob->id) && data && BKE_id_is_editable(bmain, data)) &&
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
  eCustomDataType type = (eCustomDataType)RNA_enum_get(op->ptr, "data_type");
  eAttrDomain domain = (eAttrDomain)RNA_enum_get(op->ptr, "domain");
  CustomDataLayer *layer = BKE_id_attribute_new(id, name, type, domain, op->reports);

  if (layer == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BKE_id_attributes_active_set(id, layer);

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  return OPERATOR_FINISHED;
}

static void next_color_attribute(ID *id, CustomDataLayer *layer, bool is_render)
{
  int index = BKE_id_attribute_to_index(id, layer, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);

  index++;

  layer = BKE_id_attribute_from_index(id, index, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);

  if (!layer) {
    index = 0;
    layer = BKE_id_attribute_from_index(id, index, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
  }

  if (layer) {
    if (is_render) {
      BKE_id_attributes_active_color_set(id, layer);
    }
    else {
      BKE_id_attributes_render_color_set(id, layer);
    }
  }
}

static void next_color_attributes(ID *id, CustomDataLayer *layer)
{
  next_color_attribute(id, layer, false); /* active */
  next_color_attribute(id, layer, true);  /* render */
}

void GEOMETRY_OT_attribute_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Attribute";
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

  next_color_attributes(id, layer);

  if (!BKE_id_attribute_remove(id, layer->name, op->reports)) {
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
  ot->name = "Remove Attribute";
  ot->description = "Remove attribute from geometry";
  ot->idname = "GEOMETRY_OT_attribute_remove";

  /* api callbacks */
  ot->exec = geometry_attribute_remove_exec;
  ot->poll = geometry_attributes_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int geometry_color_attribute_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  ID *id = static_cast<ID *>(ob->data);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "name", name);
  eCustomDataType type = (eCustomDataType)RNA_enum_get(op->ptr, "data_type");
  eAttrDomain domain = (eAttrDomain)RNA_enum_get(op->ptr, "domain");
  CustomDataLayer *layer = BKE_id_attribute_new(id, name, type, domain, op->reports);

  float color[4];
  RNA_float_get_array(op->ptr, "color", color);

  if (layer == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BKE_id_attributes_active_color_set(id, layer);

  if (!BKE_id_attributes_render_color_get(id)) {
    BKE_id_attributes_render_color_set(id, layer);
  }

  BKE_object_attributes_active_color_fill(ob, color, false);

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  return OPERATOR_FINISHED;
}

enum class ConvertAttributeMode {
  Generic,
  UVMap,
  VertexGroup,
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
  if (CTX_data_edit_object(C) != nullptr) {
    CTX_wm_operator_poll_msg_set(C, "Operation is not allowed in edit mode");
    return false;
  }
  if (BKE_id_attributes_active_get(data) == nullptr) {
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
      const eAttrDomain dst_domain = static_cast<eAttrDomain>(RNA_enum_get(op->ptr, "domain"));
      const eCustomDataType dst_type = static_cast<eCustomDataType>(
          RNA_enum_get(op->ptr, "data_type"));

      if (ELEM(dst_type, CD_PROP_STRING)) {
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

static void geometry_color_attribute_add_ui(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiItemR(layout, op->ptr, "name", 0, nullptr, ICON_NONE);
  uiItemR(layout, op->ptr, "domain", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, op->ptr, "data_type", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, op->ptr, "color", 0, nullptr, ICON_NONE);
}

void GEOMETRY_OT_color_attribute_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Color Attribute";
  ot->description = "Add color attribute to geometry";
  ot->idname = "GEOMETRY_OT_color_attribute_add";

  /* api callbacks */
  ot->poll = geometry_attributes_poll;
  ot->exec = geometry_color_attribute_add_exec;
  ot->invoke = WM_operator_props_popup_confirm;
  ot->ui = geometry_color_attribute_add_ui;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_string(
      ot->srna, "name", "Color", MAX_NAME, "Name", "Name of new color attribute");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "domain",
                      rna_enum_color_attribute_domain_items,
                      ATTR_DOMAIN_POINT,
                      "Domain",
                      "Type of element that attribute is stored on");

  prop = RNA_def_enum(ot->srna,
                      "data_type",
                      rna_enum_color_attribute_type_items,
                      CD_PROP_COLOR,
                      "Data Type",
                      "Type of data stored in attribute");

  static float default_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  prop = RNA_def_float_color(
      ot->srna, "color", 4, nullptr, 0.0f, FLT_MAX, "Color", "Default fill color", 0.0f, 1.0f);
  RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
  RNA_def_property_float_array_default(prop, default_color);
}

static int geometry_color_attribute_set_render_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  ID *id = static_cast<ID *>(ob->data);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "name", name);

  CustomDataLayer *layer = BKE_id_attribute_find(id, name, CD_PROP_COLOR, ATTR_DOMAIN_POINT);
  layer = !layer ? BKE_id_attribute_find(id, name, CD_PROP_BYTE_COLOR, ATTR_DOMAIN_POINT) : layer;
  layer = !layer ? BKE_id_attribute_find(id, name, CD_PROP_COLOR, ATTR_DOMAIN_CORNER) : layer;
  layer = !layer ? BKE_id_attribute_find(id, name, CD_PROP_BYTE_COLOR, ATTR_DOMAIN_CORNER) : layer;

  if (layer) {
    BKE_id_attributes_render_color_set(id, layer);

    DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void GEOMETRY_OT_color_attribute_render_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Render Color";
  ot->description = "Set default color attribute used for rendering";
  ot->idname = "GEOMETRY_OT_color_attribute_render_set";

  /* api callbacks */
  ot->poll = geometry_attributes_poll;
  ot->exec = geometry_color_attribute_set_render_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna, "name", "Color", MAX_NAME, "Name", "Name of color attribute");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int geometry_color_attribute_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  ID *id = static_cast<ID *>(ob->data);
  CustomDataLayer *layer = BKE_id_attributes_active_color_get(id);

  if (layer == nullptr) {
    return OPERATOR_CANCELLED;
  }

  next_color_attributes(id, layer);

  if (!BKE_id_attribute_remove(id, layer->name, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (GS(id->name) == ID_ME) {
    Mesh *me = static_cast<Mesh *>(ob->data);
    BKE_mesh_update_customdata_pointers(me, true);
  }

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  return OPERATOR_FINISHED;
}

static bool geometry_color_attributes_remove_poll(bContext *C)
{
  if (!geometry_attributes_poll(C)) {
    return false;
  }

  Object *ob = ED_object_context(C);
  ID *data = ob ? static_cast<ID *>(ob->data) : nullptr;

  if (BKE_id_attributes_active_color_get(data) != nullptr) {
    return true;
  }

  return false;
}

void GEOMETRY_OT_color_attribute_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Color Attribute";
  ot->description = "Remove color attribute from geometry";
  ot->idname = "GEOMETRY_OT_color_attribute_remove";

  /* api callbacks */
  ot->exec = geometry_color_attribute_remove_exec;
  ot->poll = geometry_color_attributes_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int geometry_color_attribute_duplicate_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  ID *id = static_cast<ID *>(ob->data);
  const CustomDataLayer *layer = BKE_id_attributes_active_color_get(id);

  if (layer == nullptr) {
    return OPERATOR_CANCELLED;
  }

  CustomDataLayer *new_layer = BKE_id_attribute_duplicate(id, layer->name, op->reports);
  if (new_layer == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BKE_id_attributes_active_color_set(id, new_layer);

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  return OPERATOR_FINISHED;
}

static bool geometry_color_attributes_duplicate_poll(bContext *C)
{
  if (!geometry_attributes_poll(C)) {
    return false;
  }
  if (CTX_data_edit_object(C) != nullptr) {
    CTX_wm_operator_poll_msg_set(C, "Operation is not allowed in edit mode");
    return false;
  }

  Object *ob = ED_object_context(C);
  ID *data = ob ? static_cast<ID *>(ob->data) : nullptr;

  if (BKE_id_attributes_active_color_get(data) != nullptr) {
    return true;
  }

  return false;
}

void GEOMETRY_OT_color_attribute_duplicate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Color Attribute";
  ot->description = "Duplicate color attribute";
  ot->idname = "GEOMETRY_OT_color_attribute_duplicate";

  /* api callbacks */
  ot->exec = geometry_color_attribute_duplicate_exec;
  ot->poll = geometry_color_attributes_duplicate_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void geometry_attribute_convert_ui(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

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

using blender::CPPType;
using blender::GVArray;

bool ED_geometry_attribute_convert(Mesh *mesh,
                                   const char *layer_name,
                                   eCustomDataType old_type,
                                   eAttrDomain old_domain,
                                   eCustomDataType new_type,
                                   eAttrDomain new_domain)
{
  CustomDataLayer *layer = BKE_id_attribute_find(&mesh->id, layer_name, old_type, old_domain);
  const std::string name = layer->name;

  if (!layer) {
    return false;
  }

  MeshComponent mesh_component;
  mesh_component.replace(mesh, GeometryOwnershipType::Editable);
  GVArray src_varray = mesh_component.attribute_get_for_read(name, new_domain, new_type);

  const CPPType &cpp_type = src_varray.type();
  void *new_data = MEM_malloc_arrayN(src_varray.size(), cpp_type.size(), __func__);
  src_varray.materialize_to_uninitialized(new_data);
  mesh_component.attribute_try_delete(name);
  mesh_component.attribute_try_create(name, new_domain, new_type, AttributeInitMove(new_data));

  int *active_index = BKE_id_attributes_active_index_p(&mesh->id);
  if (*active_index > 0) {
    *active_index -= 1;
  }

  return true;
}
