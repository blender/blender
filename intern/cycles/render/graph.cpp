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

#include "render/attribute.h"
#include "render/graph.h"
#include "render/nodes.h"
#include "render/scene.h"
#include "render/shader.h"
#include "render/constant_fold.h"

#include "util/util_algorithm.h"
#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_md5.h"
#include "util/util_queue.h"

CCL_NAMESPACE_BEGIN

namespace {

bool check_node_inputs_has_links(const ShaderNode *node)
{
  foreach (const ShaderInput *in, node->inputs) {
    if (in->link) {
      return true;
    }
  }
  return false;
}

bool check_node_inputs_traversed(const ShaderNode *node, const ShaderNodeSet &done)
{
  foreach (const ShaderInput *in, node->inputs) {
    if (in->link) {
      if (done.find(in->link->parent) == done.end()) {
        return false;
      }
    }
  }
  return true;
}

} /* namespace */

/* Node */

ShaderNode::ShaderNode(const NodeType *type) : Node(type)
{
  name = type->name;
  id = -1;
  bump = SHADER_BUMP_NONE;
  special_type = SHADER_SPECIAL_TYPE_NONE;

  create_inputs_outputs(type);
}

ShaderNode::~ShaderNode()
{
  foreach (ShaderInput *socket, inputs)
    delete socket;

  foreach (ShaderOutput *socket, outputs)
    delete socket;
}

void ShaderNode::create_inputs_outputs(const NodeType *type)
{
  foreach (const SocketType &socket, type->inputs) {
    if (socket.flags & SocketType::LINKABLE) {
      inputs.push_back(new ShaderInput(socket, this));
    }
  }

  foreach (const SocketType &socket, type->outputs) {
    outputs.push_back(new ShaderOutput(socket, this));
  }
}

ShaderInput *ShaderNode::input(const char *name)
{
  foreach (ShaderInput *socket, inputs) {
    if (socket->name() == name)
      return socket;
  }

  return NULL;
}

ShaderOutput *ShaderNode::output(const char *name)
{
  foreach (ShaderOutput *socket, outputs)
    if (socket->name() == name)
      return socket;

  return NULL;
}

ShaderInput *ShaderNode::input(ustring name)
{
  foreach (ShaderInput *socket, inputs) {
    if (socket->name() == name)
      return socket;
  }

  return NULL;
}

ShaderOutput *ShaderNode::output(ustring name)
{
  foreach (ShaderOutput *socket, outputs)
    if (socket->name() == name)
      return socket;

  return NULL;
}

void ShaderNode::remove_input(ShaderInput *input)
{
  assert(input->link == NULL);
  delete input;
  inputs.erase(remove(inputs.begin(), inputs.end(), input), inputs.end());
}

void ShaderNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  foreach (ShaderInput *input, inputs) {
    if (!input->link) {
      if (input->flags() & SocketType::LINK_TEXTURE_GENERATED) {
        if (shader->has_surface)
          attributes->add(ATTR_STD_GENERATED);
        if (shader->has_volume)
          attributes->add(ATTR_STD_GENERATED_TRANSFORM);
      }
      else if (input->flags() & SocketType::LINK_TEXTURE_UV) {
        if (shader->has_surface)
          attributes->add(ATTR_STD_UV);
      }
    }
  }
}

bool ShaderNode::equals(const ShaderNode &other)
{
  if (type != other.type || bump != other.bump) {
    return false;
  }

  assert(inputs.size() == other.inputs.size());

  /* Compare unlinkable sockets */
  foreach (const SocketType &socket, type->inputs) {
    if (!(socket.flags & SocketType::LINKABLE)) {
      if (!Node::equals_value(other, socket)) {
        return false;
      }
    }
  }

  /* Compare linkable input sockets */
  for (int i = 0; i < inputs.size(); ++i) {
    ShaderInput *input_a = inputs[i], *input_b = other.inputs[i];
    if (input_a->link == NULL && input_b->link == NULL) {
      /* Unconnected inputs are expected to have the same value. */
      if (!Node::equals_value(other, input_a->socket_type)) {
        return false;
      }
    }
    else if (input_a->link != NULL && input_b->link != NULL) {
      /* Expect links are to come from the same exact socket. */
      if (input_a->link != input_b->link) {
        return false;
      }
    }
    else {
      /* One socket has a link and another has not, inputs can't be
       * considered equal.
       */
      return false;
    }
  }

  return true;
}

/* Graph */

ShaderGraph::ShaderGraph()
{
  finalized = false;
  simplified = false;
  num_node_ids = 0;
  add(new OutputNode());
}

