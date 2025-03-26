/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/light_tree_debug.h"

#include "scene/light.h"
#include "scene/light_tree.h"
#include "scene/object.h"
#include "scene/scene.h"

#include "util/path.h"
#include "util/string.h"

CCL_NAMESPACE_BEGIN

static string get_node_id(const LightTreeNode &node)
{
  return string_printf("node@%p", &node);
}

static string get_emitter_id(const LightTreeEmitter &emitter)
{
  return string_printf("emitter@%p", &emitter);
}

static string set_membership_str(const uint64_t set_membership)
{
  if (set_membership == ~uint64_t(0)) {
    return "ALL";
  }
  return std::to_string(set_membership);
}

static void recursive_print_node(FILE *file, const LightTreeNode &node)
{
  int field = 0;

  string label = string_printf("<f%d> node @%p", field++, &node);
  if (node.is_instance()) {
    label += string_printf("| <f%d> instance", field++);
  }
  if (node.is_leaf()) {
    label += string_printf("| <f%d> leaf", field++);
  }
  if (node.is_inner()) {
    label += string_printf("| <f%d> inner", field++);
  }
  if (node.is_distant()) {
    label += string_printf("| <f%d> distant", field++);
  }

  if (node.light_link.set_membership == ~uint64_t(0)) {
    label += string_printf("| <f%d> set membership:ALL", field++);
  }
  else {
    label += string_printf("| <f%d> set membership:%s",
                           field++,
                           set_membership_str(node.light_link.set_membership).c_str());
  }

  label += string_printf(
      "| <f%d> shareable:%s", field++, node.light_link.shareable ? "True" : "False");

  if (node.is_inner()) {
    label += string_printf("| <left> left");
    label += string_printf("| <right> right");
  }
  else if (node.is_leaf()) {
    label += string_printf("| <emitters> emitters");
  }

  fprintf(file, "\"%s\" [\n", get_node_id(node).c_str());
  fprintf(file, "  label = \"%s\"\n", label.c_str());
  fprintf(file, "  shape = \"record\"\n");
  fprintf(file, "];\n");

  if (node.is_inner()) {
    const LightTreeNode &left_node = *node.get_inner().children[LightTree::left];
    const LightTreeNode &right_node = *node.get_inner().children[LightTree::right];

    recursive_print_node(file, left_node);
    recursive_print_node(file, right_node);
  }
}

static void print_emitters(FILE *file, const Scene &scene, const LightTree &tree)
{
  const size_t num_emitters = tree.num_emitters();
  const LightTreeEmitter *emitters = tree.get_emitters();
  for (size_t i = 0; i < num_emitters; ++i) {
    const LightTreeEmitter &emitter = emitters[i];

    int field = 0;

    string label = string_printf("<f%d> emitter %s", field++, std::to_string(i).c_str());

    /* Emitter details (type, object or light name). */
    const Object &object = *scene.objects[emitter.object_id];
    if (emitter.is_light()) {
      label += string_printf("|<f%d> light", field++);
    }
    else if (emitter.is_triangle()) {
      label += string_printf("|<f%d> triangle", field++);
    }
    else if (emitter.is_mesh()) {
      label += string_printf("|<f%d> mesh", field++);
    }
    label += string_printf("|<f%d> %s", field++, object.name.c_str());

    /* Light linking. */
    if (emitter.light_set_membership == ~uint64_t(0)) {
      label += string_printf("|<f%d> set membership:ALL", field++);
    }
    else {
      label += string_printf("|<f%d> set membership:%s",
                             field++,
                             set_membership_str(emitter.light_set_membership).c_str());
    }

    /* Bounding box. */
    label += string_printf("|<f%d> bbox.min (%f %f %f)",
                           field++,
                           double(emitter.measure.bbox.min.x),
                           double(emitter.measure.bbox.min.y),
                           double(emitter.measure.bbox.min.z));
    label += string_printf("|<f%d> bbox.max (%f %f %f)",
                           field++,
                           double(emitter.measure.bbox.max.x),
                           double(emitter.measure.bbox.max.y),
                           double(emitter.measure.bbox.max.z));

    /* Orientation bounds. */
    label += string_printf("|<f%d> bcone.axis (%f %f %f)",
                           field++,
                           double(emitter.measure.bcone.axis.x),
                           double(emitter.measure.bcone.axis.y),
                           double(emitter.measure.bcone.axis.z));
    label += string_printf("|<f%d> theta_o %f, theta_e %f",
                           field++,
                           double(emitter.measure.bcone.theta_o),
                           double(emitter.measure.bcone.theta_e));

    /* Print node to the file. */
    fprintf(file, "\"%s\" [\n", get_emitter_id(emitter).c_str());
    fprintf(file, "  label = \"%s\"\n", label.c_str());
    fprintf(file, "  shape = \"record\"\n");
    fprintf(file, "];\n");
  }
}

