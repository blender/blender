/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Original Author: Lukas Toenne
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_debug.cc
 *  \ingroup depsgraph
 *
 * Implementation of tools for debugging the depsgraph
 */

//#include <stdlib.h>
#include <string.h>

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_string.h"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_build.h"

#include "WM_api.h"
#include "WM_types.h"
}  /* extern "C" */

#include "depsgraph_debug.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"
#include "depsgraph_intern.h"
#include "depsgraph_util_foreach.h"

/* ****************** */
/* Graphviz Debugging */

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
#if 0
static const char *deg_debug_colors_dark[] = {
    "#6e8997", "#144f77", "#76945b",
    "#216a1d", "#a76665", "#971112",
    "#a87f49", "#0a9540", "#86768e",
    "#462866", "#a9a965", "#753b1a",
};
#endif
#ifdef COLOR_SCHEME_NODE_TYPE
static const char *deg_debug_colors[] = {
    "#a6cee3", "#1f78b4", "#b2df8a",
    "#33a02c", "#fb9a99", "#e31a1c",
    "#fdbf6f", "#ff7f00", "#cab2d6",
    "#6a3d9a", "#ffff99", "#b15928",
};
#endif
static const char *deg_debug_colors_light[] = {
    "#8dd3c7", "#ffffb3", "#bebada",
    "#fb8072", "#80b1d3", "#fdb462",
    "#b3de69", "#fccde5", "#d9d9d9",
    "#bc80bd", "#ccebc5", "#ffed6f",
};

#ifdef COLOR_SCHEME_NODE_TYPE
static const int deg_debug_node_type_color_map[][2] = {
    {DEPSNODE_TYPE_ROOT,         0},
    {DEPSNODE_TYPE_TIMESOURCE,   1},
    {DEPSNODE_TYPE_ID_REF,       2},
    {DEPSNODE_TYPE_SUBGRAPH,     3},

    /* Outer Types */
    {DEPSNODE_TYPE_PARAMETERS,   4},
    {DEPSNODE_TYPE_PROXY,        5},
    {DEPSNODE_TYPE_ANIMATION,    6},
    {DEPSNODE_TYPE_TRANSFORM,    7},
    {DEPSNODE_TYPE_GEOMETRY,     8},
    {DEPSNODE_TYPE_SEQUENCER,    9},
    {DEPSNODE_TYPE_SHADING,      10},
    {-1,                         0}
};
#endif

#if 0 /* unused */
static const int deg_debug_relation_type_color_map[][2] = {
    {DEPSREL_TYPE_STANDARD,         0},
    {DEPSREL_TYPE_ROOT_TO_ACTIVE,   1},
    {DEPSREL_TYPE_DATABLOCK,        2},
    {DEPSREL_TYPE_TIME,             3},
    {DEPSREL_TYPE_COMPONENT_ORDER,  4},
    {DEPSREL_TYPE_OPERATION,        5},
    {DEPSREL_TYPE_DRIVER,           6},
    {DEPSREL_TYPE_DRIVER_TARGET,    7},
    {DEPSREL_TYPE_TRANSFORM,        8},
    {DEPSREL_TYPE_GEOMETRY_EVAL,    9},
    {DEPSREL_TYPE_UPDATE,           10},
    {DEPSREL_TYPE_UPDATE_UI,        11},
    {-1,                            0}
};
#endif

static int deg_debug_node_color_index(const DepsNode *node)
{
#ifdef COLOR_SCHEME_NODE_CLASS
	/* Some special types. */
	switch (node->type) {
		case DEPSNODE_TYPE_ID_REF:
			return 5;
		case DEPSNODE_TYPE_OPERATION:
		{
			OperationDepsNode *op_node = (OperationDepsNode *)node;
			if (op_node->is_noop())
				return 8;
			break;
		}

		default:
			break;
	}
	/* Do others based on class. */
	switch (node->tclass) {
		case DEPSNODE_CLASS_OPERATION:
			return 4;
		case DEPSNODE_CLASS_COMPONENT:
			return 1;
		default:
			return 9;
	}
#endif

#ifdef COLOR_SCHEME_NODE_TYPE
	const int (*pair)[2];
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
	bool show_eval_priority;
};

static void deg_debug_fprintf(const DebugContext &ctx, const char *fmt, ...) ATTR_PRINTF_FORMAT(2, 3);
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

#if 0
static void deg_debug_graphviz_legend_line(const DebugContext &ctx,
                                           const char *name,
                                           const char *color,
                                           const char *style)
{
	/* XXX TODO */
	deg_debug_fprintf(ctx, "" NL);
}

