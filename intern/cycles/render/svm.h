/*
 * Copyright 2011-2013 Blender Foundation
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
 * limitations under the License
 */

#ifndef __SVM_H__
#define __SVM_H__

#include "attribute.h"
#include "graph.h"
#include "shader.h"

#include "util_set.h"
#include "util_string.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class ImageManager;
class Scene;
class ShaderGraph;
class ShaderInput;
class ShaderNode;
class ShaderOutput;

/* Shader Manager */

class SVMShaderManager : public ShaderManager {
public:
	SVMShaderManager();
	~SVMShaderManager();

	void reset(Scene *scene);

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene, Scene *scene);
};

/* Graph Compiler */

class SVMCompiler {
public:
	SVMCompiler(ShaderManager *shader_manager, ImageManager *image_manager,
		bool use_multi_closure_);
	void compile(Shader *shader, vector<int4>& svm_nodes, int index);

	void stack_assign(ShaderOutput *output);
	void stack_assign(ShaderInput *input);
	int stack_find_offset(ShaderSocketType type);
	void stack_clear_offset(ShaderSocketType type, int offset);
	void stack_link(ShaderInput *input, ShaderOutput *output);

	void add_node(NodeType type, int a = 0, int b = 0, int c = 0);
	void add_node(int a = 0, int b = 0, int c = 0, int d = 0);
	void add_node(NodeType type, const float3& f);
	void add_node(const float4& f);
	void add_array(float4 *f, int num);
	uint attribute(ustring name);
	uint attribute(AttributeStandard std);
	uint encode_uchar4(uint x, uint y = 0, uint z = 0, uint w = 0);
	uint closure_mix_weight_offset() { return mix_weight_offset; }

	ShaderType output_type() { return current_type; }

	ImageManager *image_manager;
	ShaderManager *shader_manager;
	bool background;

protected:
	/* stack */
	struct Stack {
		Stack() { memset(users, 0, sizeof(users)); }
		Stack(const Stack& other) { memcpy(users, other.users, sizeof(users)); }
		Stack& operator=(const Stack& other) { memcpy(users, other.users, sizeof(users)); return *this; }

		bool empty()
		{
			for(int i = 0; i < SVM_STACK_SIZE; i++)
				if(users[i])
					return false;

			return true;
		}

		void print()
		{
			printf("stack <");

			for(int i = 0; i < SVM_STACK_SIZE; i++)
				printf((users[i])? "*": " ");

			printf(">\n");
		}

		int users[SVM_STACK_SIZE];
	};

	struct StackBackup {
		Stack stack;
		vector<int> offsets;
		set<ShaderNode*> done;
	};

	void stack_backup(StackBackup& backup, set<ShaderNode*>& done);
	void stack_restore(StackBackup& backup, set<ShaderNode*>& done);

	void stack_clear_temporary(ShaderNode *node);
	int stack_size(ShaderSocketType type);
	void stack_clear_users(ShaderNode *node, set<ShaderNode*>& done);

	bool node_skip_input(ShaderNode *node, ShaderInput *input);

	/* single closure */
	void find_dependencies(set<ShaderNode*>& dependencies, const set<ShaderNode*>& done, ShaderInput *input);
	void generate_svm_nodes(const set<ShaderNode*>& nodes, set<ShaderNode*>& done);
	void generate_closure(ShaderNode *node, set<ShaderNode*>& done);

	/* multi closure */
	void generate_multi_closure(ShaderNode *node, set<ShaderNode*>& done, set<ShaderNode*>& closure_done);

	/* compile */
	void compile_type(Shader *shader, ShaderGraph *graph, ShaderType type);

	vector<int4> svm_nodes;
	ShaderType current_type;
	Shader *current_shader;
	ShaderGraph *current_graph;
	Stack active_stack;
	int max_stack_use;
	uint mix_weight_offset;
	bool use_multi_closure;
	bool compile_failed;
};

CCL_NAMESPACE_END

#endif /* __SVM_H__ */