ShaderGraph::~ShaderGraph()
{
  clear_nodes();
}

ShaderNode *ShaderGraph::add(ShaderNode *node)
{
  assert(!finalized);
  simplified = false;

  node->id = num_node_ids++;
  nodes.push_back(node);
  return node;
}

OutputNode *ShaderGraph::output()
{
  return (OutputNode *)nodes.front();
}

void ShaderGraph::connect(ShaderOutput *from, ShaderInput *to)
{
  assert(!finalized);
  assert(from && to);

  if (to->link) {
    fprintf(stderr, "Cycles shader graph connect: input already connected.\n");
    return;
  }

  if (from->type() != to->type()) {
    /* can't do automatic conversion from closure */
    if (from->type() == SocketType::CLOSURE) {
      fprintf(stderr,
              "Cycles shader graph connect: can only connect closure to closure "
              "(%s.%s to %s.%s).\n",
              from->parent->name.c_str(),
              from->name().c_str(),
              to->parent->name.c_str(),
              to->name().c_str());
      return;
    }

    /* add automatic conversion node in case of type mismatch */
    ShaderNode *convert;
    ShaderInput *convert_in;

    if (to->type() == SocketType::CLOSURE) {
      EmissionNode *emission = new EmissionNode();
      emission->color = make_float3(1.0f, 1.0f, 1.0f);
      emission->strength = 1.0f;
      convert = add(emission);
      /* Connect float inputs to Strength to save an additional Falue->Color conversion. */
      if (from->type() == SocketType::FLOAT) {
        convert_in = convert->input("Strength");
      }
      else {
        convert_in = convert->input("Color");
      }
    }
    else {
      convert = add(new ConvertNode(from->type(), to->type(), true));
      convert_in = convert->inputs[0];
    }

    connect(from, convert_in);
    connect(convert->outputs[0], to);
  }
  else {
    /* types match, just connect */
    to->link = from;
    from->links.push_back(to);
  }
}

void ShaderGraph::disconnect(ShaderOutput *from)
{
  assert(!finalized);
  simplified = false;

  foreach (ShaderInput *sock, from->links) {
    sock->link = NULL;
  }

  from->links.clear();
}

void ShaderGraph::disconnect(ShaderInput *to)
{
  assert(!finalized);
  assert(to->link);
  simplified = false;

  ShaderOutput *from = to->link;

  to->link = NULL;
  from->links.erase(remove(from->links.begin(), from->links.end(), to), from->links.end());
}

void ShaderGraph::relink(ShaderInput *from, ShaderInput *to)
{
  ShaderOutput *out = from->link;
  if (out) {
    disconnect(from);
    connect(out, to);
  }
  to->parent->copy_value(to->socket_type, *(from->parent), from->socket_type);
}

void ShaderGraph::relink(ShaderOutput *from, ShaderOutput *to)
{
  /* Copy because disconnect modifies this list. */
  vector<ShaderInput *> outputs = from->links;

  foreach (ShaderInput *sock, outputs) {
    disconnect(sock);
    if (to)
      connect(to, sock);
  }
}

void ShaderGraph::relink(ShaderNode *node, ShaderOutput *from, ShaderOutput *to)
{
  simplified = false;

  /* Copy because disconnect modifies this list */
  vector<ShaderInput *> outputs = from->links;

  /* Bypass node by moving all links from "from" to "to" */
  foreach (ShaderInput *sock, node->inputs) {
    if (sock->link)
      disconnect(sock);
  }

  foreach (ShaderInput *sock, outputs) {
    disconnect(sock);
    if (to)
      connect(to, sock);
  }
}

void ShaderGraph::simplify(Scene *scene)
{
  if (!simplified) {
    expand();
    default_inputs(scene->shader_manager->use_osl());
    clean(scene);
    refine_bump_nodes();

    simplified = true;
  }
}

void ShaderGraph::finalize(Scene *scene, bool do_bump, bool do_simplify, bool bump_in_object_space)
{
  /* before compiling, the shader graph may undergo a number of modifications.
   * currently we set default geometry shader inputs, and create automatic bump
   * from displacement. a graph can be finalized only once, and should not be
   * modified afterwards. */

  if (!finalized) {
    simplify(scene);

    if (do_bump)
      bump_from_displacement(bump_in_object_space);

    ShaderInput *surface_in = output()->input("Surface");
    ShaderInput *volume_in = output()->input("Volume");

    /* todo: make this work when surface and volume closures are tangled up */

    if (surface_in->link)
      transform_multi_closure(surface_in->link->parent, NULL, false);
    if (volume_in->link)
      transform_multi_closure(volume_in->link->parent, NULL, true);

    finalized = true;
  }
  else if (do_simplify) {
    simplify_settings(scene);
  }
}

