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
 * Contributor(s): Martin Poirier
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/armature/reeb.c
 *  \ingroup edarmature
 */

 
#include <math.h>
#include <string.h> // for memcpy
#include <stdio.h>
#include <stdlib.h> // for qsort
#include <float.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_editVert.h"
#include "BLI_edgehash.h"
#include "BLI_ghash.h"
#include "BLI_heap.h"

//#include "BDR_editobject.h"

//#include "BIF_interface.h"
//#include "BIF_toolbox.h"
//#include "BIF_graphics.h"

#include "BKE_mesh.h"

//#include "blendef.h"

#include "ONL_opennl.h"

#include "reeb.h"

#if 0 /* UNUSED 2.5 */
static ReebGraph *GLOBAL_RG = NULL;
static ReebGraph *FILTERED_RG = NULL;
#endif

/*
 * Skeleton generation algorithm based on: 
 * "Harmonic Skeleton for Realistic Character Animation"
 * Gregoire Aujay, Franck Hetroy, Francis Lazarus and Christine Depraz
 * SIGGRAPH 2007
 * 
 * Reeb graph generation algorithm based on: 
 * "Robust On-line Computation of Reeb Graphs: Simplicity and Speed"
 * Valerio Pascucci, Giorgio Scorzelli, Peer-Timo Bremer and Ajith Mascarenhas
 * SIGGRAPH 2007
 * 
 * */
 
#define DEBUG_REEB
#define DEBUG_REEB_NODE

typedef struct VertexData
{
	float w; /* weight */
	int i; /* index */
	ReebNode *n;
} VertexData;

typedef struct EdgeIndex
{
	EditEdge **edges;
	int		 *offset;
} EdgeIndex;

typedef enum {
	MERGE_LOWER,
	MERGE_HIGHER,
	MERGE_APPEND
} MergeDirection;

int mergeArcs(ReebGraph *rg, ReebArc *a0, ReebArc *a1);
void mergeArcEdges(ReebGraph *rg, ReebArc *aDst, ReebArc *aSrc, MergeDirection direction);
int mergeConnectedArcs(ReebGraph *rg, ReebArc *a0, ReebArc *a1);
EditEdge * NextEdgeForVert(EdgeIndex *indexed_edges, int index);
void mergeArcFaces(ReebGraph *rg, ReebArc *aDst, ReebArc *aSrc);
void addFacetoArc(ReebArc *arc, EditFace *efa);

void REEB_RadialSymmetry(BNode* root_node, RadialArc* ring, int count);
void REEB_AxialSymmetry(BNode* root_node, BNode* node1, BNode* node2, struct BArc* barc1, BArc* barc2);

void flipArcBuckets(ReebArc *arc);


/***************************************** UTILS **********************************************/

#if 0 /* UNUSED */
static VertexData *allocVertexData(EditMesh *em)
{
	VertexData *data;
	EditVert *eve;
	int totvert, index;
	
	totvert = BLI_countlist(&em->verts);
	
	data = MEM_callocN(sizeof(VertexData) * totvert, "VertexData");

	for(index = 0, eve = em->verts.first; eve; index++, eve = eve->next)
	{
		data[index].i = index;
		data[index].w = 0;
		eve->tmp.p = data + index;
	}
		
	return data;
}

static int indexData(EditVert *eve)
{
	return ((VertexData*)eve->tmp.p)->i;
}

static float weightData(EditVert *eve)
{
	return ((VertexData*)eve->tmp.p)->w;
}

static void weightSetData(EditVert *eve, float w)
{
	((VertexData*)eve->tmp.p)->w = w;
}

static ReebNode* nodeData(EditVert *eve)
{
	return ((VertexData*)eve->tmp.p)->n;
}

static void nodeSetData(EditVert *eve, ReebNode *n)
{
	((VertexData*)eve->tmp.p)->n = n;
}

#endif

void REEB_freeArc(BArc *barc)
{
	ReebArc *arc = (ReebArc*)barc;
	BLI_freelistN(&arc->edges);
	
	if (arc->buckets)
		MEM_freeN(arc->buckets);
		
	if (arc->faces)
		BLI_ghash_free(arc->faces, NULL, NULL);
	
	MEM_freeN(arc);
}

void REEB_freeGraph(ReebGraph *rg)
{
	ReebArc *arc;
	ReebNode *node;
	
	// free nodes
	for( node = rg->nodes.first; node; node = node->next )
	{
		BLI_freeNode((BGraph*)rg, (BNode*)node);
	}
	BLI_freelistN(&rg->nodes);
	
	// free arcs
	arc = rg->arcs.first;
	while( arc )
	{
		ReebArc *next = arc->next;
		REEB_freeArc((BArc*)arc);
		arc = next;
	}
	
	// free edge map
	BLI_edgehash_free(rg->emap, NULL);
	
	/* free linked graph */
	if (rg->link_up)
	{
		REEB_freeGraph(rg->link_up);
	}
	
	MEM_freeN(rg);
}

ReebGraph * newReebGraph(void)
{
	ReebGraph *rg;
	rg = MEM_callocN(sizeof(ReebGraph), "reeb graph");
	
	rg->totnodes = 0;
	rg->emap = BLI_edgehash_new();
	
	
	rg->free_arc = REEB_freeArc;
	rg->free_node = NULL;
	rg->radial_symmetry = REEB_RadialSymmetry;
	rg->axial_symmetry = REEB_AxialSymmetry;
	
	return rg;
}

void BIF_flagMultiArcs(ReebGraph *rg, int flag)
{
	for ( ; rg; rg = rg->link_up)
	{
		BLI_flagArcs((BGraph*)rg, flag);
	}
}

#if 0 /* UNUSED */
static ReebNode * addNode(ReebGraph *rg, EditVert *eve)
{
	float weight;
	ReebNode *node = NULL;
	
	weight = weightData(eve);
	
	node = MEM_callocN(sizeof(ReebNode), "reeb node");
	
	node->flag = 0; // clear flag on init
	node->symmetry_level = 0;
	node->arcs = NULL;
	node->degree = 0;
	node->weight = weight;
	node->index = rg->totnodes;
	copy_v3_v3(node->p, eve->co);
	
	BLI_addtail(&rg->nodes, node);
	rg->totnodes++;
	
	nodeSetData(eve, node);
	
	return node;
}

static ReebNode * copyNode(ReebGraph *rg, ReebNode *node)
{
	ReebNode *cp_node = NULL;
	
	cp_node = MEM_callocN(sizeof(ReebNode), "reeb node copy");
	
	memcpy(cp_node, node, sizeof(ReebNode));
	
	cp_node->prev = NULL;
	cp_node->next = NULL;
	cp_node->arcs = NULL;
	
	cp_node->link_up = NULL;
	cp_node->link_down = NULL;
	
	BLI_addtail(&rg->nodes, cp_node);
	rg->totnodes++;
	
	return cp_node; 
}

static void relinkNodes(ReebGraph *low_rg, ReebGraph *high_rg)
{
	ReebNode *low_node, *high_node;
	
	if (low_rg == NULL || high_rg == NULL)
	{
		return;
	}
	
	for (low_node = low_rg->nodes.first; low_node; low_node = low_node->next)
	{
		for (high_node = high_rg->nodes.first; high_node; high_node = high_node->next)
		{
			if (low_node->index == high_node->index)
			{
				high_node->link_down = low_node;
				low_node->link_up = high_node;
				break;
			}
		}
	}
}
#endif 

ReebNode *BIF_otherNodeFromIndex(ReebArc *arc, ReebNode *node)
{
	return (arc->head->index == node->index) ? arc->tail : arc->head;
}

ReebNode *BIF_NodeFromIndex(ReebArc *arc, ReebNode *node)
{
	return (arc->head->index == node->index) ? arc->head : arc->tail;
}

ReebNode *BIF_lowestLevelNode(ReebNode *node)
{
	while (node->link_down)
	{
		node = node->link_down;
	}
	
	return node;
}

#if 0 /* UNUSED */
static ReebArc * copyArc(ReebGraph *rg, ReebArc *arc)
{
	ReebArc *cp_arc;
	ReebNode *node;
	
	cp_arc = MEM_callocN(sizeof(ReebArc), "reeb arc copy");

	memcpy(cp_arc, arc, sizeof(ReebArc));
	
	cp_arc->link_up = arc;
	
	cp_arc->head = NULL;
	cp_arc->tail = NULL;

	cp_arc->prev = NULL;
	cp_arc->next = NULL;

	cp_arc->edges.first = NULL;
	cp_arc->edges.last = NULL;

	/* copy buckets */	
	cp_arc->buckets = MEM_callocN(sizeof(EmbedBucket) * cp_arc->bcount, "embed bucket");
	memcpy(cp_arc->buckets, arc->buckets, sizeof(EmbedBucket) * cp_arc->bcount);
	
	/* copy faces map */
	cp_arc->faces = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "copyArc gh");
	mergeArcFaces(rg, cp_arc, arc);
	
	/* find corresponding head and tail */
	for (node = rg->nodes.first; node && (cp_arc->head == NULL || cp_arc->tail == NULL); node = node->next)
	{
		if (node->index == arc->head->index)
		{
			cp_arc->head = node;
		}
		else if (node->index == arc->tail->index)
		{
			cp_arc->tail = node;
		}
	}
	
	BLI_addtail(&rg->arcs, cp_arc);
	
	return cp_arc;
}

static ReebGraph * copyReebGraph(ReebGraph *rg, int level)
{
	ReebNode *node;
	ReebArc *arc;
	ReebGraph *cp_rg = newReebGraph();
	
	cp_rg->resolution = rg->resolution;
	cp_rg->length = rg->length;
	cp_rg->link_up = rg;
	cp_rg->multi_level = level;

	/* Copy nodes */	
	for (node = rg->nodes.first; node; node = node->next)
	{
		ReebNode *cp_node = copyNode(cp_rg, node);
		cp_node->multi_level = level;
	}
	
	/* Copy arcs */
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		copyArc(cp_rg, arc);
	}
	
	BLI_buildAdjacencyList((BGraph*)cp_rg);
	
	return cp_rg;
}
#endif

ReebGraph *BIF_graphForMultiNode(ReebGraph *rg, ReebNode *node)
{
	ReebGraph *multi_rg = rg;
	
	while(multi_rg && multi_rg->multi_level != node->multi_level)
	{
		multi_rg = multi_rg->link_up;
	}
	
	return multi_rg;
}

#if 0 /* UNUSED */
static ReebEdge * copyEdge(ReebEdge *edge)
{
	ReebEdge *newEdge = NULL;
	
	newEdge = MEM_callocN(sizeof(ReebEdge), "reeb edge");
	memcpy(newEdge, edge, sizeof(ReebEdge));
	
	newEdge->next = NULL;
	newEdge->prev = NULL;
	
	return newEdge;
}

static void printArc(ReebArc *arc)
{
	ReebEdge *edge;
	ReebNode *head = (ReebNode*)arc->head;
	ReebNode *tail = (ReebNode*)arc->tail;
	printf("arc: (%i) %f -> (%i) %f\n", head->index, head->weight, tail->index, tail->weight);
	
	for(edge = arc->edges.first; edge ; edge = edge->next)
	{
		printf("\tedge (%i, %i)\n", edge->v1->index, edge->v2->index);
	}
}

static void flipArc(ReebArc *arc)
{
	ReebNode *tmp;
	tmp = arc->head;
	arc->head = arc->tail;
	arc->tail = tmp;
	
	flipArcBuckets(arc);
}

#ifdef DEBUG_REEB_NODE
static void NodeDegreeDecrement(ReebGraph *UNUSED(rg), ReebNode *node)
{
	node->degree--;

//	if (node->degree == 0)
//	{
//		printf("would remove node %i\n", node->index);
//	}
}

static void NodeDegreeIncrement(ReebGraph *UNUSED(rg), ReebNode *node)
{
//	if (node->degree == 0)
//	{
//		printf("first connect node %i\n", node->index);
//	}

	node->degree++;
}

#else
#define NodeDegreeDecrement(rg, node) {node->degree--;}
#define NodeDegreeIncrement(rg, node) {node->degree++;}
#endif

void repositionNodes(ReebGraph *rg)
{
	BArc *arc = NULL;
	BNode *node = NULL;
	
	// Reset node positions
	for(node = rg->nodes.first; node; node = node->next)
	{
		node->p[0] = node->p[1] = node->p[2] = 0;
	}
	
	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
		if (((ReebArc*)arc)->bcount > 0)
		{
			float p[3];
			
			copy_v3_v3(p, ((ReebArc*)arc)->buckets[0].p);
			mul_v3_fl(p, 1.0f / arc->head->degree);
			add_v3_v3(arc->head->p, p);
			
			copy_v3_v3(p, ((ReebArc*)arc)->buckets[((ReebArc*)arc)->bcount - 1].p);
			mul_v3_fl(p, 1.0f / arc->tail->degree);
			add_v3_v3(arc->tail->p, p);
		}
	}
}

