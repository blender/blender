/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_rand.hh"

#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "BLT_translation.h"

#include "ED_curves.h"
#include "ED_node.h"
#include "ED_object.h"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"

namespace blender::ed::curves {

static bool has_surface_deformation_node(const bNodeTree &ntree)
{
  if (!ntree.nodes_by_type("GeometryNodeDeformCurvesOnSurface").is_empty()) {
    return true;
  }
  for (const bNode *node : ntree.group_nodes()) {
    if (const bNodeTree *sub_tree = reinterpret_cast<const bNodeTree *>(node->id)) {
      if (has_surface_deformation_node(*sub_tree)) {
        return true;
      }
    }
  }
  return false;
}

static bool has_surface_deformation_node(const Object &curves_ob)
{
  LISTBASE_FOREACH (const ModifierData *, md, &curves_ob.modifiers) {
    if (md->type != eModifierType_Nodes) {
      continue;
    }
    const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
    if (nmd->node_group == nullptr) {
      continue;
    }
    if (has_surface_deformation_node(*nmd->node_group)) {
      return true;
    }
  }
  return false;
}

void ensure_surface_deformation_node_exists(bContext &C, Object &curves_ob)
{
  if (has_surface_deformation_node(curves_ob)) {
    return;
  }

  Main *bmain = CTX_data_main(&C);
  Scene *scene = CTX_data_scene(&C);

  ModifierData *md = ED_object_modifier_add(
      nullptr, bmain, scene, &curves_ob, DATA_("Surface Deform"), eModifierType_Nodes);
  NodesModifierData &nmd = *reinterpret_cast<NodesModifierData *>(md);
  nmd.node_group = ntreeAddTree(bmain, DATA_("Surface Deform"), "GeometryNodeTree");

  bNodeTree *ntree = nmd.node_group;
  ntreeAddSocketInterface(ntree, SOCK_IN, "NodeSocketGeometry", "Geometry");
  ntreeAddSocketInterface(ntree, SOCK_OUT, "NodeSocketGeometry", "Geometry");
  bNode *group_input = nodeAddStaticNode(&C, ntree, NODE_GROUP_INPUT);
  bNode *group_output = nodeAddStaticNode(&C, ntree, NODE_GROUP_OUTPUT);
  bNode *deform_node = nodeAddStaticNode(&C, ntree, GEO_NODE_DEFORM_CURVES_ON_SURFACE);

  ED_node_tree_propagate_change(&C, bmain, nmd.node_group);

  nodeAddLink(ntree,
              group_input,
              static_cast<bNodeSocket *>(group_input->outputs.first),
              deform_node,
              nodeFindSocket(deform_node, SOCK_IN, "Curves"));
  nodeAddLink(ntree,
              deform_node,
              nodeFindSocket(deform_node, SOCK_OUT, "Curves"),
              group_output,
              static_cast<bNodeSocket *>(group_output->inputs.first));

  group_input->locx = -200;
  group_output->locx = 200;
  deform_node->locx = 0;

  ED_node_tree_propagate_change(&C, bmain, nmd.node_group);
}

bke::CurvesGeometry primitive_random_sphere(const int curves_size, const int points_per_curve)
{
  bke::CurvesGeometry curves(points_per_curve * curves_size, curves_size);

  MutableSpan<int> offsets = curves.offsets_for_write();
  MutableSpan<float3> positions = curves.positions_for_write();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> radius = attributes.lookup_or_add_for_write_only_span<float>(
      "radius", ATTR_DOMAIN_POINT);

  for (const int i : offsets.index_range()) {
    offsets[i] = points_per_curve * i;
  }

  RandomNumberGenerator rng;

  const OffsetIndices points_by_curve = curves.points_by_curve();
  for (const int i : curves.curves_range()) {
    const IndexRange points = points_by_curve[i];
    MutableSpan<float3> curve_positions = positions.slice(points);
    MutableSpan<float> curve_radii = radius.span.slice(points);

    const float theta = 2.0f * M_PI * rng.get_float();
    const float phi = saacosf(2.0f * rng.get_float() - 1.0f);

    float3 no = {std::sin(theta) * std::sin(phi), std::cos(theta) * std::sin(phi), std::cos(phi)};
    no = math::normalize(no);

    float3 co = no;
    for (int key = 0; key < points_per_curve; key++) {
      float t = key / float(points_per_curve - 1);
      curve_positions[key] = co;
      curve_radii[key] = 0.02f * (1.0f - t);

      float3 offset = float3(rng.get_float(), rng.get_float(), rng.get_float()) * 2.0f - 1.0f;
      co += (offset + no) / points_per_curve;
    }
  }

  radius.finish();

  return curves;
}

}  // namespace blender::ed::curves