void ShaderGraph::find_dependencies(ShaderNodeSet &dependencies, ShaderInput *input)
{
  /* find all nodes that this input depends on directly and indirectly */
  ShaderNode *node = (input->link) ? input->link->parent : NULL;

  if (node != NULL && dependencies.find(node) == dependencies.end()) {
    foreach (ShaderInput *in, node->inputs)
      find_dependencies(dependencies, in);

    dependencies.insert(node);
  }
}

void ShaderGraph::clear_nodes()
{
  foreach (ShaderNode *node, nodes) {
    delete node;
  }
  nodes.clear();
}

void ShaderGraph::copy_nodes(ShaderNodeSet &nodes, ShaderNodeMap &nnodemap)
{
  /* copy a set of nodes, and the links between them. the assumption is
   * made that all nodes that inputs are linked to are in the set too. */

  /* copy nodes */
  foreach (ShaderNode *node, nodes) {
    ShaderNode *nnode = node->clone();
    nnodemap[node] = nnode;

    /* create new inputs and outputs to recreate links and ensure
     * that we still point to valid SocketType if the NodeType
     * changed in cloning, as it does for OSL nodes */
    nnode->inputs.clear();
    nnode->outputs.clear();
    nnode->create_inputs_outputs(nnode->type);
  }

  /* recreate links */
  foreach (ShaderNode *node, nodes) {
    foreach (ShaderInput *input, node->inputs) {
      if (input->link) {
        /* find new input and output */
        ShaderNode *nfrom = nnodemap[input->link->parent];
        ShaderNode *nto = nnodemap[input->parent];
        ShaderOutput *noutput = nfrom->output(input->link->name());
        ShaderInput *ninput = nto->input(input->name());

        /* connect */
        connect(noutput, ninput);
      }
    }
  }
}

/* Graph simplification */
/* ******************** */

/* Remove proxy nodes.
 *
 * These only exists temporarily when exporting groups, and we must remove them
 * early so that node->attributes() and default links do not see them.
 */
void ShaderGraph::remove_proxy_nodes()
{
  vector<bool> removed(num_node_ids, false);
  bool any_node_removed = false;

  foreach (ShaderNode *node, nodes) {
    if (node->special_type == SHADER_SPECIAL_TYPE_PROXY) {
      ConvertNode *proxy = static_cast<ConvertNode *>(node);
      ShaderInput *input = proxy->inputs[0];
      ShaderOutput *output = proxy->outputs[0];

      /* bypass the proxy node */
      if (input->link) {
        relink(proxy, output, input->link);
      }
      else {
        /* Copy because disconnect modifies this list */
        vector<ShaderInput *> links(output->links);

        foreach (ShaderInput *to, links) {
          /* remove any autoconvert nodes too if they lead to
           * sockets with an automatically set default value */
          ShaderNode *tonode = to->parent;

          if (tonode->special_type == SHADER_SPECIAL_TYPE_AUTOCONVERT) {
            bool all_links_removed = true;
            vector<ShaderInput *> links = tonode->outputs[0]->links;

            foreach (ShaderInput *autoin, links) {
              if (autoin->flags() & SocketType::DEFAULT_LINK_MASK)
                disconnect(autoin);
              else
                all_links_removed = false;
            }

            if (all_links_removed)
              removed[tonode->id] = true;
          }

          disconnect(to);

          /* transfer the default input value to the target socket */
          tonode->copy_value(to->socket_type, *proxy, input->socket_type);
        }
      }

      removed[proxy->id] = true;
      any_node_removed = true;
    }
  }

  /* remove nodes */
  if (any_node_removed) {
    list<ShaderNode *> newnodes;

    foreach (ShaderNode *node, nodes) {
      if (!removed[node->id])
        newnodes.push_back(node);
      else
        delete node;
    }

    nodes = newnodes;
  }
}

/* Constant folding.
 *
 * Try to constant fold some nodes, and pipe result directly to
 * the input socket of connected nodes.
 */
