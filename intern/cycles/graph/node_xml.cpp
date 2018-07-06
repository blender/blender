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

#include "graph/node_xml.h"

#include "util/util_foreach.h"
#include "util/util_string.h"
#include "util/util_transform.h"

CCL_NAMESPACE_BEGIN

static bool xml_read_boolean(const char *value)
{
	return string_iequals(value, "true") || (atoi(value) != 0);
}

static const char *xml_write_boolean(bool value)
{
	return (value) ? "true" : "false";
}

template<int VECTOR_SIZE, typename T>
static void xml_read_float_array(T& value, xml_attribute attr)
{
	vector<string> tokens;
	string_split(tokens, attr.value());

	if(tokens.size() % VECTOR_SIZE != 0) {
		return;
	}

	value.resize(tokens.size() / VECTOR_SIZE);
	for(size_t i = 0; i < value.size(); i++) {
		float *value_float = (float*)&value[i];

		for(size_t j = 0; j < VECTOR_SIZE; j++)
			value_float[j] = (float)atof(tokens[i * VECTOR_SIZE + j].c_str());
	}
}

void xml_read_node(XMLReader& reader, Node *node, xml_node xml_node)
{
	xml_attribute name_attr = xml_node.attribute("name");
	if(name_attr) {
		node->name = ustring(name_attr.value());
	}

	foreach(const SocketType& socket, node->type->inputs) {
		if(socket.type == SocketType::CLOSURE || socket.type == SocketType::UNDEFINED) {
			continue;
		}
		if(socket.flags & SocketType::INTERNAL) {
			continue;
		}

		xml_attribute attr = xml_node.attribute(socket.name.c_str());

		if(!attr) {
			continue;
		}

		switch(socket.type)
		{
			case SocketType::BOOLEAN:
			{
				node->set(socket, xml_read_boolean(attr.value()));
				break;
			}
			case SocketType::BOOLEAN_ARRAY:
			{
				vector<string> tokens;
				string_split(tokens, attr.value());

				array<bool> value;
				value.resize(tokens.size());
				for(size_t i = 0; i < value.size(); i++)
					value[i] = xml_read_boolean(tokens[i].c_str());
				node->set(socket, value);
				break;
			}
			case SocketType::FLOAT:
			{
				node->set(socket, (float)atof(attr.value()));
				break;
			}
			case SocketType::FLOAT_ARRAY:
			{
				array<float> value;
				xml_read_float_array<1>(value, attr);
				node->set(socket, value);
				break;
			}
			case SocketType::INT:
			{
				node->set(socket, (int)atoi(attr.value()));
				break;
			}
			case SocketType::UINT:
			{
				node->set(socket, (uint)atoi(attr.value()));
				break;
			}
			case SocketType::INT_ARRAY:
			{
				vector<string> tokens;
				string_split(tokens, attr.value());

				array<int> value;
				value.resize(tokens.size());
				for(size_t i = 0; i < value.size(); i++) {
					value[i] = (int)atoi(attr.value());
				}
				node->set(socket, value);
				break;
			}
			case SocketType::COLOR:
			case SocketType::VECTOR:
			case SocketType::POINT:
			case SocketType::NORMAL:
			{
				array<float3> value;
				xml_read_float_array<3>(value, attr);
				if(value.size() == 1) {
					node->set(socket, value[0]);
				}
				break;
			}
			case SocketType::COLOR_ARRAY:
			case SocketType::VECTOR_ARRAY:
			case SocketType::POINT_ARRAY:
			case SocketType::NORMAL_ARRAY:
			{
				array<float3> value;
				xml_read_float_array<3>(value, attr);
				node->set(socket, value);
				break;
			}
			case SocketType::POINT2:
			{
				array<float2> value;
				xml_read_float_array<2>(value, attr);
				if(value.size() == 1) {
					node->set(socket, value[0]);
				}
				break;
			}
			case SocketType::POINT2_ARRAY:
			{
				array<float2> value;
				xml_read_float_array<2>(value, attr);
				node->set(socket, value);
				break;
			}
			case SocketType::STRING:
			{
				node->set(socket, attr.value());
				break;
			}
			case SocketType::ENUM:
			{
				ustring value(attr.value());
				if(socket.enum_values->exists(value)) {
					node->set(socket, value);
				}
				else {
					fprintf(stderr, "Unknown value \"%s\" for attribute \"%s\".\n", value.c_str(), socket.name.c_str());
				}
				break;
			}
			case SocketType::STRING_ARRAY:
			{
				vector<string> tokens;
				string_split(tokens, attr.value());

				array<ustring> value;
				value.resize(tokens.size());
				for(size_t i = 0; i < value.size(); i++) {
					value[i] = ustring(tokens[i]);
				}
				node->set(socket, value);
				break;
			}
			case SocketType::TRANSFORM:
			{
				array<Transform> value;
				xml_read_float_array<12>(value, attr);
				if(value.size() == 1) {
					node->set(socket, value[0]);
				}
				break;
			}
			case SocketType::TRANSFORM_ARRAY:
			{
				array<Transform> value;
				xml_read_float_array<12>(value, attr);
				node->set(socket, value);
				break;
			}
			case SocketType::NODE:
			{
				ustring value(attr.value());
				map<ustring, Node*>::iterator it = reader.node_map.find(value);
				if(it != reader.node_map.end())
				{
					Node *value_node = it->second;
					if(value_node->type == *(socket.node_type))
						node->set(socket, it->second);
				}
				break;
			}
			case SocketType::NODE_ARRAY:
			{
				vector<string> tokens;
				string_split(tokens, attr.value());

				array<Node*> value;
				value.resize(tokens.size());
				for(size_t i = 0; i < value.size(); i++)
				{
					map<ustring, Node*>::iterator it = reader.node_map.find(ustring(tokens[i]));
					if(it != reader.node_map.end())
					{
						Node *value_node = it->second;
						value[i] = (value_node->type == *(socket.node_type)) ? value_node : NULL;
					}
					else
					{
						value[i] = NULL;
					}
				}
				node->set(socket, value);
				break;
			}
			case SocketType::CLOSURE:
			case SocketType::UNDEFINED:
				break;
		}
	}

	if(node->name)
		reader.node_map[node->name] = node;
}

