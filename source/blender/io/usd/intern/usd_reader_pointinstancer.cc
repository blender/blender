/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_pointinstancer.hh"

#include "BKE_attribute.hh"
#include "BKE_geometry_set.hh"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_object.hh"
#include "BKE_pointcloud.hh"
#include "BLI_listbase.h"
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
  bNode *node = bke::node_add_static_node(nullptr, *ntree, GEO_NODE_INPUT_NAMED_ATTRIBUTE);
  auto *storage = reinterpret_cast<NodeGeometryInputNamedAttribute *>(node->storage);
  storage->data_type = prop_type;

  bNodeSocket *socket = bke::node_find_socket(*node, SOCK_IN, "Name");
  bNodeSocketValueString *str_value = static_cast<bNodeSocketValueString *>(socket->default_value);
  BLI_strncpy(str_value->value, name, MAX_NAME);
  return node;
}

void USDPointInstancerReader::create_object(Main *bmain)
{
  PointCloud *pointcloud = BKE_pointcloud_add(bmain, name_.c_str());
  this->object_ = BKE_object_add_only_object(bmain, OB_POINTCLOUD, name_.c_str());
  this->object_->data = pointcloud;
}

void USDPointInstancerReader::read_geometry(bke::GeometrySet &geometry_set,
                                            USDMeshReadParams params,
                                            const char ** /*r_err_str*/)
{
  pxr::VtArray<pxr::GfVec3f> usd_positions;
  pxr::VtArray<pxr::GfVec3f> usd_scales;
  pxr::VtArray<pxr::GfQuath> usd_orientations;
  pxr::VtArray<int> usd_proto_indices;
  std::vector<bool> usd_mask = point_instancer_prim_.ComputeMaskAtTime(params.motion_sample_time);

  point_instancer_prim_.GetPositionsAttr().Get(&usd_positions, params.motion_sample_time);
  point_instancer_prim_.GetScalesAttr().Get(&usd_scales, params.motion_sample_time);
  point_instancer_prim_.GetOrientationsAttr().Get(&usd_orientations, params.motion_sample_time);
  point_instancer_prim_.GetProtoIndicesAttr().Get(&usd_proto_indices, params.motion_sample_time);

  PointCloud *pointcloud = geometry_set.get_pointcloud_for_write();
  if (pointcloud->totpoint != usd_positions.size()) {
    /* Size changed so we must reallocate. */
    pointcloud = BKE_pointcloud_new_nomain(usd_positions.size());
  }

  MutableSpan<float3> point_positions = pointcloud->positions_for_write();
  point_positions.copy_from(Span(usd_positions.cdata(), usd_positions.size()).cast<float3>());

  bke::MutableAttributeAccessor attributes = pointcloud->attributes_for_write();

  bke::SpanAttributeWriter<float3> scales_attribute =
      attributes.lookup_or_add_for_write_only_span<float3>("scale", bke::AttrDomain::Point);

  /* Here and below, handle the case where instancing attributes are empty or
   * not of the expected size. */
  if (usd_scales.size() < usd_positions.size()) {
    scales_attribute.span.fill(float3(1.0f));
  }

  Span<pxr::GfVec3f> scales = Span(usd_scales.cdata(), usd_scales.size());
  for (const int i : IndexRange(std::min(usd_scales.size(), usd_positions.size()))) {
    scales_attribute.span[i] = float3(scales[i][0], scales[i][1], scales[i][2]);
  }

  scales_attribute.finish();

  bke::SpanAttributeWriter<math::Quaternion> orientations_attribute =
      attributes.lookup_or_add_for_write_only_span<math::Quaternion>("orientation",
                                                                     bke::AttrDomain::Point);

  if (usd_orientations.size() < usd_positions.size()) {
    orientations_attribute.span.fill(math::Quaternion::identity());
  }

  Span<pxr::GfQuath> orientations = Span(usd_orientations.cdata(), usd_orientations.size());
  for (const int i : IndexRange(std::min(usd_orientations.size(), usd_positions.size()))) {
    orientations_attribute.span[i] = math::Quaternion(orientations[i].GetReal(),
                                                      orientations[i].GetImaginary()[0],
                                                      orientations[i].GetImaginary()[1],
                                                      orientations[i].GetImaginary()[2]);
  }

  orientations_attribute.finish();

  bke::SpanAttributeWriter<int> proto_indices_attribute =
      attributes.lookup_or_add_for_write_only_span<int>("proto_index", bke::AttrDomain::Point);

  if (usd_proto_indices.size() < usd_positions.size()) {
    proto_indices_attribute.span.fill(0);
  }

  Span<int> proto_indices = Span(usd_proto_indices.cdata(), usd_proto_indices.size());
  for (const int i : IndexRange(std::min(usd_proto_indices.size(), usd_positions.size()))) {
    proto_indices_attribute.span[i] = proto_indices[i];
  }

  proto_indices_attribute.finish();

  bke::SpanAttributeWriter<bool> mask_attribute =
      attributes.lookup_or_add_for_write_only_span<bool>("mask", bke::AttrDomain::Point);

  if (usd_mask.size() < usd_positions.size()) {
    mask_attribute.span.fill(true);
  }

  for (const int i : IndexRange(std::min(usd_mask.size(), usd_positions.size()))) {
    mask_attribute.span[i] = usd_mask[i];
  }

  mask_attribute.finish();

  geometry_set.replace_pointcloud(pointcloud);
}