void ShaderGraph::constant_fold(Scene *scene)
{
  ShaderNodeSet done, scheduled;
  queue<ShaderNode *> traverse_queue;

  bool has_displacement = (output()->input("Displacement")->link != NULL);

  /* Schedule nodes which doesn't have any dependencies. */
  foreach (ShaderNode *node, nodes) {
    if (!check_node_inputs_has_links(node)) {
      traverse_queue.push(node);
      scheduled.insert(node);
    }
  }

  while (!traverse_queue.empty()) {
    ShaderNode *node = traverse_queue.front();
    traverse_queue.pop();
    done.insert(node);
    foreach (ShaderOutput *output, node->outputs) {
      if (output->links.size() == 0) {
        continue;
      }
      /* Schedule node which was depending on the value,
       * when possible. Do it before disconnect.
       */
      foreach (ShaderInput *input, output->links) {
        if (scheduled.find(input->parent) != scheduled.end()) {
          /* Node might not be optimized yet but scheduled already
           * by other dependencies. No need to re-schedule it.
           */
          continue;
        }
        /* Schedule node if its inputs are fully done. */
        if (check_node_inputs_traversed(input->parent, done)) {
          traverse_queue.push(input->parent);
          scheduled.insert(input->parent);
        }
      }
      /* Optimize current node. */
      ConstantFolder folder(this, node, output, scene);
      node->constant_fold(folder);
    }
  }

  /* Folding might have removed all nodes connected to the displacement output
   * even tho there is displacement to be applied, so add in a value node if
   * that happens to ensure there is still a valid graph for displacement.
   */
  if (has_displacement && !output()->input("Displacement")->link) {
    ColorNode *value = (ColorNode *)add(new ColorNode());
    value->value = output()->displacement;

    connect(value->output("Color"), output()->input("Displacement"));
  }
}

/* Simplification. */
void ShaderGraph::simplify_settings(Scene *scene)
{
  foreach (ShaderNode *node, nodes) {
    node->simplify_settings(scene);
  }
}

/* Deduplicate nodes with same settings. */
void ShaderGraph::deduplicate_nodes()
{
  /* NOTES:
   * - Deduplication happens for nodes which has same exact settings and same
   *   exact input links configuration (either connected to same output or has
   *   the same exact default value).
   * - Deduplication happens in the bottom-top manner, so we know for fact that
   *   all traversed nodes are either can not be deduplicated at all or were
   *   already deduplicated.
   */

  ShaderNodeSet scheduled, done;
  map<ustring, ShaderNodeSet> candidates;
  queue<ShaderNode *> traverse_queue;
  int num_deduplicated = 0;

  /* Schedule nodes which doesn't have any dependencies. */
  foreach (ShaderNode *node, nodes) {
    if (!check_node_inputs_has_links(node)) {
      traverse_queue.push(node);
      scheduled.insert(node);
    }
  }

  while (!traverse_queue.empty()) {
    ShaderNode *node = traverse_queue.front();
    traverse_queue.pop();
    done.insert(node);
    /* Schedule the nodes which were depending on the current node. */
    bool has_output_links = false;
    foreach (ShaderOutput *output, node->outputs) {
      foreach (ShaderInput *input, output->links) {
        has_output_links = true;
        if (scheduled.find(input->parent) != scheduled.end()) {
          /* Node might not be optimized yet but scheduled already
           * by other dependencies. No need to re-schedule it.
           */
          continue;
        }
        /* Schedule node if its inputs are fully done. */
        if (check_node_inputs_traversed(input->parent, done)) {
          traverse_queue.push(input->parent);
          scheduled.insert(input->parent);
        }
      }
    }
    /* Only need to care about nodes that are actually used */
    if (!has_output_links) {
      continue;
    }
    /* Try to merge this node with another one. */
    ShaderNode *merge_with = NULL;
    foreach (ShaderNode *other_node, candidates[node->type->name]) {
      if (node != other_node && node->equals(*other_node)) {
        merge_with = other_node;
        break;
      }
    }
    /* If found an equivalent, merge; otherwise keep node for later merges */
    if (merge_with != NULL) {
      for (int i = 0; i < node->outputs.size(); ++i) {
        relink(node, node->outputs[i], merge_with->outputs[i]);
      }
      num_deduplicated++;
    }
    else {
      candidates[node->type->name].insert(node);
    }
  }

  if (num_deduplicated > 0) {
    VLOG(1) << "Deduplicated " << num_deduplicated << " nodes.";
  }
}

/* Check whether volume output has meaningful nodes, otherwise
 * disconnect the output.
 */
