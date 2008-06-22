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

#include "BLI_graph.h"

struct GHash;
struct EdgeHash;
struct ReebArc;
struct ReebEdge;
struct ReebNode;

typedef struct ReebGraph {
	ListBase	arcs;
	ListBase	nodes;
	
	FreeArc			free_arc;
	FreeNode		free_node;
	RadialSymmetry	radial_symmetry;
	AxialSymmetry	axial_symmetry;
	/*********************************/
	
	int totnodes;
	struct EdgeHash *emap;
} ReebGraph;

typedef struct EmbedBucket {
	float val;
	int	  nv;
	float p[3];
} EmbedBucket;

typedef struct ReebNode {
	void *next, *prev;
	float p[3];
	int flag;

	int degree;
	struct ReebArc **arcs;

	int symmetry_level;
	int symmetry_flag;
	float symmetry_axis[3];
	/*********************************/

	int index;
	float weight;
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
} ReebArc;

typedef struct ReebArcIterator {
	struct ReebArc	*arc;
	int index;
	int start;
	int end;
	int stride;
	int length;
} ReebArcIterator;

struct EditMesh;

int weightToHarmonic(struct EditMesh *em);
int weightFromDistance(struct EditMesh *em);
int weightFromLoc(struct EditMesh *me, int axis);
void weightToVCol(struct EditMesh *em, int index);
void arcToVCol(struct ReebGraph *rg, struct EditMesh *em, int index);
void angleToVCol(struct EditMesh *em, int index);
void renormalizeWeight(struct EditMesh *em, float newmax);

ReebGraph * generateReebGraph(struct EditMesh *me, int subdivisions);
ReebGraph * newReebGraph();

void initArcIterator(struct ReebArcIterator *iter, struct ReebArc *arc, struct ReebNode *head);
void initArcIterator2(struct ReebArcIterator *iter, struct ReebArc *arc, int start, int end);
void initArcIteratorStart(struct ReebArcIterator *iter, struct ReebArc *arc, struct ReebNode *head, int start);
struct EmbedBucket * nextBucket(struct ReebArcIterator *iter);
struct EmbedBucket * nextNBucket(ReebArcIterator *iter, int n);
struct EmbedBucket * peekBucket(ReebArcIterator *iter, int n);
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

void sortNodes(ReebGraph *rg);
void sortArcs(ReebGraph *rg);

/*------------ Sanity check ------------*/
void verifyBuckets(ReebGraph *rg);
void verifyFaces(ReebGraph *rg);

/*********************** PUBLIC *********************************/
ReebGraph *BIF_ReebGraphFromEditMesh(void);

void BIF_GlobalReebGraphFromEditMesh(void);
void BIF_GlobalReebFree(void);

void REEB_freeGraph(ReebGraph *rg);
void REEB_exportGraph(ReebGraph *rg, int count);
void REEB_draw();


#endif /*REEB_H_*/