void verifyNodeDegree(ReebGraph *rg)
{
#ifdef DEBUG_REEB
	ReebNode *node = NULL;
	ReebArc *arc = NULL;

	for(node = rg->nodes.first; node; node = node->next)
	{
		int count = 0;
		for(arc = rg->arcs.first; arc; arc = arc->next)
		{
			if (arc->head == node || arc->tail == node)
			{
				count++;
			}
		}
		if (count != node->degree)
		{
			printf("degree error in node %i: expected %i got %i\n", node->index, count, node->degree);
		}
		if (node->degree == 0)
		{
			printf("zero degree node %i with weight %f\n", node->index, node->weight);
		}
	}
#endif
}

static void verifyBucketsArc(ReebGraph *UNUSED(rg), ReebArc *arc)
{
	ReebNode *head = (ReebNode*)arc->head;
	ReebNode *tail = (ReebNode*)arc->tail;

	if (arc->bcount > 0)
	{
		int i;
		for(i = 0; i < arc->bcount; i++)
		{
			if (arc->buckets[i].nv == 0)
			{
				printArc(arc);
				printf("count error in bucket %i/%i\n", i+1, arc->bcount);
			}
		}
		
		if (ceilf(head->weight) != arc->buckets[0].val)
		{
			printArc(arc);
			printf("alloc error in first bucket: %f should be %f \n", arc->buckets[0].val, ceil(head->weight));
		}
		if (floorf(tail->weight) != arc->buckets[arc->bcount - 1].val)
		{
			printArc(arc);
			printf("alloc error in last bucket: %f should be %f \n", arc->buckets[arc->bcount - 1].val, floor(tail->weight));
		}
	}
}

void verifyBuckets(ReebGraph *rg)
{
#ifdef DEBUG_REEB
	ReebArc *arc = NULL;
	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
		verifyBucketsArc(rg, arc);
	}
#endif
}

void verifyFaces(ReebGraph *rg)
{
#ifdef DEBUG_REEB
	int total = 0;
	ReebArc *arc = NULL;
	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
		total += BLI_ghash_size(arc->faces);
	}
	
#endif
}

void verifyArcs(ReebGraph *rg)
{
	ReebArc *arc;
	
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		if (arc->head->weight > arc->tail->weight)
		{
			printf("FLIPPED ARC!\n");
		}
	}
}

static void verifyMultiResolutionLinks(ReebGraph *rg, int level)
{
#ifdef DEBUG_REEB
	ReebGraph *lower_rg = rg->link_up;
	
	if (lower_rg)
	{
		ReebArc *arc;
		
		for (arc = rg->arcs.first; arc; arc = arc->next)
		{
			if (BLI_findindex(&lower_rg->arcs, arc->link_up) == -1)
			{
				printf("missing arc %p for level %i\n", (void *)arc->link_up, level);
				printf("Source arc was ---\n");
				printArc(arc);

				arc->link_up = NULL;
			}
		}
		
		
		verifyMultiResolutionLinks(lower_rg, level + 1);
	}
#endif
}
/***************************************** BUCKET UTILS **********************************************/

static void addVertToBucket(EmbedBucket *b, float co[3])
{
	b->nv++;
	interp_v3_v3v3(b->p, b->p, co, 1.0f / b->nv);
}

#if 0 /* UNUSED 2.5 */
static void removeVertFromBucket(EmbedBucket *b, float co[3])
{
	mul_v3_fl(b->p, (float)b->nv);
	sub_v3_v3(b->p, co);
	b->nv--;
	mul_v3_fl(b->p, 1.0f / (float)b->nv);
}
#endif

static void mergeBuckets(EmbedBucket *bDst, EmbedBucket *bSrc)
{
	if (bDst->nv > 0 && bSrc->nv > 0)
	{
		bDst->nv += bSrc->nv;
		interp_v3_v3v3(bDst->p, bDst->p, bSrc->p, (float)bSrc->nv / (float)(bDst->nv));
	}
	else if (bSrc->nv > 0)
	{
		bDst->nv = bSrc->nv;
		copy_v3_v3(bDst->p, bSrc->p);
	}
}

static void mergeArcBuckets(ReebArc *aDst, ReebArc *aSrc, float start, float end)
{
	if (aDst->bcount > 0 && aSrc->bcount > 0)
	{
		int indexDst = 0, indexSrc = 0;
		
		start = MAX3(start, aDst->buckets[0].val, aSrc->buckets[0].val);
		
		while(indexDst < aDst->bcount && aDst->buckets[indexDst].val < start)
		{
			indexDst++;
		}

		while(indexSrc < aSrc->bcount && aSrc->buckets[indexSrc].val < start)
		{
			indexSrc++;
		}
		
		for( ;	indexDst < aDst->bcount &&
				indexSrc < aSrc->bcount &&
				aDst->buckets[indexDst].val <= end &&
				aSrc->buckets[indexSrc].val <= end
				
			 ;	indexDst++, indexSrc++)
		{
			mergeBuckets(aDst->buckets + indexDst, aSrc->buckets + indexSrc);
		}
	}
}

void flipArcBuckets(ReebArc *arc)
{
	int i, j;
	
	for (i = 0, j = arc->bcount - 1; i < j; i++, j--)
	{
		EmbedBucket tmp;
		
		tmp = arc->buckets[i];
		arc->buckets[i] = arc->buckets[j];
		arc->buckets[j] = tmp;
	}
}

static int countArcBuckets(ReebArc *arc)
{
	return (int)(floor(arc->tail->weight) - ceil(arc->head->weight)) + 1;
}

static void allocArcBuckets(ReebArc *arc)
{
	int i;
	float start = ceil(arc->head->weight);
	arc->bcount = countArcBuckets(arc);
	
	if (arc->bcount > 0)
	{
		arc->buckets = MEM_callocN(sizeof(EmbedBucket) * arc->bcount, "embed bucket");
		
		for(i = 0; i < arc->bcount; i++)
		{
			arc->buckets[i].val = start + i;
		}
	}
	else
	{
		arc->buckets = NULL;
	}
	
}

static void resizeArcBuckets(ReebArc *arc)
{
	EmbedBucket *oldBuckets = arc->buckets;
	int oldBCount = arc->bcount;
	
	if (countArcBuckets(arc) == oldBCount)
	{
		return;
	}
	
	allocArcBuckets(arc);
	
	if (oldBCount != 0 && arc->bcount != 0)
	{
		int oldStart = (int)oldBuckets[0].val;
		int oldEnd = (int)oldBuckets[oldBCount - 1].val;
		int newStart = (int)arc->buckets[0].val;
		int newEnd = (int)arc->buckets[arc->bcount - 1].val;
		int oldOffset = 0;
		int newOffset = 0;
		int len;
		
		if (oldStart < newStart)
		{
			oldOffset = newStart - oldStart;
		}
		else
		{
			newOffset = oldStart - newStart;
		}
		
		len = MIN2(oldEnd - (oldStart + oldOffset) + 1, newEnd - (newStart - newOffset) + 1);
		
		memcpy(arc->buckets + newOffset, oldBuckets + oldOffset, len * sizeof(EmbedBucket)); 
	}

	if (oldBuckets != NULL)
	{
		MEM_freeN(oldBuckets);
	}
}

static void reweightBuckets(ReebArc *arc)
{
	int i;
	float start = ceil((arc->head)->weight);
	
	if (arc->bcount > 0)
	{
		for(i = 0; i < arc->bcount; i++)
		{
			arc->buckets[i].val = start + i;
		}
	}
}

static void interpolateBuckets(ReebArc *arc, float *start_p, float *end_p, int start_index, int end_index)
{
	int total;
	int j;
	
	total = end_index - start_index + 2;
	
	for (j = start_index; j <= end_index; j++)
	{
		EmbedBucket *empty = arc->buckets + j;
		empty->nv = 1;
		interp_v3_v3v3(empty->p, start_p, end_p, (float)(j - start_index + 1) / total);
	}
}

static void fillArcEmptyBuckets(ReebArc *arc)
{
	float *start_p, *end_p;
	int start_index = 0, end_index = 0;
	int missing = 0;
	int i;
	
	start_p = arc->head->p;
	
	for(i = 0; i < arc->bcount; i++)
	{
		EmbedBucket *bucket = arc->buckets + i;
		
		if (missing)
		{
			if (bucket->nv > 0)
			{
				missing = 0;
				
				end_p = bucket->p;
				end_index = i - 1;
				
				interpolateBuckets(arc, start_p, end_p, start_index, end_index);
			}
		}
		else
		{
			if (bucket->nv == 0)
			{
				missing = 1;
				
				if (i > 0)
				{
					start_p = arc->buckets[i - 1].p;
				}
				start_index = i;
			}
		}
	}
	
	if (missing)
	{
		end_p = arc->tail->p;
		end_index = arc->bcount - 1;
		
		interpolateBuckets(arc, start_p, end_p, start_index, end_index);
	}
}

static void ExtendArcBuckets(ReebArc *arc)
{
	ReebArcIterator arc_iter;
	BArcIterator *iter = (BArcIterator*)&arc_iter;
	EmbedBucket *last_bucket, *first_bucket;
	float *previous = NULL;
	float average_length = 0, length;
	int padding_head = 0, padding_tail = 0;
	
	if (arc->bcount == 0)
	{
		return; /* failsafe, shouldn't happen */
	}
	
	initArcIterator(iter, arc, arc->head);
	IT_next(iter);
	previous = iter->p;
	
	for (	IT_next(iter);
			IT_stopped(iter) == 0;
			previous = iter->p, IT_next(iter)
		)
	{
		average_length += len_v3v3(previous, iter->p);
	}
	average_length /= (arc->bcount - 1);
	
	first_bucket = arc->buckets;
	last_bucket = arc->buckets + (arc->bcount - 1);
	
	length = len_v3v3(first_bucket->p, arc->head->p);
	if (length > 2 * average_length)
	{
		padding_head = (int)floor(length / average_length);
	}

	length = len_v3v3(last_bucket->p, arc->tail->p);
	if (length > 2 * average_length)
	{
		padding_tail = (int)floor(length / average_length);
	}
	
	if (padding_head + padding_tail > 0)
	{
		EmbedBucket *old_buckets = arc->buckets;
		
		arc->buckets = MEM_callocN(sizeof(EmbedBucket) * (padding_head + arc->bcount + padding_tail), "embed bucket");
		memcpy(arc->buckets + padding_head, old_buckets, arc->bcount * sizeof(EmbedBucket));
		
		arc->bcount = padding_head + arc->bcount + padding_tail;
		
		MEM_freeN(old_buckets);
	}
	
	if (padding_head > 0)
	{
		interpolateBuckets(arc, arc->head->p, first_bucket->p, 0, padding_head);
	}
	
	if (padding_tail > 0)
	{
		interpolateBuckets(arc, last_bucket->p, arc->tail->p, arc->bcount - padding_tail, arc->bcount - 1);
	}
}

/* CALL THIS ONLY AFTER FILTERING, SINCE IT MESSES UP WEIGHT DISTRIBUTION */
static void extendGraphBuckets(ReebGraph *rg)
{
	ReebArc *arc;
	
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		ExtendArcBuckets(arc);
	}
}

/**************************************** LENGTH CALCULATIONS ****************************************/

static void calculateArcLength(ReebArc *arc)
{
	ReebArcIterator arc_iter;
	BArcIterator *iter = (BArcIterator*)&arc_iter;
	float *vec0, *vec1;

	arc->length = 0;
	
	initArcIterator(iter, arc, arc->head);

	vec0 = arc->head->p;
	vec1 = arc->head->p; /* in case there's no embedding */

	while (IT_next(iter))	
	{
		vec1 = iter->p;
		
		arc->length += len_v3v3(vec0, vec1);
		
		vec0 = vec1;
	}
	
	arc->length += len_v3v3(arc->tail->p, vec1);	
}

static void calculateGraphLength(ReebGraph *rg)
{
	ReebArc *arc;
	
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		calculateArcLength(arc);
	}
}
#endif

/**************************************** SYMMETRY HANDLING ******************************************/

