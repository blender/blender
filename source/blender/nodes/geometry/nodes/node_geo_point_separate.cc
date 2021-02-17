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

#include "BKE_mesh.h"
#include "BKE_persistent_data_handle.hh"
#include "BKE_pointcloud.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
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

static void fill_new_attribute_from_input(const ReadAttribute &input_attribute,
                                          WriteAttribute &out_attribute_a,
                                          WriteAttribute &out_attribute_b,
                                          Span<bool> a_or_b)
{
  fn::GSpan in_span = input_attribute.get_span();
  int i_a = 0;
  int i_b = 0;
  for (int i_in = 0; i_in < in_span.size(); i_in++) {
    const bool move_to_b = a_or_b[i_in];
    if (move_to_b) {
      out_attribute_b.set(i_b, in_span[i_in]);
      i_b++;
    }
    else {
      out_attribute_a.set(i_a, in_span[i_in]);
      i_a++;
    }
  }
}

/**
 * Move the original attribute values to the two output components.
 *
 * \note This assumes a consistent ordering of indices before and after the split,
 * which is true for points and a simple vertex array.
 */
static void move_split_attributes(const GeometryComponent &in_component,
                                  GeometryComponent &out_component_a,
                                  GeometryComponent &out_component_b,
                                  Span<bool> a_or_b)
{
  Set<std::string> attribute_names = in_component.attribute_names();

  for (const std::string &name : attribute_names) {
    ReadAttributePtr attribute = in_component.attribute_try_get_for_read(name);
    BLI_assert(attribute);

    /* Since this node only creates points and vertices, don't copy other attributes. */
    if (attribute->domain() != ATTR_DOMAIN_POINT) {
      continue;
    }

    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(attribute->cpp_type());
    const AttributeDomain domain = attribute->domain();

    /* Don't try to create the attribute on the new component if it already exists (i.e. has been
     * initialized by someone else). */
    if (!out_component_a.attribute_exists(name)) {
      if (!out_component_a.attribute_try_create(name, domain, data_type)) {
        continue;
      }
    }
    if (!out_component_b.attribute_exists(name)) {
      if (!out_component_b.attribute_try_create(name, domain, data_type)) {
        continue;
      }
    }

    WriteAttributePtr out_attribute_a = out_component_a.attribute_try_get_for_write(name);
    WriteAttributePtr out_attribute_b = out_component_b.attribute_try_get_for_write(name);
    if (!out_attribute_a || !out_attribute_b) {
      BLI_assert(false);
      continue;
    }

    fill_new_attribute_from_input(*attribute, *out_attribute_a, *out_attribute_b, a_or_b);
  }
}

/**
 * Find total in each new set and find which of the output sets each point will belong to.
 */
static Array<bool> count_point_splits(const GeometryComponent &component,
                                      const GeoNodeExecParams &params,
                                      int *r_a_total,
                                      int *r_b_total)
{
  const BooleanReadAttribute mask_attribute = params.get_input_attribute<bool>(
      "Mask", component, ATTR_DOMAIN_POINT, false);
  Array<bool> masks = mask_attribute.get_span();
  const int in_total = masks.size();

  *r_b_total = 0;
  for (const bool mask : masks) {
    if (mask) {
      *r_b_total += 1;
    }
  }
  *r_a_total = in_total - *r_b_total;

  return masks;
}

static void separate_mesh(const MeshComponent &in_component,
                          const GeoNodeExecParams &params,
                          MeshComponent &out_component_a,
                          MeshComponent &out_component_b)
{
  const int size = in_component.attribute_domain_size(ATTR_DOMAIN_POINT);
  if (size == 0) {
    return;
  }

  int a_total;
  int b_total;
  Array<bool> a_or_b = count_point_splits(in_component, params, &a_total, &b_total);

  out_component_a.replace(BKE_mesh_new_nomain(a_total, 0, 0, 0, 0));
  out_component_b.replace(BKE_mesh_new_nomain(b_total, 0, 0, 0, 0));

  move_split_attributes(in_component, out_component_a, out_component_b, a_or_b);
}

static void separate_point_cloud(const PointCloudComponent &in_component,
                                 const GeoNodeExecParams &params,
                                 PointCloudComponent &out_component_a,
                                 PointCloudComponent &out_component_b)
{
  const int size = in_component.attribute_domain_size(ATTR_DOMAIN_POINT);
  if (size == 0) {
    return;
  }

  int a_total;
  int b_total;
  Array<bool> a_or_b = count_point_splits(in_component, params, &a_total, &b_total);

  out_component_a.replace(BKE_pointcloud_new_nomain(a_total));
  out_component_b.replace(BKE_pointcloud_new_nomain(b_total));

  move_split_attributes(in_component, out_component_a, out_component_b, a_or_b);
}

static void geo_node_point_separate_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometrySet out_set_a(geometry_set);
  GeometrySet out_set_b;

  /* TODO: This is not necessary-- the input geometry set can be read only,
   * but it must be rewritten to handle instance groups. */
  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<PointCloudComponent>()) {
    separate_point_cloud(*geometry_set.get_component_for_read<PointCloudComponent>(),
                         params,
                         out_set_a.get_component_for_write<PointCloudComponent>(),
                         out_set_b.get_component_for_write<PointCloudComponent>());
  }
  if (geometry_set.has<MeshComponent>()) {
    separate_mesh(*geometry_set.get_component_for_read<MeshComponent>(),
                  params,
                  out_set_a.get_component_for_write<MeshComponent>(),
                  out_set_b.get_component_for_write<MeshComponent>());
  }

  params.set_output("Geometry 1", std::move(out_set_a));
  params.set_output("Geometry 2", std::move(out_set_b));
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
