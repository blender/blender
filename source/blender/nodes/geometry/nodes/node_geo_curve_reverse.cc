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

#include "BLI_task.hh"

#include "BKE_spline.hh"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_curve_reverse_in[] = {
    {SOCK_GEOMETRY, N_("Curve")},
    {SOCK_STRING, N_("Selection")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_curve_reverse_out[] = {
    {SOCK_GEOMETRY, N_("Curve")},
    {-1, ""},
};

namespace blender::nodes {

/**
 * Reverse the data in a MutableSpan object.
 */
template<typename T> static void reverse_data(MutableSpan<T> r_data)
{
  const int size = r_data.size();
  for (const int i : IndexRange(size / 2)) {
    std::swap(r_data[size - 1 - i], r_data[i]);
  }
}

/**
 * Reverse and Swap the data between 2 MutableSpans.
 */
template<typename T> static void reverse_data(MutableSpan<T> left, MutableSpan<T> right)
{
  BLI_assert(left.size() == right.size());
  const int size = left.size();

  for (const int i : IndexRange(size / 2 + size % 2)) {
    std::swap(left[i], right[size - 1 - i]);
    std::swap(right[i], left[size - 1 - i]);
  }
}

static void geo_node_curve_reverse_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  geometry_set = bke::geometry_set_realize_instances(geometry_set);
  if (!geometry_set.has_curve()) {
    params.set_output("Curve", geometry_set);
    return;
  }

  /* Retrieve data for write access so we can avoid new allocations for the reversed data. */
  CurveComponent &curve_component = geometry_set.get_component_for_write<CurveComponent>();
  CurveEval &curve = *curve_component.get_for_write();
  MutableSpan<SplinePtr> splines = curve.splines();

  const std::string selection_name = params.extract_input<std::string>("Selection");
  GVArray_Typed<bool> selection = curve_component.attribute_get_for_read(
      selection_name, ATTR_DOMAIN_CURVE, true);

  parallel_for(splines.index_range(), 128, [&](IndexRange range) {
    for (const int i : range) {
      if (!selection[i]) {
        continue;
      }

      reverse_data<float3>(splines[i]->positions());
      reverse_data<float>(splines[i]->radii());
      reverse_data<float>(splines[i]->tilts());

      splines[i]->attributes.foreach_attribute(
          [&](StringRefNull name, const AttributeMetaData &meta_data) {
            std::optional<blender::fn::GMutableSpan> output_attribute =
                splines[i]->attributes.get_for_write(name);
            if (!output_attribute) {
              BLI_assert_unreachable();
              return false;
            }
            attribute_math::convert_to_static_type(meta_data.data_type, [&](auto dummy) {
              using T = decltype(dummy);
              reverse_data(output_attribute->typed<T>());
            });
            return true;
          },
          ATTR_DOMAIN_POINT);

      /* Deal with extra info on derived types. */
      if (BezierSpline *spline = dynamic_cast<BezierSpline *>(splines[i].get())) {
        reverse_data<BezierSpline::HandleType>(spline->handle_types_left());
        reverse_data<BezierSpline::HandleType>(spline->handle_types_right());
        reverse_data<float3>(spline->handle_positions_left(), spline->handle_positions_right());
      }
      else if (NURBSpline *spline = dynamic_cast<NURBSpline *>(splines[i].get())) {
        reverse_data<float>(spline->weights());
      }
      /* Nothing to do for poly splines. */

      splines[i]->mark_cache_invalid();
    }
  });

  params.set_output("Curve", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_curve_reverse()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_REVERSE, "Curve Reverse", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_curve_reverse_in, geo_node_curve_reverse_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_reverse_exec;
  nodeRegisterType(&ntype);
}
