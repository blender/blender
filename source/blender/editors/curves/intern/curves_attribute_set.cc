/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "BLI_generic_pointer.hh"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_context.hh"
#include "BKE_type_conversions.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_curves.hh"
#include "ED_geometry.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_transform.hh"
#include "ED_view3d.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "DNA_object_types.h"

#include "DEG_depsgraph.hh"

/* -------------------------------------------------------------------- */
/** \name Delete Operator
 * \{ */

namespace blender::ed::curves {

static bool active_attribute_poll(bContext *C)
{
  if (!editable_curves_in_edit_mode_poll(C)) {
    return false;
  }
  const Object *object = CTX_data_active_object(C);
  const ID &object_data = *static_cast<const ID *>(object->data);
  if (!geometry::attribute_set_poll(*C, object_data)) {
    return false;
  }
  return true;
}

static IndexMask retrieve_selected_elements(const Curves &curves_id,
                                            const bke::AttrDomain domain,
                                            IndexMaskMemory &memory)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      return retrieve_selected_points(curves_id, memory);
    case bke::AttrDomain::Curve:
      return retrieve_selected_curves(curves_id, memory);
    default:
      BLI_assert_unreachable();
      return {};
  }
}

static void validate_value(const bke::AttributeAccessor attributes,
                           const StringRef name,
                           const CPPType &type,
                           void *buffer)
{
  const bke::AttributeValidator validator = attributes.lookup_validator(name);
  if (!validator) {
    return;
  }
  BUFFER_FOR_CPP_TYPE_VALUE(type, validated_buffer);
  BLI_SCOPED_DEFER([&]() { type.destruct(validated_buffer); });

  const IndexMask single_mask(1);
  mf::ParamsBuilder params(*validator.function, &single_mask);
  params.add_readonly_single_input(GPointer(type, buffer));
  params.add_uninitialized_single_output({type, validated_buffer, 1});
  mf::ContextBuilder context;
  validator.function->call(single_mask, params, context);

  type.copy_assign(validated_buffer, buffer);
}

static wmOperatorStatus set_attribute_exec(bContext *C, wmOperator *op)
{
  Object *active_object = CTX_data_active_object(C);
  Curves &active_curves_id = *static_cast<Curves *>(active_object->data);

  AttributeOwner active_owner = AttributeOwner::from_id(&active_curves_id.id);
  const StringRef name = *BKE_attributes_active_name_get(active_owner);
  const bke::AttributeMetaData active_meta_data =
      *active_curves_id.geometry.wrap().attributes().lookup_meta_data(name);
  const bke::AttrType active_type = active_meta_data.data_type;
  const CPPType &type = bke::attribute_type_to_cpp_type(active_type);

  BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
  BLI_SCOPED_DEFER([&]() { type.destruct(buffer); });
  const GPointer value = geometry::rna_property_for_attribute_type_retrieve_value(
      *op->ptr, active_type, buffer);

  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();

  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(name);
    if (!attribute) {
      continue;
    }

    /* Use implicit conversions to try to handle the case where the active attribute has a
     * different type on multiple objects. */
    const CPPType &dst_type = attribute.span.type();
    if (&type != &dst_type && !conversions.is_convertible(type, dst_type)) {
      continue;
    }
    BUFFER_FOR_CPP_TYPE_VALUE(dst_type, dst_buffer);
    BLI_SCOPED_DEFER([&]() { dst_type.destruct(dst_buffer); });
    conversions.convert_to_uninitialized(type, dst_type, value.get(), dst_buffer);

    validate_value(attributes, name, dst_type, dst_buffer);
    const GPointer dst_value(type, dst_buffer);

    IndexMaskMemory memory;
    const IndexMask selection = retrieve_selected_elements(*curves_id, attribute.domain, memory);
    if (selection.is_empty()) {
      attribute.finish();
      continue;
    }
    dst_type.fill_assign_indices(dst_value.get(), attribute.span.data(), selection);
    attribute.finish();

    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus set_attribute_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *active_object = CTX_data_active_object(C);
  Curves &active_curves_id = *static_cast<Curves *>(active_object->data);

  AttributeOwner owner = AttributeOwner::from_id(&active_curves_id.id);
  const StringRef name = *BKE_attributes_active_name_get(owner);
  const bke::CurvesGeometry &curves = active_curves_id.geometry.wrap();
  const bke::AttributeAccessor attributes = curves.attributes();
  const bke::GAttributeReader attribute = attributes.lookup(name);
  const bke::AttrDomain domain = attribute.domain;

  IndexMaskMemory memory;
  const IndexMask selection = retrieve_selected_elements(active_curves_id, domain, memory);

  const CPPType &type = attribute.varray.type();

  PropertyRNA *prop = geometry::rna_property_for_type(*op->ptr,
                                                      bke::cpp_type_to_attribute_type(type));
  if (RNA_property_is_set(op->ptr, prop)) {
    return WM_operator_props_popup(C, op, event);
  }

  BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
  BLI_SCOPED_DEFER([&]() { type.destruct(buffer); });

  bke::attribute_math::convert_to_static_type(type, [&](auto dummy) {
    using T = decltype(dummy);
    const VArray<T> values_typed = attribute.varray.typed<T>();
    bke::attribute_math::DefaultMixer<T> mixer{MutableSpan(static_cast<T *>(buffer), 1)};
    selection.foreach_index([&](const int i) { mixer.mix_in(0, values_typed[i]); });
    mixer.finalize();
  });

  geometry::rna_property_for_attribute_type_set_value(*op->ptr, *prop, GPointer(type, buffer));

  return WM_operator_props_popup(C, op, event);
}

static void set_attribute_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = &op->layout->column(true);
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  Object *object = CTX_data_active_object(C);
  Curves &curves_id = *static_cast<Curves *>(object->data);

  AttributeOwner owner = AttributeOwner::from_id(&curves_id.id);
  const StringRef name = *BKE_attributes_active_name_get(owner);
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  const bke::AttributeMetaData meta_data = *curves.attributes().lookup_meta_data(name);
  const StringRefNull prop_name = geometry::rna_property_name_for_type(meta_data.data_type);
  layout->prop(op->ptr, prop_name, UI_ITEM_NONE, name, ICON_NONE);
}

void CURVES_OT_attribute_set(wmOperatorType *ot)
{
  using namespace blender::ed;
  using namespace blender::ed::curves;
  ot->name = "Set Attribute";
  ot->description = "Set values of the active attribute for selected elements";
  ot->idname = "CURVES_OT_attribute_set";

  ot->exec = set_attribute_exec;
  ot->invoke = set_attribute_invoke;
  ot->poll = active_attribute_poll;
  ot->ui = set_attribute_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  geometry::register_rna_properties_for_attribute_types(*ot->srna);
}

}  // namespace blender::ed::curves

/** \} */
