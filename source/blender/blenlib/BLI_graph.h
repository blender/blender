#ifndef BLI_GRAPH_H_
#define BLI_GRAPH_H_

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

/* Helper structure for radial symmetry */
typedef struct RadialArc
{
	struct BArc *arc; 
	float n[3]; /* normalized vector joining the nodes of the arc */
} RadialArc;

BNode *BLI_otherNode(BArc *arc, BNode *node);

void BLI_freeNode(BGraph *graph, BNode *node);
void BLI_removeNode(BGraph *graph, BNode *node);

void BLI_flagNodes(BGraph *graph, int flag);
void BLI_flagArcs(BGraph *graph, int flag);

int BLI_hasAdjacencyList(BGraph *rg);
void BLI_buildAdjacencyList(BGraph *rg);
void BLI_rebuildAdjacencyList(BGraph* rg);
void BLI_rebuildAdjacencyListForNode(BGraph* rg, BNode *node);
void BLI_freeAdjacencyList(BGraph *rg);

int BLI_FlagSubgraphs(BGraph *graph);
void BLI_ReflagSubgraph(BGraph *graph, int old_subgraph, int new_subgraph);

#define SHAPE_RADIX 10 /* each shape level is encoded this base */

int BLI_subtreeShape(BNode *node, BArc *rootArc, int include_root);
float BLI_subtreeLength(BNode *node, BArc *rootArc);
void BLI_calcGraphLength(BGraph *graph);

void BLI_replaceNode(BGraph *graph, BNode *node_src, BNode *node_replaced);
void BLI_removeDoubleNodes(BGraph *graph, float limit);

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

#endif /*BLI_GRAPH_H_*/
