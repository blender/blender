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

#include "DEG_depsgraph_query.h"

#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_position_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Vector>(N_("Position")).implicit_field();
  b.add_input<decl::Vector>(N_("Offset")).supports_field().subtype(PROP_TRANSLATION);
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void set_computed_position_and_offset(GeometryComponent &component,
                                             const VArray<float3> &in_positions,
                                             const VArray<float3> &in_offsets,
                                             const AttributeDomain domain,
                                             const IndexMask selection)
{

  OutputAttribute_Typed<float3> positions = component.attribute_try_get_for_output<float3>(
      "position", domain, {0, 0, 0});

  const int grain_size = 10000;

  switch (component.type()) {
    case GEO_COMPONENT_TYPE_MESH: {
      Mesh *mesh = static_cast<MeshComponent &>(component).get_for_write();
      MutableSpan<MVert> mverts{mesh->mvert, mesh->totvert};
      if (in_positions.is_same(positions.varray())) {
        devirtualize_varray(in_offsets, [&](const auto in_offsets) {
          threading::parallel_for(
              selection.index_range(), grain_size, [&](const IndexRange range) {
                for (const int i : selection.slice(range)) {
                  const float3 offset = in_offsets[i];
                  add_v3_v3(mverts[i].co, offset);
                }
              });
        });
      }
      else {
        devirtualize_varray2(
            in_positions, in_offsets, [&](const auto in_positions, const auto in_offsets) {
              threading::parallel_for(
                  selection.index_range(), grain_size, [&](const IndexRange range) {
                    for (const int i : selection.slice(range)) {
                      const float3 new_position = in_positions[i] + in_offsets[i];
                      copy_v3_v3(mverts[i].co, new_position);
                    }
                  });
            });
      }
      break;
    }
    default: {
      MutableSpan<float3> out_positions_span = positions.as_span();
      if (in_positions.is_same(positions.varray())) {
        devirtualize_varray(in_offsets, [&](const auto in_offsets) {
          threading::parallel_for(
              selection.index_range(), grain_size, [&](const IndexRange range) {
                for (const int i : selection.slice(range)) {
                  out_positions_span[i] += in_offsets[i];
                }
              });
        });
      }
      else {
        devirtualize_varray2(
            in_positions, in_offsets, [&](const auto in_positions, const auto in_offsets) {
              threading::parallel_for(
                  selection.index_range(), grain_size, [&](const IndexRange range) {
                    for (const int i : selection.slice(range)) {
                      out_positions_span[i] = in_positions[i] + in_offsets[i];
                    }
                  });
            });
      }
      break;
    }
  }

  positions.save();
}

static void set_position_in_component(GeometryComponent &component,
                                      const Field<bool> &selection_field,
                                      const Field<float3> &position_field,
                                      const Field<float3> &offset_field)
{
  AttributeDomain domain = component.type() == GEO_COMPONENT_TYPE_INSTANCES ?
                               ATTR_DOMAIN_INSTANCE :
                               ATTR_DOMAIN_POINT;
  GeometryComponentFieldContext field_context{component, domain};
  const int domain_size = component.attribute_domain_size(domain);
  if (domain_size == 0) {
    return;
  }

  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  evaluator.add(position_field);
  evaluator.add(offset_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<float3> &positions_input = evaluator.get_evaluated<float3>(0);
  const VArray<float3> &offsets_input = evaluator.get_evaluated<float3>(1);
  set_computed_position_and_offset(component, positions_input, offsets_input, domain, selection);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float3> offset_field = params.extract_input<Field<float3>>("Offset");
  Field<float3> position_field = params.extract_input<Field<float3>>("Position");

  for (const GeometryComponentType type : {GEO_COMPONENT_TYPE_MESH,
                                           GEO_COMPONENT_TYPE_POINT_CLOUD,
                                           GEO_COMPONENT_TYPE_CURVE,
                                           GEO_COMPONENT_TYPE_INSTANCES}) {
    if (geometry.has(type)) {
      set_position_in_component(
          geometry.get_component_for_write(type), selection_field, position_field, offset_field);
    }
  }

  params.set_output("Geometry", std::move(geometry));
}

}  // namespace blender::nodes::node_geo_set_position_cc

void register_node_type_geo_set_position()
{
  namespace file_ns = blender::nodes::node_geo_set_position_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_POSITION, "Set Position", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
