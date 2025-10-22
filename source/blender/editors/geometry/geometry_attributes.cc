/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgeometry
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_color.hh"
#include "BLI_listbase.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_geometry_set.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_object_deform.h"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "ED_geometry.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_sculpt.hh"

#include "geometry_intern.hh"

namespace blender::ed::geometry {

StringRefNull rna_property_name_for_type(const bke::AttrType type)
{
  switch (type) {
    case bke::AttrType::Float:
      return "value_float";
    case bke::AttrType::Float2:
      return "value_float_vector_2d";
    case bke::AttrType::Float3:
      return "value_float_vector_3d";
    case bke::AttrType::ColorByte:
    case bke::AttrType::ColorFloat:
      return "value_color";
    case bke::AttrType::Bool:
      return "value_bool";
    case bke::AttrType::Int8:
    case bke::AttrType::Int32:
      return "value_int";
    case bke::AttrType::Int16_2D:
    case bke::AttrType::Int32_2D:
      return "value_int_vector_2d";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

PropertyRNA *rna_property_for_type(PointerRNA &ptr, const bke::AttrType type)
{
  return RNA_struct_find_property(&ptr, rna_property_name_for_type(type).c_str());
}

void register_rna_properties_for_attribute_types(StructRNA &srna)
{
  static blender::float4 color_default(1);

  RNA_def_float(&srna, "value_float", 0.0f, -FLT_MAX, FLT_MAX, "Value", "", -FLT_MAX, FLT_MAX);
  RNA_def_float_array(&srna,
                      "value_float_vector_2d",
                      2,
                      nullptr,
                      -FLT_MAX,
                      FLT_MAX,
                      "Value",
                      "",
                      -FLT_MAX,
                      FLT_MAX);
  RNA_def_float_array(&srna,
                      "value_float_vector_3d",
                      3,
                      nullptr,
                      -FLT_MAX,
                      FLT_MAX,
                      "Value",
                      "",
                      -FLT_MAX,
                      FLT_MAX);
  RNA_def_int(&srna, "value_int", 0, INT_MIN, INT_MAX, "Value", "", INT_MIN, INT_MAX);
  RNA_def_int_array(
      &srna, "value_int_vector_2d", 2, nullptr, INT_MIN, INT_MAX, "Value", "", INT_MIN, INT_MAX);
  RNA_def_float_color(
      &srna, "value_color", 4, color_default, -FLT_MAX, FLT_MAX, "Value", "", 0.0f, 1.0f);
  RNA_def_boolean(&srna, "value_bool", false, "Value", "");
}

GPointer rna_property_for_attribute_type_retrieve_value(PointerRNA &ptr,
                                                        const bke::AttrType type,
                                                        void *buffer)
{
  const StringRefNull prop_name = rna_property_name_for_type(type);
  switch (type) {
    case bke::AttrType::Float:
      *static_cast<float *>(buffer) = RNA_float_get(&ptr, prop_name.c_str());
      break;
    case bke::AttrType::Float2:
      RNA_float_get_array(&ptr, prop_name.c_str(), static_cast<float *>(buffer));
      break;
    case bke::AttrType::Float3:
      RNA_float_get_array(&ptr, prop_name.c_str(), static_cast<float *>(buffer));
      break;
    case bke::AttrType::ColorFloat:
      RNA_float_get_array(&ptr, prop_name.c_str(), static_cast<float *>(buffer));
      break;
    case bke::AttrType::ColorByte: {
      ColorGeometry4f value;
      RNA_float_get_array(&ptr, prop_name.c_str(), value);
      *static_cast<ColorGeometry4b *>(buffer) = color::encode(value);
      break;
    }
    case bke::AttrType::Bool:
      *static_cast<bool *>(buffer) = RNA_boolean_get(&ptr, prop_name.c_str());
      break;
    case bke::AttrType::Int8:
      *static_cast<int8_t *>(buffer) = RNA_int_get(&ptr, prop_name.c_str());
      break;
    case bke::AttrType::Int32:
      *static_cast<int32_t *>(buffer) = RNA_int_get(&ptr, prop_name.c_str());
      break;
    case bke::AttrType::Int16_2D: {
      int2 value;
      RNA_int_get_array(&ptr, prop_name.c_str(), value);
      *static_cast<short2 *>(buffer) = short2(value);
      break;
    }
    case bke::AttrType::Int32_2D:
      RNA_int_get_array(&ptr, prop_name.c_str(), static_cast<int *>(buffer));
      break;
    default:
      BLI_assert_unreachable();
      return {};
  }
  return GPointer(bke::attribute_type_to_cpp_type(type), buffer);
}

void rna_property_for_attribute_type_set_value(PointerRNA &ptr,
                                               PropertyRNA &prop,
                                               const GPointer value)
{
  switch (bke::cpp_type_to_attribute_type(*value.type())) {
    case bke::AttrType::Float:
      RNA_property_float_set(&ptr, &prop, *value.get<float>());
      break;
    case bke::AttrType::Float2:
      RNA_property_float_set_array(&ptr, &prop, *value.get<float2>());
      break;
    case bke::AttrType::Float3:
      RNA_property_float_set_array(&ptr, &prop, *value.get<float3>());
      break;
    case bke::AttrType::ColorByte:
      RNA_property_float_set_array(&ptr, &prop, color::decode(*value.get<ColorGeometry4b>()));
      break;
    case bke::AttrType::ColorFloat:
      RNA_property_float_set_array(&ptr, &prop, *value.get<ColorGeometry4f>());
      break;
    case bke::AttrType::Bool:
      RNA_property_boolean_set(&ptr, &prop, *value.get<bool>());
      break;
    case bke::AttrType::Int8:
      RNA_property_int_set(&ptr, &prop, *value.get<int8_t>());
      break;
    case bke::AttrType::Int32:
      RNA_property_int_set(&ptr, &prop, *value.get<int32_t>());
      break;
    case bke::AttrType::Int16_2D:
      RNA_property_int_set_array(&ptr, &prop, int2(*value.get<short2>()));
      break;
    case bke::AttrType::Int32_2D:
      RNA_property_int_set_array(&ptr, &prop, *value.get<int2>());
      break;
    default:
      BLI_assert_unreachable();
  }
}

bool attribute_set_poll(bContext &C, const ID &object_data)
{
  AttributeOwner owner = AttributeOwner::from_id(&const_cast<ID &>(object_data));
  const std::optional<StringRef> name = BKE_attributes_active_name_get(owner);
  if (!name) {
    CTX_wm_operator_poll_msg_set(&C, "No active attribute");
    return false;
  }

  if (owner.type() == AttributeOwnerType::Mesh) {
    const Mesh *mesh = owner.get_mesh();
    if (mesh->runtime->edit_mesh) {
      BMDataLayerLookup attr = BM_data_layer_lookup(*mesh->runtime->edit_mesh->bm, *name);
      if (ELEM(attr.type,
               bke::AttrType::String,
               bke::AttrType::Float4x4,
               bke::AttrType::Quaternion))
      {
        CTX_wm_operator_poll_msg_set(&C, "The active attribute has an unsupported type");
        return false;
      }
      return true;
    }
  }

  bke::AttributeAccessor attributes = *owner.get_accessor();
  std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(*name);
  if (!meta_data) {
    CTX_wm_operator_poll_msg_set(&C, "No active attribute");
    return false;
  }
  if (ELEM(meta_data->data_type,
           bke::AttrType::String,
           bke::AttrType::Float4x4,
           bke::AttrType::Quaternion))
  {
    CTX_wm_operator_poll_msg_set(&C, "The active attribute has an unsupported type");
    return false;
  }
  return true;
}

/*********************** Attribute Operators ************************/

static bool geometry_attributes_poll(bContext *C)
{
  using namespace blender::bke;
  const Object *ob = object::context_object(C);
  const Main *bmain = CTX_data_main(C);
  if (!ob || !BKE_id_is_editable(bmain, &ob->id)) {
    return false;
  }
  const ID *data = (ob) ? static_cast<const ID *>(ob->data) : nullptr;
  if (!data || !BKE_id_is_editable(bmain, data)) {
    return false;
  }
  return AttributeAccessor::from_id(*data).has_value();
}

static bool geometry_attributes_remove_poll(bContext *C)
{
  if (!geometry_attributes_poll(C)) {
    return false;
  }

  Object *ob = object::context_object(C);
  ID *data = (ob) ? static_cast<ID *>(ob->data) : nullptr;
  AttributeOwner owner = AttributeOwner::from_id(data);
  if (BKE_attributes_active_name_get(owner) != std::nullopt) {
    return true;
  }

  return false;
}

static const EnumPropertyItem *geometry_attribute_domain_itemf(bContext *C,
                                                               PointerRNA * /*ptr*/,
                                                               PropertyRNA * /*prop*/,
                                                               bool *r_free)
{
  if (C == nullptr) {
    return rna_enum_dummy_NULL_items;
  }

  Object *ob = object::context_object(C);
  if (ob == nullptr) {
    return rna_enum_dummy_NULL_items;
  }

  const AttributeOwner owner = AttributeOwner::from_id(static_cast<ID *>(ob->data));
  return rna_enum_attribute_domain_itemf(owner, false, r_free);
}

static wmOperatorStatus geometry_attribute_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = object::context_object(C);
  ID *id = static_cast<ID *>(ob->data);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "name", name);
  const eCustomDataType type = eCustomDataType(RNA_enum_get(op->ptr, "data_type"));
  const bke::AttrDomain domain = bke::AttrDomain(RNA_enum_get(op->ptr, "domain"));
  AttributeOwner owner = AttributeOwner::from_id(id);

  if (owner.type() == AttributeOwnerType::Mesh) {
    CustomDataLayer *layer = BKE_attribute_new(owner, name, type, domain, op->reports);

    if (layer == nullptr) {
      return OPERATOR_CANCELLED;
    }

    BKE_attributes_active_set(owner, layer->name);

    DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);

    return OPERATOR_FINISHED;
  }

