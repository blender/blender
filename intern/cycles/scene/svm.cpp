/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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

#include "util/foreach.h"
#include "util/log.h"
#include "util/progress.h"
#include "util/task.h"

CCL_NAMESPACE_BEGIN

/* Shader Manager */

SVMShaderManager::SVMShaderManager()
{
}

SVMShaderManager::~SVMShaderManager()
{
}

void SVMShaderManager::reset(Scene * /*scene*/)
{
}

void SVMShaderManager::device_update_shader(Scene *scene,
                                            Shader *shader,
                                            Progress *progress,
                                            array<int4> *svm_nodes)
{
  if (progress->get_cancel()) {
    return;
  }
  assert(shader->graph);

  svm_nodes->push_back_slow(make_int4(NODE_SHADER_JUMP, 0, 0, 0));

  SVMCompiler::Summary summary;
  SVMCompiler compiler(scene);
  compiler.background = (shader == scene->background->get_shader(scene));
  compiler.compile(shader, *svm_nodes, 0, &summary);

  VLOG_WORK << "Compilation summary:\n"
            << "Shader name: " << shader->name << "\n"
            << summary.full_report();
}

void SVMShaderManager::device_update_specific(Device *device,
                                              DeviceScene *dscene,
                                              Scene *scene,
                                              Progress &progress)
{
  if (!need_update())
    return;

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->svm.times.add_entry({"device_update", time});
    }
  });

  const int num_shaders = scene->shaders.size();

  VLOG_INFO << "Total " << num_shaders << " shaders.";

  double start_time = time_dt();

  /* test if we need to update */
  device_free(device, dscene, scene);

  /* Build all shaders. */
  TaskPool task_pool;
  vector<array<int4>> shader_svm_nodes(num_shaders);
  for (int i = 0; i < num_shaders; i++) {
    task_pool.push(function_bind(&SVMShaderManager::device_update_shader,
                                 this,
                                 scene,
                                 scene->shaders[i],
                                 &progress,
                                 &shader_svm_nodes[i]));
  }
  task_pool.wait_work();

  if (progress.get_cancel()) {
    return;
  }

  /* The global node list contains a jump table (one node per shader)
   * followed by the nodes of all shaders. */
  int svm_nodes_size = num_shaders;
  for (int i = 0; i < num_shaders; i++) {
    /* Since we're not copying the local jump node, the size ends up being one node lower. */
    svm_nodes_size += shader_svm_nodes[i].size() - 1;
  }

  int4 *svm_nodes = dscene->svm_nodes.alloc(svm_nodes_size);

  int node_offset = num_shaders;
  for (int i = 0; i < num_shaders; i++) {
    Shader *shader = scene->shaders[i];

    shader->clear_modified();
    if (shader->get_use_mis() && shader->has_surface_emission) {
      scene->light_manager->tag_update(scene, LightManager::SHADER_COMPILED);
    }

    /* Update the global jump table.
     * Each compiled shader starts with a jump node that has offsets local
     * to the shader, so copy those and add the offset into the global node list. */
    int4 &global_jump_node = svm_nodes[shader->id];
    int4 &local_jump_node = shader_svm_nodes[i][0];

    global_jump_node.x = NODE_SHADER_JUMP;
    global_jump_node.y = local_jump_node.y - 1 + node_offset;
    global_jump_node.z = local_jump_node.z - 1 + node_offset;
    global_jump_node.w = local_jump_node.w - 1 + node_offset;

    node_offset += shader_svm_nodes[i].size() - 1;
  }

  /* Copy the nodes of each shader into the correct location. */
  svm_nodes += num_shaders;
  for (int i = 0; i < num_shaders; i++) {
    int shader_size = shader_svm_nodes[i].size() - 1;

    memcpy(svm_nodes, &shader_svm_nodes[i][1], sizeof(int4) * shader_size);
    svm_nodes += shader_size;
  }

  if (progress.get_cancel()) {
    return;
  }

  dscene->svm_nodes.copy_to_device();

  device_update_common(device, dscene, scene, progress);

  update_flags = UPDATE_NONE;

  VLOG_INFO << "Shader manager updated " << num_shaders << " shaders in " << time_dt() - start_time
            << " seconds.";
}

