/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "graph/node.h"

#include "util/map.h"
#include "util/string.h"
#include "util/xml.h"

CCL_NAMESPACE_BEGIN

struct XMLReader {
  map<ustring, Node *> node_map;
};

void xml_read_node(XMLReader &reader, Node *node, xml_node xml_node);
xml_node xml_write_node(Node *node, xml_node xml_root);

CCL_NAMESPACE_END
