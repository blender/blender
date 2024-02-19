/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "RNA_prototypes.h"

#include "NOD_zone_socket_items.hh"

#include "BKE_node.hh"

#include "BLO_read_write.hh"

namespace blender::nodes {

/* Defined here to avoid including the relevant headers in the header. */

StructRNA *SimulationItemsAccessor::item_srna = &RNA_SimulationStateItem;
int SimulationItemsAccessor::node_type = GEO_NODE_SIMULATION_OUTPUT;

void SimulationItemsAccessor::blend_write(BlendWriter *writer, const bNode &node)
{
  const auto &storage = *static_cast<const NodeGeometrySimulationOutput *>(node.storage);
  BLO_write_struct_array(writer, NodeSimulationItem, storage.items_num, storage.items);
  for (const NodeSimulationItem &item : Span(storage.items, storage.items_num)) {
    BLO_write_string(writer, item.name);
  }
}

void SimulationItemsAccessor::blend_read_data(BlendDataReader *reader, bNode &node)
{
  auto &storage = *static_cast<NodeGeometrySimulationOutput *>(node.storage);
  BLO_read_data_address(reader, &storage.items);
  for (const NodeSimulationItem &item : Span(storage.items, storage.items_num)) {
    BLO_read_data_address(reader, &item.name);
  }
}

StructRNA *RepeatItemsAccessor::item_srna = &RNA_RepeatItem;
int RepeatItemsAccessor::node_type = GEO_NODE_REPEAT_OUTPUT;

void RepeatItemsAccessor::blend_write(BlendWriter *writer, const bNode &node)
{
  const auto &storage = *static_cast<const NodeGeometryRepeatOutput *>(node.storage);
  BLO_write_struct_array(writer, NodeRepeatItem, storage.items_num, storage.items);
  for (const NodeRepeatItem &item : Span(storage.items, storage.items_num)) {
    BLO_write_string(writer, item.name);
  }
}

void RepeatItemsAccessor::blend_read_data(BlendDataReader *reader, bNode &node)
{
  auto &storage = *static_cast<NodeGeometryRepeatOutput *>(node.storage);
  BLO_read_data_address(reader, &storage.items);
  for (const NodeRepeatItem &item : Span(storage.items, storage.items_num)) {
    BLO_read_data_address(reader, &item.name);
  }
}

StructRNA *IndexSwitchItemsAccessor ::item_srna = &RNA_IndexSwitchItem;
int IndexSwitchItemsAccessor::node_type = GEO_NODE_INDEX_SWITCH;

void IndexSwitchItemsAccessor::blend_write(BlendWriter *writer, const bNode &node)
{
  const auto &storage = *static_cast<const NodeIndexSwitch *>(node.storage);
  BLO_write_struct_array(writer, IndexSwitchItem, storage.items_num, storage.items);
}

void IndexSwitchItemsAccessor::blend_read_data(BlendDataReader *reader, bNode &node)
{
  auto &storage = *static_cast<NodeIndexSwitch *>(node.storage);
  BLO_read_data_address(reader, &storage.items);
}

StructRNA *BakeItemsAccessor::item_srna = &RNA_NodeGeometryBakeItem;
int BakeItemsAccessor::node_type = GEO_NODE_BAKE;

void BakeItemsAccessor::blend_write(BlendWriter *writer, const bNode &node)
{
  const auto &storage = *static_cast<const NodeGeometryBake *>(node.storage);
  BLO_write_struct_array(writer, NodeGeometryBakeItem, storage.items_num, storage.items);
  for (const NodeGeometryBakeItem &item : Span(storage.items, storage.items_num)) {
    BLO_write_string(writer, item.name);
  }
}

void BakeItemsAccessor::blend_read_data(BlendDataReader *reader, bNode &node)
{
  auto &storage = *static_cast<NodeGeometryBake *>(node.storage);
  BLO_read_data_address(reader, &storage.items);
  for (const NodeGeometryBakeItem &item : Span(storage.items, storage.items_num)) {
    BLO_read_data_address(reader, &item.name);
  }
}

}  // namespace blender::nodes
