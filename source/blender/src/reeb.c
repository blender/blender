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
 
#include <math.h>
#include <string.h> // for memcpy
#include <stdio.h>
#include <stdlib.h> // for qsort

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_edgehash.h"

#include "BDR_editobject.h"

#include "BIF_editmesh.h"
#include "BIF_editarmature.h"
#include "BIF_interface.h"
#include "BIF_toolbox.h"
#include "BIF_graphics.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_customdata.h"

#include "blendef.h"

#include "ONL_opennl.h"

#include "reeb.h"

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

int mergeArcs(ReebGraph *rg, ReebArc *a0, ReebArc *a1);
int mergeConnectedArcs(ReebGraph *rg, ReebArc *a0, ReebArc *a1);
EditEdge * NextEdgeForVert(EditMesh *em, EditVert *v);

/***************************************** BUCKET UTILS **********************************************/

void addVertToBucket(EmbedBucket *b, float co[3])
{
	b->nv++;
	VecLerpf(b->p, b->p, co, 1.0f / b->nv);
}

void removeVertFromBucket(EmbedBucket *b, float co[3])
{
	VecMulf(b->p, (float)b->nv);
	VecSubf(b->p, b->p, co);
	b->nv--;
	VecMulf(b->p, 1.0f / (float)b->nv);
}

void mergeBuckets(EmbedBucket *bDst, EmbedBucket *bSrc)
{
	if (bDst->nv > 0 && bSrc->nv > 0)
	{
		bDst->nv += bSrc->nv;
		VecLerpf(bDst->p, bDst->p, bSrc->p, (float)bSrc->nv / (float)(bDst->nv));
	}
	else if (bSrc->nv > 0)
	{
		bDst->nv = bSrc->nv;
		VECCOPY(bDst->p, bSrc->p);
	}
}

void mergeArcBuckets(ReebArc *aDst, ReebArc *aSrc, float start, float end)
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

void allocArcBuckets(ReebArc *arc)
{
	int i;
	float start = ceil(arc->v1->weight);
	arc->bcount = (int)(floor(arc->v2->weight) - start) + 1;
	
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

void resizeArcBuckets(ReebArc *arc)
{
	EmbedBucket *oldBuckets = arc->buckets;
	int oldBCount = arc->bcount;
	
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
/***************************************** UTILS **********************************************/

ReebEdge * copyEdge(ReebEdge *edge)
{
	ReebEdge *newEdge = NULL;
	
	newEdge = MEM_callocN(sizeof(ReebEdge), "reeb edge");
	memcpy(newEdge, edge, sizeof(ReebEdge));
	
	newEdge->next = NULL;
	newEdge->prev = NULL;
	
	return newEdge;
}

void printArc(ReebArc *arc)
{
	ReebEdge *edge;
	printf("arc: (%i)%f -> (%i)%f\n", arc->v1->index, arc->v1->weight, arc->v2->index, arc->v2->weight);
	
	for(edge = arc->edges.first; edge ; edge = edge->next)
	{
		printf("\tedge (%i, %i)\n", edge->v1->index, edge->v2->index);
	}
}

void freeArc(ReebArc *arc)
{
	BLI_freelistN(&arc->edges);
	
	if (arc->buckets)
		MEM_freeN(arc->buckets);
	
	MEM_freeN(arc);
}

void freeGraph(ReebGraph *rg)
{
	ReebArc *arc;
	ReebNode *node;
	
	// free nodes
	for( node = rg->nodes.first; node; node = node->next )
	{
		// Free adjacency lists
		if (node->arcs != NULL)
		{
			MEM_freeN(node->arcs);
		}
	}
	BLI_freelistN(&rg->nodes);
	
	// free arcs
	arc = rg->arcs.first;
	while( arc )
	{
		ReebArc *next = arc->next;
		freeArc(arc);
		arc = next;
	}
	
	// free edge map
	BLI_edgehash_free(rg->emap, NULL);
	
	MEM_freeN(rg);
}

void repositionNodes(ReebGraph *rg)
{
	ReebArc *arc = NULL;
	ReebNode *node = NULL;
	
	// Reset node positions
	for(node = rg->nodes.first; node; node = node->next)
	{
		node->p[0] = node->p[1] = node->p[2] = 0;
	}
	
	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
		if (arc->bcount > 0)
		{
			float p[3];
			
			VECCOPY(p, arc->buckets[0].p);
			VecMulf(p, 1.0f / arc->v1->degree);
			VecAddf(arc->v1->p, arc->v1->p, p);
			
			VECCOPY(p, arc->buckets[arc->bcount - 1].p);
			VecMulf(p, 1.0f / arc->v2->degree);
			VecAddf(arc->v2->p, arc->v2->p, p);
		}
	}
}

void verifyNodeDegree(ReebGraph *rg)
{
	ReebNode *node = NULL;
	ReebArc *arc = NULL;

	for(node = rg->nodes.first; node; node = node->next)
	{
		int count = 0;
		for(arc = rg->arcs.first; arc; arc = arc->next)
		{
			if (arc->v1 == node || arc->v2 == node)
			{
				count++;
			}
		}
		if (count != node->degree)
		{
			printf("degree error in node %i: expected %i got %i\n", node->index, count, node->degree);
		}
	}
}

void verifyBuckets(ReebGraph *rg)
{
#ifdef DEBUG_REEB
	ReebArc *arc = NULL;
	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
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
			
			if (ceil(arc->v1->weight) < arc->buckets[0].val)
			{
				printArc(arc);
				printf("alloc error in first bucket: %f should be %f \n", arc->buckets[0].val, ceil(arc->v1->weight));
			}
			if (floor(arc->v2->weight) < arc->buckets[arc->bcount - 1].val)
			{
				printArc(arc);
				printf("alloc error in last bucket: %f should be %f \n", arc->buckets[arc->bcount - 1].val, floor(arc->v2->weight));
			}
		}
	}
