/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_pointinstancer.hh"

#include "BKE_attribute.hh"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_object.hh"
#include "BKE_pointcloud.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_string.h"

#include "DNA_collection_types.h"
#include "DNA_node_types.h"

#include <pxr/usd/usdGeom/pointInstancer.h>

namespace blender::io::usd {

/**
 * Create a node to read a geometry attribute of the given name and type.
 */
static bNode *add_input_named_attrib_node(bNodeTree *ntree, const char *name, int8_t prop_type)
{
  bNode *node = nodeAddStaticNode(nullptr, ntree, GEO_NODE_INPUT_NAMED_ATTRIBUTE);
  auto *storage = reinterpret_cast<NodeGeometryInputNamedAttribute *>(node->storage);
  storage->data_type = prop_type;

  bNodeSocket *socket = nodeFindSocket(node, SOCK_IN, "Name");
  bNodeSocketValueString *str_value = static_cast<bNodeSocketValueString *>(socket->default_value);
  BLI_strncpy(str_value->value, name, MAX_NAME);
  return node;
}

USDPointInstancerReader::USDPointInstancerReader(const pxr::UsdPrim &prim,
                                                 const USDImportParams &import_params,
                                                 const ImportSettings &settings)
    : USDXformReader(prim, import_params, settings)
{
}

bool USDPointInstancerReader::valid() const
{
  return prim_.IsValid() && prim_.IsA<pxr::UsdGeomPointInstancer>();
}

void USDPointInstancerReader::create_object(Main *bmain, const double /* motionSampleTime */)
{
  void *point_cloud = BKE_pointcloud_add(bmain, name_.c_str());
  this->object_ = BKE_object_add_only_object(bmain, OB_POINTCLOUD, name_.c_str());
  this->object_->data = point_cloud;
}

void USDPointInstancerReader::read_object_data(Main *bmain, const double motionSampleTime)
{
  PointCloud *base_point_cloud = static_cast<PointCloud *>(object_->data);

  pxr::UsdGeomPointInstancer point_instancer_prim(prim_);

  if (!point_instancer_prim) {
    return;
  }

  pxr::VtArray<pxr::GfVec3f> positions;
  pxr::VtArray<pxr::GfVec3f> scales;
  pxr::VtArray<pxr::GfQuath> orientations;
  pxr::VtArray<int> proto_indices;
  std::vector<double> time_samples;

  point_instancer_prim.GetPositionsAttr().GetTimeSamples(&time_samples);

  double sample_time = motionSampleTime;

  if (!time_samples.empty()) {
    sample_time = time_samples[0];
  }

  point_instancer_prim.GetPositionsAttr().Get(&positions, sample_time);
  point_instancer_prim.GetScalesAttr().Get(&scales, sample_time);
  point_instancer_prim.GetOrientationsAttr().Get(&orientations, sample_time);
  point_instancer_prim.GetProtoIndicesAttr().Get(&proto_indices, sample_time);

  std::vector<bool> mask = point_instancer_prim.ComputeMaskAtTime(sample_time);

  PointCloud *point_cloud = BKE_pointcloud_new_nomain(positions.size());

  MutableSpan<float3> positions_span = point_cloud->positions_for_write();

  for (int i = 0; i < positions.size(); ++i) {
    positions_span[i] = float3(positions[i][0], positions[i][1], positions[i][2]);
  }

  bke::MutableAttributeAccessor attributes = point_cloud->attributes_for_write();

  bke::SpanAttributeWriter<float3> scales_attribute =
      attributes.lookup_or_add_for_write_only_span<float3>("scale", bke::AttrDomain::Point);

  /* Here and below, handle the case where instancing attributes are empty or
   * not of the expected size. */
  if (scales.size() < positions.size()) {
    scales_attribute.span.fill(float3(1.0f));
  }

  for (const int i : IndexRange(std::min(scales.size(), positions.size()))) {
    scales_attribute.span[i] = float3(scales[i][0], scales[i][1], scales[i][2]);
  }

  scales_attribute.finish();

  bke::SpanAttributeWriter<math::Quaternion> orientations_attribute =
      attributes.lookup_or_add_for_write_only_span<math::Quaternion>("orientation",
                                                                     bke::AttrDomain::Point);

  if (orientations.size() < positions.size()) {
    orientations_attribute.span.fill(math::Quaternion::identity());
  }

  for (const int i : IndexRange(std::min(orientations.size(), positions.size()))) {
    orientations_attribute.span[i] = math::Quaternion(orientations[i].GetReal(),
                                                      orientations[i].GetImaginary()[0],
                                                      orientations[i].GetImaginary()[1],
                                                      orientations[i].GetImaginary()[2]);
  }

  orientations_attribute.finish();

  bke::SpanAttributeWriter<int> proto_indices_attribute =
      attributes.lookup_or_add_for_write_only_span<int>("proto_index", bke::AttrDomain::Point);

  if (proto_indices.size() < positions.size()) {
    proto_indices_attribute.span.fill(0);
  }

  for (const int i : IndexRange(std::min(proto_indices.size(), positions.size()))) {
    proto_indices_attribute.span[i] = proto_indices[i];
  }

  proto_indices_attribute.finish();

  bke::SpanAttributeWriter<bool> mask_attribute =
      attributes.lookup_or_add_for_write_only_span<bool>("mask", bke::AttrDomain::Point);

  if (mask.size() < positions.size()) {
    mask_attribute.span.fill(true);
  }

  for (const int i : IndexRange(std::min(mask.size(), positions.size()))) {
    mask_attribute.span[i] = mask[i];
  }

  mask_attribute.finish();

  BKE_pointcloud_nomain_to_pointcloud(point_cloud, base_point_cloud);

  ModifierData *md = BKE_modifier_new(eModifierType_Nodes);
  BLI_addtail(&object_->modifiers, md);
  NodesModifierData &nmd = *reinterpret_cast<NodesModifierData *>(md);
  nmd.node_group = ntreeAddTree(bmain, "Instances", "GeometryNodeTree");

  bNodeTree *ntree = nmd.node_group;

  ntree->tree_interface.add_socket("Geometry",
                                   "",
                                   "NodeSocketGeometry",
                                   NODE_INTERFACE_SOCKET_INPUT | NODE_INTERFACE_SOCKET_OUTPUT,
                                   nullptr);
  bNode *group_input = nodeAddStaticNode(nullptr, ntree, NODE_GROUP_INPUT);
  group_input->locx = -400.0f;
  bNode *group_output = nodeAddStaticNode(nullptr, ntree, NODE_GROUP_OUTPUT);
  group_output->locx = 500.0f;
  group_output->flag |= NODE_DO_OUTPUT;

  bNode *instance_on_points_node = nodeAddStaticNode(nullptr, ntree, GEO_NODE_INSTANCE_ON_POINTS);
  instance_on_points_node->locx = 300.0f;
  bNodeSocket *socket = nodeFindSocket(instance_on_points_node, SOCK_IN, "Pick Instance");
  socket->default_value_typed<bNodeSocketValueBoolean>()->value = true;

  bNode *mask_attrib_node = add_input_named_attrib_node(ntree, "mask", CD_PROP_BOOL);
  mask_attrib_node->locx = 100.0f;
  mask_attrib_node->locy = -100.0f;

  bNode *collection_info_node = nodeAddStaticNode(nullptr, ntree, GEO_NODE_COLLECTION_INFO);
  collection_info_node->locx = 100.0f;
  collection_info_node->locy = -300.0f;
  socket = nodeFindSocket(collection_info_node, SOCK_IN, "Separate Children");
  socket->default_value_typed<bNodeSocketValueBoolean>()->value = true;

  bNode *indices_attrib_node = add_input_named_attrib_node(ntree, "proto_index", CD_PROP_INT32);
  indices_attrib_node->locx = 100.0f;
  indices_attrib_node->locy = -500.0f;

  bNode *rotation_attrib_node = add_input_named_attrib_node(
      ntree, "orientation", CD_PROP_QUATERNION);
  rotation_attrib_node->locx = 100.0f;
  rotation_attrib_node->locy = -700.0f;

  bNode *scale_attrib_node = add_input_named_attrib_node(ntree, "scale", CD_PROP_FLOAT3);
  scale_attrib_node->locx = 100.0f;
  scale_attrib_node->locy = -900.0f;

  nodeAddLink(ntree,
              group_input,
              static_cast<bNodeSocket *>(group_input->outputs.first),
              instance_on_points_node,
              nodeFindSocket(instance_on_points_node, SOCK_IN, "Points"));

  nodeAddLink(ntree,
              mask_attrib_node,
              nodeFindSocket(mask_attrib_node, SOCK_OUT, "Attribute"),
              instance_on_points_node,
              nodeFindSocket(instance_on_points_node, SOCK_IN, "Selection"));

  nodeAddLink(ntree,
              indices_attrib_node,
              nodeFindSocket(indices_attrib_node, SOCK_OUT, "Attribute"),
              instance_on_points_node,
              nodeFindSocket(instance_on_points_node, SOCK_IN, "Instance Index"));

  nodeAddLink(ntree,
              scale_attrib_node,
              nodeFindSocket(scale_attrib_node, SOCK_OUT, "Attribute"),
              instance_on_points_node,
              nodeFindSocket(instance_on_points_node, SOCK_IN, "Scale"));

  nodeAddLink(ntree,
              rotation_attrib_node,
              nodeFindSocket(rotation_attrib_node, SOCK_OUT, "Attribute"),
              instance_on_points_node,
              nodeFindSocket(instance_on_points_node, SOCK_IN, "Rotation"));

  nodeAddLink(ntree,
              collection_info_node,
              nodeFindSocket(collection_info_node, SOCK_OUT, "Instances"),
              instance_on_points_node,
              nodeFindSocket(instance_on_points_node, SOCK_IN, "Instance"));

  nodeAddLink(ntree,
              instance_on_points_node,
              nodeFindSocket(instance_on_points_node, SOCK_OUT, "Instances"),
              group_output,
              static_cast<bNodeSocket *>(group_output->inputs.first));

  BKE_ntree_update_main_tree(bmain, ntree, nullptr);

  BKE_object_modifier_set_active(object_, md);

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

pxr::SdfPathVector USDPointInstancerReader::proto_paths() const
{
  pxr::SdfPathVector paths;

  pxr::UsdGeomPointInstancer point_instancer_prim(prim_);

  if (!point_instancer_prim) {
    return paths;
  }

  point_instancer_prim.GetPrototypesRel().GetTargets(&paths);

  return paths;
}

void USDPointInstancerReader::set_collection(Main *bmain, Collection &coll)
{
  /* create_object() should have been called already. */
  BLI_assert(object_);

  ModifierData *md = BKE_modifiers_findby_type(this->object_, eModifierType_Nodes);
  if (!md) {
    BLI_assert_unreachable();
    return;
  }

  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);

  bNodeTree *ntree = nmd->node_group;
  if (!ntree) {
    BLI_assert_unreachable();
    return;
  }

  bNode *collection_node = nodeFindNodebyName(ntree, "Collection Info");
  if (!collection_node) {
    BLI_assert_unreachable();
    return;
  }

  bNodeSocket *sock = nodeFindSocket(collection_node, SOCK_IN, "Collection");
  if (!sock) {
    BLI_assert_unreachable();
    return;
  }

  bNodeSocketValueCollection *socket_data = static_cast<bNodeSocketValueCollection *>(
      sock->default_value);

  if (socket_data->value != &coll) {
    socket_data->value = &coll;
    BKE_ntree_update_main_tree(bmain, ntree, nullptr);
  }
}

}  // namespace blender::io::usd