xml_node xml_write_node(Node *node, xml_node xml_root)
{
	xml_node xml_node = xml_root.append_child(node->type->name.c_str());

	xml_node.append_attribute("name") = node->name.c_str();

	foreach(const SocketType& socket, node->type->inputs) {
		if(socket.type == SocketType::CLOSURE || socket.type == SocketType::UNDEFINED) {
			continue;
		}
		if(socket.flags & SocketType::INTERNAL) {
			continue;
		}
		if(node->has_default_value(socket)) {
			continue;
		}

		xml_attribute attr = xml_node.append_attribute(socket.name.c_str());

		switch(socket.type)
		{
			case SocketType::BOOLEAN:
			{
				attr = xml_write_boolean(node->get_bool(socket));
				break;
			}
			case SocketType::BOOLEAN_ARRAY:
			{
				std::stringstream ss;
				const array<bool>& value = node->get_bool_array(socket);
				for(size_t i = 0; i < value.size(); i++) {
					ss << xml_write_boolean(value[i]);
					if(i != value.size() - 1)
						ss << " ";
				}
				attr = ss.str().c_str();
				break;
			}
			case SocketType::FLOAT:
			{
				attr = (double)node->get_float(socket);
				break;
			}
			case SocketType::FLOAT_ARRAY:
			{
				std::stringstream ss;
				const array<float>& value = node->get_float_array(socket);
				for(size_t i = 0; i < value.size(); i++) {
					ss << value[i];
					if(i != value.size() - 1) {
						ss << " ";
					}
				}
				attr = ss.str().c_str();
				break;
			}
			case SocketType::INT:
			{
				attr = node->get_int(socket);
				break;
			}
			case SocketType::UINT:
			{
				attr = node->get_uint(socket);
				break;
			}
			case SocketType::INT_ARRAY:
			{
				std::stringstream ss;
				const array<int>& value = node->get_int_array(socket);
				for(size_t i = 0; i < value.size(); i++) {
					ss << value[i];
					if(i != value.size() - 1) {
						ss << " ";
					}
				}
				attr = ss.str().c_str();
				break;
			}
			case SocketType::COLOR:
			case SocketType::VECTOR:
			case SocketType::POINT:
			case SocketType::NORMAL:
			{
				float3 value = node->get_float3(socket);
				attr = string_printf("%g %g %g", (double)value.x, (double)value.y, (double)value.z).c_str();
				break;
			}
			case SocketType::COLOR_ARRAY:
			case SocketType::VECTOR_ARRAY:
			case SocketType::POINT_ARRAY:
			case SocketType::NORMAL_ARRAY:
			{
				std::stringstream ss;
				const array<float3>& value = node->get_float3_array(socket);
				for(size_t i = 0; i < value.size(); i++) {
					ss << string_printf("%g %g %g", (double)value[i].x, (double)value[i].y, (double)value[i].z);
					if(i != value.size() - 1) {
						ss << " ";
					}
				}
				attr = ss.str().c_str();
				break;
			}
			case SocketType::POINT2:
			{
				float2 value = node->get_float2(socket);
				attr = string_printf("%g %g", (double)value.x, (double)value.y).c_str();
				break;
			}
			case SocketType::POINT2_ARRAY:
			{
				std::stringstream ss;
				const array<float2>& value = node->get_float2_array(socket);
				for(size_t i = 0; i < value.size(); i++) {
					ss << string_printf("%g %g", (double)value[i].x, (double)value[i].y);
					if(i != value.size() - 1) {
						ss << " ";
					}
				}
				attr = ss.str().c_str();
				break;
			}
			case SocketType::STRING:
			case SocketType::ENUM:
			{
				attr = node->get_string(socket).c_str();
				break;
			}
			case SocketType::STRING_ARRAY:
			{
				std::stringstream ss;
				const array<ustring>& value = node->get_string_array(socket);
				for(size_t i = 0; i < value.size(); i++) {
					ss << value[i];
					if(i != value.size() - 1) {
						ss << " ";
					}
				}
				attr = ss.str().c_str();
				break;
			}
			case SocketType::TRANSFORM:
			{
				Transform tfm = node->get_transform(socket);
				std::stringstream ss;
				for(int i = 0; i < 3; i++) {
					ss << string_printf("%g %g %g %g ", (double)tfm[i][0], (double)tfm[i][1], (double)tfm[i][2], (double)tfm[i][3]);
				}
				ss << string_printf("%g %g %g %g", 0.0, 0.0, 0.0, 1.0);
				attr = ss.str().c_str();
				break;
			}
			case SocketType::TRANSFORM_ARRAY:
			{
				std::stringstream ss;
				const array<Transform>& value = node->get_transform_array(socket);
				for(size_t j = 0; j < value.size(); j++) {
					const Transform& tfm = value[j];

					for(int i = 0; i < 3; i++) {
						ss << string_printf("%g %g %g %g ", (double)tfm[i][0], (double)tfm[i][1], (double)tfm[i][2], (double)tfm[i][3]);
					}
					ss << string_printf("%g %g %g %g", 0.0, 0.0, 0.0, 1.0);
					if(j != value.size() - 1) {
						ss << " ";
					}
				}
				attr = ss.str().c_str();
				break;
			}
			case SocketType::NODE:
			{
				Node *value = node->get_node(socket);
				if(value) {
					attr = value->name.c_str();
				}
				break;
			}
			case SocketType::NODE_ARRAY:
			{
				std::stringstream ss;
				const array<Node*>& value = node->get_node_array(socket);
				for(size_t i = 0; i < value.size(); i++) {
					if(value[i]) {
						ss << value[i]->name.c_str();
					}
					if(i != value.size() - 1) {
						ss << " ";
					}
				}
				attr = ss.str().c_str();
				break;
			}
			case SocketType::CLOSURE:
			case SocketType::UNDEFINED:
				break;
		}
	}

	return xml_node;
}

CCL_NAMESPACE_END