void REEB_RadialSymmetry(BNode* root_node, RadialArc* ring, int count)
{
	ReebNode *node = (ReebNode*)root_node;
	float axis[3];
	int i;
	
	copy_v3_v3(axis, root_node->symmetry_axis);
	
	/* first pass, merge incrementally */
	for (i = 0; i < count - 1; i++)
	{
		ReebNode *node1, *node2;
		ReebArc *arc1, *arc2;
		float tangent[3];
		float normal[3];
		int j = i + 1;

		add_v3_v3v3(tangent, ring[i].n, ring[j].n);
		cross_v3_v3v3(normal, tangent, axis);
		
		node1 = (ReebNode*)BLI_otherNode(ring[i].arc, root_node);
		node2 = (ReebNode*)BLI_otherNode(ring[j].arc, root_node);
		
		arc1 = (ReebArc*)ring[i].arc;
		arc2 = (ReebArc*)ring[j].arc;

		/* mirror first node and mix with the second */
		BLI_mirrorAlongAxis(node1->p, root_node->p, normal);
		interp_v3_v3v3(node2->p, node2->p, node1->p, 1.0f / (j + 1));
		
		/* Merge buckets
		 * there shouldn't be any null arcs here, but just to be safe 
		 * */
		if (arc1->bcount > 0 && arc2->bcount > 0)
		{
			ReebArcIterator arc_iter1, arc_iter2;
			BArcIterator *iter1 = (BArcIterator*)&arc_iter1;
			BArcIterator *iter2 = (BArcIterator*)&arc_iter2;
			EmbedBucket *bucket1 = NULL, *bucket2 = NULL;
			
			initArcIterator(iter1, arc1, (ReebNode*)root_node);
			initArcIterator(iter2, arc2, (ReebNode*)root_node);
			
			bucket1 = IT_next(iter1);
			bucket2 = IT_next(iter2);
		
			/* Make sure they both start at the same value */	
			while(bucket1 && bucket2 && bucket1->val < bucket2->val)
			{
				bucket1 = IT_next(iter1);
			}
			
			while(bucket1 && bucket2 && bucket2->val < bucket1->val)
			{
				bucket2 = IT_next(iter2);
			}
	
	
			for ( ;bucket1 && bucket2; bucket1 = IT_next(iter1), bucket2 = IT_next(iter2))
			{
				bucket2->nv += bucket1->nv; /* add counts */
				
				/* mirror on axis */
				BLI_mirrorAlongAxis(bucket1->p, root_node->p, normal);
				/* add bucket2 in bucket1 */
				interp_v3_v3v3(bucket2->p, bucket2->p, bucket1->p, (float)bucket1->nv / (float)(bucket2->nv));
			}
		}
	}
	
	/* second pass, mirror back on previous arcs */
	for (i = count - 1; i > 0; i--)
	{
		ReebNode *node1, *node2;
		ReebArc *arc1, *arc2;
		float tangent[3];
		float normal[3];
		int j = i - 1;

		add_v3_v3v3(tangent, ring[i].n, ring[j].n);
		cross_v3_v3v3(normal, tangent, axis);
		
		node1 = (ReebNode*)BLI_otherNode(ring[i].arc, root_node);
		node2 = (ReebNode*)BLI_otherNode(ring[j].arc, root_node);
		
		arc1 = (ReebArc*)ring[i].arc;
		arc2 = (ReebArc*)ring[j].arc;

		/* copy first node than mirror */
		copy_v3_v3(node2->p, node1->p);
		BLI_mirrorAlongAxis(node2->p, root_node->p, normal);
		
		/* Copy buckets
		 * there shouldn't be any null arcs here, but just to be safe 
		 * */
		if (arc1->bcount > 0 && arc2->bcount > 0)
		{
			ReebArcIterator arc_iter1, arc_iter2;
			BArcIterator *iter1 = (BArcIterator*)&arc_iter1;
			BArcIterator *iter2 = (BArcIterator*)&arc_iter2;
			EmbedBucket *bucket1 = NULL, *bucket2 = NULL;
			
			initArcIterator(iter1, arc1, node);
			initArcIterator(iter2, arc2, node);
			
			bucket1 = IT_next(iter1);
			bucket2 = IT_next(iter2);
		
			/* Make sure they both start at the same value */	
			while(bucket1 && bucket1->val < bucket2->val)
			{
				bucket1 = IT_next(iter1);
			}
			
			while(bucket2 && bucket2->val < bucket1->val)
			{
				bucket2 = IT_next(iter2);
			}
	
	
			for ( ;bucket1 && bucket2; bucket1 = IT_next(iter1), bucket2 = IT_next(iter2))
			{
				/* copy and mirror back to bucket2 */			
				bucket2->nv = bucket1->nv;
				copy_v3_v3(bucket2->p, bucket1->p);
				BLI_mirrorAlongAxis(bucket2->p, node->p, normal);
			}
		}
	}
}

void REEB_AxialSymmetry(BNode* root_node, BNode* node1, BNode* node2, struct BArc* barc1, BArc* barc2)
{
	ReebArc *arc1, *arc2;
	float nor[3], p[3];

	arc1 = (ReebArc*)barc1;
	arc2 = (ReebArc*)barc2;

	copy_v3_v3(nor, root_node->symmetry_axis);
	
	/* mirror node2 along axis */
	copy_v3_v3(p, node2->p);
	BLI_mirrorAlongAxis(p, root_node->p, nor);

	/* average with node1 */
	add_v3_v3(node1->p, p);
	mul_v3_fl(node1->p, 0.5f);
	
	/* mirror back on node2 */
	copy_v3_v3(node2->p, node1->p);
	BLI_mirrorAlongAxis(node2->p, root_node->p, nor);
	
	/* Merge buckets
	 * there shouldn't be any null arcs here, but just to be safe 
	 * */
	if (arc1->bcount > 0 && arc2->bcount > 0)
	{
		ReebArcIterator arc_iter1, arc_iter2;
		BArcIterator *iter1 = (BArcIterator*)&arc_iter1;
		BArcIterator *iter2 = (BArcIterator*)&arc_iter2;
		EmbedBucket *bucket1 = NULL, *bucket2 = NULL;
		
		initArcIterator(iter1, arc1, (ReebNode*)root_node);
		initArcIterator(iter2, arc2, (ReebNode*)root_node);
		
		bucket1 = IT_next(iter1);
		bucket2 = IT_next(iter2);
	
		/* Make sure they both start at the same value */	
		while(bucket1 && bucket1->val < bucket2->val)
		{
			bucket1 = IT_next(iter1);
		}
		
		while(bucket2 && bucket2->val < bucket1->val)
		{
			bucket2 = IT_next(iter2);
		}


		for ( ;bucket1 && bucket2; bucket1 = IT_next(iter1), bucket2 = IT_next(iter2))
		{
			bucket1->nv += bucket2->nv; /* add counts */
			
			/* mirror on axis */
			BLI_mirrorAlongAxis(bucket2->p, root_node->p, nor);
			/* add bucket2 in bucket1 */
			interp_v3_v3v3(bucket1->p, bucket1->p, bucket2->p, (float)bucket2->nv / (float)(bucket1->nv));

			/* copy and mirror back to bucket2 */			
			bucket2->nv = bucket1->nv;
			copy_v3_v3(bucket2->p, bucket1->p);
			BLI_mirrorAlongAxis(bucket2->p, root_node->p, nor);
		}
	}
}

/************************************** ADJACENCY LIST *************************************************/


/****************************************** SMOOTHING **************************************************/

#if 0 /* UNUSED */
void postprocessGraph(ReebGraph *rg, char mode)
{
	ReebArc *arc;
	float fac1 = 0, fac2 = 1, fac3 = 0;

	switch(mode)
	{
	case SKGEN_AVERAGE:
		fac1 = fac2 = fac3 = 1.0f / 3.0f;
		break;
	case SKGEN_SMOOTH:
		fac1 = fac3 = 0.25f;
		fac2 = 0.5f;
		break;
	case SKGEN_SHARPEN:
		fac1 = fac3 = -0.25f;
		fac2 = 1.5f;
		break;
	default:
//		XXX
//		error("Unknown post processing mode");
		return;
	}
	
	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
		EmbedBucket *buckets = arc->buckets;
		int bcount = arc->bcount;
		int index;

		for(index = 1; index < bcount - 1; index++)
		{
			interp_v3_v3v3(buckets[index].p, buckets[index].p, buckets[index - 1].p, fac1 / (fac1 + fac2));
			interp_v3_v3v3(buckets[index].p, buckets[index].p, buckets[index + 1].p, fac3 / (fac1 + fac2 + fac3));
		}
	}
}

/********************************************SORTING****************************************************/

