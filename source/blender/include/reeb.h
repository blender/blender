/**
 * $Id: 
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Martin Poirier
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef REEB_H_
#define REEB_H_

#include "DNA_listBase.h"

struct GHash;
struct EdgeHash;
struct ReebArc;
struct ReebEdge;
struct ReebNode;

typedef struct ReebGraph {
	ListBase arcs;
	ListBase nodes;
	int totnodes;
	struct EdgeHash *emap;
} ReebGraph;

typedef struct EmbedBucket {
	float val;
	int	  nv;
	float p[3];
} EmbedBucket;

typedef struct ReebNode {
	struct ReebNode *next, *prev;
	struct ReebArc **arcs;
	int index;
	int degree;
	float weight;
	float p[3];
	int flag;

	int symmetry_level;
	int symmetry_flag;
	float symmetry_axis[3];
} ReebNode;

typedef struct ReebEdge {
	struct ReebEdge *next, *prev;
	struct ReebArc  *arc;
	struct ReebNode *v1, *v2;
	struct ReebEdge *nextEdge;
	int flag;
} ReebEdge;

typedef struct ReebArc {
	struct ReebArc *next, *prev;
	ListBase edges;
	struct ReebNode *v1, *v2;
	struct EmbedBucket *buckets;
	int	bcount;
	int flag;

	int symmetry_level;
	int symmetry_flag;

	struct GHash *faces;	
	float angle;
} ReebArc;

typedef struct ReebArcIterator {
	struct ReebArc	*arc;
	int index;
	int start;
	int end;
	int stride;
} ReebArcIterator;

struct EditMesh;

int weightToHarmonic(struct EditMesh *em);
int weightFromDistance(struct EditMesh *em);
int weightFromLoc(struct EditMesh *me, int axis);
void weightToVCol(struct EditMesh *em, int index);
void arcToVCol(struct ReebGraph *rg, struct EditMesh *em, int index);
void angleToVCol(EditMesh *em, int index);
void renormalizeWeight(struct EditMesh *em, float newmax);

ReebGraph * generateReebGraph(struct EditMesh *me, int subdivisions);
ReebGraph * newReebGraph();

#define OTHER_NODE(arc, node) ((arc->v1 == node) ? arc->v2 : arc->v1)

void initArcIterator(struct ReebArcIterator *iter, struct ReebArc *arc, struct ReebNode *head);
void initArcIterator2(struct ReebArcIterator *iter, struct ReebArc *arc, int start, int end);
void initArcIteratorStart(struct ReebArcIterator *iter, struct ReebArc *arc, struct ReebNode *head, int start);
struct EmbedBucket * nextBucket(struct ReebArcIterator *iter);
struct EmbedBucket * nextNBucket(ReebArcIterator *iter, int n);
struct EmbedBucket * currentBucket(struct ReebArcIterator *iter);
struct EmbedBucket * previousBucket(struct ReebArcIterator *iter);
int iteratorStopped(struct ReebArcIterator *iter);

/* Filtering */
void filterNullReebGraph(ReebGraph *rg);
int filterSmartReebGraph(ReebGraph *rg, float threshold);
int filterExternalReebGraph(ReebGraph *rg, float threshold);
int filterInternalReebGraph(ReebGraph *rg, float threshold);

/* Post-Build processing */
void repositionNodes(ReebGraph *rg);
void postprocessGraph(ReebGraph *rg, char mode);
void removeNormalNodes(ReebGraph *rg);

/* Graph processing */
void buildAdjacencyList(ReebGraph *rg);

void sortNodes(ReebGraph *rg);
void sortArcs(ReebGraph *rg);

int subtreeDepth(ReebNode *node, ReebArc *rootArc);
int countConnectedArcs(ReebGraph *rg, ReebNode *node);
int hasAdjacencyList(ReebGraph *rg); 
int	isGraphCyclic(ReebGraph *rg);

/*------------ Symmetry handling ------------*/
void markdownSymmetry(ReebGraph *rg);

/* ReebNode symmetry flags */
#define SYM_TOPOLOGICAL	1
#define SYM_PHYSICAL	2

/* the following two are exclusive */
#define SYM_AXIAL		4
#define SYM_RADIAL		8

/* ReebArc symmetry flags
 * 
 * axial symetry sides */
#define SYM_SIDE_POSITIVE		1
#define SYM_SIDE_NEGATIVE		2



/*------------ Sanity check ------------*/
void verifyBuckets(ReebGraph *rg);
void verifyFaces(ReebGraph *rg);

/*********************** PUBLIC *********************************/
ReebGraph *BIF_ReebGraphFromEditMesh(void);
void REEB_freeGraph(ReebGraph *rg);
void REEB_exportGraph(ReebGraph *rg, int count);

#endif /*REEB_H_*/