  bke::MutableAttributeAccessor accessor = *owner.get_accessor();
  if (!accessor.domain_supported(bke::AttrDomain(domain))) {
    BKE_report(op->reports, RPT_ERROR, "Attribute domain not supported by this geometry type");
    return OPERATOR_CANCELLED;
  }
  bke::AttributeStorage &attributes = *owner.get_storage();
  const int domain_size = accessor.domain_size(bke::AttrDomain(domain));

  const CPPType &cpp_type = *bke::custom_data_type_to_cpp_type(type);
  bke::Attribute &attr = attributes.add(
      attributes.unique_name_calc(name),
      bke::AttrDomain(domain),
      *bke::custom_data_type_to_attr_type(type),
      bke::Attribute::ArrayData::from_default_value(cpp_type, domain_size));

  BKE_attributes_active_set(owner, attr.name());

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus geometry_attribute_add_invoke(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent *event)
{
  PropertyRNA *prop;
  prop = RNA_struct_find_property(op->ptr, "name");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_string_set(op->ptr, prop, DATA_("Attribute"));
  }
  /* Set a valid default domain, in case Point domain is not supported. */
  prop = RNA_struct_find_property(op->ptr, "domain");
  if (!RNA_property_is_set(op->ptr, prop)) {
    EnumPropertyItem *items;
    int totitems;
    bool free;
    RNA_property_enum_items(
        C, op->ptr, prop, const_cast<const EnumPropertyItem **>(&items), &totitems, &free);
    if (totitems > 0) {
      RNA_property_enum_set(op->ptr, prop, items[0].value);
    }
    if (free) {
      MEM_freeN(items);
    }
  }
  return WM_operator_props_popup_confirm_ex(
      C, op, event, IFACE_("Add Attribute"), CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Add"));
}

void GEOMETRY_OT_attribute_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Attribute";
  ot->description = "Add attribute to geometry";
  ot->idname = "GEOMETRY_OT_attribute_add";

