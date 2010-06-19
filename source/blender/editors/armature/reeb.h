/**
 * $Id$
 *
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
 * Contributor(s): Martin Poirier
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef REEB_H_
#define REEB_H_

#define WITH_BF_REEB

#include "DNA_listBase.h"

#include "BLI_graph.h"

struct GHash;
struct EdgeHash;
struct ReebArc;
struct ReebEdge;
struct ReebNode;

typedef struct ReebGraph {
	ListBase	arcs;
	ListBase	nodes;
	
	float length;
	
	FreeArc			free_arc;
	FreeNode		free_node;
	RadialSymmetry	radial_symmetry;
	AxialSymmetry	axial_symmetry;
	/*********************************/
	
	int resolution;
	int totnodes;
	struct EdgeHash *emap;
	int multi_level;
	struct ReebGraph *link_up; /* for multi resolution filtering, points to higher levels */
} ReebGraph;

typedef struct EmbedBucket {
	float val;
	int	  nv;
	float p[3];
	float no[3]; /* if non-null, normal of the bucket */
} EmbedBucket;

typedef struct ReebNode {
	void *next, *prev;
	float p[3];
	int flag;

	int degree;
	struct ReebArc **arcs;

	int subgraph_index;

	int symmetry_level;
	int symmetry_flag;
	float symmetry_axis[3];
	/*********************************/
	
	float no[3];

	int index;
	float weight;
	int	multi_level;
	struct ReebNode *link_down; /* for multi resolution filtering, points to lower levels, if present */
	struct ReebNode *link_up;
} ReebNode;

typedef struct ReebEdge {
	struct ReebEdge *next, *prev;
	struct ReebArc  *arc;
	struct ReebNode *v1, *v2;
	struct ReebEdge *nextEdge;
	int flag;
} ReebEdge;

typedef struct ReebArc {
	void *next, *prev;
	struct ReebNode *head, *tail;
	int flag;

	float length;

	int symmetry_level;
	int symmetry_group;
	int symmetry_flag;
	/*********************************/

	ListBase edges;
	int bcount;
	struct EmbedBucket *buckets;

	struct GHash *faces;	
	float angle;
	struct ReebArc *link_up; /* for multi resolution filtering, points to higher levels */
} ReebArc;

typedef struct ReebArcIterator {
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
	/*********************************/
	struct ReebArc	*arc;
	int start;
	int end;
	int stride;
} ReebArcIterator;

struct EditMesh;
struct EdgeIndex;

int weightToHarmonic(struct EditMesh *em, struct EdgeIndex *indexed_edges);
int weightFromDistance(struct EditMesh *em, struct EdgeIndex *indexed_edges);
int weightFromLoc(struct EditMesh *me, int axis);
void weightToVCol(struct EditMesh *em, int index);
void arcToVCol(struct ReebGraph *rg, struct EditMesh *em, int index);
void angleToVCol(struct EditMesh *em, int index);
void renormalizeWeight(struct EditMesh *em, float newmax);

ReebGraph * generateReebGraph(struct EditMesh *me, int subdivisions);
ReebGraph * newReebGraph();

void initArcIterator(BArcIterator *iter, struct ReebArc *arc, struct ReebNode *head);
void initArcIterator2(BArcIterator *iter, struct ReebArc *arc, int start, int end);
void initArcIteratorStart(BArcIterator *iter, struct ReebArc *arc, struct ReebNode *head, int start);

/* Filtering */
void filterNullReebGraph(ReebGraph *rg);
int filterSmartReebGraph(ReebGraph *rg, float threshold);
int filterExternalReebGraph(ReebGraph *rg, float threshold);
int filterInternalReebGraph(ReebGraph *rg, float threshold);

/* Post-Build processing */
void repositionNodes(ReebGraph *rg);
void postprocessGraph(ReebGraph *rg, char mode);
void removeNormalNodes(ReebGraph *rg);

void sortNodes(ReebGraph *rg);
void sortArcs(ReebGraph *rg);

/*------------ Sanity check ------------*/
void verifyBuckets(ReebGraph *rg);
void verifyFaces(ReebGraph *rg);

/*********************** PUBLIC *********************************/

#define REEB_MAX_MULTI_LEVEL	10

struct bContext;

ReebGraph *BIF_ReebGraphFromEditMesh(void);
ReebGraph *BIF_ReebGraphMultiFromEditMesh(struct bContext *C);
void BIF_flagMultiArcs(ReebGraph *rg, int flag);

void BIF_GlobalReebGraphFromEditMesh(void);
void BIF_GlobalReebFree(void);

ReebNode *BIF_otherNodeFromIndex(ReebArc *arc, ReebNode *node);
ReebNode *BIF_NodeFromIndex(ReebArc *arc, ReebNode *node);
ReebNode *BIF_lowestLevelNode(ReebNode *node);

ReebGraph *BIF_graphForMultiNode(ReebGraph *rg, ReebNode *node);

void REEB_freeGraph(ReebGraph *rg);
void REEB_freeArc(BArc *barc);
void REEB_exportGraph(ReebGraph *rg, int count);
void REEB_draw();


#endif /*REEB_H_*/
