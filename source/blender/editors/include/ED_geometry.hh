/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_generic_pointer.hh"
#include "BLI_string_ref.hh"

#include "DNA_customdata_types.h"

#include "BKE_screen.hh"

struct Mesh;
struct ReportList;
struct PointerRNA;
struct PropertyRNA;
namespace blender::bke {
enum class AttrDomain : int8_t;
}

namespace blender::ed::geometry {

/* -------------------------------------------------------------------- */
/** \name Attribute Value RNA Property Helpers
 *
 * Functions to make it easier to register RNA properties for the various attribute types and
 * retrieve/set their values.
 * \{ */

StringRefNull rna_property_name_for_type(eCustomDataType type);
PropertyRNA *rna_property_for_type(PointerRNA &ptr, const eCustomDataType type);
void register_rna_properties_for_attribute_types(StructRNA &srna);
GPointer rna_property_for_attribute_type_retrieve_value(PointerRNA &ptr,
                                                        const eCustomDataType type,
                                                        void *buffer);
void rna_property_for_attribute_type_set_value(PointerRNA &ptr, PropertyRNA &prop, GPointer value);
bool attribute_set_poll(bContext &C, const ID &object_data);

/** \} */

}  // namespace blender::ed::geometry

void ED_operatortypes_geometry();

/**
 * Convert an attribute with the given name to a new type and domain.
 * The attribute must already exist.
 *
 * \note Does not support meshes in edit mode.
 */
bool ED_geometry_attribute_convert(Mesh *mesh,
                                   const char *name,
                                   eCustomDataType dst_type,
                                   blender::bke::AttrDomain dst_domain,
                                   ReportList *reports);

namespace blender::ed::geometry {

MenuType node_group_operator_assets_menu();
MenuType node_group_operator_assets_menu_unassigned();

void clear_operator_asset_trees();

void ui_template_node_operator_asset_menu_items(uiLayout &layout,
                                                const bContext &C,
                                                StringRef catalog_path);
void ui_template_node_operator_asset_root_items(uiLayout &layout, const bContext &C);

}  // namespace blender::ed::geometry
