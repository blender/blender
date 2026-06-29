/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_foreach_geometry.hh"

#include "BKE_attribute.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_rename_attribute_cc {

enum class RenameMode {
  Single,
  Prefix,
};

const EnumPropertyItem rename_mode_items[] = {
    {int(RenameMode::Single),
     "SINGLE",
     0,
     N_("Single"),
     N_("Rename a single attribute with the provided attribute name")},
    {int(RenameMode::Prefix),
     "PREFIX",
     0,
     N_("Prefix"),
     N_("Rename all attributes with the provided prefix")},
    {},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Geometry>("Geometry"_ustr);
  b.add_output<decl::Geometry>("Geometry"_ustr).align_with_previous().propagate_all_geometry();

  b.add_input<decl::Menu>("Mode"_ustr).static_items(rename_mode_items).optional_label();

  b.add_input<decl::String>("Old"_ustr).optional_label().is_attribute_name();
  b.add_input<decl::String>("New"_ustr).optional_label();
  b.add_input<decl::Bool>("Overwrite"_ustr).default_value(false);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry"_ustr);
  const RenameMode mode = params.extract_input<RenameMode>("Mode"_ustr);
  const std::string old_name = params.extract_input<std::string>("Old"_ustr);
  const std::string new_name = params.extract_input<std::string>("New"_ustr);
  const bool overwrite = params.extract_input<bool>("Overwrite"_ustr);

  if (old_name.empty()) {
    params.set_output("Geometry"_ustr, std::move(geometry_set));
    return;
  }
  if (old_name == new_name) {
    params.set_output("Geometry"_ustr, std::move(geometry_set));
    return;
  }

  std::atomic<bool> not_found = false;
  Mutex failures_lock;
  Map<std::string, std::string> failures;
  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry) {
    for (const auto type : {
             GeometryComponent::Type::Mesh,
             GeometryComponent::Type::PointCloud,
             GeometryComponent::Type::Curve,
             GeometryComponent::Type::Instance,
             GeometryComponent::Type::GreasePencil,
         })
    {
      if (!geometry.has(type)) {
        continue;
      }
      AlignedBuffer<512, 8> strings_buffer;
      ResourceScope scope(strings_buffer);
      Map<StringRef, StringRef> renames;
      {
        const GeometryComponent &component = *geometry.get_component(type);
        const AttributeAccessor attributes = *component.attributes();
        switch (mode) {
          case RenameMode::Single: {
            if (!attributes.contains(old_name)) {
              not_found = true;
              continue;
            }
            renames.add_new(old_name, new_name);
            break;
          }
          case RenameMode::Prefix: {
            attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
              if (iter.name.startswith(old_name)) {
                renames.add(
                    scope.allocator().copy_string(iter.name),
                    scope.construct<std::string>(new_name + iter.name.substr(old_name.size())));
              }
            });
            break;
          }
        }
      }
      if (renames.is_empty()) {
        continue;
      }
      GeometryComponent &component = geometry.get_component_for_write(type);
      MutableAttributeAccessor attributes = *component.attributes_for_write();
      const Set<StringRef> failed = attributes.rename(renames, overwrite);
      if (!failed.is_empty()) {
        std::scoped_lock lock(failures_lock);
        for (const StringRef failed_name : failed) {
          failures.add_as(failed_name, renames.lookup(failed_name));
        }
      }
    }
  });

  if (not_found) {
    params.error_message_add(NodeWarningType::Warning,
                             fmt::format("{}: '{}'", TIP_("Attribute not found"), old_name));
  }
  for (const auto &[old_name, new_name] : failures.items()) {
    params.error_message_add(
        NodeWarningType::Warning,
        fmt::format(fmt::runtime("Failed to rename attribute: '{}' to '{}'"), old_name, new_name));
  }

  params.set_output("Geometry"_ustr, std::move(geometry_set));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeRenameAttribute"_ustr);
  ntype.ui_name = "Rename Attribute";
  ntype.ui_description = "Change the name of an attribute";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_rename_attribute_cc
