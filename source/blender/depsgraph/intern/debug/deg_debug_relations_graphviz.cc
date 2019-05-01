/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 *
 * Implementation of tools for debugging the depsgraph
 */

#include <cstdarg>

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

extern "C" {
#include "DNA_listBase.h"
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_debug.h"

#include "intern/depsgraph.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"
#include "intern/node/deg_node_time.h"

/* ****************** */
/* Graphviz Debugging */

namespace DEG {

#define NL "\r\n"

/* Only one should be enabled, defines whether graphviz nodes
 * get colored by individual types or classes.
 */
#define COLOR_SCHEME_NODE_CLASS 1
//#define COLOR_SCHEME_NODE_TYPE  2

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
    {NodeType::PROXY, 3},
    {NodeType::ANIMATION, 4},
    {NodeType::TRANSFORM, 5},
    {NodeType::GEOMETRY, 6},
    {NodeType::SEQUENCER, 7},
    {NodeType::SHADING, 8},
    {NodeType::SHADING_PARAMETERS, 9},
    {NodeType::CACHE, 10},
    {NodeType::POINT_CACHE, 11},
    {NodeType::LAYER_COLLECTIONS, 12},
    {NodeType::COPY_ON_WRITE, 13},
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
  const int(*pair)[2];
  for (pair = deg_debug_node_type_color_map; (*pair)[0] >= 0; ++pair) {
    if ((*pair)[0] == node->type) {
      return (*pair)[1];
    }
  }
  return -1;
#endif
}

struct DebugContext {
  FILE *file;
  bool show_tags;
};

static void deg_debug_fprintf(const DebugContext &ctx, const char *fmt, ...)
    ATTR_PRINTF_FORMAT(2, 3);
static void deg_debug_fprintf(const DebugContext &ctx, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(ctx.file, fmt, args);
  va_end(args);
}

static void deg_debug_graphviz_legend_color(const DebugContext &ctx,
                                            const char *name,
                                            const char *color)
{
  deg_debug_fprintf(ctx, "<TR>");
  deg_debug_fprintf(ctx, "<TD>%s</TD>", name);
  deg_debug_fprintf(ctx, "<TD BGCOLOR=\"%s\"></TD>", color);
  deg_debug_fprintf(ctx, "</TR>" NL);
}

static void deg_debug_graphviz_legend(const DebugContext &ctx)
{
  deg_debug_fprintf(ctx, "{" NL);
  deg_debug_fprintf(ctx, "rank = sink;" NL);
  deg_debug_fprintf(ctx, "Legend [shape=none, margin=0, label=<" NL);
  deg_debug_fprintf(
      ctx, "  <TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">" NL);
  deg_debug_fprintf(ctx, "<TR><TD COLSPAN=\"2\"><B>Legend</B></TD></TR>" NL);

#ifdef COLOR_SCHEME_NODE_CLASS
  const char **colors = deg_debug_colors_light;
  deg_debug_graphviz_legend_color(ctx, "Operation", colors[4]);
  deg_debug_graphviz_legend_color(ctx, "Component", colors[1]);
  deg_debug_graphviz_legend_color(ctx, "ID Node", colors[5]);
  deg_debug_graphviz_legend_color(ctx, "NOOP", colors[8]);
#endif

#ifdef COLOR_SCHEME_NODE_TYPE
  const int(*pair)[2];
  for (pair = deg_debug_node_type_color_map; (*pair)[0] >= 0; ++pair) {
    DepsNodeFactory *nti = type_get_factory((NodeType)(*pair)[0]);
    deg_debug_graphviz_legend_color(
        ctx, nti->tname().c_str(), deg_debug_colors_light[(*pair)[1] % deg_debug_max_colors]);
  }
#endif

  deg_debug_fprintf(ctx, "</TABLE>" NL);
  deg_debug_fprintf(ctx, ">" NL);
  deg_debug_fprintf(ctx, ",fontname=\"%s\"", deg_debug_graphviz_fontname);
  deg_debug_fprintf(ctx, "];" NL);
  deg_debug_fprintf(ctx, "}" NL);
}

static void deg_debug_graphviz_node_color(const DebugContext &ctx, const Node *node)
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
  deg_debug_fprintf(ctx, "\"%s\"", color);
}

static void deg_debug_graphviz_node_penwidth(const DebugContext &ctx, const Node *node)
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
  deg_debug_fprintf(ctx, "\"%f\"", penwidth);
}