#endif
}

/************************************** ADJACENCY LIST *************************************************/

void addArcToNodeAdjacencyList(ReebNode *node, ReebArc *arc)
{
	ReebArc **arclist;

	for(arclist = node->arcs; *arclist; arclist++)
	{	}
	
	*arclist = arc;
}

void buildAdjacencyList(ReebGraph *rg)
{
	ReebNode *node = NULL;
	ReebArc *arc = NULL;

	for(node = rg->nodes.first; node; node = node->next)
	{
		if (node->arcs != NULL)
		{
			MEM_freeN(node->arcs);
		}
		
		node->arcs = MEM_callocN((node->degree + 1) * sizeof(ReebArc*), "adjacency list");
	}

	for(arc = rg->arcs.first; arc; arc= arc->next)
	{
		addArcToNodeAdjacencyList(arc->v1, arc);
		addArcToNodeAdjacencyList(arc->v2, arc);
	}
}

int hasAdjacencyList(ReebGraph *rg)
{
	ReebNode *node;
	
	for(node = rg->nodes.first; node; node = node->next)
	{
		if (node->arcs == NULL)
		{
			return 0;
		}
	}
	
	return 1;
}

int countConnectedArcs(ReebGraph *rg, ReebNode *node)
{
	int count = 0;
	
	/* use adjacency list if present */
	if (node->arcs)
	{
		ReebArc **arcs;
	
		for(arcs = node->arcs; *arcs; arcs++)
		{
			count++;
		}
	}
	else
	{
		ReebArc *arc;
		for(arc = rg->arcs.first; arc; arc = arc->next)
		{
			if (arc->v1 == node || arc->v2 == node)
			{
				count++;
			}
		}
	}
	
	return count;
}

/****************************************** SMOOTHING **************************************************/

void postprocessGraph(ReebGraph *rg, char mode)
{
	ReebArc *arc;
	float fac1, fac2, fac3;

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
		fac1 = fac2 = -0.25f;
		fac2 = 1.5f;
		break;
	default:
		error("Unknown post processing mode");
		return;
	}
	
	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
		EmbedBucket *buckets = arc->buckets;
		int bcount = arc->bcount;
		int index;

		for(index = 1; index < bcount - 1; index++)
		{
			VecLerpf(buckets[index].p, buckets[index].p, buckets[index - 1].p, fac1 / (fac1 + fac2));
			VecLerpf(buckets[index].p, buckets[index].p, buckets[index + 1].p, fac3 / (fac1 + fac2 + fac3));
		}
	}
}

/********************************************SORTING****************************************************/

int compareNodesWeight(void *vnode1, void *vnode2)
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