  /* API callbacks. */
  ot->poll = geometry_attributes_poll;
  ot->exec = geometry_attribute_add_exec;
  ot->invoke = geometry_attribute_add_invoke;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;

  /* The default name of the new attribute can be translated if new data translation is enabled,
   * but since the user can choose it at invoke time, the translation happens in the invoke
   * callback instead of here. */
  prop = RNA_def_string(ot->srna, "name", nullptr, MAX_NAME, "Name", "Name of new attribute");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "domain",
                      rna_enum_attribute_domain_items,
                      int(bke::AttrDomain::Point),
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

static wmOperatorStatus geometry_attribute_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = object::context_object(C);
  ID *id = static_cast<ID *>(ob->data);
  AttributeOwner owner = AttributeOwner::from_id(id);
  const StringRef name = *BKE_attributes_active_name_get(owner);

  if (!BKE_attribute_remove(owner, name, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  int *active_index = BKE_attributes_active_index_p(owner);
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

  /* API callbacks. */
  ot->exec = geometry_attribute_remove_exec;
  ot->poll = geometry_attributes_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus geometry_color_attribute_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = object::context_object(C);
  ID *id = static_cast<ID *>(ob->data);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "name", name);
  eCustomDataType type = eCustomDataType(RNA_enum_get(op->ptr, "data_type"));
  bke::AttrDomain domain = bke::AttrDomain(RNA_enum_get(op->ptr, "domain"));
  AttributeOwner owner = AttributeOwner::from_id(id);
  CustomDataLayer *layer = BKE_attribute_new(owner, name, type, domain, op->reports);

  float color[4];
  RNA_float_get_array(op->ptr, "color", color);

  if (layer == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BKE_id_attributes_active_color_set(id, layer->name);

  if (!BKE_id_attributes_color_find(id, BKE_id_attributes_default_color_name(id).value_or(""))) {
    BKE_id_attributes_default_color_set(id, layer->name);
  }

  sculpt_paint::object_active_color_fill(*ob, color, false);

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus geometry_color_attribute_add_invoke(bContext *C,
                                                            wmOperator *op,
                                                            const wmEvent *event)
{
  PropertyRNA *prop;
  prop = RNA_struct_find_property(op->ptr, "name");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_string_set(op->ptr, prop, DATA_("Color"));
  }
  return WM_operator_props_popup_confirm_ex(C,
                                            op,
                                            event,
                                            IFACE_("Add Color Attribute"),
                                            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Add"));
}

enum class ConvertAttributeMode {
  Generic = 0,
  VertexGroup = 1,
};

static bool geometry_attribute_convert_poll(bContext *C)
{
  if (!geometry_attributes_poll(C)) {
    return false;
  }

  Object *ob = object::context_object(C);
  ID *data = static_cast<ID *>(ob->data);
  AttributeOwner owner = AttributeOwner::from_id(data);
  if (ob->type == OB_MESH) {
    if (CTX_data_edit_object(C) != nullptr) {
      CTX_wm_operator_poll_msg_set(C, "Operation is not allowed in edit mode");
      return false;
    }
  }
  if (BKE_attributes_active_name_get(owner) == std::nullopt) {
    return false;
  }
  return true;
}

bool convert_attribute(AttributeOwner &owner,
                       bke::MutableAttributeAccessor attributes,
                       const StringRef name,
                       const bke::AttrDomain dst_domain,
                       const bke::AttrType dst_type,
                       ReportList *reports)
{
  BLI_assert(attributes.contains(name));
  if (ELEM(dst_type, bke::AttrType::String)) {
    if (reports) {
      BKE_report(reports, RPT_ERROR, "Cannot convert to the selected type");
    }
    return false;
  }

  const bool was_active = BKE_attributes_active_name_get(owner) == name;

  const std::string name_copy = name;
  const GVArray varray = *attributes.lookup_or_default(name_copy, dst_domain, dst_type);

  const CPPType &cpp_type = varray.type();
  void *new_data = MEM_mallocN_aligned(
      varray.size() * cpp_type.size, cpp_type.alignment, __func__);
  varray.materialize_to_uninitialized(new_data);
  attributes.remove(name_copy);
  if (!attributes.add(name_copy, dst_domain, dst_type, bke::AttributeInitMoveArray(new_data))) {
    MEM_freeN(new_data);
  }

  if (was_active) {
    /* The attribute active status is stored as an index. Changing the attribute's domain will
     * change its index, so reassign the active attribute if necessary. */
    BKE_attributes_active_set(owner, name_copy);
  }

  return true;
}

static wmOperatorStatus geometry_attribute_convert_exec(bContext *C, wmOperator *op)
{
  Object *ob = object::context_object(C);
  ID *ob_data = static_cast<ID *>(ob->data);
  AttributeOwner owner = AttributeOwner::from_id(ob_data);
  const std::string name = *BKE_attributes_active_name_get(owner);

  if (ob->type == OB_MESH) {
    const ConvertAttributeMode mode = ConvertAttributeMode(RNA_enum_get(op->ptr, "mode"));

    Mesh *mesh = reinterpret_cast<Mesh *>(ob_data);
    bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();

    switch (mode) {
      case ConvertAttributeMode::Generic: {
        if (!convert_attribute(owner,
                               attributes,
                               name,
                               bke::AttrDomain(RNA_enum_get(op->ptr, "domain")),
                               *bke::custom_data_type_to_attr_type(
                                   eCustomDataType(RNA_enum_get(op->ptr, "data_type"))),
                               op->reports))
        {
          return OPERATOR_CANCELLED;
        }
        break;
      }
      case ConvertAttributeMode::VertexGroup: {
        Array<float> src_weights(mesh->verts_num);
        VArray<float> src_varray = *attributes.lookup_or_default<float>(
            name, bke::AttrDomain::Point, 0.0f);
        src_varray.materialize(src_weights);
        attributes.remove(name);

        bDeformGroup *defgroup = BKE_object_defgroup_new(ob, name);
        const int defgroup_index = BLI_findindex(BKE_id_defgroup_list_get(&mesh->id), defgroup);
        MDeformVert *dverts = BKE_object_defgroup_data_create(&mesh->id);
        for (const int i : IndexRange(mesh->verts_num)) {
          const float weight = src_weights[i];
          if (weight > 0.0f) {
            BKE_defvert_add_index_notest(dverts + i, defgroup_index, weight);
          }
        }
        AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
        int *active_index = BKE_attributes_active_index_p(owner);
        if (*active_index > 0) {
          *active_index -= 1;
        }
        break;
      }
    }
    DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &mesh->id);
  }
  else if (ob->type == OB_CURVES) {
    Curves *curves_id = static_cast<Curves *>(ob->data);
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    if (!convert_attribute(owner,
                           curves.attributes_for_write(),
                           name,
                           bke::AttrDomain(RNA_enum_get(op->ptr, "domain")),
                           *bke::custom_data_type_to_attr_type(
                               eCustomDataType(RNA_enum_get(op->ptr, "data_type"))),
                           op->reports))
    {
      return OPERATOR_CANCELLED;
    }
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id->id);
  }
  else if (ob->type == OB_POINTCLOUD) {
    PointCloud &pointcloud = *static_cast<PointCloud *>(ob->data);
    if (!convert_attribute(owner,
                           pointcloud.attributes_for_write(),
                           name,
                           bke::AttrDomain(RNA_enum_get(op->ptr, "domain")),
                           *bke::custom_data_type_to_attr_type(
                               eCustomDataType(RNA_enum_get(op->ptr, "data_type"))),
                           op->reports))
    {
      return OPERATOR_CANCELLED;
    }
    DEG_id_tag_update(&pointcloud.id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &pointcloud.id);
  }

  BKE_attributes_active_set(owner, name);

  return OPERATOR_FINISHED;
}

static void geometry_color_attribute_add_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  layout->prop(op->ptr, "name", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "domain", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "data_type", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

void GEOMETRY_OT_color_attribute_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Color Attribute";
  ot->description = "Add color attribute to geometry";
  ot->idname = "GEOMETRY_OT_color_attribute_add";

  /* API callbacks. */
  ot->poll = geometry_attributes_poll;
  ot->exec = geometry_color_attribute_add_exec;
  ot->invoke = geometry_color_attribute_add_invoke;
  ot->ui = geometry_color_attribute_add_ui;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;

  /* The default name of the new attribute can be translated if new data translation is enabled,
   * but since the user can choose it at invoke time, the translation happens in the invoke
   * callback instead of here. */
  prop = RNA_def_string(
      ot->srna, "name", nullptr, MAX_NAME, "Name", "Name of new color attribute");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "domain",
                      rna_enum_color_attribute_domain_items,
                      int(bke::AttrDomain::Point),
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
  RNA_def_property_subtype(prop, PROP_COLOR);
  RNA_def_property_float_array_default(prop, default_color);
}

static wmOperatorStatus geometry_color_attribute_set_render_exec(bContext *C, wmOperator *op)
{
  Object *ob = object::context_object(C);
  ID *id = static_cast<ID *>(ob->data);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "name", name);
  Mesh *mesh = id_cast<Mesh *>(id);
  if (mesh->runtime->edit_mesh) {
    const BMDataLayerLookup attr = BM_data_layer_lookup(*mesh->runtime->edit_mesh->bm, name);
    if (!attr) {
      return OPERATOR_CANCELLED;
    }
    if (!bke::mesh::is_color_attribute({attr.domain, attr.type})) {
      return OPERATOR_CANCELLED;
    }
    BKE_id_attributes_default_color_set(id, name);
    DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  }
  else {
    const bke::AttributeAccessor attributes = mesh->attributes();
    if (!bke::mesh::is_color_attribute(attributes.lookup_meta_data(name))) {
      return OPERATOR_CANCELLED;
    }
    BKE_id_attributes_default_color_set(id, name);
    DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  }

