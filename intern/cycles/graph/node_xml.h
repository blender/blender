/*
 * Copyright 2011-2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "graph/node.h"

#include "util/util_map.h"
#include "util/util_string.h"
#include "util/util_xml.h"

CCL_NAMESPACE_BEGIN

struct XMLReader {
	map<ustring, Node*> node_map;
};

void xml_read_node(XMLReader& reader, Node *node, xml_node xml_node);
xml_node xml_write_node(Node *node, xml_node xml_root);

CCL_NAMESPACE_END