int compareArcsWeight(void *varc1, void *varc2)
{
	ReebArc *arc1 = (ReebArc*)varc1;
	ReebArc *arc2 = (ReebArc*)varc2;
	
	if (arc1->v1->weight < arc2->v1->weight)
	{
		return -1;
	}
	if (arc1->v1->weight > arc2->v1->weight)
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

/****************************************** FILTERING **************************************************/

int compareArcs(void *varc1, void *varc2)
{
	ReebArc *arc1 = (ReebArc*)varc1;
	ReebArc *arc2 = (ReebArc*)varc2;
	float len1 = arc1->v2->weight - arc1->v1->weight;
	float len2 = arc2->v2->weight - arc2->v1->weight;
	
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

void filterArc(ReebGraph *rg, ReebNode *newNode, ReebNode *removedNode, ReebArc * srcArc, int merging)
{
	ReebArc *arc = NULL, *nextArc = NULL;

	/* first pass, merge buckets for arcs that spawned the two nodes into the source arc*/
	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
		if (arc->v1 == srcArc->v1 && arc->v2 == srcArc->v2 && arc != srcArc)
		{
			mergeArcBuckets(srcArc, arc, srcArc->v1->weight, srcArc->v2->weight);
		}
	}

	/* second pass, replace removedNode by newNode, remove arcs that are collapsed in a loop */
	arc = rg->arcs.first;
	while(arc)
	{
		nextArc = arc->next;
		
		if (arc->v1 == removedNode || arc->v2 == removedNode)
		{
			if (arc->v1 == removedNode)
			{
				arc->v1 = newNode;
			}
			else
			{
				arc->v2 = newNode;
			}

			// Remove looped arcs			
			if (arc->v1 == arc->v2)
			{
				// v1 or v2 was already newNode, since we're removing an arc, decrement degree
				newNode->degree--;
				
				// If it's safeArc, it'll be removed later, so keep it for now
				if (arc != srcArc)
				{
					BLI_remlink(&rg->arcs, arc);
					freeArc(arc);
				}
			}
			// Remove flipped arcs
			else if (arc->v1->weight > arc->v2->weight)
			{
				// Decrement degree from the other node
				OTHER_NODE(arc, newNode)->degree--;
				
				BLI_remlink(&rg->arcs, arc);
				freeArc(arc);
			}
			else
			{
				newNode->degree++; // incrementing degree since we're adding an arc

				if (merging)
				{
					// resize bucket list
					resizeArcBuckets(arc);
					mergeArcBuckets(arc, srcArc, arc->v1->weight, arc->v2->weight);
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
			ReebNode *newNode = arc->v1;
			ReebNode *removedNode = arc->v2;
			float blend;
			
			blend = (float)newNode->degree / (float)(newNode->degree + removedNode->degree); // blending factors
			
			//newNode->weight = FloatLerpf(newNode->weight, removedNode->weight, blend);
			VecLerpf(newNode->p, newNode->p, removedNode->p, blend);
			
			filterArc(rg, newNode, removedNode, arc, 0);

			// Reset nextArc, it might have changed
			nextArc = arc->next;
			
			BLI_remlink(&rg->arcs, arc);
			freeArc(arc);
			
			BLI_freelinkN(&rg->nodes, removedNode);
		}
		
		arc = nextArc;
	}
}

void filterInternalReebGraph(ReebGraph *rg, float threshold)
{
	ReebArc *arc = NULL, *nextArc = NULL;
	
	BLI_sortlist(&rg->arcs, compareArcs);

	arc = rg->arcs.first;
	while(arc)
	{
		nextArc = arc->next;

		// Only collapse non-terminal arcs that are shorter than threshold
		if ((arc->v1->degree > 1 && arc->v2->degree > 1 && arc->v2->weight - arc->v1->weight < threshold))
		{
			ReebNode *newNode = NULL;
			ReebNode *removedNode = NULL;
			
			/* Keep the node with the highestn number of connected arcs */
			if (arc->v1->degree >= arc->v2->degree)
			{
				newNode = arc->v1;
				removedNode = arc->v2;
			}
			else
			{
				newNode = arc->v2;
				removedNode = arc->v1;
			}
			
			filterArc(rg, newNode, removedNode, arc, 1);

			// Reset nextArc, it might have changed
			nextArc = arc->next;
			
			BLI_remlink(&rg->arcs, arc);
			freeArc(arc);
			
			BLI_freelinkN(&rg->nodes, removedNode);
		}
		
		arc = nextArc;
	}
}

void filterExternalReebGraph(ReebGraph *rg, float threshold)
{
	ReebArc *arc = NULL, *nextArc = NULL;
	
	BLI_sortlist(&rg->arcs, compareArcs);

	arc = rg->arcs.first;
	while(arc)
	{
		nextArc = arc->next;

		// Only collapse terminal arcs that are shorter than threshold
		if ((arc->v1->degree == 1 || arc->v2->degree == 1) && arc->v2->weight - arc->v1->weight < threshold)
		{
			ReebNode *terminalNode = NULL;
			ReebNode *middleNode = NULL;
			ReebNode *newNode = NULL;
			ReebNode *removedNode = NULL;
			int merging = 0;
			
			// Assign terminal and middle nodes
			if (arc->v1->degree == 1)
			{
				terminalNode = arc->v1;
				middleNode = arc->v2;
			}
			else
			{
				terminalNode = arc->v2;
				middleNode = arc->v1;
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
				newNode->degree--;
			}

			// Reset nextArc, it might have changed
			nextArc = arc->next;
			
			BLI_remlink(&rg->arcs, arc);
			freeArc(arc);
			
			BLI_freelinkN(&rg->nodes, removedNode);
		}
		
		arc = nextArc;
	}
}

/************************************** WEIGHT SPREADING ***********************************************/

int compareVerts( const void* a, const void* b )
{
	EditVert *va = *(EditVert**)a;
	EditVert *vb = *(EditVert**)b;
	int value = 0;
	
	if (va->tmp.fp < vb->tmp.fp)
	{
		value = -1;
	}
	else if (va->tmp.fp > vb->tmp.fp)
	{
		value = 1;
	}

	return value;		
}

void spreadWeight(EditMesh *em)
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
			
			if (i == 0 || (eve->tmp.fp - lastWeight) > FLT_EPSILON)
			{
				lastWeight = eve->tmp.fp;
			}
			else
			{
				work_needed = 1;
				eve->tmp.fp = lastWeight + FLT_EPSILON * 2;
				lastWeight = eve->tmp.fp;
			}
		}
	}
	
	MEM_freeN(verts);
}
/*********************************** GRAPH AS TREE FUNCTIONS *******************************************/

