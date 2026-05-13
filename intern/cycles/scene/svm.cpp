/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>

#include "device/device.h"

#include "scene/background.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/stats.h"
#include "scene/svm.h"

#include "kernel/svm/node_types.h"

#include "util/log.h"
#include "util/math_float3.h"
#include "util/progress.h"
#include "util/queue.h"
#include "util/task.h"

CCL_NAMESPACE_BEGIN

/* Shader Manager */

SVMShaderManager::SVMShaderManager() = default;

SVMShaderManager::~SVMShaderManager() = default;

void SVMShaderManager::device_update_shader(Scene *scene,
                                            Shader *shader,
                                            Progress &progress,
                                            array<int> *svm_nodes)
{
  if (progress.get_cancel()) {
    return;
  }
  assert(shader->graph);

  SVMCompiler::Summary summary;
  SVMCompiler compiler(scene, progress);
  compiler.background = (shader == scene->background->get_shader(scene));
  compiler.compile(shader, *svm_nodes, 0, &summary);

  LOG_DEBUG << "Compilation summary:\n"
            << "Shader name: " << shader->name << "\n"
            << summary.full_report();
}

void SVMShaderManager::device_update_specific(Device *device,
                                              DeviceScene *dscene,
                                              Scene *scene,
                                              Progress &progress)
{
  if (!need_update()) {
    return;
  }

  const scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->svm.times.add_entry({"device_update", time});
    }
  });

  const int num_shaders = scene->shaders.size();

  LOG_INFO << "Total " << num_shaders << " shaders.";

  const double start_time = time_dt();

  /* test if we need to update */
  device_free(device, dscene, scene);

  /* Build all shaders. */
  TaskPool task_pool;
  vector<array<int>> shader_svm_nodes(num_shaders);
  for (int i = 0; i < num_shaders; i++) {
    task_pool.push([this, scene, &progress, &shader_svm_nodes, i] {
      device_update_shader(scene, scene->shaders[i], progress, &shader_svm_nodes[i]);
    });
  }
  task_pool.wait_work();

  if (progress.get_cancel()) {
    return;
  }

  /* The global node list contains a jump table (one jump node per shader)
   * followed by the nodes of all shaders. */
  const int jump_node_size = 1 + sizeof(SVMNodeShaderJump) / sizeof(int);
  int svm_nodes_size = num_shaders * jump_node_size;
  for (int i = 0; i < num_shaders; i++) {
    /* Since we're not copying the local jump node, the size ends up lower. */
    svm_nodes_size += shader_svm_nodes[i].size() - jump_node_size;
  }

  int *svm_nodes = dscene->svm_nodes.alloc(svm_nodes_size);

  int node_offset = num_shaders * jump_node_size;
  for (int i = 0; i < num_shaders; i++) {
    Shader *shader = scene->shaders[i];

    shader->clear_modified();
    if (shader->emission_sampling != EMISSION_SAMPLING_NONE) {
      scene->light_manager->tag_update(scene, LightManager::SHADER_COMPILED);
    }

    /* Update the global jump table.
     * Each compiled shader starts with a jump node that has offsets local
     * to the shader, so copy those and add the offset into the global node list. */
    const int base = shader->id * jump_node_size;
    svm_nodes[base + 0] = NODE_SHADER_JUMP;
    *reinterpret_cast<SVMNodeShaderJump *>(&svm_nodes[base + 1]) = SVMNodeShaderJump{
        .offset_surface = shader_svm_nodes[i][1] - jump_node_size + node_offset,
        .offset_volume = shader_svm_nodes[i][2] - jump_node_size + node_offset,
        .offset_displacement = shader_svm_nodes[i][3] - jump_node_size + node_offset};
    node_offset += shader_svm_nodes[i].size() - jump_node_size;
  }

  /* Copy the nodes of each shader into the correct location. */
  int *dst = svm_nodes + num_shaders * jump_node_size;
  for (int i = 0; i < num_shaders; i++) {
    const int shader_size = shader_svm_nodes[i].size() - jump_node_size;

    std::copy_n(&shader_svm_nodes[i][jump_node_size], shader_size, dst);
    dst += shader_size;
  }

  if (progress.get_cancel()) {
    return;
  }

  device_update_common(device, dscene, scene, progress);

  update_flags = UPDATE_NONE;

  LOG_INFO << "Shader manager updated " << num_shaders << " shaders in " << time_dt() - start_time
           << " seconds.";
}

void SVMShaderManager::device_free(Device *device, DeviceScene *dscene, Scene *scene)
{
  device_free_common(device, dscene, scene);

  dscene->svm_nodes.free();
}

/* Graph Compiler */