static int compareNodesWeight(void *vnode1, void *vnode2)
{
	ReebNode *node1 = (ReebNode*)vnode1;
	ReebNode *node2 = (ReebNode*)vnode2;
	
	if (node1->weight < node2->weight)
	{
		return -1;
	}
	if (node1->weight > node2->weight)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void sortNodes(ReebGraph *rg)
{
	BLI_sortlist(&rg->nodes, compareNodesWeight);
}

static int compareArcsWeight(void *varc1, void *varc2)
{
	ReebArc *arc1 = (ReebArc*)varc1;
	ReebArc *arc2 = (ReebArc*)varc2;
	ReebNode *node1 = (ReebNode*)arc1->head; 
	ReebNode *node2 = (ReebNode*)arc2->head; 
	
	if (node1->weight < node2->weight)
	{
		return -1;
	}
	if (node1->weight > node2->weight)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void sortArcs(ReebGraph *rg)
{
	BLI_sortlist(&rg->arcs, compareArcsWeight);
}
/******************************************* JOINING ***************************************************/

static void reweightArc(ReebGraph *rg, ReebArc *arc, ReebNode *start_node, float start_weight)
{
	ReebNode *node;
	float old_weight;
	float end_weight = start_weight + ABS(arc->tail->weight - arc->head->weight);
	int i;
	
	node = (ReebNode*)BLI_otherNode((BArc*)arc, (BNode*)start_node);
	
	/* prevent backtracking */
	if (node->flag == 1)
	{
		return;
	}

	if (arc->tail == start_node)
	{
		flipArc(arc);
	}
	
	start_node->flag = 1;
	
	for (i = 0; i < node->degree; i++)
	{
		ReebArc *next_arc = node->arcs[i];
		
		reweightArc(rg, next_arc, node, end_weight);
	}

	/* update only if needed */	
	if (arc->head->weight != start_weight || arc->tail->weight != end_weight)
	{
		old_weight = arc->head->weight; /* backup head weight, other arcs need it intact, it will be fixed by the source arc */
		
		arc->head->weight = start_weight;
		arc->tail->weight = end_weight;
		
		reweightBuckets(arc);
		resizeArcBuckets(arc);
		fillArcEmptyBuckets(arc);
		
		arc->head->weight = old_weight;
	}
} 

static void reweightSubgraph(ReebGraph *rg, ReebNode *start_node, float start_weight)
{
	int i;
		
	BLI_flagNodes((BGraph*)rg, 0);

	for (i = 0; i < start_node->degree; i++)
	{
		ReebArc *next_arc = start_node->arcs[i];
		
		reweightArc(rg, next_arc, start_node, start_weight);
	}
	start_node->weight = start_weight;
}

static int joinSubgraphsEnds(ReebGraph *rg, float threshold, int nb_subgraphs)
{
	int joined = 0;
	int subgraph;
	
	for (subgraph = 1; subgraph <= nb_subgraphs; subgraph++)
	{
		ReebNode *start_node, *end_node;
		ReebNode *min_node_start = NULL, *min_node_end = NULL;
		float min_distance = FLT_MAX;
		
		for (start_node = rg->nodes.first; start_node; start_node = start_node->next)
		{
			if (start_node->subgraph_index == subgraph && start_node->degree == 1)
			{
				
				for (end_node = rg->nodes.first; end_node; end_node = end_node->next)
				{
					if (end_node->subgraph_index != subgraph)
					{
						float distance = len_v3v3(start_node->p, end_node->p);
						
						if (distance < threshold && distance < min_distance)
						{
							min_distance = distance;
							min_node_end = end_node;
							min_node_start = start_node;
						}
					}
				}
			}
		}
		
		end_node = min_node_end;
		start_node = min_node_start;
		
		if (end_node && start_node)
		{
			ReebArc *start_arc /* , *end_arc */ /* UNUSED */;
			int merging = 0;
			
			start_arc = start_node->arcs[0];
			/* end_arc = end_node->arcs[0]; */ /* UNUSED */
			
			if (start_arc->tail == start_node)
			{
				reweightSubgraph(rg, end_node, start_node->weight);
				
				start_arc->tail = end_node;
				
				merging = 1;
			}
			else if (start_arc->head == start_node)
			{
				reweightSubgraph(rg, start_node, end_node->weight);

				start_arc->head = end_node;

				merging = 2;
			}
			
			if (merging)
			{
				BLI_ReflagSubgraph((BGraph*)rg, end_node->flag, subgraph);
									
				resizeArcBuckets(start_arc);
				fillArcEmptyBuckets(start_arc);
				
				NodeDegreeIncrement(rg, end_node);
				BLI_rebuildAdjacencyListForNode((BGraph*)rg, (BNode*)end_node);
				
				BLI_removeNode((BGraph*)rg, (BNode*)start_node);
			}
			
			joined = 1;
		}		
	}
	
	return joined;
}

/* Reweight graph from smallest node, fix fliped arcs */
static void fixSubgraphsOrientation(ReebGraph *rg, int nb_subgraphs)
{
	int subgraph;
	
	for (subgraph = 1; subgraph <= nb_subgraphs; subgraph++)
	{
		ReebNode *node;
		ReebNode *start_node = NULL;
		
		for (node = rg->nodes.first; node; node = node->next)
		{
			if (node->subgraph_index == subgraph)
			{
				if (start_node == NULL || node->weight < start_node->weight)
				{
					start_node = node;
				}
			}
		}
		
		if (start_node)
		{
			reweightSubgraph(rg, start_node, start_node->weight);
		}
	}
}

static int joinSubgraphs(ReebGraph *rg, float threshold)
{
	int nb_subgraphs;
	int joined = 0;
	
	BLI_buildAdjacencyList((BGraph*)rg);
	
	if (BLI_isGraphCyclic((BGraph*)rg))
	{
		/* don't deal with cyclic graphs YET */
		return 0;
	}
	
	/* sort nodes before flagging subgraphs to make sure root node is subgraph 0 */
	sortNodes(rg);
	
	nb_subgraphs = BLI_FlagSubgraphs((BGraph*)rg);
	
	/* Harmonic function can create flipped arcs, take the occasion to fix them */
//	XXX
//	if (G.scene->toolsettings->skgen_options & SKGEN_HARMONIC)
//	{
		fixSubgraphsOrientation(rg, nb_subgraphs);
//	}

	if (nb_subgraphs > 1)
	{
		joined |= joinSubgraphsEnds(rg, threshold, nb_subgraphs);
		
		if (joined)
		{
			removeNormalNodes(rg);
			BLI_buildAdjacencyList((BGraph*)rg);
		}
	}
	
	return joined;
}

/****************************************** FILTERING **************************************************/

static float lengthArc(ReebArc *arc)
{
#if 0
	ReebNode *head = (ReebNode*)arc->head;
	ReebNode *tail = (ReebNode*)arc->tail;
	
	return tail->weight - head->weight;
#else
	return arc->length;
#endif
}

static int compareArcs(void *varc1, void *varc2)
{
	ReebArc *arc1 = (ReebArc*)varc1;
	ReebArc *arc2 = (ReebArc*)varc2;
	float len1 = lengthArc(arc1);
	float len2 = lengthArc(arc2);
	
	if (len1 < len2)
	{
		return -1;
	}
	if (len1 > len2)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static void filterArc(ReebGraph *rg, ReebNode *newNode, ReebNode *removedNode, ReebArc * srcArc, int merging)
{
	ReebArc *arc = NULL, *nextArc = NULL;

	if (merging)
	{
		/* first pass, merge buckets for arcs that spawned the two nodes into the source arc*/
		for(arc = rg->arcs.first; arc; arc = arc->next)
		{
			if (arc->head == srcArc->head && arc->tail == srcArc->tail && arc != srcArc)
			{
				ReebNode *head = srcArc->head;
				ReebNode *tail = srcArc->tail;
				mergeArcBuckets(srcArc, arc, head->weight, tail->weight);
			}
		}
	}

	/* second pass, replace removedNode by newNode, remove arcs that are collapsed in a loop */
	arc = rg->arcs.first;
	while(arc)
	{
		nextArc = arc->next;
		
		if (arc->head == removedNode || arc->tail == removedNode)
		{
			if (arc->head == removedNode)
			{
				arc->head = newNode;
			}
			else
			{
				arc->tail = newNode;
			}

			// Remove looped arcs			
			if (arc->head == arc->tail)
			{
				// v1 or v2 was already newNode, since we're removing an arc, decrement degree
				NodeDegreeDecrement(rg, newNode);
				
				// If it's srcArc, it'll be removed later, so keep it for now
				if (arc != srcArc)
				{
					BLI_remlink(&rg->arcs, arc);
					REEB_freeArc((BArc*)arc);
				}
			}
			else
			{
				/* flip arcs that flipped, can happen on diamond shapes, mostly on null arcs */
				if (arc->head->weight > arc->tail->weight)
				{
					flipArc(arc);
				}
				//newNode->degree++; // incrementing degree since we're adding an arc
				NodeDegreeIncrement(rg, newNode);
				mergeArcFaces(rg, arc, srcArc);

				if (merging)
				{
					ReebNode *head = arc->head;
					ReebNode *tail = arc->tail;

					// resize bucket list
					resizeArcBuckets(arc);
					mergeArcBuckets(arc, srcArc, head->weight, tail->weight);
					
					/* update length */
					arc->length += srcArc->length;
				}
			}
		}
		
		arc = nextArc;
	}
}

void filterNullReebGraph(ReebGraph *rg)
{
	ReebArc *arc = NULL, *nextArc = NULL;
	
	arc = rg->arcs.first;
	while(arc)
	{
		nextArc = arc->next;
		// Only collapse arcs too short to have any embed bucket
		if (arc->bcount == 0)
		{
			ReebNode *newNode = (ReebNode*)arc->head;
			ReebNode *removedNode = (ReebNode*)arc->tail;
			float blend;
			
			blend = (float)newNode->degree / (float)(newNode->degree + removedNode->degree); // blending factors
			
			interp_v3_v3v3(newNode->p, removedNode->p, newNode->p, blend);
			
			filterArc(rg, newNode, removedNode, arc, 0);

			// Reset nextArc, it might have changed
			nextArc = arc->next;
			
			BLI_remlink(&rg->arcs, arc);
			REEB_freeArc((BArc*)arc);
			
			BLI_removeNode((BGraph*)rg, (BNode*)removedNode);
		}
		
		arc = nextArc;
	}
}

static int filterInternalExternalReebGraph(ReebGraph *rg, float threshold_internal, float threshold_external)
{
	ReebArc *arc = NULL, *nextArc = NULL;
	int value = 0;
	
	BLI_sortlist(&rg->arcs, compareArcs);
	
	for (arc = rg->arcs.first; arc; arc = nextArc)
	{
		nextArc = arc->next;

		// Only collapse non-terminal arcs that are shorter than threshold
		if (threshold_internal > 0 && arc->head->degree > 1 && arc->tail->degree > 1 && (lengthArc(arc) < threshold_internal))
		{
			ReebNode *newNode = NULL;
			ReebNode *removedNode = NULL;
			
			/* Always remove lower node, so arcs don't flip */
			newNode = arc->head;
			removedNode = arc->tail;

			filterArc(rg, newNode, removedNode, arc, 1);

			// Reset nextArc, it might have changed
			nextArc = arc->next;
			
			BLI_remlink(&rg->arcs, arc);
			REEB_freeArc((BArc*)arc);
			
			BLI_removeNode((BGraph*)rg, (BNode*)removedNode);
			value = 1;
		}
		
		// Only collapse terminal arcs that are shorter than threshold
		else if (threshold_external > 0 && (arc->head->degree == 1 || arc->tail->degree == 1) && (lengthArc(arc) < threshold_external))
		{
			ReebNode *terminalNode = NULL;
			ReebNode *middleNode = NULL;
			ReebNode *removedNode = NULL;
			
			// Assign terminal and middle nodes
			if (arc->head->degree == 1)
			{
				terminalNode = arc->head;
				middleNode = arc->tail;
			}
			else
			{
				terminalNode = arc->tail;
				middleNode = arc->head;
			}
			
			if (middleNode->degree == 2 && middleNode != rg->nodes.first)
			{
#if 1
				// If middle node is a normal node, it will be removed later
				// Only if middle node is not the root node
				/* USE THIS IF YOU WANT TO PROLONG ARCS TO THEIR TERMINAL NODES
				 * FOR HANDS, THIS IS NOT THE BEST RESULT 
				 * */
				continue;
#else
				removedNode = terminalNode;

				// removing arc, so we need to decrease the degree of the remaining node
				NodeDegreeDecrement(rg, middleNode);
#endif
			}
			// Otherwise, just plain remove of the arc
			else
			{
				removedNode = terminalNode;

				// removing arc, so we need to decrease the degree of the remaining node
				NodeDegreeDecrement(rg, middleNode);
			}

			// Reset nextArc, it might have changed
			nextArc = arc->next;
			
			BLI_remlink(&rg->arcs, arc);
			REEB_freeArc((BArc*)arc);
			
			BLI_removeNode((BGraph*)rg, (BNode*)removedNode);
			value = 1;
		}
	}
	
	return value;
}

static int filterCyclesReebGraph(ReebGraph *rg, float UNUSED(distance_threshold))
{
	ReebArc *arc1, *arc2;
	ReebArc *next2;
	int filtered = 0;
	
	for (arc1 = rg->arcs.first; arc1; arc1 = arc1->next)
	{
		for (arc2 = arc1->next; arc2; arc2 = next2)
		{
			next2 = arc2->next;
			if (arc1 != arc2 && arc1->head == arc2->head && arc1->tail == arc2->tail)
			{
				mergeArcEdges(rg, arc1, arc2, MERGE_APPEND);
				mergeArcFaces(rg, arc1, arc2);
				mergeArcBuckets(arc1, arc2, arc1->head->weight, arc1->tail->weight);

				NodeDegreeDecrement(rg, arc1->head);
				NodeDegreeDecrement(rg, arc1->tail);

				BLI_remlink(&rg->arcs, arc2);
				REEB_freeArc((BArc*)arc2);
				
				filtered = 1;
			}
		}
	}
	
	return filtered;
}

int filterSmartReebGraph(ReebGraph *UNUSED(rg), float UNUSED(threshold))
{
	int value = 0;
#if 0 //XXX
	ReebArc *arc = NULL, *nextArc = NULL;
	
	BLI_sortlist(&rg->arcs, compareArcs);

#ifdef DEBUG_REEB
	{	
		EditFace *efa;
		for(efa=G.editMesh->faces.first; efa; efa=efa->next) {
			efa->tmp.fp = -1;
		}
	}
#endif

	arc = rg->arcs.first;
	while(arc)
	{
		nextArc = arc->next;
		
		/* need correct normals and center */
		recalc_editnormals();

		// Only test terminal arcs
		if (arc->head->degree == 1 || arc->tail->degree == 1)
		{
			GHashIterator ghi;
			int merging = 0;
			int total = BLI_ghash_size(arc->faces);
			float avg_angle = 0;
			float avg_vec[3] = {0,0,0};
			
			for(BLI_ghashIterator_init(&ghi, arc->faces);
				!BLI_ghashIterator_isDone(&ghi);
				BLI_ghashIterator_step(&ghi))
			{
				EditFace *efa = BLI_ghashIterator_getValue(&ghi);

#if 0
				ReebArcIterator arc_iter;
				BArcIterator *iter = (BArcIterator*)&arc_iter;
				EmbedBucket *bucket = NULL;
				EmbedBucket *previous = NULL;
				float min_distance = -1;
				float angle = 0;
		
				initArcIterator(iter, arc, arc->head);
		
				bucket = nextBucket(iter);
				
				while (bucket != NULL)
				{
					float *vec0 = NULL;
					float *vec1 = bucket->p;
					float midpoint[3], tangent[3];
					float distance;
		
					/* first bucket. Previous is head */
					if (previous == NULL)
					{
						vec0 = arc->head->p;
					}
					/* Previous is a valid bucket */
					else
					{
						vec0 = previous->p;
					}
					
					copy_v3_v3(midpoint, vec1);
					
					distance = len_v3v3(midpoint, efa->cent);
					
					if (min_distance == -1 || distance < min_distance)
					{
						min_distance = distance;
					
						sub_v3_v3v3(tangent, vec1, vec0);
						normalize_v3(tangent);
						
						angle = dot_v3v3(tangent, efa->n);
					}
					
					previous = bucket;
					bucket = nextBucket(iter);
				}
				
				avg_angle += saacos(fabs(angle));
#ifdef DEBUG_REEB
				efa->tmp.fp = saacos(fabs(angle));
#endif
#else
				add_v3_v3(avg_vec, efa->n);		
#endif
			}


#if 0			
			avg_angle /= total;
#else
			mul_v3_fl(avg_vec, 1.0 / total);
			avg_angle = dot_v3v3(avg_vec, avg_vec);
#endif
			
			arc->angle = avg_angle;
			
			if (avg_angle > threshold)
				merging = 1;
			
			if (merging)
			{
				ReebNode *terminalNode = NULL;
				ReebNode *middleNode = NULL;
				ReebNode *newNode = NULL;
				ReebNode *removedNode = NULL;
				int merging = 0;
				
				// Assign terminal and middle nodes
				if (arc->head->degree == 1)
				{
					terminalNode = arc->head;
					middleNode = arc->tail;
				}
				else
				{
					terminalNode = arc->tail;
					middleNode = arc->head;
				}
				
				// If middle node is a normal node, merge to terminal node
				if (middleNode->degree == 2)
				{
					merging = 1;
					newNode = terminalNode;
					removedNode = middleNode;
				}
				// Otherwise, just plain remove of the arc
				else
				{
					merging = 0;
					newNode = middleNode;
					removedNode = terminalNode;
				}
				
				// Merging arc
				if (merging)
				{
					filterArc(rg, newNode, removedNode, arc, 1);
				}
				else
				{
					// removing arc, so we need to decrease the degree of the remaining node
					//newNode->degree--;
					NodeDegreeDecrement(rg, newNode);
				}
	
				// Reset nextArc, it might have changed
				nextArc = arc->next;
				
				BLI_remlink(&rg->arcs, arc);
				REEB_freeArc((BArc*)arc);
				
				BLI_freelinkN(&rg->nodes, removedNode);
				value = 1;
			}
		}
		
		arc = nextArc;
	}
	
	#endif
	
	return value;
}

static void filterGraph(ReebGraph *rg, short options, float threshold_internal, float threshold_external)
{
	int done = 1;
	
	calculateGraphLength(rg);

	if ((options & SKGEN_FILTER_EXTERNAL) == 0)
	{
		threshold_external = 0;
	}

	if ((options & SKGEN_FILTER_INTERNAL) == 0)
	{
		threshold_internal = 0;
	}

	if (threshold_internal > 0 || threshold_external > 0)
	{ 
		/* filter until there's nothing more to do */
		while (done == 1)
		{
			done = 0; /* no work done yet */
			
			done = filterInternalExternalReebGraph(rg, threshold_internal, threshold_external);
		}
	}

	if (options & SKGEN_FILTER_SMART)
	{
		filterSmartReebGraph(rg, 0.5);
		filterCyclesReebGraph(rg, 0.5);
	}

	repositionNodes(rg);

	/* Filtering might have created degree 2 nodes, so remove them */
	removeNormalNodes(rg);
}

static void finalizeGraph(ReebGraph *rg, char passes, char method)
{
	int i;
	
	BLI_buildAdjacencyList((BGraph*)rg);

	sortNodes(rg);
	
	sortArcs(rg);
	
	for(i = 0; i <  passes; i++)
	{
		postprocessGraph(rg, method);
	}
	
	extendGraphBuckets(rg);
}

/************************************** WEIGHT SPREADING ***********************************************/

static int compareVerts( const void* a, const void* b )
{
	EditVert *va = *(EditVert**)a;
	EditVert *vb = *(EditVert**)b;
	int value = 0;
	
	if (weightData(va) < weightData(vb))
	{
		value = -1;
	}
	else if (weightData(va) > weightData(vb))
	{
		value = 1;
	}

	return value;		
}

static void spreadWeight(EditMesh *em)
{
	EditVert **verts, *eve;
	float lastWeight = 0.0f;
	int totvert = BLI_countlist(&em->verts);
	int i;
	int work_needed = 1;
	
	verts = MEM_callocN(sizeof(EditVert*) * totvert, "verts array");
	
	for(eve = em->verts.first, i = 0; eve; eve = eve->next, i++)
	{
		verts[i] = eve;
	}
	
	while(work_needed == 1)
	{
		work_needed = 0;
		qsort(verts, totvert, sizeof(EditVert*), compareVerts);
		
		for(i = 0; i < totvert; i++)
		{
			eve = verts[i];
			
			if (i == 0 || (weightData(eve) - lastWeight) > FLT_EPSILON)
			{
				lastWeight = weightData(eve);
			}
			else
			{
				work_needed = 1;
				weightSetData(eve, lastWeight + FLT_EPSILON * 2);
				lastWeight = weightData(eve);
			}
		}
	}
	
	MEM_freeN(verts);
}

/******************************************** EXPORT ***************************************************/

static void exportNode(FILE *f, const char *text, ReebNode *node)
{
	fprintf(f, "%s i:%i w:%f d:%i %f %f %f\n", text, node->index, node->weight, node->degree, node->p[0], node->p[1], node->p[2]);
}

void REEB_exportGraph(ReebGraph *rg, int count)
{
	ReebArc *arc;
	char filename[128];
	FILE *f;
	
	if (count == -1)
	{
		strcpy(filename, "test.txt");
	}
	else {
		sprintf(filename, "test%05i.txt", count);
	}
	f = BLI_fopen(filename, "w");

	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
		int i;
		float p[3];
		
		exportNode(f, "v1", arc->head);
		
		for(i = 0; i < arc->bcount; i++)
		{
			fprintf(f, "b nv:%i %f %f %f\n", arc->buckets[i].nv, arc->buckets[i].p[0], arc->buckets[i].p[1], arc->buckets[i].p[2]);
		}
		
		add_v3_v3v3(p, arc->tail->p, arc->head->p);
		mul_v3_fl(p, 0.5f);
		
		fprintf(f, "angle %0.3f %0.3f %0.3f %0.3f %i\n", p[0], p[1], p[2], arc->angle, BLI_ghash_size(arc->faces));
		exportNode(f, "v2", arc->tail);
	}	
	
	fclose(f);
}

/***************************************** MAIN ALGORITHM **********************************************/

/* edges alone will create zero degree nodes, use this function to remove them */
static void removeZeroNodes(ReebGraph *rg)
{
	ReebNode *node, *next_node;
	
	for (node = rg->nodes.first; node; node = next_node)
	{
		next_node = node->next;
		
		if (node->degree == 0)
		{
			BLI_removeNode((BGraph*)rg, (BNode*)node);
		}
	}
}

void removeNormalNodes(ReebGraph *rg)
{
	ReebArc *arc, *nextArc;
	
	// Merge degree 2 nodes
	for(arc = rg->arcs.first; arc; arc = nextArc)
	{
		nextArc = arc->next;
		
		while (arc->head->degree == 2 || arc->tail->degree == 2)
		{
			// merge at v1
			if (arc->head->degree == 2)
			{
				ReebArc *connectedArc = (ReebArc*)BLI_findConnectedArc((BGraph*)rg, (BArc*)arc, (BNode*)arc->head);

				/* If arcs are one after the other */
				if (arc->head == connectedArc->tail)
				{		
					/* remove furthest arc */		
					if (arc->tail->weight < connectedArc->head->weight)
					{
						mergeConnectedArcs(rg, arc, connectedArc);
						nextArc = arc->next;
					}
					else
					{
						mergeConnectedArcs(rg, connectedArc, arc);
						break; /* arc was removed, move to next */
					}
				}
				/* Otherwise, arcs are side by side */
				else
				{
					/* Don't do anything, we need to keep the lowest node, even if degree 2 */
					break;
				}
			}
			
			// merge at v2
			if (arc->tail->degree == 2)
			{
				ReebArc *connectedArc = (ReebArc*)BLI_findConnectedArc((BGraph*)rg, (BArc*)arc, (BNode*)arc->tail);
				
				/* If arcs are one after the other */
				if (arc->tail == connectedArc->head)
				{				
					/* remove furthest arc */		
					if (arc->head->weight < connectedArc->tail->weight)
					{
						mergeConnectedArcs(rg, arc, connectedArc);
						nextArc = arc->next;
					}
					else
					{
						mergeConnectedArcs(rg, connectedArc, arc);
						break; /* arc was removed, move to next */
					}
				}
				/* Otherwise, arcs are side by side */
				else
				{
					/* Don't do anything, we need to keep the lowest node, even if degree 2 */
					break;
				}
			}
		}
	}
	
}

static int edgeEquals(ReebEdge *e1, ReebEdge *e2)
{
	return (e1->v1 == e2->v1 && e1->v2 == e2->v2);
}

static ReebArc *nextArcMappedToEdge(ReebArc *arc, ReebEdge *e)
{
	ReebEdge *nextEdge = NULL;
	ReebEdge *edge = NULL;
	ReebArc *result = NULL;

	/* Find the ReebEdge in the edge list */
	for(edge = arc->edges.first; edge && !edgeEquals(edge, e); edge = edge->next)
	{	}
	
	nextEdge = edge->nextEdge;
	
	if (nextEdge != NULL)
	{
		result = nextEdge->arc;
	}

	return result;
}

void addFacetoArc(ReebArc *arc, EditFace *efa)
{
	BLI_ghash_insert(arc->faces, efa, efa);
}

void mergeArcFaces(ReebGraph *UNUSED(rg), ReebArc *aDst, ReebArc *aSrc)
{
	GHashIterator ghi;
	
	for(BLI_ghashIterator_init(&ghi, aSrc->faces);
		!BLI_ghashIterator_isDone(&ghi);
		BLI_ghashIterator_step(&ghi))
	{
		EditFace *efa = BLI_ghashIterator_getValue(&ghi);
		BLI_ghash_insert(aDst->faces, efa, efa);
	}
} 

void mergeArcEdges(ReebGraph *rg, ReebArc *aDst, ReebArc *aSrc, MergeDirection direction)
{
	ReebEdge *e = NULL;
	
	if (direction == MERGE_APPEND)
	{
		for(e = aSrc->edges.first; e; e = e->next)
		{
			e->arc = aDst; // Edge is stolen by new arc
		}
		
		BLI_movelisttolist(&aDst->edges , &aSrc->edges);
	}
	else
	{
		for(e = aSrc->edges.first; e; e = e->next)
		{
			ReebEdge *newEdge = copyEdge(e);

			newEdge->arc = aDst;
			
			BLI_addtail(&aDst->edges, newEdge);
			
			if (direction == MERGE_LOWER)
			{
				void **p = BLI_edgehash_lookup_p(rg->emap, e->v1->index, e->v2->index);
				
				newEdge->nextEdge = e;

				// if edge was the first in the list, point the edit edge to the new reeb edge instead.							
				if (*p == e)
				{
					*p = (void*)newEdge;
				}
				// otherwise, advance in the list until the predecessor is found then insert it there
				else
				{
					ReebEdge *previous = (ReebEdge*)*p;
					
					while(previous->nextEdge != e)
					{
						previous = previous->nextEdge;
					}
					
					previous->nextEdge = newEdge;
				}
			}
			else
			{
				newEdge->nextEdge = e->nextEdge;
				e->nextEdge = newEdge;
			}
		}
	}
} 

// return 1 on full merge
int mergeConnectedArcs(ReebGraph *rg, ReebArc *a0, ReebArc *a1)
{
	int result = 0;
	ReebNode *removedNode = NULL;
	
	a0->length += a1->length;
	
	mergeArcEdges(rg, a0, a1, MERGE_APPEND);
	mergeArcFaces(rg, a0, a1);
	
	// Bring a0 to the combine length of both arcs
	if (a0->tail == a1->head)
	{
		removedNode = a0->tail;
		a0->tail = a1->tail;
	}
	else if (a0->head == a1->tail)
	{
		removedNode = a0->head;
		a0->head = a1->head;
	}
	
	resizeArcBuckets(a0);
	// Merge a1 in a0
	mergeArcBuckets(a0, a1, a0->head->weight, a0->tail->weight);
	
	// remove a1 from graph
	BLI_remlink(&rg->arcs, a1);
	REEB_freeArc((BArc*)a1);
	
	BLI_removeNode((BGraph*)rg, (BNode*)removedNode);
	result = 1;
	
	return result;
}
// return 1 on full merge
int mergeArcs(ReebGraph *rg, ReebArc *a0, ReebArc *a1)
{
	int result = 0;
	// TRIANGLE POINTS DOWN
	if (a0->head->weight == a1->head->weight) // heads are the same
	{
		if (a0->tail->weight == a1->tail->weight) // tails also the same, arcs can be totally merge together
		{
			mergeArcEdges(rg, a0, a1, MERGE_APPEND);
			mergeArcFaces(rg, a0, a1);
			
			mergeArcBuckets(a0, a1, a0->head->weight, a0->tail->weight);
			
			// Adjust node degree
			//a1->head->degree--;
			NodeDegreeDecrement(rg, a1->head);
			//a1->tail->degree--;
			NodeDegreeDecrement(rg, a1->tail);
			
			// remove a1 from graph
			BLI_remlink(&rg->arcs, a1);
			
			REEB_freeArc((BArc*)a1);
			result = 1;
		}
		else if (a0->tail->weight > a1->tail->weight) // a1->tail->weight is in the middle
		{
			mergeArcEdges(rg, a1, a0, MERGE_LOWER);
			mergeArcFaces(rg, a1, a0);

			// Adjust node degree
			//a0->head->degree--;
			NodeDegreeDecrement(rg, a0->head);
			//a1->tail->degree++;
			NodeDegreeIncrement(rg, a1->tail);
			
			mergeArcBuckets(a1, a0, a1->head->weight, a1->tail->weight);
			a0->head = a1->tail;
			resizeArcBuckets(a0);
		}
		else // a0>n2 is in the middle
		{
			mergeArcEdges(rg, a0, a1, MERGE_LOWER);
			mergeArcFaces(rg, a0, a1);
			
			// Adjust node degree
			//a1->head->degree--;
			NodeDegreeDecrement(rg, a1->head);
			//a0->tail->degree++;
			NodeDegreeIncrement(rg, a0->tail);
			
			mergeArcBuckets(a0, a1, a0->head->weight, a0->tail->weight);
			a1->head = a0->tail;
			resizeArcBuckets(a1);
		}
	}
	// TRIANGLE POINTS UP
	else if (a0->tail->weight == a1->tail->weight) // tails are the same
	{
		if (a0->head->weight > a1->head->weight) // a0->head->weight is in the middle
		{
			mergeArcEdges(rg, a0, a1, MERGE_HIGHER);
			mergeArcFaces(rg, a0, a1);
			
			// Adjust node degree
			//a1->tail->degree--;
			NodeDegreeDecrement(rg, a1->tail);
			//a0->head->degree++;
			NodeDegreeIncrement(rg, a0->head);
			
			mergeArcBuckets(a0, a1, a0->head->weight, a0->tail->weight);
			a1->tail = a0->head;
			resizeArcBuckets(a1);
		}
		else // a1->head->weight is in the middle
		{
			mergeArcEdges(rg, a1, a0, MERGE_HIGHER);
			mergeArcFaces(rg, a1, a0);

			// Adjust node degree
			//a0->tail->degree--;
			NodeDegreeDecrement(rg, a0->tail);
			//a1->head->degree++;
			NodeDegreeIncrement(rg, a1->head);

			mergeArcBuckets(a1, a0, a1->head->weight, a1->tail->weight);
			a0->tail = a1->head;
			resizeArcBuckets(a0);
		}
	}
	else
	{
		// Need something here (OR NOT)
	}
	
	return result;
}

static void glueByMergeSort(ReebGraph *rg, ReebArc *a0, ReebArc *a1, ReebEdge *e0, ReebEdge *e1)
{
	int total = 0;
	while (total == 0 && a0 != a1 && a0 != NULL && a1 != NULL)
	{
		total = mergeArcs(rg, a0, a1);
		
		if (total == 0) // if it wasn't a total merge, go forward
		{
			if (a0->tail->weight < a1->tail->weight)
			{
				a0 = nextArcMappedToEdge(a0, e0);
			}
			else
			{
				a1 = nextArcMappedToEdge(a1, e1);
			}
		}
	}
}

static void mergePaths(ReebGraph *rg, ReebEdge *e0, ReebEdge *e1, ReebEdge *e2)
{
	ReebArc *a0, *a1, *a2;
	a0 = e0->arc;
	a1 = e1->arc;
	a2 = e2->arc;
	
	glueByMergeSort(rg, a0, a1, e0, e1);
	glueByMergeSort(rg, a0, a2, e0, e2);
} 

static ReebEdge * createArc(ReebGraph *rg, ReebNode *node1, ReebNode *node2)
{
	ReebEdge *edge;
	
	edge = BLI_edgehash_lookup(rg->emap, node1->index, node2->index);
	
	// Only add existing edges that haven't been added yet
	if (edge == NULL)
	{
		ReebArc *arc;
		ReebNode *v1, *v2;
		float len, offset;
		int i;
		
		arc = MEM_callocN(sizeof(ReebArc), "reeb arc");
		edge = MEM_callocN(sizeof(ReebEdge), "reeb edge");
		
		arc->flag = 0; // clear flag on init
		arc->symmetry_level = 0;
		arc->faces = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "createArc gh");
		
		if (node1->weight <= node2->weight)
		{
			v1 = node1;	
			v2 = node2;	
		}
		else
		{
			v1 = node2;	
			v2 = node1;	
		}
		
		arc->head = v1;
		arc->tail = v2;
		
		// increase node degree
		//v1->degree++;
		NodeDegreeIncrement(rg, v1);
		//v2->degree++;
		NodeDegreeIncrement(rg, v2);

		BLI_edgehash_insert(rg->emap, node1->index, node2->index, edge);
		
		edge->arc = arc;
		edge->nextEdge = NULL;
		edge->v1 = v1;
		edge->v2 = v2;
		
		BLI_addtail(&rg->arcs, arc);
		BLI_addtail(&arc->edges, edge);
		
		/* adding buckets for embedding */
		allocArcBuckets(arc);
		
		offset = arc->head->weight;
		len = arc->tail->weight - arc->head->weight;

#if 0
		/* This is the actual embedding filling described in the paper
		 * the problem is that it only works with really dense meshes
		 */
		if (arc->bcount > 0)
		{
			addVertToBucket(&(arc->buckets[0]), arc->head->co);
			addVertToBucket(&(arc->buckets[arc->bcount - 1]), arc->tail->co);
		}
#else
		for(i = 0; i < arc->bcount; i++)
		{
			float co[3];
			float f = (arc->buckets[i].val - offset) / len;
			
			interp_v3_v3v3(co, v1->p, v2->p, f);
			addVertToBucket(&(arc->buckets[i]), co);
		}
#endif

	}
	
	return edge;
}

static void addTriangleToGraph(ReebGraph *rg, ReebNode * n1, ReebNode * n2, ReebNode * n3, EditFace *efa)
{
	ReebEdge *re1, *re2, *re3;
	ReebEdge *e1, *e2, *e3;
	float len1, len2, len3;
	
	re1 = createArc(rg, n1, n2);
	re2 = createArc(rg, n2, n3);
	re3 = createArc(rg, n3, n1);
	
	addFacetoArc(re1->arc, efa);
	addFacetoArc(re2->arc, efa);
	addFacetoArc(re3->arc, efa);
	
	len1 = (float)fabs(n1->weight - n2->weight);
	len2 = (float)fabs(n2->weight - n3->weight);
	len3 = (float)fabs(n3->weight - n1->weight);
	
	/* The rest of the algorithm assumes that e1 is the longest edge */
	
	if (len1 >= len2 && len1 >= len3)
	{
		e1 = re1;
		e2 = re2;
		e3 = re3;
	}
	else if (len2 >= len1 && len2 >= len3)
	{
		e1 = re2;
		e2 = re1;
		e3 = re3;
	}
	else
	{
		e1 = re3;
		e2 = re2;
		e3 = re1;
	}
	
	/* And e2 is the lowest edge
	 * If e3 is lower than e2, swap them
	 */
	if (e3->v1->weight < e2->v1->weight)
	{
		ReebEdge *etmp = e2;
		e2 = e3;
		e3 = etmp;
	}
	
	
	mergePaths(rg, e1, e2, e3);
}

ReebGraph * generateReebGraph(EditMesh *em, int subdivisions)
{
	ReebGraph *rg;
	EditVert *eve;
	EditFace *efa;
	int index;
	/*int totvert;*/
	
#ifdef DEBUG_REEB
	int totfaces;
	int countfaces = 0;
#endif

	rg = newReebGraph();
	
	rg->resolution = subdivisions;
	
	/*totvert = BLI_countlist(&em->verts);*/ /*UNUSED*/
#ifdef DEBUG_REEB
	totfaces = BLI_countlist(&em->faces);
#endif
	
	renormalizeWeight(em, 1.0f);
	
	/* Spread weight to minimize errors */
	spreadWeight(em);

	renormalizeWeight(em, (float)rg->resolution);

	/* Adding vertice */
	for(index = 0, eve = em->verts.first; eve; eve = eve->next)
	{
		if (eve->h == 0)
		{
			addNode(rg, eve);
			eve->f2 = 0;
			index++;
		}
	}
	
	/* Adding face, edge per edge */
	for(efa = em->faces.first; efa; efa = efa->next)
	{
		if (efa->h == 0)
		{
			ReebNode *n1, *n2, *n3;
			
			n1 = nodeData(efa->v1);
			n2 = nodeData(efa->v2);
			n3 = nodeData(efa->v3);
			
			addTriangleToGraph(rg, n1, n2, n3, efa);
			
			if (efa->v4)
			{
				ReebNode *n4 = nodeData(efa->v4);
				addTriangleToGraph(rg, n1, n3, n4, efa);
			}
#ifdef DEBUG_REEB
			countfaces++;
			if (countfaces % 100 == 0)
			{
				printf("\rface %i of %i", countfaces, totfaces);
			}
#endif
		}
	}
	
	printf("\n");
	
	removeZeroNodes(rg);
	
	removeNormalNodes(rg);
	
	return rg;
}

/***************************************** WEIGHT UTILS **********************************************/

void renormalizeWeight(EditMesh *em, float newmax)
{
	EditVert *eve;
	float minimum, maximum, range;
	
	if (em == NULL || BLI_countlist(&em->verts) == 0)
		return;

	/* First pass, determine maximum and minimum */
	eve = em->verts.first;
	minimum = weightData(eve);
	maximum = minimum;
	for(; eve; eve = eve->next)
	{
		maximum = MAX2(maximum, weightData(eve));
		minimum = MIN2(minimum, weightData(eve));
	}
	
	range = maximum - minimum;

	/* Normalize weights */
	for(eve = em->verts.first; eve; eve = eve->next)
	{
		float weight = (weightData(eve) - minimum) / range * newmax;
		weightSetData(eve, weight);
	}
}


int weightFromLoc(EditMesh *em, int axis)
{
	EditVert *eve;
	
	if (em == NULL || BLI_countlist(&em->verts) == 0 || axis < 0 || axis > 2)
		return 0;

	/* Copy coordinate in weight */
	for(eve = em->verts.first; eve; eve = eve->next)
	{
		weightSetData(eve, eve->co[axis]);
	}

	return 1;
}

static float cotan_weight(float *v1, float *v2, float *v3)
{
	float a[3], b[3], c[3], clen;

	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v3, v1);
	cross_v3_v3v3(c, a, b);

	clen = len_v3(c);

	if (clen == 0.0f)
		return 0.0f;
	
	return dot_v3v3(a, b)/clen;
}

