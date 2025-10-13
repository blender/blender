/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Implementation of tools for debugging the depsgraph
 */

#include <cstdarg>

#include "BLI_dot_export.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_debug.hh"

#include "intern/depsgraph.hh"
#include "intern/depsgraph_relation.hh"

#include "intern/node/deg_node_component.hh"
#include "intern/node/deg_node_id.hh"
#include "intern/node/deg_node_operation.hh"
#include "intern/node/deg_node_time.hh"

#include <sstream>

namespace deg = blender::deg;
namespace dot_export = blender::dot_export;

/* ****************** */
/* Graphviz Debugging */

namespace blender::deg {

/* Only one should be enabled, defines whether graphviz nodes
 * get colored by individual types or classes.
 */
#define COLOR_SCHEME_NODE_CLASS 1
// #define COLOR_SCHEME_NODE_TYPE  2

static const char *deg_debug_graphviz_fontname = "helvetica";
static float deg_debug_graphviz_graph_label_size = 20.0f;
static float deg_debug_graphviz_node_label_size = 14.0f;
static const int deg_debug_max_colors = 12;
#ifdef COLOR_SCHEME_NODE_TYPE
static const char *deg_debug_colors[] = {
    "#a6cee3",
    "#1f78b4",
    "#b2df8a",
    "#33a02c",
    "#fb9a99",
    "#e31a1c",
    "#fdbf6f",
    "#ff7f00",
    "#cab2d6",
    "#6a3d9a",
    "#ffff99",
    "#b15928",
    "#ff00ff",
};
#endif
static const char *deg_debug_colors_light[] = {
    "#8dd3c7",
    "#ffffb3",
    "#bebada",
    "#fb8072",
    "#80b1d3",
    "#fdb462",
    "#b3de69",
    "#fccde5",
    "#d9d9d9",
    "#bc80bd",
    "#ccebc5",
    "#ffed6f",
    "#ff00ff",
};

#ifdef COLOR_SCHEME_NODE_TYPE
static const int deg_debug_node_type_color_map[][2] = {
    {NodeType::TIMESOURCE, 0},
    {NodeType::ID_REF, 1},

    /* Outer Types */
    {NodeType::PARAMETERS, 2},
    {NodeType::ANIMATION, 4},
    {NodeType::TRANSFORM, 5},
    {NodeType::GEOMETRY, 6},
    {NodeType::SEQUENCER, 7},
    {NodeType::SHADING, 8},
    {NodeType::CACHE, 9},
    {NodeType::POINT_CACHE, 10},
    {NodeType::LAYER_COLLECTIONS, 11},
    {NodeType::COPY_ON_EVAL, 12},
    {-1, 0},
};
#endif

static int deg_debug_node_color_index(const Node *node)
{
#ifdef COLOR_SCHEME_NODE_CLASS
  /* Some special types. */
  switch (node->type) {
    case NodeType::ID_REF:
      return 5;
    case NodeType::OPERATION: {
      OperationNode *op_node = (OperationNode *)node;
      if (op_node->is_noop()) {
        if (op_node->flag & OperationFlag::DEPSOP_FLAG_PINNED) {
          return 7;
        }
        return 8;
      }
      break;
    }

    default:
      break;
  }
  /* Do others based on class. */
  switch (node->get_class()) {
    case NodeClass::OPERATION:
      return 4;
    case NodeClass::COMPONENT:
      return 1;
    default:
      return 9;
  }
#endif

#ifdef COLOR_SCHEME_NODE_TYPE
  const int (*pair)[2];
  for (pair = deg_debug_node_type_color_map; (*pair)[0] >= 0; pair++) {
    if ((*pair)[0] == node->type) {
      return (*pair)[1];
    }
  }
  return -1;
#endif
}

struct DotExportContext {
  bool show_tags;
  dot_export::DirectedGraph &digraph;
  Map<const Node *, dot_export::Node *> nodes_map;
  Map<const Node *, dot_export::Cluster *> clusters_map;
};

static void deg_debug_graphviz_legend_color(const char *name,
                                            const char *color,
                                            std::stringstream &ss)
{

  ss << "<TR>";
  ss << "<TD>" << name << "</TD>";
  ss << "<TD BGCOLOR=\"" << color << "\"></TD>";
  ss << "</TR>";
}

static void deg_debug_graphviz_legend(DotExportContext &ctx)
{
  dot_export::Node &legend_node = ctx.digraph.new_node("");
  legend_node.attributes.set("rank", "sink");
  legend_node.attributes.set("shape", "none");
  legend_node.attributes.set("margin", 0);

  std::stringstream ss;
  ss << "<";
  ss << R"(<TABLE BORDER="0" CELLBORDER="1" CELLSPACING="0" CELLPADDING="4">)";
  ss << "<TR><TD COLSPAN=\"2\"><B>Legend</B></TD></TR>";

#ifdef COLOR_SCHEME_NODE_CLASS
  const char **colors = deg_debug_colors_light;
  deg_debug_graphviz_legend_color("Operation", colors[4], ss);
  deg_debug_graphviz_legend_color("Component", colors[1], ss);
  deg_debug_graphviz_legend_color("ID Node", colors[5], ss);
  deg_debug_graphviz_legend_color("NOOP", colors[8], ss);
  deg_debug_graphviz_legend_color("Pinned OP", colors[7], ss);
#endif

#ifdef COLOR_SCHEME_NODE_TYPE
  const int (*pair)[2];
  for (pair = deg_debug_node_type_color_map; (*pair)[0] >= 0; pair++) {
    DepsNodeFactory *nti = type_get_factory((NodeType)(*pair)[0]);
    deg_debug_graphviz_legend_color(
        ctx, nti->tname().c_str(), deg_debug_colors_light[(*pair)[1] % deg_debug_max_colors], ss);
  }
#endif

  ss << "</TABLE>";
  ss << ">";
  legend_node.attributes.set("label", ss.str());
  legend_node.attributes.set("fontname", deg_debug_graphviz_fontname);
}

static void deg_debug_graphviz_node_color(DotExportContext &ctx,
                                          const Node *node,
                                          dot_export::Attributes &dot_attributes)
{
  const char *color_default = "black";
  const char *color_modified = "orangered4";
  const char *color_update = "dodgerblue3";
  const char *color = color_default;
  if (ctx.show_tags) {
    if (node->get_class() == NodeClass::OPERATION) {
      OperationNode *op_node = (OperationNode *)node;
      if (op_node->flag & DEPSOP_FLAG_DIRECTLY_MODIFIED) {
        color = color_modified;
      }
      else if (op_node->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
        color = color_update;
      }
    }
  }
  dot_attributes.set("color", color);
}

static void deg_debug_graphviz_node_penwidth(DotExportContext &ctx,
                                             const Node *node,
                                             dot_export::Attributes &dot_attributes)
{
  float penwidth_default = 1.0f;
  float penwidth_modified = 4.0f;
  float penwidth_update = 4.0f;
  float penwidth = penwidth_default;
  if (ctx.show_tags) {
    if (node->get_class() == NodeClass::OPERATION) {
      OperationNode *op_node = (OperationNode *)node;
      if (op_node->flag & DEPSOP_FLAG_DIRECTLY_MODIFIED) {
        penwidth = penwidth_modified;
      }
      else if (op_node->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
        penwidth = penwidth_update;
      }
    }
  }
  dot_attributes.set("penwidth", penwidth);
}

static void deg_debug_graphviz_node_fillcolor(const Node *node,
                                              dot_export::Attributes &dot_attributes)
{
  const char *defaultcolor = "gainsboro";
  int color_index = deg_debug_node_color_index(node);
  const char *fillcolor = color_index < 0 ?
                              defaultcolor :
                              deg_debug_colors_light[color_index % deg_debug_max_colors];
  dot_attributes.set("fillcolor", fillcolor);
}

static void deg_debug_graphviz_relation_color(const Relation *rel, dot_export::DirectedEdge &edge)
{
  const char *color_default = "black";
  const char *color_cyclic = "red4";   /* The color of crime scene. */
  const char *color_godmode = "blue4"; /* The color of beautiful sky. */
  const char *color = color_default;
  if (rel->flag & RELATION_FLAG_CYCLIC) {
    color = color_cyclic;
  }
  else if (rel->flag & RELATION_FLAG_GODMODE) {
    color = color_godmode;
  }
  edge.attributes.set("color", color);
}

static void deg_debug_graphviz_relation_style(const Relation *rel, dot_export::DirectedEdge &edge)
{
  const char *style_default = "solid";
  const char *style_no_flush = "dashed";
  const char *style_flush_user_only = "dotted";
  const char *style = style_default;
  if (rel->flag & RELATION_FLAG_NO_FLUSH) {
    style = style_no_flush;
  }
  if (rel->flag & RELATION_FLAG_FLUSH_USER_EDIT_ONLY) {
    style = style_flush_user_only;
  }
  edge.attributes.set("style", style);
}

static void deg_debug_graphviz_relation_arrowhead(const Relation *rel,
                                                  dot_export::DirectedEdge &edge)
{
  const char *shape_default = "normal";
  const char *shape_no_cow = "box";
  const char *shape = shape_default;
  if (rel->from->get_class() == NodeClass::OPERATION &&
      rel->to->get_class() == NodeClass::OPERATION)
  {
    OperationNode *op_from = (OperationNode *)rel->from;
    OperationNode *op_to = (OperationNode *)rel->to;
    if (op_from->owner->type == NodeType::COPY_ON_EVAL &&
        /* The #ID::recalc flag depends on run-time state which is not valid at this point in time.
         * Pass in all flags although there may be a better way to represent this. */
        !op_to->owner->need_tag_cow_before_update(ID_RECALC_ALL))
    {
      shape = shape_no_cow;
    }
  }
  edge.attributes.set("arrowhead", shape);
}

static void deg_debug_graphviz_node_style(DotExportContext &ctx,
                                          const Node *node,
                                          dot_export::Attributes &dot_attributes)
{
  StringRef base_style = "filled"; /* default style */
  if (ctx.show_tags) {
    if (node->get_class() == NodeClass::OPERATION) {
      OperationNode *op_node = (OperationNode *)node;
      if (op_node->flag & (DEPSOP_FLAG_DIRECTLY_MODIFIED | DEPSOP_FLAG_NEEDS_UPDATE)) {
        base_style = "striped";
      }
    }
  }
  switch (node->get_class()) {
    case NodeClass::GENERIC:
      dot_attributes.set("style", base_style);
      break;
    case NodeClass::COMPONENT:
      dot_attributes.set("style", base_style);
      break;
    case NodeClass::OPERATION:
      dot_attributes.set("style", base_style + ",rounded");
      break;
  }
}

static void deg_debug_graphviz_node_single(DotExportContext &ctx,
                                           const Node *node,
                                           dot_export::Cluster *parent_cluster)
{
  std::string name = node->identifier();

  dot_export::Node &dot_node = ctx.digraph.new_node(name);
  ctx.nodes_map.add_new(node, &dot_node);
  dot_node.set_parent_cluster(parent_cluster);
  dot_node.attributes.set("fontname", deg_debug_graphviz_fontname);
  dot_node.attributes.set("frontsize", deg_debug_graphviz_node_label_size);
  dot_node.attributes.set("shape", "box");

  deg_debug_graphviz_node_style(ctx, node, dot_node.attributes);
  deg_debug_graphviz_node_color(ctx, node, dot_node.attributes);
  deg_debug_graphviz_node_fillcolor(node, dot_node.attributes);
  deg_debug_graphviz_node_penwidth(ctx, node, dot_node.attributes);
}

static dot_export::Cluster &deg_debug_graphviz_node_cluster_create(
    DotExportContext &ctx, const Node *node, dot_export::Cluster *parent_cluster)
{
  std::string name = node->identifier();
  dot_export::Cluster &cluster = ctx.digraph.new_cluster(name);
  cluster.set_parent_cluster(parent_cluster);
  cluster.attributes.set("fontname", deg_debug_graphviz_fontname);
  cluster.attributes.set("fontsize", deg_debug_graphviz_node_label_size);
  cluster.attributes.set("margin", 16);
  deg_debug_graphviz_node_style(ctx, node, cluster.attributes);
  deg_debug_graphviz_node_color(ctx, node, cluster.attributes);
  deg_debug_graphviz_node_fillcolor(node, cluster.attributes);
  deg_debug_graphviz_node_penwidth(ctx, node, cluster.attributes);
  /* dummy node, so we can add edges between clusters */
  dot_export::Node &dot_node = ctx.digraph.new_node("");
  dot_node.attributes.set("shape", "point");
  dot_node.attributes.set("style", "invis");
  dot_node.set_parent_cluster(&cluster);
  ctx.nodes_map.add_new(node, &dot_node);
  ctx.clusters_map.add_new(node, &cluster);
  return cluster;
}

static void deg_debug_graphviz_graph_nodes(DotExportContext &ctx, const Depsgraph *graph);
static void deg_debug_graphviz_graph_relations(DotExportContext &ctx, const Depsgraph *graph);

static void deg_debug_graphviz_node(DotExportContext &ctx,
                                    const Node *node,
                                    dot_export::Cluster *parent_cluster)
{
  switch (node->type) {
    case NodeType::ID_REF: {
      const IDNode *id_node = (const IDNode *)node;
      if (id_node->components.is_empty()) {
        deg_debug_graphviz_node_single(ctx, node, parent_cluster);
      }
      else {
        dot_export::Cluster &cluster = deg_debug_graphviz_node_cluster_create(
            ctx, node, parent_cluster);
        for (const ComponentNode *comp : id_node->components.values()) {
          deg_debug_graphviz_node(ctx, comp, &cluster);
        }
      }
      break;
    }
    case NodeType::PARAMETERS:
    case NodeType::ANIMATION:
    case NodeType::TRANSFORM:
    case NodeType::GEOMETRY:
    case NodeType::SEQUENCER:
    case NodeType::EVAL_POSE:
    case NodeType::BONE:
    case NodeType::SHADING:
    case NodeType::CACHE:
    case NodeType::POINT_CACHE:
    case NodeType::IMAGE_ANIMATION:
    case NodeType::LAYER_COLLECTIONS:
    case NodeType::PARTICLE_SYSTEM:
    case NodeType::PARTICLE_SETTINGS:
    case NodeType::COPY_ON_EVAL:
    case NodeType::OBJECT_FROM_LAYER:
    case NodeType::HIERARCHY:
    case NodeType::BATCH_CACHE:
    case NodeType::INSTANCING:
    case NodeType::SYNCHRONIZATION:
    case NodeType::AUDIO:
    case NodeType::ARMATURE:
    case NodeType::GENERIC_DATABLOCK:
    case NodeType::SCENE:
    case NodeType::VISIBILITY:
    case NodeType::NTREE_OUTPUT:
    case NodeType::NTREE_GEOMETRY_PREPROCESS: {
      ComponentNode *comp_node = (ComponentNode *)node;
      if (comp_node->operations.is_empty()) {
        deg_debug_graphviz_node_single(ctx, node, parent_cluster);
      }
      else {
        dot_export::Cluster &cluster = deg_debug_graphviz_node_cluster_create(
            ctx, node, parent_cluster);
        for (Node *op_node : comp_node->operations) {
          deg_debug_graphviz_node(ctx, op_node, &cluster);
        }
      }
      break;
    }
    case NodeType::UNDEFINED:
    case NodeType::TIMESOURCE:
    case NodeType::OPERATION:
      deg_debug_graphviz_node_single(ctx, node, parent_cluster);
      break;
    case NodeType::NUM_TYPES:
      break;
  }
}

static void deg_debug_graphviz_node_relations(DotExportContext &ctx, const Node *node)
{
  for (Relation *rel : node->inlinks) {
    float penwidth = 2.0f;

    const Node *head = rel->to; /* same as node */
    const Node *tail = rel->from;
    dot_export::Node &dot_tail = *ctx.nodes_map.lookup(tail);
    dot_export::Node &dot_head = *ctx.nodes_map.lookup(head);

    dot_export::DirectedEdge &edge = ctx.digraph.new_edge(dot_tail, dot_head);

    /* NOTE: without label an id seem necessary to avoid bugs in graphviz/dot. */
    edge.attributes.set("id", rel->name);
    deg_debug_graphviz_relation_color(rel, edge);
    deg_debug_graphviz_relation_style(rel, edge);
    deg_debug_graphviz_relation_arrowhead(rel, edge);
    edge.attributes.set("penwidth", penwidth);

    /* NOTE: edge from node to our own cluster is not possible and gives graphviz
     * warning, avoid this here by just linking directly to the invisible
     * placeholder node. */
    dot_export::Cluster *tail_cluster = ctx.clusters_map.lookup_default(tail, nullptr);
    if (tail_cluster != nullptr && tail_cluster->contains(dot_head)) {
      edge.attributes.set("ltail", tail_cluster->name());
    }
    dot_export::Cluster *head_cluster = ctx.clusters_map.lookup_default(head, nullptr);
    if (head_cluster != nullptr && head_cluster->contains(dot_tail)) {
      edge.attributes.set("lhead", head_cluster->name());
    }
  }
}

static void deg_debug_graphviz_graph_nodes(DotExportContext &ctx, const Depsgraph *graph)
{
  for (Node *node : graph->id_nodes) {
    deg_debug_graphviz_node(ctx, node, nullptr);
  }
  TimeSourceNode *time_source = graph->find_time_source();
  if (time_source != nullptr) {
    deg_debug_graphviz_node(ctx, time_source, nullptr);
  }
}

static void deg_debug_graphviz_graph_relations(DotExportContext &ctx, const Depsgraph *graph)
{
  for (IDNode *id_node : graph->id_nodes) {
    for (ComponentNode *comp_node : id_node->components.values()) {
      for (OperationNode *op_node : comp_node->operations) {
        deg_debug_graphviz_node_relations(ctx, op_node);
      }
    }
  }

  TimeSourceNode *time_source = graph->find_time_source();
  if (time_source != nullptr) {
    deg_debug_graphviz_node_relations(ctx, time_source);
  }
}

}  // namespace blender::deg

std::string DEG_debug_graph_to_dot(const Depsgraph &graph, const blender::StringRef label)
{
  const deg::Depsgraph &deg_graph = reinterpret_cast<const deg::Depsgraph &>(graph);

  dot_export::DirectedGraph digraph;
  deg::DotExportContext ctx{false, digraph};

  digraph.set_rankdir(dot_export::Attr_rankdir::LeftToRight);
  digraph.attributes.set("compound", "true");
  digraph.attributes.set("labelloc", "t");
  digraph.attributes.set("fontsize", deg::deg_debug_graphviz_graph_label_size);
  digraph.attributes.set("fontname", deg::deg_debug_graphviz_fontname);
  digraph.attributes.set("label", label);
  digraph.attributes.set("splines", "ortho");
  digraph.attributes.set("overlap", "scalexy");

  deg::deg_debug_graphviz_graph_nodes(ctx, &deg_graph);
  deg::deg_debug_graphviz_graph_relations(ctx, &deg_graph);

  deg::deg_debug_graphviz_legend(ctx);

  return digraph.to_dot_string();
}