void ShaderGraph::verify_volume_output()
{
  /* Check whether we can optimize the whole volume graph out. */
  ShaderInput *volume_in = output()->input("Volume");
  if (volume_in->link == NULL) {
    return;
  }
  bool has_valid_volume = false;
  ShaderNodeSet scheduled;
  queue<ShaderNode *> traverse_queue;
  /* Schedule volume output. */
  traverse_queue.push(volume_in->link->parent);
  scheduled.insert(volume_in->link->parent);
  /* Traverse down the tree. */
  while (!traverse_queue.empty()) {
    ShaderNode *node = traverse_queue.front();
    traverse_queue.pop();
    /* Node is fully valid for volume, can't optimize anything out. */
    if (node->has_volume_support()) {
      has_valid_volume = true;
      break;
    }
    foreach (ShaderInput *input, node->inputs) {
      if (input->link == NULL) {
        continue;
      }
      if (scheduled.find(input->link->parent) != scheduled.end()) {
        continue;
      }
      traverse_queue.push(input->link->parent);
      scheduled.insert(input->link->parent);
    }
  }
  if (!has_valid_volume) {
    VLOG(1) << "Disconnect meaningless volume output.";
    disconnect(volume_in->link);
  }
}

void ShaderGraph::break_cycles(ShaderNode *node, vector<bool> &visited, vector<bool> &on_stack)
{
  visited[node->id] = true;
  on_stack[node->id] = true;

  foreach (ShaderInput *input, node->inputs) {
    if (input->link) {
      ShaderNode *depnode = input->link->parent;

      if (on_stack[depnode->id]) {
        /* break cycle */
        disconnect(input);
        fprintf(stderr, "Cycles shader graph: detected cycle in graph, connection removed.\n");
      }
      else if (!visited[depnode->id]) {
        /* visit dependencies */
        break_cycles(depnode, visited, on_stack);
      }
    }
  }

  on_stack[node->id] = false;
}

void ShaderGraph::compute_displacement_hash()
{
  /* Compute hash of all nodes linked to displacement, to detect if we need
   * to recompute displacement when shader nodes change. */
  ShaderInput *displacement_in = output()->input("Displacement");

  if (!displacement_in->link) {
    displacement_hash = "";
    return;
  }

  ShaderNodeSet nodes_displace;
  find_dependencies(nodes_displace, displacement_in);

  MD5Hash md5;
  foreach (ShaderNode *node, nodes_displace) {
    node->hash(md5);
    foreach (ShaderInput *input, node->inputs) {
      int link_id = (input->link) ? input->link->parent->id : 0;
      md5.append((uint8_t *)&link_id, sizeof(link_id));
    }

    if (node->special_type == SHADER_SPECIAL_TYPE_OSL) {
      /* Hash takes into account socket values, to detect changes
       * in the code of the node we need an exception. */
      OSLNode *oslnode = static_cast<OSLNode *>(node);
      md5.append(oslnode->bytecode_hash);
    }
  }

  displacement_hash = md5.get_hex();
}

void ShaderGraph::clean(Scene *scene)
{
  /* Graph simplification */

  /* NOTE: Remove proxy nodes was already done. */
  constant_fold(scene);
  simplify_settings(scene);
  deduplicate_nodes();
  verify_volume_output();

  /* we do two things here: find cycles and break them, and remove unused
   * nodes that don't feed into the output. how cycles are broken is
   * undefined, they are invalid input, the important thing is to not crash */

  vector<bool> visited(num_node_ids, false);
  vector<bool> on_stack(num_node_ids, false);

  /* break cycles */
  break_cycles(output(), visited, on_stack);

  /* disconnect unused nodes */
  foreach (ShaderNode *node, nodes) {
    if (!visited[node->id]) {
      foreach (ShaderInput *to, node->inputs) {
        ShaderOutput *from = to->link;

        if (from) {
          to->link = NULL;
          from->links.erase(remove(from->links.begin(), from->links.end(), to), from->links.end());
        }
      }
    }
  }

  /* remove unused nodes */
  list<ShaderNode *> newnodes;

  foreach (ShaderNode *node, nodes) {
    if (visited[node->id])
      newnodes.push_back(node);
    else
      delete node;
  }

  nodes = newnodes;
}

void ShaderGraph::expand()
{
  /* Call expand on all nodes, to generate additional nodes. */
  foreach (ShaderNode *node, nodes) {
    node->expand(this);
  }
}