  return OPERATOR_FINISHED;
}

void GEOMETRY_OT_color_attribute_render_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Render Color";
  ot->description = "Set default color attribute used for rendering";
  ot->idname = "GEOMETRY_OT_color_attribute_render_set";

  /* API callbacks. */
  ot->poll = geometry_attributes_poll;
  ot->exec = geometry_color_attribute_set_render_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna, "name", "Color", MAX_NAME, "Name", "Name of color attribute");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static wmOperatorStatus geometry_color_attribute_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = object::context_object(C);
  ID *id = static_cast<ID *>(ob->data);
  const std::string active_name = BKE_id_attributes_active_color_name(id).value_or("");
  if (active_name.empty()) {
    return OPERATOR_CANCELLED;
  }
  AttributeOwner owner = AttributeOwner::from_id(id);
  if (!BKE_attribute_remove(owner, active_name, op->reports)) {
    return OPERATOR_CANCELLED;
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

  const Object *ob = object::context_object(C);
  const ID *data = static_cast<ID *>(ob->data);

  if (BKE_id_attributes_color_find(data, BKE_id_attributes_active_color_name(data).value_or(""))) {
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

  /* API callbacks. */
  ot->exec = geometry_color_attribute_remove_exec;
  ot->poll = geometry_color_attributes_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus geometry_color_attribute_duplicate_exec(bContext *C, wmOperator *op)
{
  Object *ob = object::context_object(C);
  ID *id = static_cast<ID *>(ob->data);
  const std::optional<StringRef> active_name = BKE_id_attributes_active_color_name(id);
  if (!active_name) {
    return OPERATOR_CANCELLED;
  }

  AttributeOwner owner = AttributeOwner::from_id(id);
  CustomDataLayer *new_layer = BKE_attribute_duplicate(owner, *active_name, op->reports);
  if (new_layer == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BKE_id_attributes_active_color_set(id, new_layer->name);

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

  const Object *ob = object::context_object(C);
  const ID *data = static_cast<ID *>(ob->data);

  if (BKE_id_attributes_color_find(data, BKE_id_attributes_active_color_name(data).value_or(""))) {
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

  /* API callbacks. */
  ot->exec = geometry_color_attribute_duplicate_exec;
  ot->poll = geometry_color_attributes_duplicate_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus geometry_attribute_convert_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent * /*event*/)
{
  Object *ob = object::context_object(C);
  ID *id = static_cast<ID *>(ob->data);
  AttributeOwner owner = AttributeOwner::from_id(id);
  const bke::AttributeAccessor accessor = *bke::AttributeAccessor::from_id(*id);
  const bke::AttributeMetaData meta_data = *accessor.lookup_meta_data(
      *BKE_attributes_active_name_get(owner));

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "domain");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_enum_set(op->ptr, prop, int(meta_data.domain));
  }
  prop = RNA_struct_find_property(op->ptr, "data_type");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_enum_set(op->ptr, prop, *bke::attr_type_to_custom_data_type(meta_data.data_type));
  }

  return WM_operator_props_dialog_popup(
      C, op, 300, IFACE_("Convert Attribute Domain"), IFACE_("Convert"));
}

static void geometry_attribute_convert_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  Object *ob = object::context_object(C);
  if (ob->type == OB_MESH) {
    layout->prop(op->ptr, "mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  const ConvertAttributeMode mode = ob->type == OB_MESH ?
                                        ConvertAttributeMode(RNA_enum_get(op->ptr, "mode")) :
                                        ConvertAttributeMode::Generic;

  if (mode == ConvertAttributeMode::Generic) {
    if (ob->type != OB_POINTCLOUD) {
      layout->prop(op->ptr, "domain", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
    layout->prop(op->ptr, "data_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static EnumPropertyItem convert_attribute_mode_items[] = {
    {int(ConvertAttributeMode::Generic), "GENERIC", 0, "Generic", ""},
    {int(ConvertAttributeMode::VertexGroup), "VERTEX_GROUP", 0, "Vertex Group", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem *geometry_attribute_convert_mode_itemf(bContext *C,
                                                                     PointerRNA * /*ptr*/,
                                                                     PropertyRNA * /*prop*/,
                                                                     bool *r_free)
{
  if (C == nullptr) {
    return rna_enum_dummy_NULL_items;
  }

  Object *ob = object::context_object(C);
  if (ob == nullptr) {
    return rna_enum_dummy_NULL_items;
  }

  if (ob->type == OB_MESH) {
    *r_free = false;
    return convert_attribute_mode_items;
  }

  EnumPropertyItem *items = nullptr;
  int totitem = 0;
  for (const EnumPropertyItem &item : convert_attribute_mode_items) {
    if (item.value == int(ConvertAttributeMode::Generic)) {
      RNA_enum_item_add(&items, &totitem, &item);
    }
  }
  RNA_enum_item_end(&items, &totitem);
  *r_free = true;
  return items;
}

static bool geometry_color_attribute_convert_poll(bContext *C)
{
  if (!geometry_attributes_poll(C)) {
    return false;
  }

  if (CTX_data_edit_object(C) != nullptr) {
    CTX_wm_operator_poll_msg_set(C, "Operation is not allowed in edit mode");
    return false;
  }

  Object *ob = object::context_object(C);
  ID *id = static_cast<ID *>(ob->data);
  if (GS(id->name) != ID_ME) {
    return false;
  }
  const Mesh *mesh = static_cast<const Mesh *>(ob->data);
  const char *name = mesh->active_color_attribute;
  const bke::AttributeAccessor attributes = mesh->attributes();
  if (!bke::mesh::is_color_attribute(attributes.lookup_meta_data(name))) {
    return false;
  }

  return true;
}

static wmOperatorStatus geometry_color_attribute_convert_exec(bContext *C, wmOperator *op)
{
  Object *ob = object::context_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  convert_attribute(
      owner,
      mesh->attributes_for_write(),
      mesh->active_color_attribute,
      bke::AttrDomain(RNA_enum_get(op->ptr, "domain")),
      *bke::custom_data_type_to_attr_type(eCustomDataType(RNA_enum_get(op->ptr, "data_type"))),
      op->reports);
  DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, &mesh->id);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus geometry_color_attribute_convert_invoke(bContext *C,
                                                                wmOperator *op,
                                                                const wmEvent * /*event*/)
{
  Object *ob = object::context_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  const char *name = mesh->active_color_attribute;
  const bke::AttributeMetaData meta_data = *mesh->attributes().lookup_meta_data(name);

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "domain");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_enum_set(op->ptr, prop, int(meta_data.domain));
  }
  prop = RNA_struct_find_property(op->ptr, "data_type");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_enum_set(op->ptr, prop, *bke::attr_type_to_custom_data_type(meta_data.data_type));
  }

  return WM_operator_props_dialog_popup(
      C, op, 300, IFACE_("Convert Color Attribute Domain"), IFACE_("Convert"));
}

static void geometry_color_attribute_convert_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  layout->prop(op->ptr, "domain", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "data_type", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

void GEOMETRY_OT_color_attribute_convert(wmOperatorType *ot)
{
  ot->name = "Convert Color Attribute";
  ot->description = "Change how the color attribute is stored";
  ot->idname = "GEOMETRY_OT_color_attribute_convert";

  ot->invoke = geometry_color_attribute_convert_invoke;
  ot->exec = geometry_color_attribute_convert_exec;
  ot->poll = geometry_color_attribute_convert_poll;
  ot->ui = geometry_color_attribute_convert_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;

  prop = RNA_def_enum(ot->srna,
                      "domain",
                      rna_enum_color_attribute_domain_items,
                      int(bke::AttrDomain::Point),
                      "Domain",
                      "Type of element that attribute is stored on");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "data_type",
                      rna_enum_color_attribute_type_items,
                      CD_PROP_COLOR,
                      "Data Type",
                      "Type of data stored in attribute");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
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

  PropertyRNA *prop;

  prop = RNA_def_enum(ot->srna,
                      "mode",
                      convert_attribute_mode_items,
                      int(ConvertAttributeMode::Generic),
                      "Mode",
                      "");
  RNA_def_enum_funcs(prop, geometry_attribute_convert_mode_itemf);

  prop = RNA_def_enum(ot->srna,
                      "domain",
                      rna_enum_attribute_domain_items,
                      int(bke::AttrDomain::Point),
                      "Domain",
                      "Which geometry element to move the attribute to");
  RNA_def_enum_funcs(prop, geometry_attribute_domain_itemf);

  RNA_def_enum(
      ot->srna, "data_type", rna_enum_attribute_type_items, CD_PROP_FLOAT, "Data Type", "");
}

}  // namespace blender::ed::geometry
