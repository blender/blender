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

#include "BKE_attribute_math.hh"
#include "BKE_mesh.h"
#include "BKE_pointcloud.h"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_point_instance_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Mask")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_point_instance_out[] = {
    {SOCK_GEOMETRY, N_("Geometry 1")},
    {SOCK_GEOMETRY, N_("Geometry 2")},
    {-1, ""},
};

namespace blender::nodes {

template<typename T>
static void copy_data_based_on_mask(Span<T> data,
                                    Span<bool> masks,
                                    const bool invert,
                                    MutableSpan<T> out_data)
{
  int offset = 0;
  for (const int i : data.index_range()) {
    if (masks[i] != invert) {
      out_data[offset] = data[i];
      offset++;
    }
  }
}

static void copy_attributes_based_on_mask(const GeometryComponent &in_component,
                                          GeometryComponent &result_component,
                                          Span<bool> masks,
                                          const bool invert)
{
  for (const std::string &name : in_component.attribute_names()) {
    ReadAttributePtr attribute = in_component.attribute_try_get_for_read(name);
    const CustomDataType data_type = attribute->custom_data_type();

    /* Only copy point attributes. Theoretically this could interpolate attributes on other
     * domains to the point domain, but that would conflict with attributes that are built-in
     * on other domains, which causes creating the attributes to fail. */
    if (attribute->domain() != ATTR_DOMAIN_POINT) {
      continue;
    }

    OutputAttributePtr result_attribute = result_component.attribute_try_get_for_output(
        name, ATTR_DOMAIN_POINT, data_type);

    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      Span<T> span = attribute->get_span<T>();
      MutableSpan<T> out_span = result_attribute->get_span_for_write_only<T>();
      copy_data_based_on_mask(span, masks, invert, out_span);
    });

    result_attribute.apply_span_and_save();
  }
}

static void create_component_points(GeometryComponent &component, const int total)
{
  switch (component.type()) {
    case GEO_COMPONENT_TYPE_MESH:
      static_cast<MeshComponent &>(component).replace(BKE_mesh_new_nomain(total, 0, 0, 0, 0));
      break;
    case GEO_COMPONENT_TYPE_POINT_CLOUD:
      static_cast<PointCloudComponent &>(component).replace(BKE_pointcloud_new_nomain(total));
      break;
    default:
      BLI_assert(false);
      break;
  }
}

static void separate_points_from_component(const GeometryComponent &in_component,
                                           GeometryComponent &out_component,
                                           const StringRef mask_name,
                                           const bool invert)
{
  if (!in_component.attribute_domain_supported(ATTR_DOMAIN_POINT) ||
      in_component.attribute_domain_size(ATTR_DOMAIN_POINT) == 0) {
    return;
  }

  const BooleanReadAttribute mask_attribute = in_component.attribute_get_for_read<bool>(
      mask_name, ATTR_DOMAIN_POINT, false);
  Span<bool> masks = mask_attribute.get_span();

  const int total = masks.count(!invert);
  if (total == 0) {
    return;
  }

  create_component_points(out_component, total);

  copy_attributes_based_on_mask(in_component, out_component, masks, invert);
}

static GeometrySet separate_geometry_set(const GeometrySet &set_in,
                                         const StringRef mask_name,
                                         const bool invert)
{
  GeometrySet set_out;
  for (const GeometryComponent *component : set_in.get_components_for_read()) {
    GeometryComponent &out_component = set_out.get_component_for_write(component->type());
    separate_points_from_component(*component, out_component, mask_name, invert);
  }
  return set_out;
}

static void geo_node_point_separate_exec(GeoNodeExecParams params)
{
  const std::string mask_attribute_name = params.extract_input<std::string>("Mask");
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  /* TODO: This is not necessary-- the input geometry set can be read only,
   * but it must be rewritten to handle instance groups. */
  geometry_set = geometry_set_realize_instances(geometry_set);

  params.set_output("Geometry 1", separate_geometry_set(geometry_set, mask_attribute_name, true));
  params.set_output("Geometry 2", separate_geometry_set(geometry_set, mask_attribute_name, false));
}

}  // namespace blender::nodes

void register_node_type_geo_point_separate()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_POINT_SEPARATE, "Point Separate", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_point_instance_in, geo_node_point_instance_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_point_separate_exec;
  nodeRegisterType(&ntype);
}
