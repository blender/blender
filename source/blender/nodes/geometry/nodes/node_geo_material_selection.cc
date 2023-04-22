/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_task.hh"

#include "BKE_material.h"

namespace blender::nodes::node_geo_material_selection_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Material>("Material").hide_label(true);
  b.add_output<decl::Bool>("Selection").field_source();
}

static VArray<bool> select_mesh_faces_by_material(const Mesh &mesh,
                                                  const Material *material,
                                                  const IndexMask face_mask)
{
  Vector<int> slots;
  for (const int slot_i : IndexRange(mesh.totcol)) {
    if (mesh.mat[slot_i] == material) {
      slots.append(slot_i);
    }
  }
  if (slots.is_empty()) {
    return VArray<bool>::ForSingle(false, mesh.totpoly);
  }

  const AttributeAccessor attributes = mesh.attributes();
  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", ATTR_DOMAIN_FACE, 0);
  if (material_indices.is_single()) {
    const int slot_i = material_indices.get_internal_single();
    return VArray<bool>::ForSingle(slots.contains(slot_i), mesh.totpoly);
  }

  const VArraySpan<int> material_indices_span(material_indices);

  Array<bool> face_selection(face_mask.min_array_size());
  threading::parallel_for(face_mask.index_range(), 1024, [&](IndexRange range) {
    for (const int i : range) {
      const int face_index = face_mask[i];
      const int slot_i = material_indices_span[face_index];
      face_selection[face_index] = slots.contains(slot_i);
    }
  });

  return VArray<bool>::ForContainer(std::move(face_selection));
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
                                 const IndexMask mask) const final
  {
    if (context.type() != GEO_COMPONENT_TYPE_MESH) {
      return {};
    }
    const Mesh *mesh = context.mesh();
    if (mesh == nullptr) {
      return {};
    }

    const eAttrDomain domain = context.domain();
    const IndexMask domain_mask = (domain == ATTR_DOMAIN_FACE) ? mask : IndexMask(mesh->totpoly);

    VArray<bool> selection = select_mesh_faces_by_material(*mesh, material_, domain_mask);
    return mesh->attributes().adapt_domain<bool>(std::move(selection), ATTR_DOMAIN_FACE, domain);
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

  std::optional<eAttrDomain> preferred_domain(
      const GeometryComponent & /*component*/) const override
  {
    return ATTR_DOMAIN_FACE;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Material *material = params.extract_input<Material *>("Material");
  Field<bool> material_field{std::make_shared<MaterialSelectionFieldInput>(material)};
  params.set_output("Selection", std::move(material_field));
}

}  // namespace blender::nodes::node_geo_material_selection_cc

void register_node_type_geo_material_selection()
{
  namespace file_ns = blender::nodes::node_geo_material_selection_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_MATERIAL_SELECTION, "Material Selection", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