void ShaderGraph::default_inputs(bool do_osl)
{
  /* nodes can specify default texture coordinates, for now we give
   * everything the position by default, except for the sky texture */

  ShaderNode *geom = NULL;
  ShaderNode *texco = NULL;

  foreach (ShaderNode *node, nodes) {
    foreach (ShaderInput *input, node->inputs) {
      if (!input->link && (!(input->flags() & SocketType::OSL_INTERNAL) || do_osl)) {
        if (input->flags() & SocketType::LINK_TEXTURE_GENERATED) {
          if (!texco)
            texco = new TextureCoordinateNode();

          connect(texco->output("Generated"), input);
        }
        if (input->flags() & SocketType::LINK_TEXTURE_NORMAL) {
          if (!texco)
            texco = new TextureCoordinateNode();

          connect(texco->output("Normal"), input);
        }
        else if (input->flags() & SocketType::LINK_TEXTURE_UV) {
          if (!texco)
            texco = new TextureCoordinateNode();

          connect(texco->output("UV"), input);
        }
        else if (input->flags() & SocketType::LINK_INCOMING) {
          if (!geom)
            geom = new GeometryNode();

          connect(geom->output("Incoming"), input);
        }
        else if (input->flags() & SocketType::LINK_NORMAL) {
          if (!geom)
            geom = new GeometryNode();

          connect(geom->output("Normal"), input);
        }
        else if (input->flags() & SocketType::LINK_POSITION) {
          if (!geom)
            geom = new GeometryNode();

          connect(geom->output("Position"), input);
        }
        else if (input->flags() & SocketType::LINK_TANGENT) {
          if (!geom)
            geom = new GeometryNode();

          connect(geom->output("Tangent"), input);
        }
      }
    }
  }

  if (geom)
    add(geom);
  if (texco)
    add(texco);
}

void ShaderGraph::refine_bump_nodes()
{
  /* we transverse the node graph looking for bump nodes, when we find them,
   * like in bump_from_displacement(), we copy the sub-graph defined from "bump"
   * input to the inputs "center","dx" and "dy" What is in "bump" input is moved
   * to "center" input. */

  foreach (ShaderNode *node, nodes) {
    if (node->special_type == SHADER_SPECIAL_TYPE_BUMP && node->input("Height")->link) {
      ShaderInput *bump_input = node->input("Height");
      ShaderNodeSet nodes_bump;

      /* make 2 extra copies of the subgraph defined in Bump input */
      ShaderNodeMap nodes_dx;
      ShaderNodeMap nodes_dy;

      /* find dependencies for the given input */
      find_dependencies(nodes_bump, bump_input);

      copy_nodes(nodes_bump, nodes_dx);
      copy_nodes(nodes_bump, nodes_dy);

      /* mark nodes to indicate they are use for bump computation, so
         that any texture coordinates are shifted by dx/dy when sampling */
      foreach (ShaderNode *node, nodes_bump)
        node->bump = SHADER_BUMP_CENTER;
      foreach (NodePair &pair, nodes_dx)
        pair.second->bump = SHADER_BUMP_DX;
      foreach (NodePair &pair, nodes_dy)
        pair.second->bump = SHADER_BUMP_DY;

      ShaderOutput *out = bump_input->link;
      ShaderOutput *out_dx = nodes_dx[out->parent]->output(out->name());
      ShaderOutput *out_dy = nodes_dy[out->parent]->output(out->name());

      connect(out_dx, node->input("SampleX"));
      connect(out_dy, node->input("SampleY"));

      /* add generated nodes */
      foreach (NodePair &pair, nodes_dx)
        add(pair.second);
      foreach (NodePair &pair, nodes_dy)
        add(pair.second);

      /* connect what is connected is bump to samplecenter input*/
      connect(out, node->input("SampleCenter"));

      /* bump input is just for connectivity purpose for the graph input,
       * we re-connected this input to samplecenter, so lets disconnect it
       * from bump input */
      disconnect(bump_input);
    }
  }
}