SVMCompiler::SVMCompiler(Scene *scene, Progress &progress) : scene(scene), progress(progress)
{
  max_stack_use = 0;
  current_type = SHADER_TYPE_SURFACE;
  current_shader = nullptr;
  current_graph = nullptr;
  background = false;
  mix_weight_offset = SVM_STACK_INVALID;
  bump_state_offset = SVM_STACK_INVALID;
  compile_failed = false;

  /* This struct has one entry for every node, in order of ShaderNodeType definition. */
  svm_node_types_used = (std::atomic_int *)&scene->dscene.data.svm_usage;
}

int SVMCompiler::stack_size(SocketType::Type type)
{
  int size = 0;

  switch (type) {
    case SocketType::FLOAT:
    case SocketType::INT:
      size = 1;
      break;
    case SocketType::COLOR:
    case SocketType::VECTOR:
    case SocketType::NORMAL:
    case SocketType::POINT:
      size = 3;
      break;
    case SocketType::CLOSURE:
      size = 0;
      break;
    default:
      assert(0);
      break;
  }

  return size;
}

int SVMCompiler::stack_size(const ShaderIO *io)
{
  const SocketType::Type type = io->type();
  const bool derivative = io->parent->need_derivatives();

  return derivative ? stack_size(type) * 3 : stack_size(type);
}

SVMStackOffset SVMCompiler::stack_find_offset(const int size)
{
  int offset = -1;

  /* find free space in stack & mark as used */
  for (int i = 0, num_unused = 0; i < SVM_STACK_SIZE; i++) {
    if (active_stack.users[i]) {
      num_unused = 0;
    }
    else {
      num_unused++;
    }

    if (num_unused == size) {
      offset = i + 1 - size;
      max_stack_use = max(i + 1, max_stack_use);

      while (i >= offset) {
        active_stack.users[i--] = 1;
      }

      return offset;
    }
  }

  if (!compile_failed) {
    compile_failed = true;
    LOG_ERROR << "Shader graph: out of SVM stack space, shader \"" << current_shader->name
              << "\" too big.";
  }

  return 0;
}

SVMStackOffset SVMCompiler::stack_find_offset(const ShaderIO *io)
{
  return stack_find_offset(stack_size(io));
}

void SVMCompiler::stack_clear_offset(const ShaderIO *io, const SVMStackOffset offset)
{
  const int size = stack_size(io);

  for (int i = 0; i < size; i++) {
    active_stack.users[offset + i]--;
  }
}

SVMStackOffset SVMCompiler::stack_assign(ShaderInput *input)
{
  /* stack offset assign? */
  if (input->stack_offset == SVM_STACK_INVALID) {
    if (input->link) {
      /* linked to output -> use output offset */
      assert(input->link->stack_offset != SVM_STACK_INVALID);
      input->stack_offset = input->link->stack_offset;
    }
    else {
      const ShaderNode *node = input->parent;

      /* not linked to output -> add nodes to load default value */
      input->stack_offset = stack_find_offset(input);

      if (input->type() == SocketType::FLOAT) {
        add_value_node(node, node->get_float(input->socket_type), input->stack_offset);
      }
      else if (input->type() == SocketType::INT) {
        add_value_node(
            node, __int_as_float(node->get_int(input->socket_type)), input->stack_offset);
      }
      else if (input->type() == SocketType::VECTOR || input->type() == SocketType::NORMAL ||
               input->type() == SocketType::POINT || input->type() == SocketType::COLOR)
      {
        add_value_node(node, node->get_float3(input->socket_type), input->stack_offset);
      }
      else { /* should not get called for closure */
        assert(0);
      }
    }
  }

  return input->stack_offset;
}

SVMStackOffset SVMCompiler::stack_assign(ShaderOutput *output)
{
  /* if no stack offset assigned yet, find one */
  if (output->stack_offset == SVM_STACK_INVALID) {
    output->stack_offset = stack_find_offset(output);
  }

  return output->stack_offset;
}

SVMInputFloat SVMCompiler::input_float(const char *name)
{
  ShaderInput *input = current_node->input(name);
  if (input->link) {
    return SVMInputFloat{SVM_INPUT_STACK_OFFSET_MASK | uint(stack_assign(input))};
  }
  float default_value = input->parent->get_float(input->socket_type);
  /* Filter out NaN that would collide with SVM_INPUT_STACK_OFFSET_MASK. */
  if (!isfinite_safe(default_value)) {
    default_value = 0.0f;
  }
  return SVMInputFloat{__float_as_uint(default_value)};
}