void USDPointInstancerReader::read_object_data(Main *bmain, const pxr::UsdTimeCode time)
{
  PointCloud *pointcloud = static_cast<PointCloud *>(object_->data);

  bke::GeometrySet geometry_set = bke::GeometrySet::from_pointcloud(
      pointcloud, bke::GeometryOwnershipType::Editable);

  const USDMeshReadParams params = create_mesh_read_params(time.GetValue(),
                                                           import_params_.mesh_read_flag);

  read_geometry(geometry_set, params, nullptr);

  PointCloud *read_pointcloud =
      geometry_set.get_component_for_write<bke::PointCloudComponent>().release();

  if (read_pointcloud != pointcloud) {
    BKE_pointcloud_nomain_to_pointcloud(read_pointcloud, pointcloud);
  }

  if (is_animated()) {
    /* If the point cloud has time-varying data, we add the cache modifier. */
    add_cache_modifier();
  }

  ModifierData *md = BKE_modifier_new(eModifierType_Nodes);
  BLI_addtail(&object_->modifiers, md);
  BKE_modifiers_persistent_uid_init(*object_, *md);

  NodesModifierData &nmd = *reinterpret_cast<NodesModifierData *>(md);
  nmd.node_group = bke::node_tree_add_tree(bmain, "Instances", "GeometryNodeTree");

  bNodeTree *ntree = nmd.node_group;

  ntree->tree_interface.add_socket(
      "Geometry", "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_OUTPUT, nullptr);
  ntree->tree_interface.add_socket(
      "Geometry", "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_INPUT, nullptr);
  bNode *group_input = bke::node_add_static_node(nullptr, *ntree, NODE_GROUP_INPUT);
  group_input->location[0] = -400.0f;
  bNode *group_output = bke::node_add_static_node(nullptr, *ntree, NODE_GROUP_OUTPUT);
  group_output->location[0] = 500.0f;
  group_output->flag |= NODE_DO_OUTPUT;

  bNode *instance_on_points_node = bke::node_add_static_node(
      nullptr, *ntree, GEO_NODE_INSTANCE_ON_POINTS);
  instance_on_points_node->location[0] = 300.0f;
  bNodeSocket *socket = bke::node_find_socket(*instance_on_points_node, SOCK_IN, "Pick Instance");
  socket->default_value_typed<bNodeSocketValueBoolean>()->value = true;

  bNode *mask_attrib_node = add_input_named_attrib_node(ntree, "mask", CD_PROP_BOOL);
  mask_attrib_node->location[0] = 100.0f;
  mask_attrib_node->location[1] = -100.0f;

  bNode *collection_info_node = bke::node_add_static_node(
      nullptr, *ntree, GEO_NODE_COLLECTION_INFO);
  collection_info_node->location[0] = 100.0f;
  collection_info_node->location[1] = -300.0f;
  socket = bke::node_find_socket(*collection_info_node, SOCK_IN, "Separate Children");
  socket->default_value_typed<bNodeSocketValueBoolean>()->value = true;

  bNode *indices_attrib_node = add_input_named_attrib_node(ntree, "proto_index", CD_PROP_INT32);
  indices_attrib_node->location[0] = 100.0f;
  indices_attrib_node->location[1] = -500.0f;

  bNode *rotation_attrib_node = add_input_named_attrib_node(
      ntree, "orientation", CD_PROP_QUATERNION);
  rotation_attrib_node->location[0] = 100.0f;
  rotation_attrib_node->location[1] = -700.0f;

  bNode *scale_attrib_node = add_input_named_attrib_node(ntree, "scale", CD_PROP_FLOAT3);
  scale_attrib_node->location[0] = 100.0f;
  scale_attrib_node->location[1] = -900.0f;

  bke::node_add_link(*ntree,
                     *group_input,
                     *static_cast<bNodeSocket *>(group_input->outputs.first),
                     *instance_on_points_node,
                     *bke::node_find_socket(*instance_on_points_node, SOCK_IN, "Points"));

  bke::node_add_link(*ntree,
                     *mask_attrib_node,
                     *bke::node_find_socket(*mask_attrib_node, SOCK_OUT, "Attribute"),
                     *instance_on_points_node,
                     *bke::node_find_socket(*instance_on_points_node, SOCK_IN, "Selection"));

  bke::node_add_link(*ntree,
                     *indices_attrib_node,
                     *bke::node_find_socket(*indices_attrib_node, SOCK_OUT, "Attribute"),
                     *instance_on_points_node,
                     *bke::node_find_socket(*instance_on_points_node, SOCK_IN, "Instance Index"));

  bke::node_add_link(*ntree,
                     *scale_attrib_node,
                     *bke::node_find_socket(*scale_attrib_node, SOCK_OUT, "Attribute"),
                     *instance_on_points_node,
                     *bke::node_find_socket(*instance_on_points_node, SOCK_IN, "Scale"));

  bke::node_add_link(*ntree,
                     *rotation_attrib_node,
                     *bke::node_find_socket(*rotation_attrib_node, SOCK_OUT, "Attribute"),
                     *instance_on_points_node,
                     *bke::node_find_socket(*instance_on_points_node, SOCK_IN, "Rotation"));

  bke::node_add_link(*ntree,
                     *collection_info_node,
                     *bke::node_find_socket(*collection_info_node, SOCK_OUT, "Instances"),
                     *instance_on_points_node,
                     *bke::node_find_socket(*instance_on_points_node, SOCK_IN, "Instance"));

  bke::node_add_link(*ntree,
                     *instance_on_points_node,
                     *bke::node_find_socket(*instance_on_points_node, SOCK_OUT, "Instances"),
                     *group_output,
                     *static_cast<bNodeSocket *>(group_output->inputs.first));

  BKE_ntree_update_after_single_tree_change(*bmain, *ntree);

  BKE_object_modifier_set_active(object_, md);

  USDXformReader::read_object_data(bmain, time);
}

pxr::SdfPathVector USDPointInstancerReader::proto_paths() const
{
  pxr::SdfPathVector paths;
  point_instancer_prim_.GetPrototypesRel().GetTargets(&paths);

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

  bNode *collection_node = bke::node_find_node_by_name(*ntree, "Collection Info");
  if (!collection_node) {
    BLI_assert_unreachable();
    return;
  }

  bNodeSocket *sock = bke::node_find_socket(*collection_node, SOCK_IN, "Collection");
  if (!sock) {
    BLI_assert_unreachable();
    return;
  }

  bNodeSocketValueCollection *socket_data = static_cast<bNodeSocketValueCollection *>(
      sock->default_value);

  if (socket_data->value != &coll) {
    socket_data->value = &coll;
    BKE_ntree_update_tag_socket_property(ntree, sock);
    BKE_ntree_update_after_single_tree_change(*bmain, *ntree);
  }
}

bool USDPointInstancerReader::is_animated() const
{
  bool is_animated = false;
  is_animated |= point_instancer_prim_.GetPositionsAttr().ValueMightBeTimeVarying();
  is_animated |= point_instancer_prim_.GetScalesAttr().ValueMightBeTimeVarying();
  is_animated |= point_instancer_prim_.GetOrientationsAttr().ValueMightBeTimeVarying();
  is_animated |= point_instancer_prim_.GetProtoIndicesAttr().ValueMightBeTimeVarying();

  return is_animated;
}

}  // namespace blender::io::usd
