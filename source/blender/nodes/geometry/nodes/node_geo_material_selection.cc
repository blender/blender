/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "DNA_mesh_types.h"

#include "BLI_index_mask.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

namespace blender::nodes::node_geo_material_selection_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Material>("Material").optional_label(true);
  b.add_output<decl::Bool>("Selection").field_source();
}

static VArray<bool> select_by_material(const Span<Material *> materials,
                                       const Material *material,
                                       const AttributeAccessor &attributes,
                                       const AttrDomain domain,
                                       const IndexMask &domain_mask)
{
  const int domain_size = attributes.domain_size(domain);
  Vector<int> slots;
  for (const int slot_i : materials.index_range()) {
    if (materials[slot_i] == material) {
      slots.append(slot_i);
    }
  }
  if (slots.is_empty()) {
    return VArray<bool>::from_single(false, domain_size);
  }

  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", domain, 0);
  if (const std::optional<int> single = material_indices.get_if_single()) {
    return VArray<bool>::from_single(slots.contains(*single), domain_size);
  }

  const VArraySpan<int> material_indices_span(material_indices);
  Array<bool> domain_selection(domain_mask.min_array_size());
  domain_mask.foreach_index_optimized<int>(GrainSize(1024), [&](const int domain_index) {
    const int slot_i = material_indices_span[domain_index];
    domain_selection[domain_index] = slots.contains(slot_i);
  });
  return VArray<bool>::from_container(std::move(domain_selection));
}

class MaterialSelectionFieldInput final : public bke::GeometryFieldInput {
  Material *material_;

 public:
  MaterialSelectionFieldInput(Material *material)
      : bke::GeometryFieldInput(CPPType::get<bool>(), "Material Selection node"),
        material_(material)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final
  {
    switch (context.type()) {
      case GeometryComponent::Type::Mesh: {
        const Mesh *mesh = context.mesh();
        if (!mesh) {
          return {};
        }
        const AttrDomain domain = context.domain();
        const IndexMask domain_mask = (domain == AttrDomain::Face) ? mask :
                                                                     IndexMask(mesh->faces_num);
        const AttributeAccessor attributes = mesh->attributes();
        VArray<bool> selection = select_by_material(
            {mesh->mat, mesh->totcol}, material_, attributes, AttrDomain::Face, domain_mask);
        return attributes.adapt_domain<bool>(std::move(selection), AttrDomain::Face, domain);
      }
      case GeometryComponent::Type::GreasePencil: {
        const bke::CurvesGeometry *curves = context.curves_or_strokes();
        if (!curves) {
          return {};
        }
        const AttrDomain domain = context.domain();
        const IndexMask domain_mask = (domain == AttrDomain::Curve) ?
                                          mask :
                                          IndexMask(curves->curves_num());
        const AttributeAccessor attributes = curves->attributes();
        const VArray<int> material_indices = *attributes.lookup_or_default<int>(
            "material_index", AttrDomain::Curve, 0);
        const GreasePencil &grease_pencil = *context.grease_pencil();
        VArray<bool> selection = select_by_material(
            {grease_pencil.material_array, grease_pencil.material_array_num},
            material_,
            attributes,
            AttrDomain::Curve,
            domain_mask);
        return attributes.adapt_domain<bool>(std::move(selection), AttrDomain::Curve, domain);
      }
      case GeometryComponent::Type::Curve: {
        const Curves *curves_id = context.curves_id();
        if (!curves_id) {
          return {};
        }
        const bke::CurvesGeometry *curves = context.curves_or_strokes();
        if (!curves) {
          return {};
        }
        const AttrDomain domain = context.domain();
        const IndexMask domain_mask = (domain == AttrDomain::Curve) ?
                                          mask :
                                          IndexMask(curves->curves_num());
        const AttributeAccessor attributes = curves->attributes();
        const VArray<int> material_indices = *attributes.lookup_or_default<int>(
            "material_index", AttrDomain::Curve, 0);
        VArray<bool> selection = select_by_material({curves_id->mat, curves_id->totcol},
                                                    material_,
                                                    attributes,
                                                    AttrDomain::Curve,
                                                    domain_mask);
        return attributes.adapt_domain<bool>(std::move(selection), AttrDomain::Curve, domain);
      }
      default:
        return {};
    }
  }

  uint64_t hash() const override
  {
    return get_default_hash(material_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const MaterialSelectionFieldInput *other_material_selection =
            dynamic_cast<const MaterialSelectionFieldInput *>(&other))
    {
      return material_ == other_material_selection->material_;
    }
    return false;
  }

  std::optional<AttrDomain> preferred_domain(
      const GeometryComponent & /*component*/) const override
  {
    return AttrDomain::Face;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Material *material = params.extract_input<Material *>("Material");
  Field<bool> material_field{std::make_shared<MaterialSelectionFieldInput>(material)};
  params.set_output("Selection", std::move(material_field));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMaterialSelection", GEO_NODE_MATERIAL_SELECTION);
  ntype.ui_name = "Material Selection";
  ntype.ui_description = "Provide a selection of faces that use the specified material";
  ntype.enum_name_legacy = "MATERIAL_SELECTION";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_material_selection_cc