SVMInputFloat3 SVMCompiler::input_float3(const char *name)
{
  ShaderInput *input = current_node->input(name);
  if (input->link) {
    return SVMInputFloat3{
        SVMInputFloat{SVM_INPUT_STACK_OFFSET_MASK | uint(stack_assign(input))}, {0}, {0}};
  }
  float3 default_value = input->parent->get_float3(input->socket_type);
  /* Filter out NaN that would collide with SVM_INPUT_STACK_OFFSET_MASK. */
  if (!isfinite_safe(default_value)) {
    default_value = zero_float3();
  }
  return SVMInputFloat3{
      {__float_as_uint(default_value.x)},
      {__float_as_uint(default_value.y)},
      {__float_as_uint(default_value.z)},
  };
}

SVMInputFloat3 SVMCompiler::input_float3_from_offset(const SVMStackOffset offset)
{
  return SVMInputFloat3{SVMInputFloat{SVM_INPUT_STACK_OFFSET_MASK | uint(offset)}, {0}, {0}};
}

SVMStackOffset SVMCompiler::input_link(const char *name)
{
  /* This is for sockets like normal which always expect a link. For the constant_folded_in we have
   * to write the value to the stack with another load and return a linked svm offset, as these
   * never store the default value in the SVMNode. */
  ShaderInput *input = current_node->input(name);
  return (input->link || input->constant_folded_in) ? stack_assign(input) : SVM_STACK_INVALID;
}

SVMStackOffset SVMCompiler::output(const char *name)
{
  ShaderOutput *shader_output = current_node->output(name);
  return output(shader_output);
}

SVMStackOffset SVMCompiler::output(ShaderOutput *shader_output)
{
  return (!shader_output->links.empty()) ? stack_assign(shader_output) : SVM_STACK_INVALID;
}

void SVMCompiler::stack_link(ShaderInput *input, ShaderOutput *output)
{
  if (output->stack_offset == SVM_STACK_INVALID) {
    assert(input->link);
    assert(stack_size(output->type()) == stack_size(input->link->type()));

    output->stack_offset = input->link->stack_offset;
    const int size = stack_size(output);

    for (int i = 0; i < size; i++) {
      active_stack.users[output->stack_offset + i]++;
    }
  }
}

void SVMCompiler::stack_clear_users(ShaderNode *node, ShaderNodeSet &done)
{
  /* optimization we should add:
   * find and lower user counts for outputs for which all inputs are done.
   * this is done before the node is compiled, under the assumption that the
   * node will first load all inputs from the stack and then writes its
   * outputs. this used to work, but was disabled because it gave trouble
   * with inputs getting stack positions assigned */

  for (ShaderInput *input : node->inputs) {
    ShaderOutput *output = input->link;

    if (output && output->stack_offset != SVM_STACK_INVALID) {
      bool all_done = true;

      /* optimization we should add: verify if in->parent is actually used */
      for (ShaderInput *in : output->links) {
        if (in->parent != node && !done.contains(in->parent)) {
          all_done = false;
        }
      }

      if (all_done) {
        stack_clear_offset(output, output->stack_offset);
        output->stack_offset = SVM_STACK_INVALID;

        for (ShaderInput *in : output->links) {
          in->stack_offset = SVM_STACK_INVALID;
        }
      }
    }
  }
}

void SVMCompiler::stack_clear_temporary(ShaderNode *node)
{
  for (ShaderInput *input : node->inputs) {
    if (!input->link && input->stack_offset != SVM_STACK_INVALID) {
      stack_clear_offset(input, input->stack_offset);
      input->stack_offset = SVM_STACK_INVALID;
    }
  }
}

void SVMCompiler::add_node(ShaderNodeType type)
{
  svm_node_types_used[type] = true;
  current_svm_nodes.push_back_slow(type);
}

static ShaderNodeType svm_node_type_with_derivatives(ShaderNodeType type)
{
  switch (type) {
#define SHADER_NODE_TYPE_DERIVATIVE(name) \
  case name: \
    return name##_DERIVATIVE;
#include "kernel/svm/node_types_template.h"

    default:
      break;
  }

  return type;
}

ShaderNodeType SVMCompiler::node_type(const ShaderNode *shader_node,
                                      const ShaderNodeType type,
                                      const bool use_derivatives)
{
  if ((use_derivatives || (shader_node && shader_node->need_derivatives())) &&
      current_type != SHADER_TYPE_VOLUME)
  {
    return svm_node_type_with_derivatives(type);
  }
  return type;
}

void SVMCompiler::add_node_data_float4(const float4 &f)
{
  current_svm_nodes.push_back_slow(__float_as_int(f.x));
  current_svm_nodes.push_back_slow(__float_as_int(f.y));
  current_svm_nodes.push_back_slow(__float_as_int(f.z));
  current_svm_nodes.push_back_slow(__float_as_int(f.w));
}