int subtreeDepth(ReebNode *node)
{
	int depth = 0;
	
	/* Base case, no arcs leading away */
	if (node->arcs == NULL || *(node->arcs) == NULL)
	{
		return 0;
	}
	else
	{
		ReebArc ** pArc;

		for(pArc = node->arcs; *pArc; pArc++)
		{
			ReebArc *arc = *pArc;
			
			/* only arcs that go down the tree */
			if (arc->v1 == node)
			{
				depth = MAX2(depth, subtreeDepth(arc->v2));
			}
		}
	}
	
	return depth + 1;
}

/*************************************** CYCLE DETECTION ***********************************************/

int detectCycle(ReebNode *node)
{
	int value = 0;
	
	if (node->flags != 0)
	{
		ReebArc ** pArc;

		for(pArc = node->arcs; *pArc && value == 0; pArc++)
		{
			ReebArc *arc = *pArc;
			
			value = detectCycle(OTHER_NODE(arc, node));
		}
	}
	else
	{
		value = 1;
	}
	
	return value;
}

int	isGraphAcyclic(ReebGraph *rg)
{
	ReebNode *node;
	int value = 0;
	
	/* NEED TO CHECK IF ADJACENCY LIST EXIST */
	
	/* Mark all nodes as not visited */
	for(node = rg->nodes.first; node; node = node->next)
	{
		node->flags = 0;
	}

	/* detectCycles in subgraphs */	
	for(node = rg->nodes.first; node && value == 0; node = node->next)
	{
		/* only for nodes in subgraphs that haven't been visited yet */
		if (node->flags == 0)
		{
			value = detectCycle(node);
		}		
	}
	
	return value;
}

/******************************************** EXPORT ***************************************************/

void exportNode(FILE *f, char *text, ReebNode *node)
{
	fprintf(f, "%s i:%i w:%f d:%i %f %f %f\n", text, node->index, node->weight, node->degree, node->p[0], node->p[1], node->p[2]);
}

void exportGraph(ReebGraph *rg, int count)
{
	ReebArc *arc;
	char filename[128];
	FILE *f;
	
	if (count == -1)
	{
		sprintf(filename, "test.txt");
	}
	else
	{
		sprintf(filename, "test%05i.txt", count);
	}
	f = fopen(filename, "w");

	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
		int i;
		
		exportNode(f, "v1", arc->v1);
		
		for(i = 0; i < arc->bcount; i++)
		{
			fprintf(f, "b nv:%i %f %f %f\n", arc->buckets[i].nv, arc->buckets[i].p[0], arc->buckets[i].p[1], arc->buckets[i].p[2]);
		}
		
		exportNode(f, "v2", arc->v2);
	}	
	
	fclose(f);
}

/***************************************** MAIN ALGORITHM **********************************************/

ReebArc * findConnectedArc(ReebGraph *rg, ReebArc *arc, ReebNode *v)
{
	ReebArc *nextArc = arc->next;
	
	for(nextArc = rg->arcs.first; nextArc; nextArc = nextArc->next)
	{
		if (arc != nextArc && (nextArc->v1 == v || nextArc->v2 == v))
		{
			break;
		}
	}
	
	return nextArc;
}