static void deg_debug_graphviz_legend_cluster(const DebugContext &ctx,
                                              const char *name,
                                              const char *color,
                                              const char *style)
{
	deg_debug_fprintf(ctx, "<TR>");
	deg_debug_fprintf(ctx, "<TD>%s</TD>", name);
	deg_debug_fprintf(ctx, "<TD CELLPADDING=\"4\"><TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">");
	deg_debug_fprintf(ctx, "<TR><TD BGCOLOR=\"%s\"></TD></TR>", color);
	deg_debug_fprintf(ctx, "</TABLE></TD>");
	deg_debug_fprintf(ctx, "</TR>" NL);
}
#endif

static void deg_debug_graphviz_legend(const DebugContext &ctx)
{
	deg_debug_fprintf(ctx, "{" NL);
	deg_debug_fprintf(ctx, "rank = sink;" NL);
	deg_debug_fprintf(ctx, "Legend [shape=none, margin=0, label=<" NL);
	deg_debug_fprintf(ctx, "  <TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">" NL);
	deg_debug_fprintf(ctx, "<TR><TD COLSPAN=\"2\"><B>Legend</B></TD></TR>" NL);

#ifdef COLOR_SCHEME_NODE_CLASS
	const char **colors = deg_debug_colors_light;
	deg_debug_graphviz_legend_color(ctx, "Operation", colors[4]);
	deg_debug_graphviz_legend_color(ctx, "Component", colors[1]);
	deg_debug_graphviz_legend_color(ctx, "ID Node", colors[5]);
	deg_debug_graphviz_legend_color(ctx, "NOOP", colors[8]);
#endif

#ifdef COLOR_SCHEME_NODE_TYPE
	const int (*pair)[2];
	for (pair = deg_debug_node_type_color_map; (*pair)[0] >= 0; ++pair) {
		DepsNodeFactory *nti = DEG_get_node_factory((eDepsNode_Type)(*pair)[0]);
		deg_debug_graphviz_legend_color(ctx,
		                                nti->tname().c_str(),
		                                deg_debug_colors_light[(*pair)[1] % deg_debug_max_colors]);
	}
#endif

	deg_debug_fprintf(ctx, "</TABLE>" NL);
	deg_debug_fprintf(ctx, ">" NL);
	deg_debug_fprintf(ctx, ",fontname=\"%s\"", deg_debug_graphviz_fontname);
	deg_debug_fprintf(ctx, "];" NL);
	deg_debug_fprintf(ctx, "}" NL);
}

#if 0 /* unused */
static int deg_debug_relation_type_color_index(eDepsRelation_Type type)
{
	const int (*pair)[2];
	for (pair = deg_debug_relation_type_color_map; (*pair)[0] >= 0; ++pair) {
		if ((*pair)[0] == type) {
			return (*pair)[1];
		}
	}
	return -1;
}
#endif

static void deg_debug_graphviz_node_color(const DebugContext &ctx,
                                          const DepsNode *node)
{
	const char *color_default = "black";
	const char *color_modified = "orangered4";
	const char *color_update = "dodgerblue3";
	const char *color = color_default;
	if (ctx.show_tags) {
		if (node->tclass == DEPSNODE_CLASS_OPERATION) {
			OperationDepsNode *op_node = (OperationDepsNode *)node;
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

static void deg_debug_graphviz_node_penwidth(const DebugContext &ctx,
                                             const DepsNode *node)
{
	float penwidth_default = 1.0f;
	float penwidth_modified = 4.0f;
	float penwidth_update = 4.0f;
	float penwidth = penwidth_default;
	if (ctx.show_tags) {
		if (node->tclass == DEPSNODE_CLASS_OPERATION) {
			OperationDepsNode *op_node = (OperationDepsNode *)node;
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

static void deg_debug_graphviz_node_fillcolor(const DebugContext &ctx,
                                              const DepsNode *node)
{
	const char *defaultcolor = "gainsboro";
	int color_index = deg_debug_node_color_index(node);
	const char *fillcolor = color_index < 0 ? defaultcolor : deg_debug_colors_light[color_index % deg_debug_max_colors];
	deg_debug_fprintf(ctx, "\"%s\"", fillcolor);
}

#if 0 /* implementation using stripes, a bit too noisy ... */
static void deg_debug_graphviz_node_fillcolor(const DebugContext &ctx,
                                              const DepsNode *node)
{
	const char *defaultcolor = "gainsboro";
	const char *color_needs_update = "orange";
	const int num_stripes = 10;
	int color_index = deg_debug_node_color_index(node);
	const char *base_color = color_index < 0 ? defaultcolor : deg_debug_colors_light[color_index % deg_debug_max_colors];
	if (ctx.show_tags &&
	    (node->flag & (DEPSNODE_FLAG_DIRECTLY_MODIFIED | DEPSNODE_FLAG_NEEDS_UPDATE)))
	{
		deg_debug_fprintf(ctx, "\"");
		for (int i = 0; i < num_stripes; ++i) {
			if (i > 0) {
				deg_debug_fprintf(ctx, ":");
			}
			deg_debug_fprintf(ctx, "%s:%s", base_color, color_needs_update);
		}
		deg_debug_fprintf(ctx, "\"");
	}
	else {
		deg_debug_fprintf(ctx, "\"%s\"", base_color);
	}
}
#endif

static void deg_debug_graphviz_relation_color(const DebugContext &ctx,
                                              const DepsRelation *rel)
{
	const char *color_default = "black";
	const char *color_error = "red4";
	const char *color = color_default;
#if 0 /* disabled for now, edge colors are hardly distinguishable */
	int color = deg_debug_relation_type_color_index(rel->type);
	if (color < 0) {
		deg_debug_fprintf(ctx, "%s", defaultcolor);
	}
	else {
		deg_debug_fprintf(ctx, "\"%s\"", deg_debug_colors_dark[color % deg_debug_max_colors]);
	}
#else
	if (rel->flag & DEPSREL_FLAG_CYCLIC)
		color = color_error;
	
	deg_debug_fprintf(ctx, "%s", color);
#endif
}

static void deg_debug_graphviz_node_style(const DebugContext &ctx, const DepsNode *node)
{
	const char *base_style = "filled"; /* default style */
	if (ctx.show_tags) {
		if (node->tclass == DEPSNODE_CLASS_OPERATION) {
			OperationDepsNode *op_node = (OperationDepsNode *)node;
			if (op_node->flag & (DEPSOP_FLAG_DIRECTLY_MODIFIED | DEPSOP_FLAG_NEEDS_UPDATE)) {
				base_style = "striped";
			}
		}
	}
	switch (node->tclass) {
		case DEPSNODE_CLASS_GENERIC:
			deg_debug_fprintf(ctx, "\"%s\"", base_style);
			break;
		case DEPSNODE_CLASS_COMPONENT:
			deg_debug_fprintf(ctx, "\"%s\"", base_style);
			break;
		case DEPSNODE_CLASS_OPERATION:
			deg_debug_fprintf(ctx, "\"%s,rounded\"", base_style);
			break;
	}
}

static void deg_debug_graphviz_node_single(const DebugContext &ctx,
                                           const DepsNode *node)
{
	const char *shape = "box";
	string name = node->identifier();
	float priority = -1.0f;
	if (node->type == DEPSNODE_TYPE_ID_REF) {
		IDDepsNode *id_node = (IDDepsNode *)node;
		char buf[256];
		BLI_snprintf(buf, sizeof(buf), " (Layers: %d)", id_node->layers);
		name += buf;
	}
	if (ctx.show_eval_priority && node->tclass == DEPSNODE_CLASS_OPERATION) {
		priority = ((OperationDepsNode *)node)->eval_priority;
	}
	deg_debug_fprintf(ctx, "// %s\n", name.c_str());
	deg_debug_fprintf(ctx, "\"node_%p\"", node);
	deg_debug_fprintf(ctx, "[");
//	deg_debug_fprintf(ctx, "label=<<B>%s</B>>", name);
	if (priority >= 0.0f) {
		deg_debug_fprintf(ctx, "label=<%s<BR/>(<I>%.2f</I>)>",
		                 name.c_str(),
		                 priority);
	}
	else {
		deg_debug_fprintf(ctx, "label=<%s>", name.c_str());
	}
	deg_debug_fprintf(ctx, ",fontname=\"%s\"", deg_debug_graphviz_fontname);
	deg_debug_fprintf(ctx, ",fontsize=%f", deg_debug_graphviz_node_label_size);
	deg_debug_fprintf(ctx, ",shape=%s", shape);
	deg_debug_fprintf(ctx, ",style="); deg_debug_graphviz_node_style(ctx, node);
	deg_debug_fprintf(ctx, ",color="); deg_debug_graphviz_node_color(ctx, node);
	deg_debug_fprintf(ctx, ",fillcolor="); deg_debug_graphviz_node_fillcolor(ctx, node);
	deg_debug_fprintf(ctx, ",penwidth="); deg_debug_graphviz_node_penwidth(ctx, node);
	deg_debug_fprintf(ctx, "];" NL);
	deg_debug_fprintf(ctx, NL);
}

static void deg_debug_graphviz_node_cluster_begin(const DebugContext &ctx,
                                                  const DepsNode *node)
{
	string name = node->identifier().c_str();
	if (node->type == DEPSNODE_TYPE_ID_REF) {
		IDDepsNode *id_node = (IDDepsNode *)node;
		char buf[256];
		BLI_snprintf(buf, sizeof(buf), " (Layers: %d)", id_node->layers);
		name += buf;
	}
	deg_debug_fprintf(ctx, "// %s\n", name.c_str());
	deg_debug_fprintf(ctx, "subgraph \"cluster_%p\" {" NL, node);
//	deg_debug_fprintf(ctx, "label=<<B>%s</B>>;" NL, name);
	deg_debug_fprintf(ctx, "label=<%s>;" NL, name.c_str());
	deg_debug_fprintf(ctx, "fontname=\"%s\";" NL, deg_debug_graphviz_fontname);
	deg_debug_fprintf(ctx, "fontsize=%f;" NL, deg_debug_graphviz_node_label_size);
	deg_debug_fprintf(ctx, "margin=\"%d\";" NL, 16);
	deg_debug_fprintf(ctx, "style="); deg_debug_graphviz_node_style(ctx, node); deg_debug_fprintf(ctx, ";" NL);
	deg_debug_fprintf(ctx, "color="); deg_debug_graphviz_node_color(ctx, node); deg_debug_fprintf(ctx, ";" NL);
	deg_debug_fprintf(ctx, "fillcolor="); deg_debug_graphviz_node_fillcolor(ctx, node); deg_debug_fprintf(ctx, ";" NL);
	deg_debug_fprintf(ctx, "penwidth="); deg_debug_graphviz_node_penwidth(ctx, node); deg_debug_fprintf(ctx, ";" NL);
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

static void deg_debug_graphviz_graph_nodes(const DebugContext &ctx,
                                           const Depsgraph *graph);
static void deg_debug_graphviz_graph_relations(const DebugContext &ctx,
                                               const Depsgraph *graph);

static void deg_debug_graphviz_node(const DebugContext &ctx,
                                    const DepsNode *node)
{
	switch (node->type) {
		case DEPSNODE_TYPE_ID_REF:
		{
			const IDDepsNode *id_node = (const IDDepsNode *)node;
			if (id_node->components.empty()) {
				deg_debug_graphviz_node_single(ctx, node);
			}
			else {
				deg_debug_graphviz_node_cluster_begin(ctx, node);
				for (IDDepsNode::ComponentMap::const_iterator it = id_node->components.begin();
				     it != id_node->components.end();
				     ++it)
				{
					const ComponentDepsNode *comp = it->second;
					deg_debug_graphviz_node(ctx, comp);
				}
				deg_debug_graphviz_node_cluster_end(ctx);
			}
			break;
		}
		case DEPSNODE_TYPE_SUBGRAPH:
		{
			SubgraphDepsNode *sub_node = (SubgraphDepsNode *)node;
			if (sub_node->graph) {
				deg_debug_graphviz_node_cluster_begin(ctx, node);
				deg_debug_graphviz_graph_nodes(ctx, sub_node->graph);
				deg_debug_graphviz_node_cluster_end(ctx);
			}
			else {
				deg_debug_graphviz_node_single(ctx, node);
			}
			break;
		}
		case DEPSNODE_TYPE_PARAMETERS:
		case DEPSNODE_TYPE_ANIMATION:
		case DEPSNODE_TYPE_TRANSFORM:
		case DEPSNODE_TYPE_PROXY:
		case DEPSNODE_TYPE_GEOMETRY:
		case DEPSNODE_TYPE_SEQUENCER:
		case DEPSNODE_TYPE_EVAL_POSE:
		case DEPSNODE_TYPE_BONE:
		case DEPSNODE_TYPE_SHADING:
		case DEPSNODE_TYPE_EVAL_PARTICLES:
		{
			ComponentDepsNode *comp_node = (ComponentDepsNode *)node;
			if (!comp_node->operations.empty()) {
				deg_debug_graphviz_node_cluster_begin(ctx, node);
				for (ComponentDepsNode::OperationMap::const_iterator it = comp_node->operations.begin();
				     it != comp_node->operations.end();
				     ++it)
				{
					const DepsNode *op_node = it->second;
					deg_debug_graphviz_node(ctx, op_node);
				}
				deg_debug_graphviz_node_cluster_end(ctx);
			}
			else {
				deg_debug_graphviz_node_single(ctx, node);
			}
			break;
		}
		default:
			deg_debug_graphviz_node_single(ctx, node);
			break;
	}
}

static bool deg_debug_graphviz_is_cluster(const DepsNode *node)
{
	switch (node->type) {
		case DEPSNODE_TYPE_ID_REF:
		{
			const IDDepsNode *id_node = (const IDDepsNode *)node;
			return !id_node->components.empty();
		}
		case DEPSNODE_TYPE_SUBGRAPH:
		{
			SubgraphDepsNode *sub_node = (SubgraphDepsNode *)node;
			return sub_node->graph != NULL;
		}
		case DEPSNODE_TYPE_PARAMETERS:
		case DEPSNODE_TYPE_ANIMATION:
		case DEPSNODE_TYPE_TRANSFORM:
		case DEPSNODE_TYPE_PROXY:
		case DEPSNODE_TYPE_GEOMETRY:
		case DEPSNODE_TYPE_SEQUENCER:
		case DEPSNODE_TYPE_EVAL_POSE:
		case DEPSNODE_TYPE_BONE:
		{
			ComponentDepsNode *comp_node = (ComponentDepsNode *)node;
			return !comp_node->operations.empty();
		}
		default:
			return false;
	}
}

static bool deg_debug_graphviz_is_owner(const DepsNode *node,
                                        const DepsNode *other)
{
	switch (node->tclass) {
		case DEPSNODE_CLASS_COMPONENT:
		{
			ComponentDepsNode *comp_node = (ComponentDepsNode *)node;
			if (comp_node->owner == other)
				return true;
			break;
		}
		case DEPSNODE_CLASS_OPERATION:
		{
			OperationDepsNode *op_node = (OperationDepsNode *)node;
			if (op_node->owner == other)
				return true;
			else if (op_node->owner->owner == other)
				return true;
			break;
		}
		default: break;
	}
	return false;
}

static void deg_debug_graphviz_node_relations(const DebugContext &ctx,
                                              const DepsNode *node)
{
	DEPSNODE_RELATIONS_ITER_BEGIN(node->inlinks, rel)
	{
		float penwidth = 2.0f;
		
		const DepsNode *tail = rel->to; /* same as node */
		const DepsNode *head = rel->from;
		deg_debug_fprintf(ctx, "// %s -> %s\n",
		                 head->identifier().c_str(),
		                 tail->identifier().c_str());
		deg_debug_fprintf(ctx, "\"node_%p\"", head);
		deg_debug_fprintf(ctx, " -> ");
		deg_debug_fprintf(ctx, "\"node_%p\"", tail);

		deg_debug_fprintf(ctx, "[");
		/* XXX labels on relations are not very helpful:
		 * - they tend to appear too far away to be associated with the edge lines
		 * - names are mostly redundant, reflecting simply their from/to nodes
		 * - no behavior or typing of relations themselves to justify labels
		 */
#if 0
		deg_debug_fprintf(ctx, "label=\"%s\"", rel->name);
		deg_debug_fprintf(ctx, ",fontname=\"%s\"", deg_debug_graphviz_fontname);
#else
		/* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
		deg_debug_fprintf(ctx, "id=\"%s\"", rel->name);
#endif
		deg_debug_fprintf(ctx, ",color="); deg_debug_graphviz_relation_color(ctx, rel);
		deg_debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
		/* NOTE: edge from node to own cluster is not possible and gives graphviz
		 * warning, avoid this here by just linking directly to the invisible
		 * placeholder node
		 */
		if (deg_debug_graphviz_is_cluster(tail) && !deg_debug_graphviz_is_owner(head, tail)) {
			deg_debug_fprintf(ctx, ",ltail=\"cluster_%p\"", tail);
		}
		if (deg_debug_graphviz_is_cluster(head) && !deg_debug_graphviz_is_owner(tail, head)) {
			deg_debug_fprintf(ctx, ",lhead=\"cluster_%p\"", head);
		}
		deg_debug_fprintf(ctx, "];" NL);
		deg_debug_fprintf(ctx, NL);
	}
	DEPSNODE_RELATIONS_ITER_END;

#if 0
	if (node->tclass == DEPSNODE_CLASS_COMPONENT) {
		const ComponentDepsNode *comp_node = (const ComponentDepsNode *)node;
		for (ComponentDepsNode::OperationMap::const_iterator it = comp_node->operations.begin();
		     it != comp_node->operations.end();
		     ++it)
		{
			OperationDepsNode *op_node = it->second;
			deg_debug_graphviz_node_relations(ctx, op_node);
		}
	}
	else if (node->type == DEPSNODE_TYPE_ID_REF) {
		const IDDepsNode *id_node = (const IDDepsNode *)node;
		for (IDDepsNode::ComponentMap::const_iterator it = id_node->components.begin();
		     it != id_node->components.end();
		     ++it)
		{
			const ComponentDepsNode *comp = it->second;
			deg_debug_graphviz_node_relations(ctx, comp);
		}
	}
	else if (node->type == DEPSNODE_TYPE_SUBGRAPH) {
		SubgraphDepsNode *sub_node = (SubgraphDepsNode *)node;
		if (sub_node->graph) {
			deg_debug_graphviz_graph_relations(ctx, sub_node->graph);
		}
	}
#endif
}

static void deg_debug_graphviz_graph_nodes(const DebugContext &ctx,
                                           const Depsgraph *graph)
{
	if (graph->root_node) {
		deg_debug_graphviz_node(ctx, graph->root_node);
	}
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin();
	     it != graph->id_hash.end();
	     ++it)
	{
		DepsNode *node = it->second;
		deg_debug_graphviz_node(ctx, node);
	}
	TimeSourceDepsNode *time_source = graph->find_time_source(NULL);
	if (time_source != NULL) {
		deg_debug_graphviz_node(ctx, time_source);
	}
}

static void deg_debug_graphviz_graph_relations(const DebugContext &ctx,
                                               const Depsgraph *graph)
{
#if 0
	if (graph->root_node) {
		deg_debug_graphviz_node_relations(ctx, graph->root_node);
	}
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin();
	     it != graph->id_hash.end();
	     ++it)
	{
		DepsNode *id_node = it->second;
		deg_debug_graphviz_node_relations(ctx, id_node);
	}
#else
	/* XXX not in use yet */
//	for (Depsgraph::OperationNodes::const_iterator it = graph->all_opnodes.begin();
//	     it != graph->all_opnodes.end();
//	     ++it)
//	{
//		OperationDepsNode *op_node = *it;
//		deg_debug_graphviz_node_relations(ctx, op_node);
//	}
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin();
	     it != graph->id_hash.end();
	     ++it)
	{
		IDDepsNode *id_node = it->second;
		for (IDDepsNode::ComponentMap::const_iterator it = id_node->components.begin();
		     it != id_node->components.end();
		     ++it)
		{
			ComponentDepsNode *comp_node = it->second;
			for (ComponentDepsNode::OperationMap::const_iterator it = comp_node->operations.begin();
			     it != comp_node->operations.end();
			     ++it)
			{
				OperationDepsNode *op_node = it->second;
				deg_debug_graphviz_node_relations(ctx, op_node);
			}
		}
	}

	TimeSourceDepsNode *time_source = graph->find_time_source(NULL);
	if (time_source != NULL) {
		deg_debug_graphviz_node_relations(ctx, time_source);
	}
#endif
}

void DEG_debug_graphviz(const Depsgraph *graph, FILE *f, const char *label, bool show_eval)
{
#if 0 /* generate shaded color set */
	static char colors[][3] = {{0xa6, 0xce, 0xe3},{0x1f, 0x78, 0xb4},{0xb2, 0xdf, 0x8a},{0x33, 0xa0, 0x2c},
	                           {0xfb, 0x9a, 0x99},{0xe3, 0x1a, 0x1c},{0xfd, 0xbf, 0x6f},{0xff, 0x7f, 0x00},
	                           {0xca, 0xb2, 0xd6},{0x6a, 0x3d, 0x9a},{0xff, 0xff, 0x99},{0xb1, 0x59, 0x28}};
	int i;
	const float factor = 0.666f;
	for (i=0; i < 12; ++i)
		printf("\"#%x%x%x\"\n", (char)(colors[i][0] * factor), (char)(colors[i][1] * factor), (char)(colors[i][2] * factor));
#endif

	if (!graph) {
		return;
	}

	DebugContext ctx;
	ctx.file = f;
	ctx.show_tags = show_eval;
	ctx.show_eval_priority = show_eval;

	deg_debug_fprintf(ctx, "digraph depgraph {" NL);
	deg_debug_fprintf(ctx, "rankdir=LR;" NL);
	deg_debug_fprintf(ctx, "graph [");
	deg_debug_fprintf(ctx, "compound=true");
	deg_debug_fprintf(ctx, ",labelloc=\"t\"");
	deg_debug_fprintf(ctx, ",fontsize=%f", deg_debug_graphviz_graph_label_size);
	deg_debug_fprintf(ctx, ",fontname=\"%s\"", deg_debug_graphviz_fontname);
	deg_debug_fprintf(ctx, ",label=\"%s\"", label);
	deg_debug_fprintf(ctx, ",splines=ortho");
	deg_debug_fprintf(ctx, ",overlap=scalexy"); // XXX: only when using neato
	deg_debug_fprintf(ctx, "];" NL);

	deg_debug_graphviz_graph_nodes(ctx, graph);
	deg_debug_graphviz_graph_relations(ctx, graph);

	deg_debug_graphviz_legend(ctx);

	deg_debug_fprintf(ctx, "}" NL);
}

#undef NL

/* ************************************************ */

static string get_component_name(eDepsNode_Type type, const string &name = "")
{
	DepsNodeFactory *factory = DEG_get_node_factory(type);
	if (name.empty()) {
		return string(factory->tname());
	}
	else {
		return string(factory->tname()) + " | " + name;
	}
}

static void times_clear(DepsgraphStatsTimes &times)
{
	times.duration_last = 0.0f;
}

static void times_add(DepsgraphStatsTimes &times, float time)
{
	times.duration_last += time;
}

void DepsgraphDebug::eval_begin(const EvaluationContext *UNUSED(eval_ctx))
{
	/* TODO(sergey): Stats are currently globally disabled. */
	/* verify_stats(); */
	reset_stats();
}

void DepsgraphDebug::eval_end(const EvaluationContext *UNUSED(eval_ctx))
{
	WM_main_add_notifier(NC_SPACE | ND_SPACE_INFO_REPORT, NULL);
}

void DepsgraphDebug::eval_step(const EvaluationContext *UNUSED(eval_ctx),
                               const char *message)
{
#ifdef DEG_DEBUG_BUILD
	if (deg_debug_eval_cb)
		deg_debug_eval_cb(deg_debug_eval_userdata, message);
#else
	(void)message;  /* Ignored. */
#endif
}

void DepsgraphDebug::task_started(Depsgraph *graph,
                                  const OperationDepsNode *node)
{
	if (stats) {
		BLI_spin_lock(&graph->lock);

		ComponentDepsNode *comp = node->owner;
		ID *id = comp->owner->id;

		DepsgraphStatsID *id_stats = get_id_stats(id, true);
		times_clear(id_stats->times);

		/* XXX TODO use something like: if (id->flag & ID_DEG_DETAILS) {...} */
		if (0) {
			/* XXX component name usage needs cleanup! currently mixes identifier and description strings! */
			DepsgraphStatsComponent *comp_stats = get_component_stats(id, get_component_name(comp->type, comp->name), true);
			times_clear(comp_stats->times);
		}

		BLI_spin_unlock(&graph->lock);
	}
}

void DepsgraphDebug::task_completed(Depsgraph *graph,
                                    const OperationDepsNode *node,
                                    double time)
{
	if (stats) {
		BLI_spin_lock(&graph->lock);

		ComponentDepsNode *comp = node->owner;
		ID *id = comp->owner->id;

		DepsgraphStatsID *id_stats = get_id_stats(id, true);
		times_add(id_stats->times, time);

		/* XXX TODO use something like: if (id->flag & ID_DEG_DETAILS) {...} */
		if (0) {
			/* XXX component name usage needs cleanup! currently mixes identifier and description strings! */
			DepsgraphStatsComponent *comp_stats = get_component_stats(id, get_component_name(comp->type, comp->name), true);
			times_add(comp_stats->times, time);
		}

		BLI_spin_unlock(&graph->lock);
	}
}

/* ********** */
/* Statistics */

DepsgraphStats *DepsgraphDebug::stats = NULL;

/* GHash callback */
static void deg_id_stats_free(void *val)
{
	DepsgraphStatsID *id_stats = (DepsgraphStatsID *)val;

	if (id_stats) {
		BLI_freelistN(&id_stats->components);
		MEM_freeN(id_stats);
	}
}

void DepsgraphDebug::stats_init()
{
	if (!stats) {
		stats = (DepsgraphStats *)MEM_callocN(sizeof(DepsgraphStats), "Depsgraph Stats");
		stats->id_stats = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "Depsgraph ID Stats Hash");
	}
}

void DepsgraphDebug::stats_free()
{
	if (stats) {
		BLI_ghash_free(stats->id_stats, NULL, deg_id_stats_free);
		MEM_freeN(stats);
		stats = NULL;
	}
}

void DepsgraphDebug::verify_stats()
{
	stats_init();
}

void DepsgraphDebug::reset_stats()
{
	if (!stats) {
		return;
	}

	/* XXX this doesn't work, will immediately clear all info,
	 * since most depsgraph updates have none or very few updates to handle.
	 *
	 * Could consider clearing only zero-user ID blocks here
	 */
//	BLI_ghash_clear(stats->id_stats, NULL, deg_id_stats_free);
}

DepsgraphStatsID *DepsgraphDebug::get_id_stats(ID *id, bool create)
{
	DepsgraphStatsID *id_stats = (DepsgraphStatsID *)BLI_ghash_lookup(stats->id_stats, id);

	if (!id_stats && create) {
		id_stats = (DepsgraphStatsID *)MEM_callocN(sizeof(DepsgraphStatsID), "Depsgraph ID Stats");
		id_stats->id = id;

		BLI_ghash_insert(stats->id_stats, id, id_stats);
	}

	return id_stats;
}

DepsgraphStatsComponent *DepsgraphDebug::get_component_stats(
        DepsgraphStatsID *id_stats,
        const string &name,
        bool create)
{
	DepsgraphStatsComponent *comp_stats;
	for (comp_stats = (DepsgraphStatsComponent *)id_stats->components.first;
	     comp_stats != NULL;
	     comp_stats = comp_stats->next)
	{
		if (STREQ(comp_stats->name, name.c_str()))
			break;
	}
	if (!comp_stats && create) {
		comp_stats = (DepsgraphStatsComponent *)MEM_callocN(sizeof(DepsgraphStatsComponent), "Depsgraph Component Stats");
		BLI_strncpy(comp_stats->name, name.c_str(), sizeof(comp_stats->name));
		BLI_addtail(&id_stats->components, comp_stats);
	}
	return comp_stats;
}

/* ------------------------------------------------ */

DepsgraphStats *DEG_stats(void)
{
	return DepsgraphDebug::stats;
}

void DEG_stats_verify()
{
	DepsgraphDebug::verify_stats();
}

DepsgraphStatsID *DEG_stats_id(ID *id)
{
	if (!DepsgraphDebug::stats) {
		return NULL;
	}
	return DepsgraphDebug::get_id_stats(id, false);
}

bool DEG_debug_compare(const struct Depsgraph *graph1,
                       const struct Depsgraph *graph2)
{
	BLI_assert(graph1 != NULL);
	BLI_assert(graph2 != NULL);
	if (graph1->operations.size() != graph2->operations.size()) {
		return false;
	}
	/* TODO(sergey): Currently we only do real stupid check,
	 * which is fast but which isn't 100% reliable.
	 *
	 * Would be cool to make it more robust, but it's good enough
	 * for now. Also, proper graph check is actually NP-complex
	 * problem..
	 */
	return true;
}

bool DEG_debug_scene_relations_validate(Main *bmain,
                                        Scene *scene)
{
	Depsgraph *depsgraph = DEG_graph_new();
	bool valid = true;
	DEG_graph_build_from_scene(depsgraph, bmain, scene);
	if (!DEG_debug_compare(depsgraph, scene->depsgraph)) {
		fprintf(stderr, "ERROR! Depsgraph wasn't tagged for update when it should have!\n");
		BLI_assert(!"This should not happen!");
		valid = false;
	}
	DEG_graph_free(depsgraph);
	return valid;
}

bool DEG_debug_consistency_check(Depsgraph *graph)
{
	/* Validate links exists in both directions. */
	foreach (OperationDepsNode *node, graph->operations) {
		foreach (DepsRelation *rel, node->outlinks) {
			int counter1 = 0;
			foreach (DepsRelation *tmp_rel, node->outlinks) {
				if (tmp_rel == rel) {
					++counter1;
				}
			}

			int counter2 = 0;
			foreach (DepsRelation *tmp_rel, rel->to->inlinks) {
				if (tmp_rel == rel) {
					++counter2;
				}
			}

			if (counter1 != counter2) {
				printf("Relation exists in outgoing direction but not in incoming (%d vs. %d).\n",
				       counter1, counter2);
				return false;
			}
		}
	}

	foreach (OperationDepsNode *node, graph->operations) {
		foreach (DepsRelation *rel, node->inlinks) {
			int counter1 = 0;
			foreach (DepsRelation *tmp_rel, node->inlinks) {
				if (tmp_rel == rel) {
					++counter1;
				}
			}

			int counter2 = 0;
			foreach (DepsRelation *tmp_rel, rel->from->outlinks) {
				if (tmp_rel == rel) {
					++counter2;
				}
			}

			if (counter1 != counter2) {
				printf("Relation exists in incoming direction but not in outcoming (%d vs. %d).\n",
				       counter1, counter2);
			}
		}
	}

	/* Validate node valency calculated in both directions. */
	foreach (OperationDepsNode *node, graph->operations) {
		node->num_links_pending = 0;
		node->done = 0;
	}

	foreach (OperationDepsNode *node, graph->operations) {
		if (node->done) {
			printf("Node %s is twice in the operations!\n",
			       node->identifier().c_str());
			return false;
		}
		foreach (DepsRelation *rel, node->outlinks) {
			if (rel->to->type == DEPSNODE_TYPE_OPERATION) {
				OperationDepsNode *to = (OperationDepsNode *)rel->to;
				BLI_assert(to->num_links_pending < to->inlinks.size());
				++to->num_links_pending;
			}
		}
		node->done = 1;
	}

	foreach (OperationDepsNode *node, graph->operations) {
		int num_links_pending = 0;
		foreach (DepsRelation *rel, node->inlinks) {
			if (rel->from->type == DEPSNODE_TYPE_OPERATION) {
				++num_links_pending;
			}
		}
		if (node->num_links_pending != num_links_pending) {
			printf("Valency mismatch: %s, %u != %d\n",
			       node->identifier().c_str(),
			       node->num_links_pending, num_links_pending);
			printf("Number of inlinks: %d\n", (int)node->inlinks.size());
			return false;
		}
	}
	return true;
}

/* ------------------------------------------------ */

/**
 * Obtain simple statistics about the complexity of the depsgraph
 * \param[out] r_outer       The number of outer nodes in the graph
 * \param[out] r_operations  The number of operation nodes in the graph
 * \param[out] r_relations   The number of relations between (executable) nodes in the graph
 */
void DEG_stats_simple(const Depsgraph *graph, size_t *r_outer,
                      size_t *r_operations, size_t *r_relations)
{
	/* number of operations */
	if (r_operations) {
		/* All operations should be in this list, allowing us to count the total
		 * number of nodes.
		 */
		*r_operations = graph->operations.size();
	}

	/* Count number of outer nodes and/or relations between these. */
	if (r_outer || r_relations) {
		size_t tot_outer = 0;
		size_t tot_rels = 0;

		for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin();
		     it != graph->id_hash.end();
		     ++it)
		{
			IDDepsNode *id_node = it->second;
			tot_outer++;
			for (IDDepsNode::ComponentMap::const_iterator it = id_node->components.begin();
			     it != id_node->components.end();
			     ++it)
			{
				ComponentDepsNode *comp_node = it->second;
				tot_outer++;
				for (ComponentDepsNode::OperationMap::const_iterator it = comp_node->operations.begin();
				     it != comp_node->operations.end();
				     ++it)
				{
					OperationDepsNode *op_node = it->second;
					tot_rels += op_node->inlinks.size();
				}
			}
		}

		TimeSourceDepsNode *time_source = graph->find_time_source(NULL);
		if (time_source != NULL) {
			tot_rels += time_source->inlinks.size();
		}

		if (r_relations) *r_relations = tot_rels;
		if (r_outer)     *r_outer     = tot_outer;
	}
}