void SVMCompiler::add_node_data_float(const float f)
{
  current_svm_nodes.push_back_slow(__float_as_int(f));
}

void SVMCompiler::add_value_node(const ShaderNode *shader_node,
                                 const float value,
                                 const int stack_offset)
{
  add_node(shader_node,
           NODE_VALUE_F,
           SVMNodeValueF{
               .value = value,
               .out_offset = (SVMStackOffset)stack_offset,
           });
}

void SVMCompiler::add_value_node(const ShaderNode *shader_node,
                                 const float3 &value,
                                 const int stack_offset)
{
  add_node(shader_node,
           NODE_VALUE_V,
           SVMNodeValueV{
               .out_offset = (SVMStackOffset)stack_offset,
               .value = value,
           });
}

void SVMCompiler::stack_zero_incomplete_derivatives(const ShaderNode *node)
{
  /* No derivatives in volumes yet. */
  if (current_type == SHADER_TYPE_VOLUME) {
    return;
  }
  /* Does this node need derivatives but it doesn't have a derivative variation? */
  const bool incomplete_derivatives = node->need_derivatives() &&
                                      svm_node_type_with_derivatives(node->shader_node_type()) ==
                                          node->shader_node_type();
  if (!incomplete_derivatives) {
    return;
  }

  /* Zero derivatives. Note we can not use add_value_node since it will
   * automatically write derivatives. */
  for (const ShaderOutput *output : node->outputs) {
    if (output->stack_offset == SVM_STACK_INVALID) {
      continue;
    }
    const int base_size = stack_size(output->type());
    if (base_size == 3) {
      add_value_node(nullptr, zero_float3(), output->stack_offset + 3);
      add_value_node(nullptr, zero_float3(), output->stack_offset + 6);
    }
    else if (base_size == 1) {
      add_value_node(nullptr, 0.0f, output->stack_offset + 1);
      add_value_node(nullptr, 0.0f, output->stack_offset + 2);
    }
  }
}

uint SVMCompiler::attribute(ustring name)
{
  return scene->shader_manager->get_attribute_id(name);
}

uint SVMCompiler::attribute(AttributeStandard std)
{
  return scene->shader_manager->get_attribute_id(std);
}

uint SVMCompiler::attribute_standard(ustring name)
{
  const AttributeStandard std = Attribute::name_standard(name.c_str());
  return (std) ? attribute(std) : attribute(name);
}

void SVMCompiler::find_dependencies(ShaderNodeSet &dependencies,
                                    const ShaderNodeSet &done,
                                    ShaderInput *input,
                                    ShaderNode *skip_node)
{
  ShaderNode *node = (input->link) ? input->link->parent : nullptr;
  if (node != nullptr && !done.contains(node) && node != skip_node && !dependencies.contains(node))
  {
    for (ShaderInput *in : node->inputs) {
      find_dependencies(dependencies, done, in, skip_node);
    }
    dependencies.insert(node);
  }
}

void SVMCompiler::generate_node(ShaderNode *node, ShaderNodeSet &done)
{
  current_node = node;
  node->compile(*this);
  current_node = nullptr;
  stack_zero_incomplete_derivatives(node);
  stack_clear_users(node, done);
  stack_clear_temporary(node);

  if (current_type == SHADER_TYPE_SURFACE) {
    if (node->has_spatial_varying()) {
      current_shader->has_surface_spatial_varying = true;
    }
    if (node->get_feature() & KERNEL_FEATURE_NODE_RAYTRACE) {
      current_shader->has_surface_raytrace = true;
    }
  }
  else if (current_type == SHADER_TYPE_VOLUME) {
    if (node->has_spatial_varying()) {
      current_shader->has_volume_spatial_varying = true;
    }
    if (node->has_attribute_dependency()) {
      current_shader->has_volume_attribute_dependency = true;
    }
  }
}

void SVMCompiler::generate_svm_nodes(const ShaderNodeSet &nodes, CompilerState *state)
{
  ShaderNodeSet &done = state->nodes_done;
  vector<bool> &done_flag = state->nodes_done_flag;

  bool nodes_done;
  do {
    nodes_done = true;

    for (ShaderNode *node : nodes) {
      if (!done_flag[node->id]) {
        bool inputs_done = true;

        for (ShaderInput *input : node->inputs) {
          if (input->link && !done_flag[input->link->parent->id]) {
            inputs_done = false;
          }
        }
        if (inputs_done) {
          generate_node(node, done);
          done.insert(node);
          done_flag[node->id] = true;
        }
        else {
          nodes_done = false;
        }
      }
    }
  } while (!nodes_done);
}

