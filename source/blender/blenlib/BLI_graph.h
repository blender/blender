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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BLI_GRAPH_H__
#define __BLI_GRAPH_H__

/** \file BLI_graph.h
 *  \ingroup bli
 */

#include "DNA_listBase.h"

struct BGraph;
struct BNode;
struct BArc;

struct RadialArc;

typedef void (*FreeArc)(struct BArc*);
typedef void (*FreeNode)(struct BNode*);
typedef void (*RadialSymmetry)(struct BNode* root_node, struct RadialArc* ring, int total);
typedef void (*AxialSymmetry)(struct BNode* root_node, struct BNode* node1, struct BNode* node2, struct BArc* arc1, struct BArc* arc2);

/* IF YOU MODIFY THOSE TYPES, YOU NEED TO UPDATE ALL THOSE THAT "INHERIT" FROM THEM
 * 
 * RigGraph, ReebGraph
 * 
 * */

typedef struct BGraph {
	ListBase	arcs;
	ListBase	nodes;
	
	float length;
	
	/* function pointer to deal with custom fonctionnality */
	FreeArc			free_arc;
	FreeNode		free_node;
	RadialSymmetry	radial_symmetry;
	AxialSymmetry	axial_symmetry;
} BGraph;

typedef struct BNode {
	void *next, *prev;
	float p[3];
	int flag;

	int degree;
	struct BArc **arcs;
	
	int subgraph_index;

	int symmetry_level;
	int symmetry_flag;
	float symmetry_axis[3];
} BNode;

typedef struct BArc {
	void *next, *prev;
	struct BNode *head, *tail;
	int flag;

	float length;

	int symmetry_level;
	int symmetry_group;
	int symmetry_flag;
} BArc;

struct BArcIterator;

void* IT_head(void* iter);
void* IT_tail(void* iter);
void* IT_peek(void* iter, int n);
void* IT_next(void* iter);
void* IT_nextN(void* iter, int n);
void* IT_previous(void* iter);
int   IT_stopped(void* iter);

typedef void* (*HeadFct)(void* iter);
typedef void* (*TailFct)(void* iter);
typedef void* (*PeekFct)(void* iter, int n);
typedef void* (*NextFct)(void* iter);
typedef void* (*NextNFct)(void* iter, int n);
typedef void* (*PreviousFct)(void* iter);
typedef int   (*StoppedFct)(void* iter);

typedef struct BArcIterator {
	HeadFct		head;
	TailFct		tail;
	PeekFct		peek;
	NextFct		next;
	NextNFct	nextN;
	PreviousFct	previous;
	StoppedFct	stopped;
	
	float *p, *no;
	float size;
	
	int length;
	int index;
} BArcIterator;

/* Helper structure for radial symmetry */
typedef struct RadialArc
{
	struct BArc *arc; 
	float n[3]; /* normalized vector joining the nodes of the arc */
} RadialArc;

BNode *BLI_otherNode(BArc *arc, BNode *node);

void BLI_freeNode(BGraph *graph, BNode *node);
void BLI_removeNode(BGraph *graph, BNode *node);

void BLI_removeArc(BGraph *graph, BArc *arc);

void BLI_flagNodes(BGraph *graph, int flag);
void BLI_flagArcs(BGraph *graph, int flag);

int BLI_hasAdjacencyList(BGraph *rg);
void BLI_buildAdjacencyList(BGraph *rg);
void BLI_rebuildAdjacencyListForNode(BGraph* rg, BNode *node);
void BLI_freeAdjacencyList(BGraph *rg);

int BLI_FlagSubgraphs(BGraph *graph);
void BLI_ReflagSubgraph(BGraph *graph, int old_subgraph, int new_subgraph);

#define SHAPE_RADIX 10 /* each shape level is encoded this base */

int BLI_subtreeShape(BGraph *graph, BNode *node, BArc *rootArc, int include_root);
float BLI_subtreeLength(BNode *node);
void BLI_calcGraphLength(BGraph *graph);

void BLI_replaceNode(BGraph *graph, BNode *node_src, BNode *node_replaced);
void BLI_replaceNodeInArc(BGraph *graph, BArc *arc, BNode *node_src, BNode *node_replaced);
void BLI_removeDoubleNodes(BGraph *graph, float limit);
BNode * BLI_FindNodeByPosition(BGraph *graph, float *p, float limit);

BArc * BLI_findConnectedArc(BGraph *graph, BArc *arc, BNode *v);

int	BLI_isGraphCyclic(BGraph *graph);

/*------------ Symmetry handling ------------*/
void BLI_markdownSymmetry(BGraph *graph, BNode *root_node, float limit);

void BLI_mirrorAlongAxis(float v[3], float center[3], float axis[3]);

/* BNode symmetry flags */
#define SYM_TOPOLOGICAL	1
#define SYM_PHYSICAL	2

/* the following two are exclusive */
#define SYM_AXIAL		4
#define SYM_RADIAL		8

/* BArc symmetry flags
 * 
 * axial symetry sides */
#define SYM_SIDE_POSITIVE		1
#define SYM_SIDE_NEGATIVE		2
/* Anything higher is the order in radial symmetry */
#define SYM_SIDE_RADIAL			3

#endif /*__BLI_GRAPH_H__*/