void SVMShaderManager::device_free(Device *device, DeviceScene *dscene, Scene *scene)
{
  device_free_common(device, dscene, scene);

  dscene->svm_nodes.free();
}

/* Graph Compiler */

SVMCompiler::SVMCompiler(Scene *scene) : scene(scene)
{
  max_stack_use = 0;
  current_type = SHADER_TYPE_SURFACE;
  current_shader = NULL;
  current_graph = NULL;
  background = false;
  mix_weight_offset = SVM_STACK_INVALID;
  compile_failed = false;
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

int SVMCompiler::stack_find_offset(int size)
{
  int offset = -1;

  /* find free space in stack & mark as used */
  for (int i = 0, num_unused = 0; i < SVM_STACK_SIZE; i++) {
    if (active_stack.users[i])
      num_unused = 0;
    else
      num_unused++;

    if (num_unused == size) {
      offset = i + 1 - size;
      max_stack_use = max(i + 1, max_stack_use);

      while (i >= offset)
        active_stack.users[i--] = 1;

      return offset;
    }
  }

  if (!compile_failed) {
    compile_failed = true;
    fprintf(stderr,
            "Cycles: out of SVM stack space, shader \"%s\" too big.\n",
            current_shader->name.c_str());
  }

  return 0;
}

int SVMCompiler::stack_find_offset(SocketType::Type type)
{
  return stack_find_offset(stack_size(type));
}

void SVMCompiler::stack_clear_offset(SocketType::Type type, int offset)
{
  int size = stack_size(type);

  for (int i = 0; i < size; i++)
    active_stack.users[offset + i]--;
}

int SVMCompiler::stack_assign(ShaderInput *input)
{
  /* stack offset assign? */
  if (input->stack_offset == SVM_STACK_INVALID) {
    if (input->link) {
      /* linked to output -> use output offset */
      assert(input->link->stack_offset != SVM_STACK_INVALID);
      input->stack_offset = input->link->stack_offset;
    }
    else {
      Node *node = input->parent;

      /* not linked to output -> add nodes to load default value */
      input->stack_offset = stack_find_offset(input->type());

      if (input->type() == SocketType::FLOAT) {
        add_node(NODE_VALUE_F,
                 __float_as_int(node->get_float(input->socket_type)),
                 input->stack_offset);
      }
      else if (input->type() == SocketType::INT) {
        add_node(NODE_VALUE_F, node->get_int(input->socket_type), input->stack_offset);
      }
      else if (input->type() == SocketType::VECTOR || input->type() == SocketType::NORMAL ||
               input->type() == SocketType::POINT || input->type() == SocketType::COLOR) {

        add_node(NODE_VALUE_V, input->stack_offset);
        add_node(NODE_VALUE_V, node->get_float3(input->socket_type));
      }
      else /* should not get called for closure */
        assert(0);
    }
  }

  return input->stack_offset;
}

int SVMCompiler::stack_assign(ShaderOutput *output)
{
  /* if no stack offset assigned yet, find one */
  if (output->stack_offset == SVM_STACK_INVALID)
    output->stack_offset = stack_find_offset(output->type());

  return output->stack_offset;
}

int SVMCompiler::stack_assign_if_linked(ShaderInput *input)
{
  if (input->link || input->constant_folded_in)
    return stack_assign(input);

  return SVM_STACK_INVALID;
}

int SVMCompiler::stack_assign_if_linked(ShaderOutput *output)
{
  if (!output->links.empty())
    return stack_assign(output);

  return SVM_STACK_INVALID;
}

void SVMCompiler::stack_link(ShaderInput *input, ShaderOutput *output)
{
  if (output->stack_offset == SVM_STACK_INVALID) {
    assert(input->link);
    assert(stack_size(output->type()) == stack_size(input->link->type()));

    output->stack_offset = input->link->stack_offset;

    int size = stack_size(output->type());

    for (int i = 0; i < size; i++)
      active_stack.users[output->stack_offset + i]++;
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

  foreach (ShaderInput *input, node->inputs) {
    ShaderOutput *output = input->link;

    if (output && output->stack_offset != SVM_STACK_INVALID) {
      bool all_done = true;

      /* optimization we should add: verify if in->parent is actually used */
      foreach (ShaderInput *in, output->links)
        if (in->parent != node && done.find(in->parent) == done.end())
          all_done = false;

      if (all_done) {
        stack_clear_offset(output->type(), output->stack_offset);
        output->stack_offset = SVM_STACK_INVALID;

        foreach (ShaderInput *in, output->links)
          in->stack_offset = SVM_STACK_INVALID;
      }
    }
  }
}

void SVMCompiler::stack_clear_temporary(ShaderNode *node)
{
  foreach (ShaderInput *input, node->inputs) {
    if (!input->link && input->stack_offset != SVM_STACK_INVALID) {
      stack_clear_offset(input->type(), input->stack_offset);
      input->stack_offset = SVM_STACK_INVALID;
    }
  }
}

uint SVMCompiler::encode_uchar4(uint x, uint y, uint z, uint w)
{
  assert(x <= 255);
  assert(y <= 255);
  assert(z <= 255);
  assert(w <= 255);

  return (x) | (y << 8) | (z << 16) | (w << 24);
}

void SVMCompiler::add_node(int a, int b, int c, int d)
{
  current_svm_nodes.push_back_slow(make_int4(a, b, c, d));
}

void SVMCompiler::add_node(ShaderNodeType type, int a, int b, int c)
{
  current_svm_nodes.push_back_slow(make_int4(type, a, b, c));
}

void SVMCompiler::add_node(ShaderNodeType type, const float3 &f)
{
  current_svm_nodes.push_back_slow(
      make_int4(type, __float_as_int(f.x), __float_as_int(f.y), __float_as_int(f.z)));
}

void SVMCompiler::add_node(const float4 &f)
{
  current_svm_nodes.push_back_slow(make_int4(
      __float_as_int(f.x), __float_as_int(f.y), __float_as_int(f.z), __float_as_int(f.w)));
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
  AttributeStandard std = Attribute::name_standard(name.c_str());
  return (std) ? attribute(std) : attribute(name);
}

void SVMCompiler::find_dependencies(ShaderNodeSet &dependencies,
                                    const ShaderNodeSet &done,
                                    ShaderInput *input,
                                    ShaderNode *skip_node)
{
  ShaderNode *node = (input->link) ? input->link->parent : NULL;
  if (node != NULL && done.find(node) == done.end() && node != skip_node &&
      dependencies.find(node) == dependencies.end()) {
    foreach (ShaderInput *in, node->inputs) {
      find_dependencies(dependencies, done, in, skip_node);
    }
    dependencies.insert(node);
  }
}

void SVMCompiler::generate_node(ShaderNode *node, ShaderNodeSet &done)
{
  node->compile(*this);
  stack_clear_users(node, done);
  stack_clear_temporary(node);

  if (current_type == SHADER_TYPE_SURFACE) {
    if (node->has_spatial_varying())
      current_shader->has_surface_spatial_varying = true;
    if (node->get_feature() & KERNEL_FEATURE_NODE_RAYTRACE)
      current_shader->has_surface_raytrace = true;
  }
  else if (current_type == SHADER_TYPE_VOLUME) {
    if (node->has_spatial_varying())
      current_shader->has_volume_spatial_varying = true;
    if (node->has_attribute_dependency())
      current_shader->has_volume_attribute_dependency = true;
  }

  if (node->has_integrator_dependency()) {
    current_shader->has_integrator_dependency = true;
  }
}

void SVMCompiler::generate_svm_nodes(const ShaderNodeSet &nodes, CompilerState *state)
{
  ShaderNodeSet &done = state->nodes_done;
  vector<bool> &done_flag = state->nodes_done_flag;

  bool nodes_done;
  do {
    nodes_done = true;

    foreach (ShaderNode *node, nodes) {
      if (!done_flag[node->id]) {
        bool inputs_done = true;

        foreach (ShaderInput *input, node->inputs) {
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
  const int node_feature = node->get_feature();
  if ((state->node_feature_mask & node_feature) != node_feature) {
    return;
  }

  /* execute dependencies for closure */
  foreach (ShaderInput *in, node->inputs) {
    if (in->link != NULL) {
      ShaderNodeSet dependencies;
      find_dependencies(dependencies, state->nodes_done, in);
      generate_svm_nodes(dependencies, state);
    }
  }

  /* closure mix weight */
  const char *weight_name = (current_type == SHADER_TYPE_VOLUME) ? "VolumeMixWeight" :
                                                                   "SurfaceMixWeight";
  ShaderInput *weight_in = node->input(weight_name);

  if (weight_in && (weight_in->link || node->get_float(weight_in->socket_type) != 1.0f))
    mix_weight_offset = stack_assign(weight_in);
  else
    mix_weight_offset = SVM_STACK_INVALID;

  /* compile closure itself */
  generate_node(node, state->nodes_done);

  mix_weight_offset = SVM_STACK_INVALID;

  if (current_type == SHADER_TYPE_SURFACE) {
    if (node->has_surface_emission())
      current_shader->has_surface_emission = true;
    if (node->has_surface_transparent())
      current_shader->has_surface_transparent = true;
    if (node->has_surface_bssrdf()) {
      current_shader->has_surface_bssrdf = true;
      if (node->has_bssrdf_bump())
        current_shader->has_bssrdf_bump = true;
    }
    if (node->has_bump()) {
      current_shader->has_bump = true;
    }
  }
}

void SVMCompiler::generated_shared_closure_nodes(ShaderNode *root_node,
                                                 ShaderNode *node,
                                                 CompilerState *state,
                                                 const ShaderNodeSet &shared)
{
  if (shared.find(node) != shared.end()) {
    generate_multi_closure(root_node, node, state);
  }
  else {
    foreach (ShaderInput *in, node->inputs) {
      if (in->type() == SocketType::CLOSURE && in->link)
        generated_shared_closure_nodes(root_node, in->link->parent, state, shared);
    }
  }
}

void SVMCompiler::find_aov_nodes_and_dependencies(ShaderNodeSet &aov_nodes,
                                                  ShaderGraph *graph,
                                                  CompilerState *state)
{
  foreach (ShaderNode *node, graph->nodes) {
    if (node->special_type == SHADER_SPECIAL_TYPE_OUTPUT_AOV) {
      OutputAOVNode *aov_node = static_cast<OutputAOVNode *>(node);
      if (aov_node->offset >= 0) {
        aov_nodes.insert(aov_node);
        foreach (ShaderInput *in, node->inputs) {
          if (in->link != NULL) {
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
  if (state->closure_done.find(node) != state->closure_done.end())
    return;

  state->closure_done.insert(node);

  if (node->special_type == SHADER_SPECIAL_TYPE_COMBINE_CLOSURE) {
    /* weighting is already taken care of in ShaderGraph::transform_multi_closure */
    ShaderInput *cl1in = node->input("Closure1");
    ShaderInput *cl2in = node->input("Closure2");
    ShaderInput *facin = node->input("Fac");

    /* skip empty mix/add closure nodes */
    if (!cl1in->link && !cl2in->link)
      return;

    if (facin && facin->link) {
      /* mix closure: generate instructions to compute mix weight */
      ShaderNodeSet dependencies;
      find_dependencies(dependencies, state->nodes_done, facin);
      generate_svm_nodes(dependencies, state);

      /* execute shared dependencies. this is needed to allow skipping
       * of zero weight closures and their dependencies later, so we
       * ensure that they only skip dependencies that are unique to them */
      ShaderNodeSet cl1deps, cl2deps, shareddeps;

      find_dependencies(cl1deps, state->nodes_done, cl1in);
      find_dependencies(cl2deps, state->nodes_done, cl2in);

      ShaderNodeIDComparator node_id_comp;
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
        foreach (ShaderInput *in, root_node->inputs) {
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
      if (state->aov_nodes.size()) {
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
        current_svm_nodes.push_back_slow(make_int4(NODE_JUMP_IF_ONE, 0, stack_assign(facin), 0));
        int node_jump_skip_index = current_svm_nodes.size() - 1;

        generate_multi_closure(root_node, cl1in->link->parent, state);

        /* Fill in jump instruction location to be after closure. */
        current_svm_nodes[node_jump_skip_index].y = current_svm_nodes.size() -
                                                    node_jump_skip_index - 1;
      }

      /* generate instructions for input closure 2 */
      if (cl2in->link) {
        /* Add instruction to skip closure and its dependencies if mix
         * weight is zero.
         */
        current_svm_nodes.push_back_slow(make_int4(NODE_JUMP_IF_ZERO, 0, stack_assign(facin), 0));
        int node_jump_skip_index = current_svm_nodes.size() - 1;

        generate_multi_closure(root_node, cl2in->link->parent, state);

        /* Fill in jump instruction location to be after closure. */
        current_svm_nodes[node_jump_skip_index].y = current_svm_nodes.size() -
                                                    node_jump_skip_index - 1;
      }

      /* unassign */
      facin->stack_offset = SVM_STACK_INVALID;
    }
    else {
      /* execute closures and their dependencies, no runtime checks
       * to skip closures here because was already optimized due to
       * fixed weight or add closure that always needs both */
      if (cl1in->link)
        generate_multi_closure(root_node, cl1in->link->parent, state);
      if (cl2in->link)
        generate_multi_closure(root_node, cl2in->link->parent, state);
    }
  }
  else {
    generate_closure_node(node, state);
  }

  state->nodes_done.insert(node);
  state->nodes_done_flag[node->id] = true;
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
  ShaderInput *clin = NULL;

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

  foreach (ShaderNode *node, graph->nodes) {
    foreach (ShaderInput *input, node->inputs)
      input->stack_offset = SVM_STACK_INVALID;
    foreach (ShaderOutput *output, node->outputs)
      output->stack_offset = SVM_STACK_INVALID;
  }

  /* for the bump shader we need add a node to store the shader state */
  bool need_bump_state = (type == SHADER_TYPE_BUMP) &&
                         (shader->get_displacement_method() == DISPLACE_BOTH);
  int bump_state_offset = SVM_STACK_INVALID;
  if (need_bump_state) {
    bump_state_offset = stack_find_offset(SVM_BUMP_EVAL_STATE_SIZE);
    add_node(NODE_ENTER_BUMP_EVAL, bump_state_offset);
  }

  if (shader->reference_count()) {
    CompilerState state(graph);
    if (clin->link) {
      bool generate = false;

      switch (type) {
        case SHADER_TYPE_SURFACE: /* generate surface shader */
          generate = true;
          shader->has_surface = true;
          state.node_feature_mask = KERNEL_FEATURE_NODE_MASK_SURFACE;
          break;
        case SHADER_TYPE_VOLUME: /* generate volume shader */
          generate = true;
          shader->has_volume = true;
          state.node_feature_mask = KERNEL_FEATURE_NODE_MASK_VOLUME;
          break;
        case SHADER_TYPE_DISPLACEMENT: /* generate displacement shader */
          generate = true;
          shader->has_displacement = true;
          state.node_feature_mask = KERNEL_FEATURE_NODE_MASK_DISPLACEMENT;
          break;
        case SHADER_TYPE_BUMP: /* generate bump shader */
          generate = true;
          state.node_feature_mask = KERNEL_FEATURE_NODE_MASK_BUMP;
          break;
        default:
          break;
      }

      if (generate) {
        if (type == SHADER_TYPE_SURFACE) {
          find_aov_nodes_and_dependencies(state.aov_nodes, graph, &state);
        }
        generate_multi_closure(clin->link->parent, clin->link->parent, &state);
      }
    }

    /* compile output node */
    output->compile(*this);

    if (!state.aov_nodes.empty()) {
      /* AOV passes are only written if the object is directly visible, so
       * there is no point in evaluating all the nodes generated only for the
       * AOV outputs if that's not the case. Therefore, we insert
       * NODE_AOV_START into the shader before the AOV-only nodes are
       * generated which tells the kernel that it can stop evaluation
       * early if AOVs will not be written. */
      add_node(NODE_AOV_START, 0, 0, 0);
      generate_svm_nodes(state.aov_nodes, &state);
    }
  }

  /* add node to restore state after bump shader has finished */
  if (need_bump_state) {
    add_node(NODE_LEAVE_BUMP_EVAL, bump_state_offset);
  }

  /* if compile failed, generate empty shader */
  if (compile_failed) {
    current_svm_nodes.clear();
    compile_failed = false;
  }

  /* for bump shaders we fall thru to the surface shader, but if this is any other kind of shader
   * it ends here */
  if (type != SHADER_TYPE_BUMP) {
    add_node(NODE_END, 0, 0, 0);
  }
}

void SVMCompiler::compile(Shader *shader, array<int4> &svm_nodes, int index, Summary *summary)
{
  /* copy graph for shader with bump mapping */
  ShaderNode *output = shader->graph->output();
  int start_num_svm_nodes = svm_nodes.size();

  const double time_start = time_dt();

  bool has_bump = (shader->get_displacement_method() != DISPLACE_TRUE) &&
                  output->input("Surface")->link && output->input("Displacement")->link;

  /* finalize */
  {
    scoped_timer timer((summary != NULL) ? &summary->time_finalize : NULL);
    shader->graph->finalize(scene,
                            has_bump,
                            shader->has_integrator_dependency,
                            shader->get_displacement_method() == DISPLACE_BOTH);
  }

  current_shader = shader;

  shader->has_surface = false;
  shader->has_surface_emission = false;
  shader->has_surface_transparent = false;
  shader->has_surface_raytrace = false;
  shader->has_surface_bssrdf = false;
  shader->has_bump = has_bump;
  shader->has_bssrdf_bump = has_bump;
  shader->has_volume = false;
  shader->has_displacement = false;
  shader->has_surface_spatial_varying = false;
  shader->has_volume_spatial_varying = false;
  shader->has_volume_attribute_dependency = false;
  shader->has_integrator_dependency = false;

  /* generate bump shader */
  if (has_bump) {
    scoped_timer timer((summary != NULL) ? &summary->time_generate_bump : NULL);
    compile_type(shader, shader->graph, SHADER_TYPE_BUMP);
    svm_nodes[index].y = svm_nodes.size();
    svm_nodes.append(current_svm_nodes);
  }

  /* generate surface shader */
  {
    scoped_timer timer((summary != NULL) ? &summary->time_generate_surface : NULL);
    compile_type(shader, shader->graph, SHADER_TYPE_SURFACE);
    /* only set jump offset if there's no bump shader, as the bump shader will fall thru to this
     * one if it exists */
    if (!has_bump) {
      svm_nodes[index].y = svm_nodes.size();
    }
    svm_nodes.append(current_svm_nodes);
  }

  /* generate volume shader */
  {
    scoped_timer timer((summary != NULL) ? &summary->time_generate_volume : NULL);
    compile_type(shader, shader->graph, SHADER_TYPE_VOLUME);
    svm_nodes[index].z = svm_nodes.size();
    svm_nodes.append(current_svm_nodes);
  }

  /* generate displacement shader */
  {
    scoped_timer timer((summary != NULL) ? &summary->time_generate_displacement : NULL);
    compile_type(shader, shader->graph, SHADER_TYPE_DISPLACEMENT);
    svm_nodes[index].w = svm_nodes.size();
    svm_nodes.append(current_svm_nodes);
  }

  /* Fill in summary information. */
  if (summary != NULL) {
    summary->time_total = time_dt() - time_start;
    summary->peak_stack_usage = max_stack_use;
    summary->num_svm_nodes = svm_nodes.size() - start_num_svm_nodes;
  }
}

/* Compiler summary implementation. */

SVMCompiler::Summary::Summary()
    : num_svm_nodes(0),
      peak_stack_usage(0),
      time_finalize(0.0),
      time_generate_surface(0.0),
      time_generate_bump(0.0),
      time_generate_volume(0.0),
      time_generate_displacement(0.0),
      time_total(0.0)
{
}

string SVMCompiler::Summary::full_report() const
{
  string report = "";
  report += string_printf("Number of SVM nodes: %d\n", num_svm_nodes);
  report += string_printf("Peak stack usage:    %d\n", peak_stack_usage);

  report += string_printf("Time (in seconds):\n");
  report += string_printf("Finalize:            %f\n", time_finalize);
  report += string_printf("  Surface:           %f\n", time_generate_surface);
  report += string_printf("  Bump:              %f\n", time_generate_bump);
  report += string_printf("  Volume:            %f\n", time_generate_volume);
  report += string_printf("  Displacement:      %f\n", time_generate_displacement);
  report += string_printf("Generate:            %f\n",
                          time_generate_surface + time_generate_bump + time_generate_volume +
                              time_generate_displacement);
  report += string_printf("Total:               %f\n", time_total);

  return report;
}

/* Global state of the compiler. */

SVMCompiler::CompilerState::CompilerState(ShaderGraph *graph)
{
  int max_id = 0;
  foreach (ShaderNode *node, graph->nodes) {
    max_id = max(node->id, max_id);
  }
  nodes_done_flag.resize(max_id + 1, false);
  node_feature_mask = 0;
}

CCL_NAMESPACE_END