void SVMCompiler::generate_closure_node(ShaderNode *node, CompilerState *state)
{
  /* Skip generating closure that are not supported or needed for a particular
   * type of shader. For example a BSDF in a volume shader. */
  const uint node_feature = node->get_feature();
  if ((state->node_feature_mask & node_feature) != node_feature) {
    return;
  }

  /* execute dependencies for closure */
  for (ShaderInput *in : node->inputs) {
    if (in->link != nullptr) {
      ShaderNodeSet dependencies;
      find_dependencies(dependencies, state->nodes_done, in);
      generate_svm_nodes(dependencies, state);
    }
  }

  /* closure mix weight */
  const char *weight_name = (current_type == SHADER_TYPE_VOLUME) ? "VolumeMixWeight" :
                                                                   "SurfaceMixWeight";
  ShaderInput *weight_in = node->input(weight_name);

  if (weight_in && (weight_in->link || node->get_float(weight_in->socket_type) != 1.0f)) {
    mix_weight_offset = stack_assign(weight_in);
  }
  else {
    mix_weight_offset = SVM_STACK_INVALID;
  }

  /* compile closure itself */
  generate_node(node, state->nodes_done);

  mix_weight_offset = SVM_STACK_INVALID;

  if (current_type == SHADER_TYPE_SURFACE) {
    if (node->has_surface_transparent()) {
      current_shader->has_surface_transparent = true;
    }
    if (node->has_surface_bssrdf()) {
      current_shader->has_surface_bssrdf = true;
      if (node->has_bssrdf_bump()) {
        current_shader->has_bssrdf_bump = true;
      }
    }
    if (node->has_bump()) {
      current_shader->has_bump_from_surface = true;
    }
  }
}

void SVMCompiler::generated_shared_closure_nodes(ShaderNode *root_node,
                                                 ShaderNode *node,
                                                 CompilerState *state,
                                                 const ShaderNodeSet &shared)
{
  if (shared.contains(node)) {
    generate_multi_closure(root_node, node, state);
  }
  else {
    for (ShaderInput *in : node->inputs) {
      if (in->type() == SocketType::CLOSURE && in->link) {
        generated_shared_closure_nodes(root_node, in->link->parent, state, shared);
      }
    }
  }
}

void SVMCompiler::find_aov_nodes_and_dependencies(ShaderNodeSet &aov_nodes,
                                                  ShaderGraph *graph,
                                                  CompilerState *state)
{
  for (ShaderNode *node : graph->nodes) {
    if (node->special_type == SHADER_SPECIAL_TYPE_OUTPUT_AOV) {
      OutputAOVNode *aov_node = static_cast<OutputAOVNode *>(node);
      if (aov_node->offset >= 0) {
        aov_nodes.insert(aov_node);
        for (ShaderInput *in : node->inputs) {
          if (in->link != nullptr) {
            find_dependencies(aov_nodes, state->nodes_done, in);
          }
        }
      }
    }
  }
}