static void deg_debug_graphviz_node_fillcolor(const DebugContext &ctx, const Node *node)
{
  const char *defaultcolor = "gainsboro";
  int color_index = deg_debug_node_color_index(node);
  const char *fillcolor = color_index < 0 ?
                              defaultcolor :
                              deg_debug_colors_light[color_index % deg_debug_max_colors];
  deg_debug_fprintf(ctx, "\"%s\"", fillcolor);
}

static void deg_debug_graphviz_relation_color(const DebugContext &ctx, const Relation *rel)
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
  deg_debug_fprintf(ctx, "%s", color);
}

static void deg_debug_graphviz_relation_style(const DebugContext &ctx, const Relation *rel)
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
  deg_debug_fprintf(ctx, "%s", style);
}

static void deg_debug_graphviz_relation_arrowhead(const DebugContext &ctx, const Relation *rel)
{
  const char *shape_default = "normal";
  const char *shape_no_cow = "box";
  const char *shape = shape_default;
  if (rel->from->get_class() == NodeClass::OPERATION &&
      rel->to->get_class() == NodeClass::OPERATION) {
    OperationNode *op_from = (OperationNode *)rel->from;
    OperationNode *op_to = (OperationNode *)rel->to;
    if (op_from->owner->type == NodeType::COPY_ON_WRITE &&
        !op_to->owner->need_tag_cow_before_update()) {
      shape = shape_no_cow;
    }
  }
  deg_debug_fprintf(ctx, "%s", shape);
}

static void deg_debug_graphviz_node_style(const DebugContext &ctx, const Node *node)
{
  const char *base_style = "filled"; /* default style */
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
      deg_debug_fprintf(ctx, "\"%s\"", base_style);
      break;
    case NodeClass::COMPONENT:
      deg_debug_fprintf(ctx, "\"%s\"", base_style);
      break;
    case NodeClass::OPERATION:
      deg_debug_fprintf(ctx, "\"%s,rounded\"", base_style);
      break;
  }
}

static void deg_debug_graphviz_node_single(const DebugContext &ctx, const Node *node)
{
  const char *shape = "box";
  string name = node->identifier();
  deg_debug_fprintf(ctx, "// %s\n", name.c_str());
  deg_debug_fprintf(ctx, "\"node_%p\"", node);
  deg_debug_fprintf(ctx, "[");
  //  deg_debug_fprintf(ctx, "label=<<B>%s</B>>", name);
  deg_debug_fprintf(ctx, "label=<%s>", name.c_str());
  deg_debug_fprintf(ctx, ",fontname=\"%s\"", deg_debug_graphviz_fontname);
  deg_debug_fprintf(ctx, ",fontsize=%f", deg_debug_graphviz_node_label_size);
  deg_debug_fprintf(ctx, ",shape=%s", shape);
  deg_debug_fprintf(ctx, ",style=");
  deg_debug_graphviz_node_style(ctx, node);
  deg_debug_fprintf(ctx, ",color=");
  deg_debug_graphviz_node_color(ctx, node);
  deg_debug_fprintf(ctx, ",fillcolor=");
  deg_debug_graphviz_node_fillcolor(ctx, node);
  deg_debug_fprintf(ctx, ",penwidth=");
  deg_debug_graphviz_node_penwidth(ctx, node);
  deg_debug_fprintf(ctx, "];" NL);
  deg_debug_fprintf(ctx, NL);
}

static void deg_debug_graphviz_node_cluster_begin(const DebugContext &ctx, const Node *node)
{
  string name = node->identifier();
  deg_debug_fprintf(ctx, "// %s\n", name.c_str());
  deg_debug_fprintf(ctx, "subgraph \"cluster_%p\" {" NL, node);
  //  deg_debug_fprintf(ctx, "label=<<B>%s</B>>;" NL, name);
  deg_debug_fprintf(ctx, "label=<%s>;" NL, name.c_str());
  deg_debug_fprintf(ctx, "fontname=\"%s\";" NL, deg_debug_graphviz_fontname);
  deg_debug_fprintf(ctx, "fontsize=%f;" NL, deg_debug_graphviz_node_label_size);
  deg_debug_fprintf(ctx, "margin=\"%d\";" NL, 16);
  deg_debug_fprintf(ctx, "style=");
  deg_debug_graphviz_node_style(ctx, node);
  deg_debug_fprintf(ctx, ";" NL);
  deg_debug_fprintf(ctx, "color=");
  deg_debug_graphviz_node_color(ctx, node);
  deg_debug_fprintf(ctx, ";" NL);
  deg_debug_fprintf(ctx, "fillcolor=");
  deg_debug_graphviz_node_fillcolor(ctx, node);
  deg_debug_fprintf(ctx, ";" NL);
  deg_debug_fprintf(ctx, "penwidth=");
  deg_debug_graphviz_node_penwidth(ctx, node);
  deg_debug_fprintf(ctx, ";" NL);
  /* dummy node, so we can add edges between clusters */
  deg_debug_fprintf(ctx, "\"node_%p\"", node);
  deg_debug_fprintf(ctx, "[");
  deg_debug_fprintf(ctx, "shape=%s", "point");
  deg_debug_fprintf(ctx, ",style=%s", "invis");
  deg_debug_fprintf(ctx, "];" NL);
  deg_debug_fprintf(ctx, NL);
}

