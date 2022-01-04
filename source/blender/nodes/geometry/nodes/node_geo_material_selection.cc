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
  b.add_input<decl::Material>(N_("Material")).hide_label(true);
  b.add_output<decl::Bool>(N_("Selection")).field_source();
}

static void select_mesh_by_material(const Mesh &mesh,
                                    const Material *material,
                                    const IndexMask mask,
                                    const MutableSpan<bool> r_selection)
{
  BLI_assert(mesh.totpoly >= r_selection.size());
  Vector<int> material_indices;
  for (const int i : IndexRange(mesh.totcol)) {
    if (mesh.mat[i] == material) {
      material_indices.append(i);
    }
  }
  threading::parallel_for(mask.index_range(), 1024, [&](IndexRange range) {
    for (const int i : range) {
      const int face_index = mask[i];
      r_selection[i] = material_indices.contains(mesh.mpoly[face_index].mat_nr);
    }
  });
}

class MaterialSelectionFieldInput final : public GeometryFieldInput {
  Material *material_;

 public:
  MaterialSelectionFieldInput(Material *material)
      : GeometryFieldInput(CPPType::get<bool>(), "Material Selection node"), material_(material)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask mask) const final
  {
    if (component.type() != GEO_COMPONENT_TYPE_MESH) {
      return {};
    }
    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    const Mesh *mesh = mesh_component.get_for_read();
    if (mesh == nullptr) {
      return {};
    }

    if (domain == ATTR_DOMAIN_FACE) {
      Array<bool> selection(mask.min_array_size());
      select_mesh_by_material(*mesh, material_, mask, selection);
      return VArray<bool>::ForContainer(std::move(selection));
    }

    Array<bool> selection(mesh->totpoly);
    select_mesh_by_material(*mesh, material_, IndexMask(mesh->totpoly), selection);
    return mesh_component.attribute_try_adapt_domain<bool>(
        VArray<bool>::ForContainer(std::move(selection)), ATTR_DOMAIN_FACE, domain);

    return nullptr;
  }

  uint64_t hash() const override
  {
    return get_default_hash(material_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const MaterialSelectionFieldInput *other_material_selection =
            dynamic_cast<const MaterialSelectionFieldInput *>(&other)) {
      return material_ == other_material_selection->material_;
    }
    return false;
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