void SVMCompiler::generate_multi_closure(ShaderNode *root_node,
                                         ShaderNode *node,
                                         CompilerState *state)
{
  /* only generate once */
  if (state->closure_done.contains(node)) {
    return;
  }

  state->closure_done.insert(node);

  if (node->special_type == SHADER_SPECIAL_TYPE_COMBINE_CLOSURE) {
    /* weighting is already taken care of in ShaderGraph::transform_multi_closure */
    ShaderInput *cl1in = node->input("Closure1");
    ShaderInput *cl2in = node->input("Closure2");
    ShaderInput *facin = node->input("Fac");

    /* skip empty mix/add closure nodes */
    if (!cl1in->link && !cl2in->link) {
      return;
    }

    if (facin && facin->link) {
      /* mix closure: generate instructions to compute mix weight */
      ShaderNodeSet dependencies;
      find_dependencies(dependencies, state->nodes_done, facin);
      generate_svm_nodes(dependencies, state);

      /* execute shared dependencies. this is needed to allow skipping
       * of zero weight closures and their dependencies later, so we
       * ensure that they only skip dependencies that are unique to them */
      ShaderNodeSet cl1deps;
      ShaderNodeSet cl2deps;
      ShaderNodeSet shareddeps;

      find_dependencies(cl1deps, state->nodes_done, cl1in);
      find_dependencies(cl2deps, state->nodes_done, cl2in);

      const ShaderNodeIDComparator node_id_comp;
      set_intersection(cl1deps.begin(),
                       cl1deps.end(),
                       cl2deps.begin(),
                       cl2deps.end(),
                       std::inserter(shareddeps, shareddeps.begin()),
                       node_id_comp);

      /* it's possible some nodes are not shared between this mix node
       * inputs, but still needed to be always executed, this mainly
       * happens when a node of current subbranch is used by a parent
       * node or so */
      if (root_node != node) {
        for (ShaderInput *in : root_node->inputs) {
          ShaderNodeSet rootdeps;
          find_dependencies(rootdeps, state->nodes_done, in, node);
          set_intersection(rootdeps.begin(),
                           rootdeps.end(),
                           cl1deps.begin(),
                           cl1deps.end(),
                           std::inserter(shareddeps, shareddeps.begin()),
                           node_id_comp);
          set_intersection(rootdeps.begin(),
                           rootdeps.end(),
                           cl2deps.begin(),
                           cl2deps.end(),
                           std::inserter(shareddeps, shareddeps.begin()),
                           node_id_comp);
        }
      }

      /* For dependencies AOV nodes, prevent them from being categorized
       * as exclusive deps of one or the other closure, since the need to
       * execute them for AOV writing is not dependent on the closure
       * weights. */
      if (!state->aov_nodes.empty()) {
        set_intersection(state->aov_nodes.begin(),
                         state->aov_nodes.end(),
                         cl1deps.begin(),
                         cl1deps.end(),
                         std::inserter(shareddeps, shareddeps.begin()),
                         node_id_comp);
        set_intersection(state->aov_nodes.begin(),
                         state->aov_nodes.end(),
                         cl2deps.begin(),
                         cl2deps.end(),
                         std::inserter(shareddeps, shareddeps.begin()),
                         node_id_comp);
      }

      if (!shareddeps.empty()) {
        if (cl1in->link) {
          generated_shared_closure_nodes(root_node, cl1in->link->parent, state, shareddeps);
        }
        if (cl2in->link) {
          generated_shared_closure_nodes(root_node, cl2in->link->parent, state, shareddeps);
        }

        generate_svm_nodes(shareddeps, state);
      }

      /* generate instructions for input closure 1 */
      if (cl1in->link) {
        /* Add instruction to skip closure and its dependencies if mix
         * weight is zero.
         */
        const int node_start = current_svm_nodes.size();
        add_node(nullptr, NODE_JUMP_IF_ONE, SVMNodeJumpIfOne{0, stack_assign(facin)});

        generate_multi_closure(root_node, cl1in->link->parent, state);

        /* Fill in jump instruction location to be after closure. */
        const int jump_node_size = 1 + sizeof(SVMNodeJumpIfOne) / sizeof(int);
        current_svm_nodes[node_start + 1] = current_svm_nodes.size() -
                                            (node_start + jump_node_size);
      }

      /* generate instructions for input closure 2 */
      if (cl2in->link) {
        /* Add instruction to skip closure and its dependencies if mix
         * weight is zero.
         */
        const int node_start = current_svm_nodes.size();
        add_node(nullptr, NODE_JUMP_IF_ZERO, SVMNodeJumpIfZero{0, stack_assign(facin)});

        generate_multi_closure(root_node, cl2in->link->parent, state);

        /* Fill in jump instruction location to be after closure. */
        const int jump_node_size = 1 + sizeof(SVMNodeJumpIfZero) / sizeof(int);
        current_svm_nodes[node_start + 1] = current_svm_nodes.size() -
                                            (node_start + jump_node_size);
      }

      /* unassign */
      facin->stack_offset = SVM_STACK_INVALID;
    }
    else {
      /* execute closures and their dependencies, no runtime checks
       * to skip closures here because was already optimized due to
       * fixed weight or add closure that always needs both */
      if (cl1in->link) {
        generate_multi_closure(root_node, cl1in->link->parent, state);
      }
      if (cl2in->link) {
        generate_multi_closure(root_node, cl2in->link->parent, state);
      }
    }
  }
  else {
    generate_closure_node(node, state);
  }

  state->nodes_done.insert(node);
  state->nodes_done_flag[node->id] = true;
}

static void mark_nodes_requiring_derivatives(const SVMCompiler &compiler,
                                             ShaderGraph *graph,
                                             const ShaderType type)
{
  if (type == SHADER_TYPE_VOLUME) {
    /* Only support derivatives for surface for now. */
    return;
  }
  queue<ShaderNode *> traverse_queue;
  ShaderNodeSet scheduled;
  /* Check if texture nodes need derivatives. */
  for (ShaderNode *node : graph->nodes) {
    if (node->is_texture_node_and_needs_derivatives(compiler)) {
      traverse_queue.push(node);
      scheduled.insert(node);
    }
  }
  /* Mark all ancestors of texture nodes as requiring derivatives, if the texture nodes themselves
   * need derivatives. */
  while (!traverse_queue.empty()) {
    ShaderNode *node = traverse_queue.front();
    traverse_queue.pop();
    node->set_need_derivatives();
    LOG_DEBUG << "Marking " << node->name << " as requiring derivatives";
    for (ShaderInput *input : node->inputs) {
      if (input->link == nullptr) {
        continue;
      }
      if (scheduled.contains(input->link->parent)) {
        continue;
      }
      traverse_queue.push(input->link->parent);
      scheduled.insert(input->link->parent);
    }
  }
}