static void addTriangle(EditVert *v1, EditVert *v2, EditVert *v3, int e1, int e2, int e3)
{
	/* Angle opposite e1 */
	float t1= cotan_weight(v1->co, v2->co, v3->co) / e2;
	
	/* Angle opposite e2 */
	float t2 = cotan_weight(v2->co, v3->co, v1->co) / e3;

	/* Angle opposite e3 */
	float t3 = cotan_weight(v3->co, v1->co, v2->co) / e1;
	
	int i1 = indexData(v1);
	int i2 = indexData(v2);
	int i3 = indexData(v3);
	
	nlMatrixAdd(i1, i1, t2+t3);
	nlMatrixAdd(i2, i2, t1+t3);
	nlMatrixAdd(i3, i3, t1+t2);

	nlMatrixAdd(i1, i2, -t3);
	nlMatrixAdd(i2, i1, -t3);

	nlMatrixAdd(i2, i3, -t1);
	nlMatrixAdd(i3, i2, -t1);

	nlMatrixAdd(i3, i1, -t2);
	nlMatrixAdd(i1, i3, -t2);
}

int weightToHarmonic(EditMesh *em, EdgeIndex *indexed_edges)
{
	NLboolean success;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	int totvert = 0;
	int index;
	int rval;
	
	/* Find local extrema */
	for(eve = em->verts.first; eve; eve = eve->next)
	{
		totvert++;
	}

	/* Solve with openNL */
	
	nlNewContext();

	nlSolverParameteri(NL_NB_VARIABLES, totvert);

	nlBegin(NL_SYSTEM);
	
	/* Find local extrema */
	for(index = 0, eve = em->verts.first; eve; index++, eve = eve->next)
	{
		if (eve->h == 0)
		{
			EditEdge *eed;
			int maximum = 1;
			int minimum = 1;
			
			NextEdgeForVert(indexed_edges, -1); /* Reset next edge */
			for(eed = NextEdgeForVert(indexed_edges, index); eed && (maximum || minimum); eed = NextEdgeForVert(indexed_edges, index))
			{
				EditVert *eve2;
				
				if (eed->v1 == eve)
				{
					eve2 = eed->v2;
				}
				else
				{
					eve2 = eed->v1;
				}
				
				if (eve2->h == 0)
				{
					/* Adjacent vertex is bigger, not a local maximum */
					if (weightData(eve2) > weightData(eve))
					{
						maximum = 0;
					}
					/* Adjacent vertex is smaller, not a local minimum */
					else if (weightData(eve2) < weightData(eve))
					{
						minimum = 0;
					}
				}
			}
			
			if (maximum || minimum)
			{
				float w = weightData(eve);
				eve->f1 = 0;
				nlSetVariable(0, index, w);
				nlLockVariable(index);
			}
			else
			{
				eve->f1 = 1;
			}
		}
	}
	
	nlBegin(NL_MATRIX);

	/* Zero edge weight */
	for(eed = em->edges.first; eed; eed = eed->next)
	{
		eed->tmp.l = 0;
	}
	
	/* Add faces count to the edge weight */
	for(efa = em->faces.first; efa; efa = efa->next)
	{
		if (efa->h == 0)
		{
			efa->e1->tmp.l++;
			efa->e2->tmp.l++;
			efa->e3->tmp.l++;
			
			if (efa->e4)
			{
				efa->e4->tmp.l++;
			}
		}
	}

	/* Add faces angle to the edge weight */
	for(efa = em->faces.first; efa; efa = efa->next)
	{
		if (efa->h == 0)
		{
			if (efa->v4 == NULL)
			{
				addTriangle(efa->v1, efa->v2, efa->v3, efa->e1->tmp.l, efa->e2->tmp.l, efa->e3->tmp.l);
			}
			else
			{
				addTriangle(efa->v1, efa->v2, efa->v3, efa->e1->tmp.l, efa->e2->tmp.l, 2);
				addTriangle(efa->v3, efa->v4, efa->v1, efa->e3->tmp.l, efa->e4->tmp.l, 2);
			}
		}
	}
	
	nlEnd(NL_MATRIX);

	nlEnd(NL_SYSTEM);

	success = nlSolveAdvanced(NULL, NL_TRUE);

	if (success)
	{
		rval = 1;
		for(index = 0, eve = em->verts.first; eve; index++, eve = eve->next)
		{
			weightSetData(eve, nlGetVariable(0, index));
		}
	}
	else
	{
		rval = 0;
	}

	nlDeleteContext(nlGetCurrent());

	return rval;
}