void ShaderGraph::bump_from_displacement(bool use_object_space)
{
  /* generate bump mapping automatically from displacement. bump mapping is
   * done using a 3-tap filter, computing the displacement at the center,
   * and two other positions shifted by ray differentials.
   *
   * since the input to displacement is a node graph, we need to ensure that
   * all texture coordinates use are shift by the ray differentials. for this
   * reason we make 3 copies of the node subgraph defining the displacement,
   * with each different geometry and texture coordinate nodes that generate
   * different shifted coordinates.
   *
   * these 3 displacement values are then fed into the bump node, which will
   * output the perturbed normal. */

  ShaderInput *displacement_in = output()->input("Displacement");

  if (!displacement_in->link)
    return;

  /* find dependencies for the given input */
  ShaderNodeSet nodes_displace;
  find_dependencies(nodes_displace, displacement_in);

  /* copy nodes for 3 bump samples */
  ShaderNodeMap nodes_center;
  ShaderNodeMap nodes_dx;
  ShaderNodeMap nodes_dy;

  copy_nodes(nodes_displace, nodes_center);
  copy_nodes(nodes_displace, nodes_dx);
  copy_nodes(nodes_displace, nodes_dy);

  /* mark nodes to indicate they are use for bump computation, so
   * that any texture coordinates are shifted by dx/dy when sampling */
  foreach (NodePair &pair, nodes_center)
    pair.second->bump = SHADER_BUMP_CENTER;
  foreach (NodePair &pair, nodes_dx)
    pair.second->bump = SHADER_BUMP_DX;
  foreach (NodePair &pair, nodes_dy)
    pair.second->bump = SHADER_BUMP_DY;

  /* add set normal node and connect the bump normal ouput to the set normal
   * output, so it can finally set the shader normal, note we are only doing
   * this for bump from displacement, this will be the only bump allowed to
   * overwrite the shader normal */
  ShaderNode *set_normal = add(new SetNormalNode());

  /* add bump node and connect copied graphs to it */
  BumpNode *bump = (BumpNode *)add(new BumpNode());
  bump->use_object_space = use_object_space;
  bump->distance = 1.0f;

  ShaderOutput *out = displacement_in->link;
  ShaderOutput *out_center = nodes_center[out->parent]->output(out->name());
  ShaderOutput *out_dx = nodes_dx[out->parent]->output(out->name());
  ShaderOutput *out_dy = nodes_dy[out->parent]->output(out->name());

  /* convert displacement vector to height */
  VectorMathNode *dot_center = (VectorMathNode *)add(new VectorMathNode());
  VectorMathNode *dot_dx = (VectorMathNode *)add(new VectorMathNode());
  VectorMathNode *dot_dy = (VectorMathNode *)add(new VectorMathNode());

  dot_center->type = NODE_VECTOR_MATH_DOT_PRODUCT;
  dot_dx->type = NODE_VECTOR_MATH_DOT_PRODUCT;
  dot_dy->type = NODE_VECTOR_MATH_DOT_PRODUCT;

  GeometryNode *geom = (GeometryNode *)add(new GeometryNode());
  connect(geom->output("Normal"), dot_center->input("Vector2"));
  connect(geom->output("Normal"), dot_dx->input("Vector2"));
  connect(geom->output("Normal"), dot_dy->input("Vector2"));

  connect(out_center, dot_center->input("Vector1"));
  connect(out_dx, dot_dx->input("Vector1"));
  connect(out_dy, dot_dy->input("Vector1"));

  connect(dot_center->output("Value"), bump->input("SampleCenter"));
  connect(dot_dx->output("Value"), bump->input("SampleX"));
  connect(dot_dy->output("Value"), bump->input("SampleY"));

  /* connect the bump out to the set normal in: */
  connect(bump->output("Normal"), set_normal->input("Direction"));

  /* connect to output node */
  connect(set_normal->output("Normal"), output()->input("Normal"));

  /* finally, add the copied nodes to the graph. we can't do this earlier
   * because we would create dependency cycles in the above loop */
  foreach (NodePair &pair, nodes_center)
    add(pair.second);
  foreach (NodePair &pair, nodes_dx)
    add(pair.second);
  foreach (NodePair &pair, nodes_dy)
    add(pair.second);
}

void ShaderGraph::transform_multi_closure(ShaderNode *node, ShaderOutput *weight_out, bool volume)
{
  /* for SVM in multi closure mode, this transforms the shader mix/add part of
   * the graph into nodes that feed weights into closure nodes. this is too
   * avoid building a closure tree and then flattening it, and instead write it
   * directly to an array */

  if (node->special_type == SHADER_SPECIAL_TYPE_COMBINE_CLOSURE) {
    ShaderInput *fin = node->input("Fac");
    ShaderInput *cl1in = node->input("Closure1");
    ShaderInput *cl2in = node->input("Closure2");
    ShaderOutput *weight1_out, *weight2_out;

    if (fin) {
      /* mix closure: add node to mix closure weights */
      MixClosureWeightNode *mix_node = new MixClosureWeightNode();
      add(mix_node);
      ShaderInput *fac_in = mix_node->input("Fac");
      ShaderInput *weight_in = mix_node->input("Weight");

      if (fin->link)
        connect(fin->link, fac_in);
      else
        mix_node->fac = node->get_float(fin->socket_type);

      if (weight_out)
        connect(weight_out, weight_in);

      weight1_out = mix_node->output("Weight1");
      weight2_out = mix_node->output("Weight2");
    }
    else {
      /* add closure: just pass on any weights */
      weight1_out = weight_out;
      weight2_out = weight_out;
    }

    if (cl1in->link)
      transform_multi_closure(cl1in->link->parent, weight1_out, volume);
    if (cl2in->link)
      transform_multi_closure(cl2in->link->parent, weight2_out, volume);
  }
  else {
    ShaderInput *weight_in = node->input((volume) ? "VolumeMixWeight" : "SurfaceMixWeight");

    /* not a closure node? */
    if (!weight_in)
      return;

    /* already has a weight connected to it? add weights */
    float weight_value = node->get_float(weight_in->socket_type);
    if (weight_in->link || weight_value != 0.0f) {
      MathNode *math_node = new MathNode();
      add(math_node);

      if (weight_in->link)
        connect(weight_in->link, math_node->input("Value1"));
      else
        math_node->value1 = weight_value;

      if (weight_out)
        connect(weight_out, math_node->input("Value2"));
      else
        math_node->value2 = 1.0f;

      weight_out = math_node->output("Value");
      if (weight_in->link)
        disconnect(weight_in);
    }

    /* connected to closure mix weight */
    if (weight_out)
      connect(weight_out, weight_in);
    else
      node->set(weight_in->socket_type, weight_value + 1.0f);
  }
}