static void deg_debug_graphviz_node_cluster_end(const DebugContext &ctx)
{
  deg_debug_fprintf(ctx, "}" NL);
  deg_debug_fprintf(ctx, NL);
}

static void deg_debug_graphviz_graph_nodes(const DebugContext &ctx, const Depsgraph *graph);
static void deg_debug_graphviz_graph_relations(const DebugContext &ctx, const Depsgraph *graph);

static void deg_debug_graphviz_node(const DebugContext &ctx, const Node *node)
{
  switch (node->type) {
    case NodeType::ID_REF: {
      const IDNode *id_node = (const IDNode *)node;
      if (BLI_ghash_len(id_node->components) == 0) {
        deg_debug_graphviz_node_single(ctx, node);
      }
      else {
        deg_debug_graphviz_node_cluster_begin(ctx, node);
        GHASH_FOREACH_BEGIN (const ComponentNode *, comp, id_node->components) {
          deg_debug_graphviz_node(ctx, comp);
        }
        GHASH_FOREACH_END();
        deg_debug_graphviz_node_cluster_end(ctx);
      }
      break;
    }
    case NodeType::PARAMETERS:
    case NodeType::ANIMATION:
    case NodeType::TRANSFORM:
    case NodeType::PROXY:
    case NodeType::GEOMETRY:
    case NodeType::SEQUENCER:
    case NodeType::EVAL_POSE:
    case NodeType::BONE:
    case NodeType::SHADING:
    case NodeType::SHADING_PARAMETERS:
    case NodeType::CACHE:
    case NodeType::POINT_CACHE:
    case NodeType::LAYER_COLLECTIONS:
    case NodeType::PARTICLE_SYSTEM:
    case NodeType::PARTICLE_SETTINGS:
    case NodeType::COPY_ON_WRITE:
    case NodeType::OBJECT_FROM_LAYER:
    case NodeType::BATCH_CACHE:
    case NodeType::DUPLI:
    case NodeType::SYNCHRONIZATION:
    case NodeType::AUDIO:
    case NodeType::GENERIC_DATABLOCK: {
      ComponentNode *comp_node = (ComponentNode *)node;
      if (!comp_node->operations.empty()) {
        deg_debug_graphviz_node_cluster_begin(ctx, node);
        for (Node *op_node : comp_node->operations) {
          deg_debug_graphviz_node(ctx, op_node);
        }
        deg_debug_graphviz_node_cluster_end(ctx);
      }
      else {
        deg_debug_graphviz_node_single(ctx, node);
      }
      break;
    }
    case NodeType::UNDEFINED:
    case NodeType::TIMESOURCE:
    case NodeType::OPERATION:
      deg_debug_graphviz_node_single(ctx, node);
      break;
    case NodeType::NUM_TYPES:
      break;
  }
}

static bool deg_debug_graphviz_is_cluster(const Node *node)
{
  switch (node->type) {
    case NodeType::ID_REF: {
      const IDNode *id_node = (const IDNode *)node;
      return BLI_ghash_len(id_node->components) > 0;
    }
    case NodeType::PARAMETERS:
    case NodeType::ANIMATION:
    case NodeType::TRANSFORM:
    case NodeType::PROXY:
    case NodeType::GEOMETRY:
    case NodeType::SEQUENCER:
    case NodeType::EVAL_POSE:
    case NodeType::BONE: {
      ComponentNode *comp_node = (ComponentNode *)node;
      return !comp_node->operations.empty();
    }
    default:
      return false;
  }
}

static bool deg_debug_graphviz_is_owner(const Node *node, const Node *other)
{
  switch (node->get_class()) {
    case NodeClass::COMPONENT: {
      ComponentNode *comp_node = (ComponentNode *)node;
      if (comp_node->owner == other) {
        return true;
      }
      break;
    }
    case NodeClass::OPERATION: {
      OperationNode *op_node = (OperationNode *)node;
      if (op_node->owner == other) {
        return true;
      }
      else if (op_node->owner->owner == other) {
        return true;
      }
      break;
    }
    default:
      break;
  }
  return false;
}