EditEdge * NextEdgeForVert(EdgeIndex *indexed_edges, int index)
{
	static int offset = -1;
	
	/* Reset method, call with NULL mesh pointer */
	if (index == -1)
	{
		offset = -1;
		return NULL;
	}
	
	/* first pass, start at the head of the list */
	if (offset == -1)
	{
		offset = indexed_edges->offset[index];
	}
	/* subsequent passes, start on the next edge */
	else
	{
		offset++;
	}
	
	return indexed_edges->edges[offset];
}

static void shortestPathsFromVert(EditMesh *em, EditVert *starting_vert, EdgeIndex *indexed_edges)
{
	Heap	 *edge_heap;
	EditVert *current_eve = NULL;
	EditEdge *eed = NULL;
	EditEdge *select_eed = NULL;
	
	edge_heap = BLI_heap_new();
	
	current_eve = starting_vert;
	
	/* insert guard in heap, when that is returned, no more edges */
	BLI_heap_insert(edge_heap, FLT_MAX, NULL);

	/* Initialize edge flag */
	for(eed= em->edges.first; eed; eed= eed->next)
	{
		eed->f1 = 0;
	}
	
	while (BLI_heap_size(edge_heap) > 0)
	{
		float current_weight;
		
		current_eve->f1 = 1; /* mark vertex as selected */
		
		/* Add all new edges connected to current_eve to the list */
		NextEdgeForVert(indexed_edges, -1); // Reset next edge
		for(eed = NextEdgeForVert(indexed_edges, indexData(current_eve)); eed; eed = NextEdgeForVert(indexed_edges, indexData(current_eve)))
		{ 
			if (eed->f1 == 0)
			{
				BLI_heap_insert(edge_heap, weightData(current_eve) + eed->tmp.fp, eed);
				eed->f1 = 1;
			}
		}
		
		/* Find next shortest edge with unselected verts */
		do
		{
			current_weight = BLI_heap_node_value(BLI_heap_top(edge_heap));
			select_eed = BLI_heap_popmin(edge_heap);
		} while (select_eed != NULL && select_eed->v1->f1 != 0 && select_eed->v2->f1);
		
		if (select_eed != NULL)
		{
			select_eed->f1 = 2;
			
			if (select_eed->v1->f1 == 0) /* v1 is the new vertex */
			{
				current_eve = select_eed->v1;
			}
			else /* otherwise, it's v2 */
			{
				current_eve = select_eed->v2;
			}
			
			weightSetData(current_eve, current_weight);
		}
	}
	
	BLI_heap_free(edge_heap, NULL);
}