int ShaderGraph::get_num_closures()
{
  int num_closures = 0;
  foreach (ShaderNode *node, nodes) {
    ClosureType closure_type = node->get_closure_type();
    if (closure_type == CLOSURE_NONE_ID) {
      continue;
    }
    else if (CLOSURE_IS_BSSRDF(closure_type)) {
      num_closures += 3;
    }
    else if (CLOSURE_IS_GLASS(closure_type)) {
      num_closures += 2;
    }
    else if (CLOSURE_IS_BSDF_MULTISCATTER(closure_type)) {
      num_closures += 2;
    }
    else if (CLOSURE_IS_PRINCIPLED(closure_type)) {
      num_closures += 8;
    }
    else if (CLOSURE_IS_VOLUME(closure_type)) {
      num_closures += VOLUME_STACK_SIZE;
    }
    else if (closure_type == CLOSURE_BSDF_HAIR_PRINCIPLED_ID) {
      num_closures += 4;
    }
    else {
      ++num_closures;
    }
  }
  return num_closures;
}

void ShaderGraph::dump_graph(const char *filename)
{
  FILE *fd = fopen(filename, "w");

  if (fd == NULL) {
    printf("Error opening file for dumping the graph: %s\n", filename);
    return;
  }

  fprintf(fd, "digraph shader_graph {\n");
  fprintf(fd, "ranksep=1.5\n");
  fprintf(fd, "rankdir=LR\n");
  fprintf(fd, "splines=false\n");

  foreach (ShaderNode *node, nodes) {
    fprintf(fd, "// NODE: %p\n", node);
    fprintf(fd, "\"%p\" [shape=record,label=\"{", node);
    if (node->inputs.size()) {
      fprintf(fd, "{");
      foreach (ShaderInput *socket, node->inputs) {
        if (socket != node->inputs[0]) {
          fprintf(fd, "|");
        }
        fprintf(fd, "<IN_%p>%s", socket, socket->name().c_str());
      }
      fprintf(fd, "}|");
    }
    fprintf(fd, "%s", node->name.c_str());
    if (node->bump == SHADER_BUMP_CENTER) {
      fprintf(fd, " (bump:center)");
    }
    else if (node->bump == SHADER_BUMP_DX) {
      fprintf(fd, " (bump:dx)");
    }
    else if (node->bump == SHADER_BUMP_DY) {
      fprintf(fd, " (bump:dy)");
    }
    if (node->outputs.size()) {
      fprintf(fd, "|{");
      foreach (ShaderOutput *socket, node->outputs) {
        if (socket != node->outputs[0]) {
          fprintf(fd, "|");
        }
        fprintf(fd, "<OUT_%p>%s", socket, socket->name().c_str());
      }
      fprintf(fd, "}");
    }
    fprintf(fd, "}\"]");
  }

  foreach (ShaderNode *node, nodes) {
    foreach (ShaderOutput *output, node->outputs) {
      foreach (ShaderInput *input, output->links) {
        fprintf(fd,
                "// CONNECTION: OUT_%p->IN_%p (%s:%s)\n",
                output,
                input,
                output->name().c_str(),
                input->name().c_str());
        fprintf(fd,
                "\"%p\":\"OUT_%p\":e -> \"%p\":\"IN_%p\":w [label=\"\"]\n",
                output->parent,
                output,
                input->parent,
                input);
      }
    }
  }

  fprintf(fd, "}\n");
  fclose(fd);
}

CCL_NAMESPACE_END