static void recursive_print_node_relations(FILE *file,
                                           const LightTree &tree,
                                           const LightTreeNode &node,
                                           int &relation_id)
{
  const string from_node_id = get_node_id(node);

  if (node.is_leaf() || node.is_distant()) {
    const LightTreeEmitter *emitters = tree.get_emitters();
    for (int i = 0; i < node.get_leaf().num_emitters; i++) {
      const LightTreeEmitter &emitter = emitters[node.get_leaf().first_emitter_index + i];
      fprintf(file,
              "\"%s\":<emitters> -> \"%s\":f0 [ id = %d  ];\n",
              from_node_id.c_str(),
              get_emitter_id(emitter).c_str(),
              relation_id++);
    }
    return;
  }

  if (!node.is_inner()) {
    return;
  }

  const LightTreeNode &left_node = *node.get_inner().children[LightTree::left];
  const LightTreeNode &right_node = *node.get_inner().children[LightTree::right];

  const string left_node_id = get_node_id(left_node);
  const string right_node_id = get_node_id(right_node);

  fprintf(file,
          "\"%s\":left -> \"%s\":f0 [ id = %d  ];\n",
          from_node_id.c_str(),
          left_node_id.c_str(),
          relation_id++);

  fprintf(file,
          "\"%s\":right -> \"%s\":f0 [ id = %d  ];\n",
          from_node_id.c_str(),
          right_node_id.c_str(),
          relation_id++);

  recursive_print_node_relations(file, tree, left_node, relation_id);
  recursive_print_node_relations(file, tree, right_node, relation_id);
}

void light_tree_plot_to_file(const Scene &scene,
                             const LightTree &tree,
                             const LightTreeNode &root_node,
                             const string &filename)
{
  FILE *file = path_fopen(filename, "w");
  if (!file) {
    return;
  }

  int relation_id = 0;

  fprintf(file, "digraph g {\n");
  fprintf(file, "graph [\n");
  fprintf(file, "  rankdir = \"LR\"\n");
  fprintf(file, "];\n");
  recursive_print_node(file, root_node);
  print_emitters(file, scene, tree);
  recursive_print_node_relations(file, tree, root_node, relation_id);
  fprintf(file, "}\n");

  fclose(file);
}

static string get_knode_id(const KernelLightTreeNode &knode)
{
  return string_printf("knode@%p", &knode);
}

static void recursive_print_knode(FILE *file,
                                  const uint knode_index,
                                  const KernelLightTreeNode *knodes,
                                  int &relation_id)
{
  const KernelLightTreeNode &knode = knodes[knode_index];

  /* The distant node is also considered o leaf node. */
  const bool is_leaf = knode.type >= LIGHT_TREE_LEAF;
  const bool is_inner = !is_leaf;

  assert(!is_inner || knode.type != LIGHT_TREE_INSTANCE);

  int field = 0;

  string label = string_printf("<f%d> knode %u", field++, knode_index);

  /* Bounding box. */
  label += string_printf("|<f%d> min (%f %f %f)",
                         field++,
                         double(knode.bbox.min.x),
                         double(knode.bbox.min.y),
                         double(knode.bbox.min.z));
  label += string_printf("|<f%d> max (%f %f %f)",
                         field++,
                         double(knode.bbox.max.x),
                         double(knode.bbox.max.y),
                         double(knode.bbox.max.z));

  /* Orientation bounds. */
  label += string_printf("|<f%d> bcone.axis (%f %f %f)",
                         field++,
                         double(knode.bcone.axis.x),
                         double(knode.bcone.axis.y),
                         double(knode.bcone.axis.z));
  label += string_printf("|<f%d> theta_o %f, theta_e %f",
                         field++,
                         double(knode.bcone.theta_o),
                         double(knode.bcone.theta_e));

  label += string_printf("|<f%d> energy %f", field++, knode.energy);

  if (is_leaf) {
    label += string_printf("|<f%d> first emitter %d", field++, knode.leaf.first_emitter);
    label += string_printf("|<f%d> num emitters %d", field++, knode.num_emitters);
  }

  label += string_printf("|<f%d> bit trail %u", field++, knode.bit_trail);
  label += string_printf("|<f%d> bit skip %d", field++, int(knode.bit_skip));

  if (is_inner) {
    label += string_printf("| <left> left");
    label += string_printf("| <right> right");
  }

  const string knode_id = get_knode_id(knode);

  /* Print node to the file. */
  fprintf(file, "\"%s\" [\n", knode_id.c_str());
  fprintf(file, "  label = \"%s\"\n", label.c_str());
  fprintf(file, "  shape = \"record\"\n");
  fprintf(file, "];\n");

  /* Print relations. */
  if (is_inner) {
    recursive_print_knode(file, knode.inner.left_child, knodes, relation_id);
    recursive_print_knode(file, knode.inner.right_child, knodes, relation_id);

    fprintf(file,
            "\"%s\":left -> \"%s\":f0 [ id = %d  ];\n",
            knode_id.c_str(),
            get_knode_id(knodes[knode.inner.left_child]).c_str(),
            relation_id++);
    fprintf(file,
            "\"%s\":right -> \"%s\":f0 [ id = %d  ];\n",
            knode_id.c_str(),
            get_knode_id(knodes[knode.inner.right_child]).c_str(),
            relation_id++);
  }
}

void klight_tree_plot_to_file(uint root, const KernelLightTreeNode *knodes, const string &filename)
{
  FILE *file = path_fopen(filename, "w");
  if (!file) {
    return;
  }

  int relation_id = 0;

  fprintf(file, "digraph g {\n");
  fprintf(file, "graph [\n");
  fprintf(file, "  rankdir = \"LR\"\n");
  fprintf(file, "];\n");
  recursive_print_knode(file, root, knodes, relation_id);
  fprintf(file, "}\n");

  fclose(file);
}

CCL_NAMESPACE_END