static void freeEdgeIndex(EdgeIndex *indexed_edges)
{
	MEM_freeN(indexed_edges->offset);
	MEM_freeN(indexed_edges->edges);
}

static void buildIndexedEdges(EditMesh *em, EdgeIndex *indexed_edges)
{
	EditVert *eve;
	EditEdge *eed;
	int totvert = 0;
	int tot_indexed = 0;
	int offset = 0;

	totvert = BLI_countlist(&em->verts);

	indexed_edges->offset = MEM_callocN(totvert * sizeof(int), "EdgeIndex offset");

	for(eed = em->edges.first; eed; eed = eed->next)
	{
		if (eed->v1->h == 0 && eed->v2->h == 0)
		{
			tot_indexed += 2;
			indexed_edges->offset[indexData(eed->v1)]++;
			indexed_edges->offset[indexData(eed->v2)]++;
		}
	}
	
	tot_indexed += totvert;

	indexed_edges->edges = MEM_callocN(tot_indexed * sizeof(EditEdge*), "EdgeIndex edges");

	/* setting vert offsets */
	for(eve = em->verts.first; eve; eve = eve->next)
	{
		if (eve->h == 0)
		{
			int d = indexed_edges->offset[indexData(eve)];
			indexed_edges->offset[indexData(eve)] = offset;
			offset += d + 1;
		}
	}

	/* adding edges in array */
	for(eed = em->edges.first; eed; eed= eed->next)
	{
		if (eed->v1->h == 0 && eed->v2->h == 0)
		{
			int i;
			for (i = indexed_edges->offset[indexData(eed->v1)]; i < tot_indexed; i++)
			{
				if (indexed_edges->edges[i] == NULL)
				{
					indexed_edges->edges[i] = eed;
					break;
				}
			}
			
			for (i = indexed_edges->offset[indexData(eed->v2)]; i < tot_indexed; i++)
			{
				if (indexed_edges->edges[i] == NULL)
				{
					indexed_edges->edges[i] = eed;
					break;
				}
			}
		}
	}
}

int weightFromDistance(EditMesh *em, EdgeIndex *indexed_edges)
{
	EditVert *eve;
	int totedge = 0;
	int totvert = 0;
	int vCount = 0;
	
	totvert = BLI_countlist(&em->verts);
	
	if (em == NULL || totvert == 0)
	{
		return 0;
	}
	
	totedge = BLI_countlist(&em->edges);
	
	if (totedge == 0)
	{
		return 0;
	}
	
	/* Initialize vertice flag and find at least one selected vertex */
	for(eve = em->verts.first; eve; eve = eve->next)
	{
		eve->f1 = 0;
		if (eve->f & SELECT)
		{
			vCount = 1;
		}
	}
	
	if (vCount == 0)
	{
		return 0; /* no selected vert, failure */
	}
	else
	{
		EditEdge *eed;
		int allDone = 0;

		/* Calculate edge weight */
		for(eed = em->edges.first; eed; eed= eed->next)
		{
			if (eed->v1->h == 0 && eed->v2->h == 0)
			{
				eed->tmp.fp = len_v3v3(eed->v1->co, eed->v2->co);
			}
		}

		/* Apply dijkstra spf for each selected vert */
		for(eve = em->verts.first; eve; eve = eve->next)
		{
			if (eve->f & SELECT)
			{
				shortestPathsFromVert(em, eve, indexed_edges);				
			}
		}
		
		/* connect unselected islands */
		while (allDone == 0)
		{
			EditVert *selected_eve = NULL;
			float selected_weight = 0;
			float min_distance = FLT_MAX;
			
			allDone = 1;
			
			for (eve = em->verts.first; eve; eve = eve->next)
			{
				/* for every vertex visible that hasn't been processed yet */
				if (eve->h == 0 && eve->f1 != 1)
				{
					EditVert *closest_eve;
					
					/* find the closest processed vertex */
					for (closest_eve = em->verts.first; closest_eve; closest_eve = closest_eve->next)
					{
						/* vertex is already processed and distance is smaller than current minimum */
						if (closest_eve->f1 == 1)
						{
							float distance = len_v3v3(closest_eve->co, eve->co);
							if (distance < min_distance)
							{
								min_distance = distance;
								selected_eve = eve;
								selected_weight = weightData(closest_eve);
							}
						}
					}
				}
			}
			
			if (selected_eve)
			{
				allDone = 0;

				weightSetData(selected_eve, selected_weight + min_distance);
				shortestPathsFromVert(em, selected_eve, indexed_edges);
			}
		}
	}

	for(eve = em->verts.first; eve && vCount == 0; eve = eve->next)
	{
		if (eve->f1 == 0)
		{
			printf("vertex not reached\n");
			break;
		}
	}
	
	return 1;
}
#endif

/****************************************** BUCKET ITERATOR **************************************************/

static void* headNode(void *arg);
static void* tailNode(void *arg);
static void* nextBucket(void *arg);
static void* nextNBucket(void *arg, int n);
static void* peekBucket(void *arg, int n);
static void* previousBucket(void *arg);
static int   iteratorStopped(void *arg);

static void initIteratorFct(ReebArcIterator *iter)
{
	iter->head = headNode;
	iter->tail = tailNode;
	iter->peek = peekBucket;
	iter->next = nextBucket;
	iter->nextN = nextNBucket;
	iter->previous = previousBucket;
	iter->stopped = iteratorStopped;	
}

static void setIteratorValues(ReebArcIterator *iter, EmbedBucket *bucket)
{
	if (bucket)
	{
		iter->p = bucket->p;
		iter->no = bucket->no;
	}
	else
	{
		iter->p = NULL;
		iter->no = NULL;
	}
	iter->size = 0;
}

void initArcIterator(BArcIterator *arg, ReebArc *arc, ReebNode *head)
{
	ReebArcIterator *iter = (ReebArcIterator*)arg;

	initIteratorFct(iter);
	iter->arc = arc;
	
	if (head == arc->head)
	{
		iter->start = 0;
		iter->end = arc->bcount - 1;
		iter->stride = 1;
	}
	else
	{
		iter->start = arc->bcount - 1;
		iter->end = 0;
		iter->stride = -1;
	}
	
	iter->length = arc->bcount;
	
	iter->index = -1;
}

void initArcIteratorStart(BArcIterator *arg, struct ReebArc *arc, struct ReebNode *head, int start)
{
	ReebArcIterator *iter = (ReebArcIterator*)arg;

	initIteratorFct(iter);
	iter->arc = arc;
	
	if (head == arc->head)
	{
		iter->start = start;
		iter->end = arc->bcount - 1;
		iter->stride = 1;
	}
	else
	{
		iter->start = arc->bcount - 1 - start;
		iter->end = 0;
		iter->stride = -1;
	}
	
	iter->index = -1;
	
	iter->length = arc->bcount - start;

	if (start >= arc->bcount)
	{
		iter->start = iter->end; /* stop iterator since it's past its end */
	}
}

void initArcIterator2(BArcIterator *arg, ReebArc *arc, int start, int end)
{
	ReebArcIterator *iter = (ReebArcIterator*)arg;

	initIteratorFct(iter);
	iter->arc = arc;
	
	iter->start = start;
	iter->end = end;
	
	if (end > start)
	{
		iter->stride = 1;
	}
	else
	{
		iter->stride = -1;
	}

	iter->index = -1;

	iter->length = abs(iter->end - iter->start) + 1;
}