void SVMCompiler::compile_type(Shader *shader, ShaderGraph *graph, ShaderType type)
{
  /* Converting a shader graph into svm_nodes that can be executed
   * sequentially on the virtual machine is fairly simple. We can keep
   * looping over nodes and each time all the inputs of a node are
   * ready, we add svm_nodes for it that read the inputs from the
   * stack and write outputs back to the stack.
   *
   * With the SVM, we always sample only a single closure. We can think
   * of all closures nodes as a binary tree with mix closures as inner
   * nodes and other closures as leafs. The SVM will traverse that tree,
   * each time deciding to go left or right depending on the mix weights,
   * until a closure is found.
   *
   * We only execute nodes that are needed for the mix weights and chosen
   * closure.
   */

  current_type = type;
  current_graph = graph;

  /* get input in output node */
  ShaderNode *output = graph->output();
  ShaderInput *clin = nullptr;

  switch (type) {
    case SHADER_TYPE_SURFACE:
      clin = output->input("Surface");
      break;
    case SHADER_TYPE_VOLUME:
      clin = output->input("Volume");
      break;
    case SHADER_TYPE_DISPLACEMENT:
      clin = output->input("Displacement");
      break;
    case SHADER_TYPE_BUMP:
      clin = output->input("Normal");
      break;
    default:
      assert(0);
      break;
  }

  /* clear all compiler state */
  memset((void *)&active_stack, 0, sizeof(active_stack));
  current_svm_nodes.clear();

  for (ShaderNode *node : graph->nodes) {
    for (ShaderInput *input : node->inputs) {
      input->stack_offset = SVM_STACK_INVALID;
    }
    for (ShaderOutput *output : node->outputs) {
      output->stack_offset = SVM_STACK_INVALID;
    }
  }

  mark_nodes_requiring_derivatives(*this, graph, type);

  /* for the bump shader we need add a node to store the shader state */
  const bool need_bump_state = (type == SHADER_TYPE_BUMP) &&
                               (shader->get_displacement_method() == DISPLACE_BOTH);
  if (need_bump_state) {
    bump_state_offset = stack_find_offset(SVM_BUMP_EVAL_STATE_SIZE);
    add_node(
        nullptr, NODE_ENTER_BUMP_EVAL, SVMNodeEnterBumpEval{.state_offset = bump_state_offset});
  }

  if (shader->reference_count()) {
    CompilerState state(graph);

    switch (type) {
      case SHADER_TYPE_SURFACE: /* generate surface shader */
        find_aov_nodes_and_dependencies(state.aov_nodes, graph, &state);
        if (shader->has_surface) {
          state.node_feature_mask = KERNEL_FEATURE_NODE_MASK_SURFACE;
        }
        break;
      case SHADER_TYPE_VOLUME: /* generate volume shader */
        if (shader->has_volume) {
          state.node_feature_mask = KERNEL_FEATURE_NODE_MASK_VOLUME;
        }
        break;
      case SHADER_TYPE_DISPLACEMENT: /* generate displacement shader */
        if (shader->has_displacement) {
          state.node_feature_mask = KERNEL_FEATURE_NODE_MASK_DISPLACEMENT;
        }
        break;
      case SHADER_TYPE_BUMP: /* generate bump shader */
        if (clin->link) {
          state.node_feature_mask = KERNEL_FEATURE_NODE_MASK_BUMP;
        }
        break;
      default:
        break;
    }

    if (clin->link) {
      generate_multi_closure(clin->link->parent, clin->link->parent, &state);
    }

    /* compile output node */
    current_node = output;
    output->compile(*this);
    current_node = nullptr;

    if (!state.aov_nodes.empty()) {
      /* AOV passes are only written if the object is directly visible, so
       * there is no point in evaluating all the nodes generated only for the
       * AOV outputs if that's not the case. Therefore, we insert
       * NODE_AOV_START into the shader before the AOV-only nodes are
       * generated which tells the kernel that it can stop evaluation
       * early if AOVs will not be written. */
      add_node(NODE_AOV_START);
      generate_svm_nodes(state.aov_nodes, &state);
    }
  }

  /* add node to restore state after bump shader has finished */
  if (need_bump_state) {
    add_node(
        nullptr, NODE_LEAVE_BUMP_EVAL, SVMNodeLeaveBumpEval{.state_offset = bump_state_offset});
    bump_state_offset = SVM_STACK_INVALID;
  }

  /* if compile failed, generate empty shader */
  if (compile_failed) {
    current_svm_nodes.clear();
    compile_failed = false;
  }

  /* for bump shaders we fall thru to the surface shader, but if this is any other kind of shader
   * it ends here */
  if (type != SHADER_TYPE_BUMP) {
    add_node(NODE_END);
  }
}