void removeNormalNodes(ReebGraph *rg)
{
	ReebArc *arc;
	
	// Merge degree 2 nodes
	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
		while (arc->v1->degree == 2 || arc->v2->degree == 2)
		{
			// merge at v1
			if (arc->v1->degree == 2)
			{
				ReebArc *nextArc = findConnectedArc(rg, arc, arc->v1);

				if (nextArc == NULL)
					printf("uhm1\n");
	
				// Merge arc only if needed
				if (arc->v1 == nextArc->v2)
				{				
					mergeConnectedArcs(rg, arc, nextArc);
				}
				// Otherwise, mark down vert
				else
				{
					arc->v1->degree = 3;
				}
			}
			
			// merge at v2
			if (arc->v2->degree == 2)
			{
				ReebArc *nextArc = findConnectedArc(rg, arc, arc->v2);
				
				if (nextArc == NULL) 
					printf("uhm %p\n", arc->v2);
					
				// Merge arc only if needed
				if (arc->v2 == nextArc->v1)
				{				
					mergeConnectedArcs(rg, arc, nextArc);
				}
				// Otherwise, mark down vert
				else
				{
					arc->v2->degree = 3;
				}
			}
		}
	}
	
}

int edgeEquals(ReebEdge *e1, ReebEdge *e2)
{
	return (e1->v1 == e2->v1 && e1->v2 == e2->v2);
}

ReebArc *nextArcMappedToEdge(ReebArc *arc, ReebEdge *e)
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

#if 0
	if (result == arc)
	{
		printf("WTF");
		getchar();
		exit(1);
	}
#endif

	return result;
}

typedef enum {
	MERGE_LOWER,
	MERGE_HIGHER,
	MERGE_APPEND
} MergeDirection;