static void* headNode(void *arg)
{
	ReebArcIterator *iter = (ReebArcIterator*)arg;
	ReebNode *node;
	
	if (iter->start < iter->end)
	{
		node = iter->arc->head;
	}
	else
	{
		node = iter->arc->tail;
	}
	
	iter->p = node->p;
	iter->no = node->no;
	iter->size = 0;
	
	return node;
}

static void* tailNode(void *arg)
{
	ReebArcIterator *iter = (ReebArcIterator*)arg;
	ReebNode *node;
	
	if (iter->start < iter->end)
	{
		node = iter->arc->tail;
	}
	else
	{
		node = iter->arc->head;
	}
	
	iter->p = node->p;
	iter->no = node->no;
	iter->size = 0;
	
	return node;
}

static void* nextBucket(void *arg)
{
	ReebArcIterator *iter = (ReebArcIterator*)arg;
	EmbedBucket *result = NULL;
	
	iter->index++;
	
	if (iter->index < iter->length)
	{
		result = &(iter->arc->buckets[iter->start + (iter->stride * iter->index)]);
	}
	
	setIteratorValues(iter, result);
	return result;
}

static void* nextNBucket(void *arg, int n)
{
	ReebArcIterator *iter = (ReebArcIterator*)arg;
	EmbedBucket *result = NULL;
		
	iter->index += n;

	/* check if passed end */
	if (iter->index < iter->length)
	{
		result = &(iter->arc->buckets[iter->start + (iter->stride * iter->index)]);
	}
	
	setIteratorValues(iter, result);
	return result;
}

static void* peekBucket(void *arg, int n)
{
	ReebArcIterator *iter = (ReebArcIterator*)arg;
	EmbedBucket *result = NULL;
	int index = iter->index + n;

	/* check if passed end */
	if (index < iter->length)
	{
		result = &(iter->arc->buckets[iter->start + (iter->stride * index)]);
	}

	setIteratorValues(iter, result);
	return result;
}

static void* previousBucket(void *arg)
{
	ReebArcIterator *iter = (ReebArcIterator*)arg;
	EmbedBucket *result = NULL;
	
	if (iter->index > 0)
	{
		iter->index--;
		result = &(iter->arc->buckets[iter->start + (iter->stride * iter->index)]);
	}

	setIteratorValues(iter, result);
	return result;
}

static int iteratorStopped(void *arg)
{
	ReebArcIterator *iter = (ReebArcIterator*)arg;

	if (iter->index >= iter->length)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/************************ PUBLIC FUNCTIONS *********************************************/

ReebGraph *BIF_ReebGraphMultiFromEditMesh(bContext *C)
{
	(void)C;
	return NULL;
#if 0
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	EditMesh *em = BKE_mesh_get_editmesh(((Mesh*)obedit->data));
	EdgeIndex indexed_edges;
	VertexData *data;
	ReebGraph *rg = NULL;
	ReebGraph *rgi, *previous;
	int i, nb_levels = REEB_MAX_MULTI_LEVEL;

	if (em == NULL)
		return NULL;
	
	data = allocVertexData(em);

	buildIndexedEdges(em, &indexed_edges);

	if (weightFromDistance(em, &indexed_edges) == 0)
	{
		// XXX error("No selected vertex\n");
		freeEdgeIndex(&indexed_edges);
		return NULL;
	}
	
	renormalizeWeight(em, 1.0f);

	if (scene->toolsettings->skgen_options & SKGEN_HARMONIC)
	{
		weightToHarmonic(em, &indexed_edges);
	}
	
	freeEdgeIndex(&indexed_edges);
	
	rg = generateReebGraph(em, scene->toolsettings->skgen_resolution);

	/* Remove arcs without embedding */
	filterNullReebGraph(rg);

	/* smart filter and loop filter on basic level */
	filterGraph(rg, SKGEN_FILTER_SMART, 0, 0);

	repositionNodes(rg);

	/* Filtering might have created degree 2 nodes, so remove them */
	removeNormalNodes(rg);
	
	joinSubgraphs(rg, 1.0);

	BLI_buildAdjacencyList((BGraph*)rg);
	
	/* calc length before copy, so we have same length on all levels */
	BLI_calcGraphLength((BGraph*)rg);

	previous = NULL;
	for (i = 0; i <= nb_levels; i++)
	{
		rgi = rg;
		
		/* don't filter last level */
		if (i > 0)
		{
			float internal_threshold;
			float external_threshold;

			/* filter internal progressively in second half only*/
			if (i > nb_levels / 2)
			{
				internal_threshold = rg->length * scene->toolsettings->skgen_threshold_internal;
			}
			else
			{
				internal_threshold = rg->length * scene->toolsettings->skgen_threshold_internal * (2 * i / (float)nb_levels);
			}
			
			external_threshold = rg->length * scene->toolsettings->skgen_threshold_external * (i / (float)nb_levels);

			filterGraph(rgi, scene->toolsettings->skgen_options, internal_threshold, external_threshold);
		}

		if (i < nb_levels)
		{
			rg = copyReebGraph(rgi, i + 1);
		}

		finalizeGraph(rgi, scene->toolsettings->skgen_postpro_passes, scene->toolsettings->skgen_postpro);

		BLI_markdownSymmetry((BGraph*)rgi, rgi->nodes.first, scene->toolsettings->skgen_symmetry_limit);
		
		if (previous != NULL)
		{
			relinkNodes(rgi, previous);
		}
		previous = rgi;
	}
	
	verifyMultiResolutionLinks(rg, 0);
	
	MEM_freeN(data);

	/* no need to load the editmesh back into the object, just
	 * free it (avoids ngon conversion issues too going back the other way) */
	free_editMesh(em);
	MEM_freeN(em);
	
	return rg;
#endif
}

#if 0

ReebGraph *BIF_ReebGraphFromEditMesh(void)
{
	EditMesh *em = G.editMesh;
	EdgeIndex indexed_edges;
	VertexData *data;
	ReebGraph *rg = NULL;
	
	if (em == NULL)
		return NULL;

	data = allocVertexData(em);

	buildIndexedEdges(em, &indexed_edges);
	
	if (weightFromDistance(em, &indexed_edges) == 0)
	{
		error("No selected vertex\n");
		freeEdgeIndex(&indexed_edges);
		freeEdgeIndex(&indexed_edges);
		return NULL;
	}
	
	renormalizeWeight(em, 1.0f);

	if (G.scene->toolsettings->skgen_options & SKGEN_HARMONIC)
	{
		weightToHarmonic(em, &indexed_edges);
	}
	
	freeEdgeIndex(&indexed_edges);
	
#ifdef DEBUG_REEB
	weightToVCol(em, 1);
#endif
	
	rg = generateReebGraph(em, G.scene->toolsettings->skgen_resolution);


	/* Remove arcs without embedding */
	filterNullReebGraph(rg);

	/* smart filter and loop filter on basic level */
	filterGraph(rg, SKGEN_FILTER_SMART, 0, 0);

	repositionNodes(rg);

	/* Filtering might have created degree 2 nodes, so remove them */
	removeNormalNodes(rg);
	
	joinSubgraphs(rg, 1.0);

	BLI_buildAdjacencyList((BGraph*)rg);
	
	/* calc length before copy, so we have same length on all levels */
	BLI_calcGraphLength((BGraph*)rg);
	
	filterGraph(rg, G.scene->toolsettings->skgen_options, G.scene->toolsettings->skgen_threshold_internal, G.scene->toolsettings->skgen_threshold_external);

	finalizeGraph(rg, G.scene->toolsettings->skgen_postpro_passes, G.scene->toolsettings->skgen_postpro);

#ifdef DEBUG_REEB
	REEB_exportGraph(rg, -1);
	
	arcToVCol(rg, em, 0);
	//angleToVCol(em, 1);
#endif

	printf("DONE\n");
	printf("%i subgraphs\n", BLI_FlagSubgraphs((BGraph*)rg));
	
	MEM_freeN(data);

	return rg;
}

void BIF_GlobalReebFree()
{
	if (GLOBAL_RG != NULL)
	{
		REEB_freeGraph(GLOBAL_RG);
		GLOBAL_RG = NULL;
	}
}

void BIF_GlobalReebGraphFromEditMesh(void)
{
	ReebGraph *rg;
	
	BIF_GlobalReebFree();
	
	rg = BIF_ReebGraphMultiFromEditMesh();

	GLOBAL_RG = rg;
}

void REEB_draw()
{
	ReebGraph *rg;
	ReebArc *arc;
	int i = 0;
	
	if (GLOBAL_RG == NULL)
	{
		return;
	}
	
	if (GLOBAL_RG->link_up && G.scene->toolsettings->skgen_options & SKGEN_DISP_ORIG)
	{
		for (rg = GLOBAL_RG; rg->link_up; rg = rg->link_up);
	}
	else
	{
		i = G.scene->toolsettings->skgen_multi_level;
		
		for (rg = GLOBAL_RG; rg->multi_level != i && rg->link_up; rg = rg->link_up);
	}
	
	glPointSize(BIF_GetThemeValuef(TH_VERTEX_SIZE));
	
	glDisable(GL_DEPTH_TEST);
	for (arc = rg->arcs.first; arc; arc = arc->next, i++)
	{
		ReebArcIterator arc_iter;
		BArcIterator *iter = (BArcIterator*)&arc_iter;
		float vec[3];
		char text[128];
		char *s = text;
		
		glLineWidth(BIF_GetThemeValuef(TH_VERTEX_SIZE) + 2);
		glColor3f(0, 0, 0);
		glBegin(GL_LINE_STRIP);
			glVertex3fv(arc->head->p);
			
			if (arc->bcount)
			{
				initArcIterator(iter, arc, arc->head);
				for (IT_next(iter); IT_stopped(iter) == 0; IT_next(iter))
				{
					glVertex3fv(iter->p);
				}
			}
			
			glVertex3fv(arc->tail->p);
		glEnd();

		glLineWidth(BIF_GetThemeValuef(TH_VERTEX_SIZE));

		if (arc->symmetry_level == 1)
		{
			glColor3f(1, 0, 0);
		}
		else if (arc->symmetry_flag == SYM_SIDE_POSITIVE || arc->symmetry_flag == SYM_SIDE_NEGATIVE)
		{
			glColor3f(1, 0.5f, 0);
		}
		else if (arc->symmetry_flag >= SYM_SIDE_RADIAL)
		{
			glColor3f(0.5f, 1, 0);
		}
		else
		{
			glColor3f(1, 1, 0);
		}
		glBegin(GL_LINE_STRIP);
			glVertex3fv(arc->head->p);
			
			if (arc->bcount)
			{
				initArcIterator(iter, arc, arc->head);
				for (iter->next(iter); IT_stopped(iter) == 0; iter->next(iter))
				{
					glVertex3fv(iter->p);
				}
			}
			
			glVertex3fv(arc->tail->p);
		glEnd();

		
		if (G.scene->toolsettings->skgen_options & SKGEN_DISP_EMBED)
		{
			glColor3f(1, 1, 1);				
			glBegin(GL_POINTS);
				glVertex3fv(arc->head->p);
				glVertex3fv(arc->tail->p);
				
				glColor3f(0.5f, 0.5f, 1);				
				if (arc->bcount)
				{
					initArcIterator(iter, arc, arc->head);
					for (iter->next(iter); IT_stopped(iter) == 0; iter->next(iter))
					{
						glVertex3fv(iter->p);
					}
				}
			glEnd();
		}
		
		if (G.scene->toolsettings->skgen_options & SKGEN_DISP_INDEX)
		{
			mid_v3_v3v3(vec, arc->head->p, arc->tail->p);
			s += sprintf(s, "%i (%i-%i-%i) ", i, arc->symmetry_level, arc->symmetry_flag, arc->symmetry_group);
		
			if (G.scene->toolsettings->skgen_options & SKGEN_DISP_WEIGHT)
			{
				s += sprintf(s, "w:%0.3f ", arc->tail->weight - arc->head->weight);
			}
			
			if (G.scene->toolsettings->skgen_options & SKGEN_DISP_LENGTH)
			{
				s += sprintf(s, "l:%0.3f", arc->length);
			}
			
			glColor3f(0, 1, 0);
			glRasterPos3fv(vec);
			BMF_DrawString( G.fonts, text);
		}

		if (G.scene->toolsettings->skgen_options & SKGEN_DISP_INDEX)
		{
			sprintf(text, "  %i", arc->head->index);
			glRasterPos3fv(arc->head->p);
			BMF_DrawString( G.fonts, text);
	
			sprintf(text, "  %i", arc->tail->index);
			glRasterPos3fv(arc->tail->p);
			BMF_DrawString( G.fonts, text);
		}
	}
	glEnable(GL_DEPTH_TEST);
	
	glLineWidth(1.0);
	glPointSize(1.0);
}

#endif