static void deg_debug_graphviz_node_relations(const DebugContext &ctx, const Node *node)
{
  for (Relation *rel : node->inlinks) {
    float penwidth = 2.0f;

    const Node *tail = rel->to; /* same as node */
    const Node *head = rel->from;
    deg_debug_fprintf(
        ctx, "// %s -> %s\n", head->identifier().c_str(), tail->identifier().c_str());
    deg_debug_fprintf(ctx, "\"node_%p\"", head);
    deg_debug_fprintf(ctx, " -> ");
    deg_debug_fprintf(ctx, "\"node_%p\"", tail);

    deg_debug_fprintf(ctx, "[");
    /* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
    deg_debug_fprintf(ctx, "id=\"%s\"", rel->name);
    // deg_debug_fprintf(ctx, "label=\"%s\"", rel->name);
    deg_debug_fprintf(ctx, ",color=");
    deg_debug_graphviz_relation_color(ctx, rel);
    deg_debug_fprintf(ctx, ",style=");
    deg_debug_graphviz_relation_style(ctx, rel);
    deg_debug_fprintf(ctx, ",arrowhead=");
    deg_debug_graphviz_relation_arrowhead(ctx, rel);
    deg_debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
    /* NOTE: edge from node to own cluster is not possible and gives graphviz
     * warning, avoid this here by just linking directly to the invisible
     * placeholder node. */
    if (deg_debug_graphviz_is_cluster(tail) && !deg_debug_graphviz_is_owner(head, tail)) {
      deg_debug_fprintf(ctx, ",ltail=\"cluster_%p\"", tail);
    }
    if (deg_debug_graphviz_is_cluster(head) && !deg_debug_graphviz_is_owner(tail, head)) {
      deg_debug_fprintf(ctx, ",lhead=\"cluster_%p\"", head);
    }
    deg_debug_fprintf(ctx, "];" NL);
    deg_debug_fprintf(ctx, NL);
  }
}

static void deg_debug_graphviz_graph_nodes(const DebugContext &ctx, const Depsgraph *graph)
{
  for (Node *node : graph->id_nodes) {
    deg_debug_graphviz_node(ctx, node);
  }
  TimeSourceNode *time_source = graph->find_time_source();
  if (time_source != NULL) {
    deg_debug_graphviz_node(ctx, time_source);
  }
}

static void deg_debug_graphviz_graph_relations(const DebugContext &ctx, const Depsgraph *graph)
{
  for (IDNode *id_node : graph->id_nodes) {
    GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, id_node->components) {
      for (OperationNode *op_node : comp_node->operations) {
        deg_debug_graphviz_node_relations(ctx, op_node);
      }
    }
    GHASH_FOREACH_END();
  }

  TimeSourceNode *time_source = graph->find_time_source();
  if (time_source != NULL) {
    deg_debug_graphviz_node_relations(ctx, time_source);
  }
}

}  // namespace DEG

void DEG_debug_relations_graphviz(const Depsgraph *graph, FILE *f, const char *label)
{
  if (!graph) {
    return;
  }

  const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(graph);

  DEG::DebugContext ctx;
  ctx.file = f;

  DEG::deg_debug_fprintf(ctx, "digraph depgraph {" NL);
  DEG::deg_debug_fprintf(ctx, "rankdir=LR;" NL);
  DEG::deg_debug_fprintf(ctx, "graph [");
  DEG::deg_debug_fprintf(ctx, "compound=true");
  DEG::deg_debug_fprintf(ctx, ",labelloc=\"t\"");
  DEG::deg_debug_fprintf(ctx, ",fontsize=%f", DEG::deg_debug_graphviz_graph_label_size);
  DEG::deg_debug_fprintf(ctx, ",fontname=\"%s\"", DEG::deg_debug_graphviz_fontname);
  DEG::deg_debug_fprintf(ctx, ",label=\"%s\"", label);
  DEG::deg_debug_fprintf(ctx, ",splines=ortho");
  DEG::deg_debug_fprintf(ctx, ",overlap=scalexy");  // XXX: only when using neato
  DEG::deg_debug_fprintf(ctx, "];" NL);

  DEG::deg_debug_graphviz_graph_nodes(ctx, deg_graph);
  DEG::deg_debug_graphviz_graph_relations(ctx, deg_graph);

  DEG::deg_debug_graphviz_legend(ctx);

  DEG::deg_debug_fprintf(ctx, "}" NL);
}

#undef NL