void mergeArcEdges(ReebGraph *rg, ReebArc *aDst, ReebArc *aSrc, MergeDirection direction)
{
	ReebEdge *e = NULL;
	
	if (direction == MERGE_APPEND)
	{
		for(e = aSrc->edges.first; e; e = e->next)
		{
			e->arc = aDst; // Edge is stolen by new arc
		}
		
		addlisttolist(&aDst->edges , &aSrc->edges);
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
	
	mergeArcEdges(rg, a0, a1, MERGE_APPEND);
	
	// Bring a0 to the combine length of both arcs
	if (a0->v2 == a1->v1)
	{
		removedNode = a0->v2;
		a0->v2 = a1->v2;
	}
	else if (a0->v1 == a1->v2)
	{
		removedNode = a0->v1;
		a0->v1 = a1->v1;
	}
	
	resizeArcBuckets(a0);
	// Merge a1 in a0
	mergeArcBuckets(a0, a1, a0->v1->weight, a0->v2->weight);
	
	// remove a1 from graph
	BLI_remlink(&rg->arcs, a1);
	freeArc(a1);
	
	BLI_freelinkN(&rg->nodes, removedNode);
	result = 1;
	
	return result;
}
// return 1 on full merge
int mergeArcs(ReebGraph *rg, ReebArc *a0, ReebArc *a1)
{
	int result = 0;
	// TRIANGLE POINTS DOWN
	if (a0->v1->weight == a1->v1->weight) // heads are the same
	{
		if (a0->v2->weight == a1->v2->weight) // tails also the same, arcs can be totally merge together
		{
			mergeArcEdges(rg, a0, a1, MERGE_APPEND);
			
			mergeArcBuckets(a0, a1, a0->v1->weight, a0->v2->weight);
			
			// Adjust node degree
			a1->v1->degree--;
			a1->v2->degree--;
			
			// remove a1 from graph
			BLI_remlink(&rg->arcs, a1);
			
			freeArc(a1);
			result = 1;
		}
		else if (a0->v2->weight > a1->v2->weight) // a1->v2->weight is in the middle
		{
			mergeArcEdges(rg, a1, a0, MERGE_LOWER);

			// Adjust node degree
			a0->v1->degree--;
			a1->v2->degree++;
			
			mergeArcBuckets(a1, a0, a1->v1->weight, a1->v2->weight);
			a0->v1 = a1->v2;
			resizeArcBuckets(a0);
		}
		else // a0>n2 is in the middle
		{
			mergeArcEdges(rg, a0, a1, MERGE_LOWER);
			
			// Adjust node degree
			a1->v1->degree--;
			a0->v2->degree++;
			
			mergeArcBuckets(a0, a1, a0->v1->weight, a0->v2->weight);
			a1->v1 = a0->v2;
			resizeArcBuckets(a1);
		}
	}
	// TRIANGLE POINTS UP
	else if (a0->v2->weight == a1->v2->weight) // tails are the same
	{
		if (a0->v1->weight > a1->v1->weight) // a0->v1->weight is in the middle
		{
			mergeArcEdges(rg, a0, a1, MERGE_HIGHER);
			
			// Adjust node degree
			a1->v2->degree--;
			a0->v1->degree++;
			
			mergeArcBuckets(a0, a1, a0->v1->weight, a0->v2->weight);
			a1->v2 = a0->v1;
			resizeArcBuckets(a1);
		}
		else // a1->v1->weight is in the middle
		{
			mergeArcEdges(rg, a1, a0, MERGE_HIGHER);

			// Adjust node degree
			a0->v2->degree--;
			a1->v1->degree++;

			mergeArcBuckets(a1, a0, a1->v1->weight, a1->v2->weight);
			a0->v2 = a1->v1;
			resizeArcBuckets(a0);
		}
	}
	else
	{
		// Need something here (OR NOT)
	}
	
	return result;
}

void glueByMergeSort(ReebGraph *rg, ReebArc *a0, ReebArc *a1, ReebEdge *e0, ReebEdge *e1)
{
	int total = 0;
	while (total == 0 && a0 != a1 && a0 != NULL && a1 != NULL)
	{
		total = mergeArcs(rg, a0, a1);
		
		if (total == 0) // if it wasn't a total merge, go forward
		{
			if (a0->v2->weight < a1->v2->weight)
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

void mergePaths(ReebGraph *rg, ReebEdge *e0, ReebEdge *e1, ReebEdge *e2)
{
	ReebArc *a0, *a1, *a2;
	a0 = e0->arc;
	a1 = e1->arc;
	a2 = e2->arc;
	
	glueByMergeSort(rg, a0, a1, e0, e1);
	glueByMergeSort(rg, a0, a2, e0, e2);
} 

ReebNode * addNode(ReebGraph *rg, EditVert *eve, float weight)
{
	ReebNode *node = NULL;
	
	node = MEM_callocN(sizeof(ReebNode), "reeb node");
	
	node->flags = 0; // clear flags on init
	node->arcs = NULL;
	node->degree = 0;
	node->weight = weight;
	node->index = rg->totnodes;
	VECCOPY(node->p, eve->co);	
	
	BLI_addtail(&rg->nodes, node);
	rg->totnodes++;
	
	return node;
}

ReebEdge * createArc(ReebGraph *rg, ReebNode *node1, ReebNode *node2)
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
		
		arc->flags = 0; // clear flags on init
		
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
		
		arc->v1 = v1;
		arc->v2 = v2;
		
		// increase node degree
		v1->degree++;
		v2->degree++;

		BLI_edgehash_insert(rg->emap, node1->index, node2->index, edge);
		
		edge->arc = arc;
		edge->nextEdge = NULL;
		edge->v1 = v1;
		edge->v2 = v2;
		
		BLI_addtail(&rg->arcs, arc);
		BLI_addtail(&arc->edges, edge);
		
		/* adding buckets for embedding */
		allocArcBuckets(arc);
		
		offset = arc->v1->weight;
		len = arc->v2->weight - arc->v1->weight;

#if 0
		/* This is the actual embedding filling described in the paper
		 * the problem is that it only works with really dense meshes
		 */
		if (arc->bcount > 0)
		{
			addVertToBucket(&(arc->buckets[0]), arc->v1->co);
			addVertToBucket(&(arc->buckets[arc->bcount - 1]), arc->v2->co);
		}
#else
		for(i = 0; i < arc->bcount; i++)
		{
			float co[3];
			float f = (arc->buckets[i].val - offset) / len;
			
			VecLerpf(co, v1->p, v2->p, f);
			addVertToBucket(&(arc->buckets[i]), co);
		}
#endif

	}
	
	return edge;
}

void addTriangleToGraph(ReebGraph *rg, ReebNode * n1, ReebNode * n2, ReebNode * n3)
{
	ReebEdge *re1, *re2, *re3;
	ReebEdge *e1, *e2, *e3;
	float len1, len2, len3;
	
	re1 = createArc(rg, n1, n2);
	re2 = createArc(rg, n2, n3);
	re3 = createArc(rg, n3, n1);
	
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
	struct DynamicList * dlist;
	EditVert *eve;
	EditFace *efa;
	int index;
	int totvert;
	int totfaces;
	
#ifdef DEBUG_REEB
	int countfaces = 0;
#endif
 	
	rg = MEM_callocN(sizeof(ReebGraph), "reeb graph");
	
	rg->totnodes = 0;
	rg->emap = BLI_edgehash_new();
	
	totvert = BLI_countlist(&em->verts);
	totfaces = BLI_countlist(&em->faces);
	
	renormalizeWeight(em, 1.0f);
	
	/* Spread weight to minimize errors */
	spreadWeight(em);

	renormalizeWeight(em, (float)subdivisions);

	/* Adding vertice */
	for(index = 0, eve = em->verts.first; eve; index++, eve = eve->next)
	{
		eve->hash = index;
		eve->f2 = 0;
		eve->tmp.p = addNode(rg, eve, eve->tmp.fp);
	}
	
	/* Temporarely convert node list to dynamic list, for indexed access */
	dlist = BLI_dlist_from_listbase(&rg->nodes);
	
	/* Adding face, edge per edge */
	for(efa = em->faces.first; efa; efa = efa->next)
	{
		ReebNode *n1, *n2, *n3;
		
		n1 = (ReebNode*)BLI_dlist_find_link(dlist, efa->v1->hash);
		n2 = (ReebNode*)BLI_dlist_find_link(dlist, efa->v2->hash);
		n3 = (ReebNode*)BLI_dlist_find_link(dlist, efa->v3->hash);
		
		addTriangleToGraph(rg, n1, n2, n3);
		
		if (efa->v4)
		{
			ReebNode *n4 = (ReebNode*)efa->v4->tmp.p;
			addTriangleToGraph(rg, n1, n3, n4);
		}

#ifdef DEBUG_REEB
		countfaces++;
		if (countfaces % 100 == 0)
		{
			printf("face %i of %i\n", countfaces, totfaces);
		}
#endif
		
		
	}
	BLI_listbase_from_dlist(dlist, &rg->nodes);
	
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
	minimum = eve->tmp.fp;
	maximum = eve->tmp.fp;
	for(eve = em->verts.first; eve; eve = eve->next)
	{
		maximum = MAX2(maximum, eve->tmp.fp);
		minimum = MIN2(minimum, eve->tmp.fp);
	}
	
	range = maximum - minimum;

	/* Normalize weights */
	for(eve = em->verts.first; eve; eve = eve->next)
	{
		eve->tmp.fp = (eve->tmp.fp - minimum) / range * newmax;
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
		eve->tmp.fp = eve->co[axis];
	}

	return 1;
}

static float cotan_weight(float *v1, float *v2, float *v3)
{
	float a[3], b[3], c[3], clen;

	VecSubf(a, v2, v1);
	VecSubf(b, v3, v1);
	Crossf(c, a, b);

	clen = VecLength(c);

	if (clen == 0.0f)
		return 0.0f;
	
	return Inpf(a, b)/clen;
}

int weightToHarmonic(EditMesh *em)
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
		EditEdge *eed;
		int maximum = 1;
		int minimum = 1;
		
		eve->hash = index; /* Assign index to vertex */
		
		NextEdgeForVert(NULL, NULL); /* Reset next edge */
		for(eed = NextEdgeForVert(em, eve); eed && (maximum || minimum); eed = NextEdgeForVert(em, eve))
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
			
			/* Adjacent vertex is bigger, not a local maximum */
			if (eve2->tmp.fp > eve->tmp.fp)
			{
				maximum = 0;
			}
			/* Adjacent vertex is smaller, not a local minimum */
			else if (eve2->tmp.fp < eve->tmp.fp)
			{
				minimum = 0;
			}
		}
		
		if (maximum || minimum)
		{
			float w = eve->tmp.fp;
			eve->f1 = 0;
			nlSetVariable(0, index, w);
			nlLockVariable(index);
		}
		else
		{
			eve->f1 = 1;
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
		efa->e1->tmp.l++;
		efa->e2->tmp.l++;
		efa->e3->tmp.l++;
	}

	/* Add faces angle to the edge weight */
	for(efa = em->faces.first; efa; efa = efa->next)
	{
		/* Angle opposite e1 */
		float t1= cotan_weight(efa->v1->co, efa->v2->co, efa->v3->co) / efa->e2->tmp.l;
		
		/* Angle opposite e2 */
		float t2 = cotan_weight(efa->v2->co, efa->v3->co, efa->v1->co) / efa->e3->tmp.l;

		/* Angle opposite e3 */
		float t3 = cotan_weight(efa->v3->co, efa->v1->co, efa->v2->co) / efa->e1->tmp.l;
		
		int i1 = efa->v1->hash;
		int i2 = efa->v2->hash;
		int i3 = efa->v3->hash;
		
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
	
	nlEnd(NL_MATRIX);

	nlEnd(NL_SYSTEM);

	success = nlSolveAdvanced(NULL, NL_TRUE);

	if (success)
	{
		rval = 1;
		for(index = 0, eve = em->verts.first; eve; index++, eve = eve->next)
		{
			eve->tmp.fp = nlGetVariable(0, index);
		}
	}
	else
	{
		rval = 0;
	}

	nlDeleteContext(nlGetCurrent());

	return rval;
}


EditEdge * NextEdgeForVert(EditMesh *em, EditVert *v)
{
	static EditEdge *e = NULL;
	
	/* Reset method, call with NULL mesh pointer */
	if (em == NULL)
	{
		e = NULL;
		return NULL;
	}
	
	/* first pass, start at the head of the list */
	if (e == NULL)
	{
		e = em->edges.first;
	}
	/* subsequent passes, start on the next edge */
	else
	{
		e = e->next;
	}

	for( ; e ; e = e->next)
	{
		if (e->v1 == v || e->v2 == v)
		{
			break;
		}
	}	
	
	return e;
}

int weightFromDistance(EditMesh *em)
{
	EditVert *eve, *current_eve = NULL;
	int totedge = 0;
	int vCount = 0;
	
	if (em == NULL || BLI_countlist(&em->verts) == 0)
	{
		return 0;
	}
	
	totedge = BLI_countlist(&em->edges);
	
	if (totedge == 0)
	{
		return 0;
	}

	/* Initialize vertice flags and find selected vertex */
	for(eve = em->verts.first; eve; eve = eve->next)
	{
		eve->f1 = 0;
		if (current_eve == NULL && eve->f & SELECT)
		{
			current_eve = eve;
			eve->f1 = 1;
			vCount = 1;
		}
	}

	if (current_eve != NULL)
	{
		EditEdge *eed = NULL;
		EditEdge *select_eed = NULL;
		EditEdge **edges = NULL;
		float	 currentWeight = 0;
		int 	 eIndex = 0;
		
		edges = MEM_callocN(totedge * sizeof(EditEdge*), "Edges");
		
		/* Calculate edge weight and initialize edge flags */
		for(eed= em->edges.first; eed; eed= eed->next)
		{
			eed->tmp.fp = VecLenf(eed->v1->co, eed->v2->co);
			eed->f1 = 0;
		}
		
		do {
			int i;
			
			current_eve->f1 = 1; /* mark vertex as selected */
			
			/* Add all new edges connected to current_eve to the list */
			NextEdgeForVert(NULL, NULL); // Reset next edge
			for(eed = NextEdgeForVert(em, current_eve); eed; eed = NextEdgeForVert(em, current_eve))
			{ 
				if (eed->f1 == 0)
				{
					edges[eIndex] = eed;
					eed->f1 = 1;
					eIndex++;
				}
			}
			
			/* Find next shortest edge */
			select_eed = NULL;
			for(i = 0; i < eIndex; i++)
			{
				eed = edges[i];
				
				if (eed->f1 != 2 && (eed->v1->f1 == 0 || eed->v2->f1 == 0)) /* eed is not selected yet and leads to a new node */
				{
					float newWeight = 0;
					if (eed->v1->f1 == 1)
					{
						newWeight = eed->v1->tmp.fp + eed->tmp.fp;
					}
					else
					{
						newWeight = eed->v2->tmp.fp + eed->tmp.fp;
					}
					
					if (select_eed == NULL || newWeight < currentWeight) /* no selected edge or current smaller than selected */
					{
						currentWeight = newWeight;
						select_eed = eed;
					}
				}
			}
			
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
				current_eve->tmp.fp = currentWeight;
			}
		} while (select_eed != NULL);
		
		MEM_freeN(edges);
	}

	return 1;
}