void SVMCompiler::compile(Shader *shader, array<int> &svm_nodes, const int index, Summary *summary)
{
  svm_node_types_used[NODE_SHADER_JUMP] = true;
  add_node(nullptr, NODE_SHADER_JUMP, SVMNodeShaderJump{0, 0, 0});
  svm_nodes.append(current_svm_nodes);
  current_svm_nodes.clear();

  /* copy graph for shader with bump mapping */
  const int start_num_svm_nodes = svm_nodes.size();

  const double time_start = time_dt();

  const bool has_bump_from_displacement = shader->has_bump_from_displacement;

  current_shader = shader;

  /* generate bump shader */
  if (has_bump_from_displacement) {
    const scoped_timer timer((summary != nullptr) ? &summary->time_generate_bump : nullptr);
    compile_type(shader, shader->graph.get(), SHADER_TYPE_BUMP);
    svm_nodes[index + 1] = svm_nodes.size();
    svm_nodes.append(current_svm_nodes);
  }

  /* generate surface shader */
  {
    const scoped_timer timer((summary != nullptr) ? &summary->time_generate_surface : nullptr);
    compile_type(shader, shader->graph.get(), SHADER_TYPE_SURFACE);
    /* only set jump offset if there's no bump shader, as the bump shader will fall thru to this
     * one if it exists */
    if (!has_bump_from_displacement) {
      svm_nodes[index + 1] = svm_nodes.size();
    }
    svm_nodes.append(current_svm_nodes);
  }

  /* generate volume shader */
  {
    const scoped_timer timer((summary != nullptr) ? &summary->time_generate_volume : nullptr);
    compile_type(shader, shader->graph.get(), SHADER_TYPE_VOLUME);
    svm_nodes[index + 2] = svm_nodes.size();
    svm_nodes.append(current_svm_nodes);
  }

  /* generate displacement shader */
  {
    const scoped_timer timer((summary != nullptr) ? &summary->time_generate_displacement :
                                                    nullptr);
    compile_type(shader, shader->graph.get(), SHADER_TYPE_DISPLACEMENT);
    svm_nodes[index + 3] = svm_nodes.size();
    svm_nodes.append(current_svm_nodes);
  }

  /* Fill in summary information. */
  if (summary != nullptr) {
    summary->time_total = time_dt() - time_start;
    summary->peak_stack_usage = max_stack_use;
    summary->num_svm_nodes = svm_nodes.size() - start_num_svm_nodes;
  }

  /* Estimate emission for MIS. */
  shader->estimate_emission();
}

/* Compiler summary implementation. */

SVMCompiler::Summary::Summary()
    : num_svm_nodes(0),
      peak_stack_usage(0),
      time_generate_surface(0.0),
      time_generate_bump(0.0),
      time_generate_volume(0.0),
      time_generate_displacement(0.0),
      time_total(0.0)
{
}

string SVMCompiler::Summary::full_report() const
{
  string report;
  report += string_printf("Number of SVM nodes: %d\n", num_svm_nodes);
  report += string_printf("Peak stack usage:    %d\n", peak_stack_usage);

  report += string_printf("Time (in seconds):\n");
  report += string_printf("Generate:            %f\n",
                          time_generate_surface + time_generate_bump + time_generate_volume +
                              time_generate_displacement);
  report += string_printf("  Surface:           %f\n", time_generate_surface);
  report += string_printf("  Bump:              %f\n", time_generate_bump);
  report += string_printf("  Volume:            %f\n", time_generate_volume);
  report += string_printf("  Displacement:      %f\n", time_generate_displacement);

  return report;
}

/* Global state of the compiler. */

SVMCompiler::CompilerState::CompilerState(ShaderGraph *graph)
{
  int max_id = 0;
  for (ShaderNode *node : graph->nodes) {
    max_id = max(node->id, max_id);
  }
  nodes_done_flag.resize(max_id + 1, false);
  node_feature_mask = 0;
}

CCL_NAMESPACE_END
