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

#include "testing/testing.h"
#include "testing/mock_log.h"

#include "render/graph.h"
#include "render/scene.h"
#include "render/nodes.h"
#include "util/util_logging.h"
#include "util/util_string.h"
#include "util/util_vector.h"

using testing::AnyNumber;
using testing::HasSubstr;
using testing::ScopedMockLog;
using testing::_;

CCL_NAMESPACE_BEGIN

namespace {

template<typename T>
class ShaderNodeBuilder {
public:
	ShaderNodeBuilder(const string& name)
	  : name_(name)
	{
		node_ = new T();
	}

	const string& name() const {
		return name_;
	}

	ShaderNode *node() const {
		return node_;
	}

	template<typename V>
	ShaderNodeBuilder& set(const string& input_name, V value)
	{
		ShaderInput *input_socket = node_->input(input_name.c_str());
		EXPECT_NE((void*)NULL, input_socket);
		input_socket->set(value);
		return *this;
	}

protected:
	string name_;
	ShaderNode *node_;
};

class ShaderGraphBuilder {
public:
	explicit ShaderGraphBuilder(ShaderGraph *graph)
	  : graph_(graph)
	{
	}

	ShaderNode *find_node(const string& name)
	{
		map<string, ShaderNode *>::iterator it = node_map_.find(name);
		if(it == node_map_.end()) {
			return NULL;
		}
		return it->second;
	}

	template<typename T>
	ShaderGraphBuilder& add_node(const T& node)
	{
		EXPECT_EQ(NULL, find_node(node.name()));
		graph_->add(node.node());
		node_map_[node.name()] = node.node();
		return *this;
	}

	ShaderGraphBuilder& add_connection(const string& from,
	                                   const string& to)
	{
		vector<string> tokens_from, tokens_to;
		string_split(tokens_from, from, "::");
		string_split(tokens_to, to, "::");
		EXPECT_EQ(2, tokens_from.size());
		EXPECT_EQ(2, tokens_to.size());
		ShaderNode *node_from = find_node(tokens_from[0]),
		           *node_to = find_node(tokens_to[0]);
		EXPECT_NE((void*)NULL, node_from);
		EXPECT_NE((void*)NULL, node_to);
		EXPECT_NE(node_from, node_to);
		ShaderOutput *socket_from = node_from->output(tokens_from[1].c_str());
		ShaderInput *socket_to = node_to->input(tokens_to[1].c_str());
		EXPECT_NE((void*)NULL, socket_from);
		EXPECT_NE((void*)NULL, socket_to);
		graph_->connect(socket_from, socket_to);
		return *this;
	}

protected:
	ShaderGraph *graph_;
	map<string, ShaderNode *> node_map_;
};

}  // namespace

#define DEFINE_COMMON_VARIABLES(builder_name, mock_log_name) \
	util_logging_start(); \
	util_logging_verbosity_set(1); \
	ScopedMockLog mock_log_name; \
	DeviceInfo device_info; \
	SceneParams scene_params; \
	Scene scene(scene_params, device_info); \
	ShaderGraph graph; \
	ShaderGraphBuilder builder(&graph); \

TEST(render_graph, constant_fold_rgb_to_bw)
{
	DEFINE_COMMON_VARIABLES(builder, log);

	EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
	EXPECT_CALL(log, Log(google::INFO, _,
	                     HasSubstr("Replacing rgb_to_bw with constant 0.8.")));

	builder
		.add_node(ShaderNodeBuilder<OutputNode>("OutputNode"))
		.add_node(ShaderNodeBuilder<EmissionNode>("EmissionNode"))
		.add_node(ShaderNodeBuilder<RGBToBWNode>("RGBToBWNodeNode")
		          .set("Color", make_float3(0.8f, 0.8f, 0.8f)))
		.add_connection("RGBToBWNodeNode::Val", "EmissionNode::Color")
		.add_connection("EmissionNode::Emission", "OutputNode::Surface");

	graph.finalize(&scene);
}

CCL_NAMESPACE_END
