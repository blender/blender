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
 * limitations under the License.
 */

#ifndef __SVM_H__
#define __SVM_H__

#include "render/attribute.h"
#include "render/graph.h"
#include "render/shader.h"

#include "util/util_array.h"
#include "util/util_set.h"
#include "util/util_string.h"
#include "util/util_thread.h"

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

  void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_free(Device *device, DeviceScene *dscene, Scene *scene);

 protected:
  void device_update_shader(Scene *scene,
                            Shader *shader,
                            Progress *progress,
                            array<int4> *svm_nodes);
};

/* Graph Compiler */

class SVMCompiler {
 public:
  struct Summary {
    Summary();

    /* Number of SVM nodes shader was compiled into. */
    int num_svm_nodes;

    /* Peak stack usage during shader evaluation. */
    int peak_stack_usage;

    /* Time spent on surface graph finalization. */
    double time_finalize;

    /* Time spent on generating SVM nodes for surface shader. */
    double time_generate_surface;

    /* Time spent on generating SVM nodes for bump shader. */
    double time_generate_bump;

    /* Time spent on generating SVM nodes for volume shader. */
    double time_generate_volume;

    /* Time spent on generating SVM nodes for displacement shader. */
    double time_generate_displacement;

    /* Total time spent on all routines. */
    double time_total;

    /* A full multiline description of the state of the compiler after
     * compilation.
     */
    string full_report() const;
  };

  SVMCompiler(ShaderManager *shader_manager,
              ImageManager *image_manager,
              LightManager *light_manager);
  void compile(
      Scene *scene, Shader *shader, array<int4> &svm_nodes, int index, Summary *summary = NULL);

  int stack_assign(ShaderOutput *output);
  int stack_assign(ShaderInput *input);
  int stack_assign_if_linked(ShaderInput *input);
  int stack_assign_if_linked(ShaderOutput *output);
  int stack_find_offset(int size);
  int stack_find_offset(SocketType::Type type);
  void stack_clear_offset(SocketType::Type type, int offset);
  void stack_link(ShaderInput *input, ShaderOutput *output);

  void add_node(ShaderNodeType type, int a = 0, int b = 0, int c = 0);
  void add_node(int a = 0, int b = 0, int c = 0, int d = 0);
  void add_node(ShaderNodeType type, const float3 &f);
  void add_node(const float4 &f);
  uint attribute(ustring name);
  uint attribute(AttributeStandard std);
  uint attribute_standard(ustring name);
  uint encode_uchar4(uint x, uint y = 0, uint z = 0, uint w = 0);
  uint closure_mix_weight_offset()
  {
    return mix_weight_offset;
  }

  ShaderType output_type()
  {
    return current_type;
  }

  ImageManager *image_manager;
  ShaderManager *shader_manager;
  LightManager *light_manager;
  bool background;

 protected:
  /* stack */
  struct Stack {
    Stack()
    {
      memset(users, 0, sizeof(users));
    }
    Stack(const Stack &other)
    {
      memcpy(users, other.users, sizeof(users));
    }
    Stack &operator=(const Stack &other)
    {
      memcpy(users, other.users, sizeof(users));
      return *this;
    }

    bool empty()
    {
      for (int i = 0; i < SVM_STACK_SIZE; i++)
        if (users[i])
          return false;

      return true;
    }

    void print()
    {
      printf("stack <");

      for (int i = 0; i < SVM_STACK_SIZE; i++)
        printf((users[i]) ? "*" : " ");

      printf(">\n");
    }

    int users[SVM_STACK_SIZE];
  };

  /* Global state of the compiler accessible from the compilation routines. */
  struct CompilerState {
    explicit CompilerState(ShaderGraph *graph);

    /* ** Global state, used by various compilation steps. ** */

    /* Set of nodes which were already compiled. */
    ShaderNodeSet nodes_done;

    /* Set of closures which were already compiled. */
    ShaderNodeSet closure_done;

    /* ** SVM nodes generation state ** */

    /* Flag whether the node with corresponding ID was already compiled or
     * not. Array element with index i corresponds to a node with such if.
     *
     * TODO(sergey): This is actually a copy of nodes_done just in another
     * notation. We can de-duplicate this storage actually after switching
     * all areas to use this flags array.
     */
    vector<bool> nodes_done_flag;
  };

  void stack_clear_temporary(ShaderNode *node);
  int stack_size(SocketType::Type type);
  void stack_clear_users(ShaderNode *node, ShaderNodeSet &done);

  /* single closure */
  void find_dependencies(ShaderNodeSet &dependencies,
                         const ShaderNodeSet &done,
                         ShaderInput *input,
                         ShaderNode *skip_node = NULL);
  void generate_node(ShaderNode *node, ShaderNodeSet &done);
  void generate_closure_node(ShaderNode *node, CompilerState *state);
  void generated_shared_closure_nodes(ShaderNode *root_node,
                                      ShaderNode *node,
                                      CompilerState *state,
                                      const ShaderNodeSet &shared);
  void generate_svm_nodes(const ShaderNodeSet &nodes, CompilerState *state);

  /* multi closure */
  void generate_multi_closure(ShaderNode *root_node, ShaderNode *node, CompilerState *state);

  /* compile */
  void compile_type(Shader *shader, ShaderGraph *graph, ShaderType type);

  array<int4> current_svm_nodes;
  ShaderType current_type;
  Shader *current_shader;
  ShaderGraph *current_graph;
  Stack active_stack;
  int max_stack_use;
  uint mix_weight_offset;
  bool compile_failed;
};

CCL_NAMESPACE_END

#endif /* __SVM_H__ */