MCol MColFromWeight(EditVert *eve)
{
	MCol col;
	col.a = 255;
	col.b = (char)(eve->tmp.fp * 255);
	col.g = 0;
	col.r = (char)((1.0f - eve->tmp.fp) * 255);
	return col;
}

void weightToVCol(EditMesh *em)
{
	EditFace *efa;
	MCol *mcol;
	if (!EM_vertColorCheck()) {
		return;
	}
	
	for(efa=em->faces.first; efa; efa=efa->next) {
		mcol = CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
				
		mcol[0] = MColFromWeight(efa->v1);
		mcol[1] = MColFromWeight(efa->v2);
		mcol[2] = MColFromWeight(efa->v3);

		if(efa->v4) {
			mcol[3] = MColFromWeight(efa->v4);
		}
	}
}

/****************************************** BUCKET ITERATOR **************************************************/

void initArcIterator(ReebArcIterator *iter, ReebArc *arc, ReebNode *head)
{
	iter->arc = arc;
	
	if (head == arc->v1)
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
	
	iter->index = iter->start - iter->stride;
}

void initArcIterator2(ReebArcIterator *iter, ReebArc *arc, int start, int end)
{
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

	iter->index = iter->start - iter->stride;
}

EmbedBucket * nextBucket(ReebArcIterator *iter)
{
	EmbedBucket *result = NULL;
	
	if (iter->index != iter->end)
	{
		iter->index += iter->stride;
		result = &(iter->arc->buckets[iter->index]);
	}
	
	return result;
}
