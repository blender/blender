/* ***************************************
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****



    node.c	nov/dec 1992
    			may 1999

    $Id$

 *************************************** */

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BIF_toolbox.h"

#include "radio.h"

#include "BLO_sys_types.h" // for intptr_t support

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* locals */
void *malloc_fast(int size);
void *calloc_fast(int size);
void free_fast(void *poin, int siz);
void deleteTriNodes(RNode *node);
/* lower because of local type define */
/*  void check_mallocgroup(MallocGroup *mg); */


/* ********** fastmalloc ************** */

#define MAL_GROUPSIZE	256
#define MAL_AVAILABLE	1
#define MAL_FULL		2




ListBase MallocBase= {0, 0};
int totfastmem= 0;

typedef struct MallocGroup {
	struct MallocGroup *next, *prev;
	short size, flag;
	short curfree, tot;
	char flags[MAL_GROUPSIZE];
	char *data;
} MallocGroup;

/* one more local */
void check_mallocgroup(MallocGroup *mg);

void check_mallocgroup(MallocGroup *mg)
{
	int a;
	char *cp;
	
	if(mg->tot==MAL_GROUPSIZE) {
		mg->flag= MAL_FULL;
		return;
	}
	
	cp= mg->flags;
	
	if(mg->curfree<MAL_GROUPSIZE-1) {
		if(cp[mg->curfree+1]==0) {
			mg->curfree++;
			return;
		}
	}
	if(mg->curfree>0) {
		if(cp[mg->curfree-1]==0) {
			mg->curfree--;
			return;
		}
	}

	for(a=0; a<MAL_GROUPSIZE; a++) {
		if(cp[a]==0) {
			mg->curfree= a;
			return;
		}
	}
	printf("fastmalloc: shouldnt be here\n");
}

void *malloc_fast(int size)
{
	MallocGroup *mg;
	void *retval;
	
	mg= MallocBase.last;
	while(mg) {
		if(mg->size==size) {
			if(mg->flag & MAL_AVAILABLE) {
				mg->flags[mg->curfree]= 1;
				mg->tot++;
				retval= mg->data+mg->curfree*mg->size;
				check_mallocgroup(mg);
				return retval;
			}
		}
		mg= mg->prev;
	}

	/* no free block found */
	mg= MEM_callocN(sizeof(MallocGroup), "mallocgroup");
	BLI_addtail(&MallocBase, mg);
	mg->data= MEM_mallocN(MAL_GROUPSIZE*size, "mallocgroupdata");
	mg->flag= MAL_AVAILABLE;
	mg->flags[0]= 1;
	mg->curfree= 1;
	mg->size= size;
	mg->tot= 1;
	
	totfastmem+= sizeof(MallocGroup)+MAL_GROUPSIZE*size;

	return mg->data;
}

void *calloc_fast(int size)
{
	void *poin;
	
	poin= malloc_fast(size);
	memset(poin, 0, size);
	
	return poin;
}

void free_fast(void *poin, int size)
{
	MallocGroup *mg;
	intptr_t val;

	mg= MallocBase.last;
	while(mg) {
		if(mg->size==size) {
			if( ((intptr_t)poin) >= ((intptr_t)mg->data) ) {
				if( ((intptr_t)poin) < ((intptr_t)(mg->data+MAL_GROUPSIZE*size)) ) {
					val= ((intptr_t)poin) - ((intptr_t)mg->data);
					val/= size;
					mg->curfree= val;
					mg->flags[val]= 0;
					mg->flag= MAL_AVAILABLE;
					
					mg->tot--;
					if(mg->tot==0) {
						BLI_remlink(&MallocBase, mg);
						MEM_freeN(mg->data);
						MEM_freeN(mg);
						totfastmem-= sizeof(MallocGroup)+MAL_GROUPSIZE*size;
					}
					return;
				}
			}
		}
		mg= mg->prev;
	}
	printf("fast free: pointer not in memlist %p size %d\n",
		   poin, size);
}

/* security: only one function in a time can use it */
static char *fastmallocstr= 0;

void free_fastAll()
{
	MallocGroup *mg;
	
	mg= MallocBase.first;
	while(mg) {
		BLI_remlink(&MallocBase, mg);
		MEM_freeN(mg->data);
		MEM_freeN(mg);
		mg= MallocBase.first;
	}
	totfastmem= 0;
	fastmallocstr= 0;
}

void start_fastmalloc(char *str)
{
	if(fastmallocstr) {
		error("Fastmalloc in use: %s", fastmallocstr);
		return;
	}
	fastmallocstr= str;
}

/* **************************************** */

float nodelimit;

void setnodelimit(float limit)
{
	nodelimit= limit;

}

/* ************  memory management ***********  */

int Ntotvert=0, Ntotnode=0, Ntotpatch=0;

float *mallocVert()
{
	Ntotvert++;
	return (float *)malloc_fast(16);
}

float *callocVert()
{
	Ntotvert++;
	return (float *)calloc_fast(16);
}

void freeVert(float *vert)
{
	free_fast(vert, 16);
	Ntotvert--;
}

int totalRadVert()
{
	return Ntotvert;
}

RNode *mallocNode()
{
	Ntotnode++;
	return (RNode *)malloc_fast(sizeof(RNode));
}

RNode *callocNode()
{
	Ntotnode++;
	return (RNode *)calloc_fast(sizeof(RNode));
}

void freeNode(RNode *node)
{
	free_fast(node, sizeof(RNode));
	Ntotnode--;
}

void freeNode_recurs(RNode *node)
{

	if(node->down1) {
		freeNode_recurs(node->down1);
		freeNode_recurs(node->down2);
	}

	node->down1= node->down2= 0;
	freeNode(node);

}

RPatch *mallocPatch()
{
	Ntotpatch++;
	return (RPatch *)malloc_fast(sizeof(RPatch));
}

RPatch *callocPatch()
{
	Ntotpatch++;
	return (RPatch *)calloc_fast(sizeof(RPatch));
}

void freePatch(RPatch *patch)
{
	free_fast(patch, sizeof(RPatch));
	Ntotpatch--;
}

/* ************  SUBDIVIDE  ***********  */


void replaceAllNode(RNode *neighb, RNode *newn)
{
	/* changes from all neighbours the edgepointers that point to newn->up in new */
	int ok= 0;
	
	
	if(neighb==0) return;
	if(newn->up==0) return;
	
	if(neighb->ed1==newn->up) {
		neighb->ed1= newn;
		ok= 1;
	}
	else if(neighb->ed2==newn->up) {
		neighb->ed2= newn;
		ok= 1;
	}
	else if(neighb->ed3==newn->up) {
		neighb->ed3= newn;
		ok= 1;
	}
	else if(neighb->ed4==newn->up) {
		neighb->ed4= newn;
		ok= 1;
	}
	
	if(ok && neighb->down1) {
		replaceAllNode(neighb->down1, newn);
		replaceAllNode(neighb->down2, newn);
	}
}

void replaceAllNodeInv(RNode *neighb, RNode *old)
{
	/* changes from all neighbours the edgepointers that point to old in old->up */
	if(neighb==0) return;
	if(old->up==0) return;
	
	if(neighb->ed1==old) {
		neighb->ed1= old->up;
	}
	else if(neighb->ed2==old) {
		neighb->ed2= old->up;
	}
	else if(neighb->ed3==old) {
		neighb->ed3= old->up;
	}
	else if(neighb->ed4==old) {
		neighb->ed4= old->up;
	}
	
	if(neighb->down1) {
		replaceAllNodeInv(neighb->down1, old);
		replaceAllNodeInv(neighb->down2, old);
	}
}

void replaceAllNodeUp(RNode *neighb, RNode *old)
{
	/* changes from all neighbours the edgepointers that point to old in old->up */
	if(neighb==0) return;
	if(old->up==0) return;
	neighb= neighb->up;
	if(neighb==0) return;
	
	if(neighb->ed1==old) {
		neighb->ed1= old->up;
	}
	else if(neighb->ed2==old) {
		neighb->ed2= old->up;
	}
	else if(neighb->ed3==old) {
		neighb->ed3= old->up;
	}
	else if(neighb->ed4==old) {
		neighb->ed4= old->up;
	}
	
	if(neighb->up) {
		replaceAllNodeUp(neighb, old);
	}
}


void replaceTestNode(RNode *neighb, RNode **edpp, RNode *newn, int level, float *vert)
{
	/*	IF neighb->ed points to newn->up
	 *		IF edgelevels equal
				IF testvert is in neighb->ed
					change pointers both ways
				ELSE
					RETURN
			ELSE
				IF neighb edgelevel is deeper
					change neighb pointer
		
	 */
	int ok= 0;
	
	if(neighb==0) return;
	if(newn->up==0) return;
	
	if(neighb->ed1==newn->up) {
		if(neighb->lev1==level) {
			if(vert==neighb->v1 || vert==neighb->v2) {
				*edpp= neighb;
				neighb->ed1= newn;
			}
			else return;
		}
		else if(neighb->lev1>level) {
			neighb->ed1= newn;
		}
		ok= 1;
	}
	else if(neighb->ed2==newn->up) {
		if(neighb->lev2==level) {
			if(vert==neighb->v2 || vert==neighb->v3) {
				*edpp= neighb;
				neighb->ed2= newn;
			}
			else return;
		}
		else if(neighb->lev2>level) {
			neighb->ed2= newn;
		}
		ok= 1;
	}
	else if(neighb->ed3==newn->up) {
		if(neighb->lev3==level) {
			if(neighb->type==3) {
				if(vert==neighb->v3 || vert==neighb->v1) {
					*edpp= neighb;
					neighb->ed3= newn;
				}
				else return;
			}
			else {
				if(vert==neighb->v3 || vert==neighb->v4) {
					*edpp= neighb;
					neighb->ed3= newn;
				}
				else return;
			}
		}
		else if(neighb->lev3>level) {
			neighb->ed3= newn;
		}
		ok= 1;
	}
	else if(neighb->ed4==newn->up) {
		if(neighb->lev4==level) {
			if(vert==neighb->v4 || vert==neighb->v1) {
				*edpp= neighb;
				neighb->ed4= newn;
			}
			else return;
		}
		else if(neighb->lev4>level) {
			neighb->ed4= newn;
		}
		ok= 1;
	}
	
	if(ok && neighb->down1) {
		replaceTestNode(neighb->down1, edpp, newn, level, vert);
		replaceTestNode(neighb->down2, edpp, newn, level, vert);
	}
	
}

int setvertexpointersNode(RNode *neighb, RNode *node, int level, float **v1, float **v2)
{
	/* compares edgelevels , if equal it sets the vertexpointers */
	
	if(neighb==0) return 0;
	
	if(neighb->ed1==node) {
		if(neighb->lev1==level) {
			*v1= neighb->v1;
			*v2= neighb->v2;
			return 1;
		}
	}
	else if(neighb->ed2==node) {
		if(neighb->lev2==level) {
			*v1= neighb->v2;
			*v2= neighb->v3;
			return 1;
		}
	}
	else if(neighb->ed3==node) {
		if(neighb->lev3==level) {
			if(neighb->type==3) {
				*v1= neighb->v3;
				*v2= neighb->v1;
			}
			else {
				*v1= neighb->v3;
				*v2= neighb->v4;
			}
			return 1;
		}
	}
	else if(neighb->ed4==node) {
		if(neighb->lev4==level) {
			*v1= neighb->v4;
			*v2= neighb->v1;
			return 1;
		}
	}
	return 0;
}

float edlen(float *v1, float *v2)
{
	return (v1[0]-v2[0])*(v1[0]-v2[0])+ (v1[1]-v2[1])*(v1[1]-v2[1])+ (v1[2]-v2[2])*(v1[2]-v2[2]);
}


void subdivideTriNode(RNode *node, RNode *edge)
{
	RNode *n1, *n2, *up;
	float fu, fv, fl, *v1, *v2; /*  , AreaT3Dfl(); ... from arithb... */
	int uvl;
	
	if(node->down1 || node->down2) {
		/* printf("trinode: subd already done\n"); */
		return;
	}
	
	/* defines subdivide direction */

	if(edge==0) {
		/* areathreshold */
		if(node->area<nodelimit) return;

		fu= edlen(node->v1, node->v2);
		fv= edlen(node->v2, node->v3);
		fl= edlen(node->v3, node->v1);

		if(fu>fv && fu>fl) uvl= 1;
		else if(fv>fu && fv>fl) uvl= 2;
		else uvl= 3;
	}
	else {
		
		if(edge==node->ed1) uvl= 1;
		else if(edge==node->ed2) uvl= 2;
		else uvl= 3;
	}
	
	/*  should neighbour nodes be deeper? Recursive! */
	n1= 0; 
	if(uvl==1) {
		if(node->ed1 && node->ed1->down1==0) n1= node->ed1;
	}
	else if(uvl==2) {
		if(node->ed2 && node->ed2->down1==0) n1= node->ed2;
	}
	else {
		if(node->ed3 && node->ed3->down1==0) n1= node->ed3;
	}
	if(n1) {
		up= node->up;
		while(up) {							/* also test for ed4 !!! */
			if(n1->ed1==up || n1->ed2==up || n1->ed3==up || n1->ed4==up) {
				subdivideNode(n1, up);
				break;
			}
			up= up->up;
		}
	}
	
	/* the subdividing */
	n1= mallocNode();
	memcpy(n1, node, sizeof(RNode));
	n2= mallocNode();
	memcpy(n2, node, sizeof(RNode));

	n1->up= node;
	n2->up= node;
	
	node->down1= n1;
	node->down2= n2;

	/* subdivide edge 1 */
	if(uvl==1) {
	
		/* FIRST NODE  gets edge 2 */
		n1->ed3= n2;
		n1->lev3= 0;
		replaceAllNode(n1->ed2, n1);
		n1->lev1++;
		replaceTestNode(n1->ed1, &(n1->ed1), n1, n1->lev1, n1->v2);

		/* SECOND NODE  gets edge 3 */
		n2->ed2= n1;
		n2->lev2= 0;
		replaceAllNode(n2->ed3, n2);
		n2->lev1++;
		replaceTestNode(n2->ed1, &(n2->ed1), n2, n2->lev1, n2->v1);
		
		/* NEW VERTEX from edge 1 */
		if( setvertexpointersNode(n1->ed1, n1, n1->lev1, &v1, &v2) ) {	/* nodes have equal levels */
			if(v1== n1->v2) {
				n1->v1= v2;
				n2->v2= v2;
			}
			else {
				n1->v1= v1;
				n2->v2= v1;
			}
		}
		else {
			n1->v1= n2->v2= mallocVert();
			n1->v1[0]= 0.5*(node->v1[0]+ node->v2[0]);
			n1->v1[1]= 0.5*(node->v1[1]+ node->v2[1]);
			n1->v1[2]= 0.5*(node->v1[2]+ node->v2[2]);
			n1->v1[3]= node->v1[3];	/* color */
		}
	}
	else if(uvl==2) {
	
		/* FIRST NODE gets edge 1 */
		n1->ed3= n2;
		n1->lev3= 0;
		replaceAllNode(n1->ed1, n1);
		n1->lev2++;
		replaceTestNode(n1->ed2, &(n1->ed2), n1, n1->lev2, n1->v2);

		/* SECOND NODE gets edge 3 */
		n2->ed1= n1;
		n2->lev1= 0;
		replaceAllNode(n2->ed3, n2);
		n2->lev2++;
		replaceTestNode(n2->ed2, &(n2->ed2), n2, n2->lev2, n2->v3);
		
		/* NEW VERTEX from edge 2 */
		if( setvertexpointersNode(n1->ed2, n1, n1->lev2, &v1, &v2) ) {	/* nodes have equal levels */
			if(v1== n1->v2) {
				n1->v3= v2;
				n2->v2= v2;
			}
			else {
				n1->v3= v1;
				n2->v2= v1;
			}
		}
		else {
			n1->v3= n2->v2= mallocVert();
			n1->v3[0]= 0.5*(node->v2[0]+ node->v3[0]);
			n1->v3[1]= 0.5*(node->v2[1]+ node->v3[1]);
			n1->v3[2]= 0.5*(node->v2[2]+ node->v3[2]);
			n1->v3[3]= node->v1[3];	/* color */
		}
	}
	else if(uvl==3) {
	
		/* FIRST NODE gets edge 1 */
		n1->ed2= n2;
		n1->lev2= 0;
		replaceAllNode(n1->ed1, n1);
		n1->lev3++;
		replaceTestNode(n1->ed3, &(n1->ed3), n1, n1->lev3, n1->v1);

		/* SECOND NODE gets edge 2 */
		n2->ed1= n1;
		n2->lev1= 0;
		replaceAllNode(n2->ed2, n2);
		n2->lev3++;
		replaceTestNode(n2->ed3, &(n2->ed3), n2, n2->lev3, n2->v3);
		
		/* NEW VERTEX from edge 3 */
		if( setvertexpointersNode(n1->ed3, n1, n1->lev3, &v1, &v2) ) {	/* nodes have equal levels */
			if(v1== n1->v1) {
				n1->v3= v2;
				n2->v1= v2;
			}
			else {
				n1->v3= v1;
				n2->v1= v1;
			}
		}
		else {
			n1->v3= n2->v1= mallocVert();
			n1->v3[0]= 0.5*(node->v1[0]+ node->v3[0]);
			n1->v3[1]= 0.5*(node->v1[1]+ node->v3[1]);
			n1->v3[2]= 0.5*(node->v1[2]+ node->v3[2]);
			n1->v3[3]= node->v3[3];	/* color */
		}
	}
	n1->area= AreaT3Dfl(n1->v1, n1->v2, n1->v3);
	n2->area= AreaT3Dfl(n2->v1, n2->v2, n2->v3);

}


void subdivideNode(RNode *node, RNode *edge)
{
	RNode *n1, *n2, *up;
	float fu, fv, *v1, *v2;/*, AreaQ3Dfl(); ... from arithb... */
	int uvl;
	
	if(Ntotnode>RG.maxnode) return;
	
	if(node->type==3) {
		subdivideTriNode(node, edge);
		return;
	}

	if(node->down1 || node->down2) {
		/* printf("subdivide Node: already done \n"); */
		return;
	}
	
	/* defines subdivide direction */

	if(edge==0) {
		/* areathreshold */
		if(node->area<nodelimit) {
			return;
		}
		fu= fabs(node->v1[0]- node->v2[0])+ fabs(node->v1[1]- node->v2[1]) +fabs(node->v1[2]- node->v2[2]);
		fv= fabs(node->v1[0]- node->v4[0])+ fabs(node->v1[1]- node->v4[1]) +fabs(node->v1[2]- node->v4[2]);
		if(fu>fv) uvl= 1;
		else uvl= 2;
	}
	else {
		if(edge==node->ed1 || edge==node->ed3) uvl= 1;
		else uvl= 2;
	}
	
	/*  do neighbour nodes have to be deeper? Recursive! */
	n1= n2= 0; 
	if(uvl==1) {
		if(node->ed1 && node->ed1->down1==0) n1= node->ed1;
		if(node->ed3 && node->ed3->down1==0) n2= node->ed3;
	}
	else {
		if(node->ed2 && node->ed2->down1==0) n1= node->ed2;
		if(node->ed4 && node->ed4->down1==0) n2= node->ed4;
	}
	if(n1) {
		up= node->up;
		while(up) {
			if(n1->ed1==up || n1->ed2==up || n1->ed3==up || n1->ed4==up) {
				/* printf("recurs subd\n"); */
				subdivideNode(n1, up);
				break;
			}
			up= up->up;
		}
	}
	if(n2) {
		up= node->up;
		while(up) {
			if(n2->ed1==up || n2->ed2==up || n2->ed3==up || n2->ed4==up) {
				/* printf("recurs subd\n"); */
				subdivideNode(n2, up);
				break;
			}
			up= up->up;
		}
	}

	/* the subdividing */
	n1= mallocNode();
	memcpy(n1, node, sizeof(RNode));
	n2= mallocNode();
	memcpy(n2, node, sizeof(RNode));

	n1->up= node;
	n2->up= node;
	
	node->down1= n1;
	node->down2= n2;

	/* subdivide edge 1 and 3 */
	if(uvl==1) {
		
		/* FIRST NODE  gets edge 2 */
		n1->ed4= n2;
		n1->lev4= 0;
		replaceAllNode(n1->ed2, n1);
		n1->lev1++;
		n1->lev3++;
		replaceTestNode(n1->ed1, &(n1->ed1), n1, n1->lev1, n1->v2);
		replaceTestNode(n1->ed3, &(n1->ed3), n1, n1->lev3, n1->v3);

		/* SECOND NODE gets edge 4 */
		n2->ed2= n1;
		n2->lev2= 0;
		replaceAllNode(n2->ed4, n2);
		n2->lev1++;
		n2->lev3++;
		replaceTestNode(n2->ed1, &(n2->ed1), n2, n2->lev1, n2->v1);
		replaceTestNode(n2->ed3, &(n2->ed3), n2, n2->lev3, n2->v4);
		
		/* NEW VERTEX from edge 1 */
		if( setvertexpointersNode(n1->ed1, n1, n1->lev1, &v1, &v2) ) {	/* nodes have equal levels */
			if(v1== n1->v2) {
				n1->v1= v2;
				n2->v2= v2;
			}
			else {
				n1->v1= v1;
				n2->v2= v1;
			}
		}
		else {
			n1->v1= n2->v2= mallocVert();
			n1->v1[0]= 0.5*(node->v1[0]+ node->v2[0]);
			n1->v1[1]= 0.5*(node->v1[1]+ node->v2[1]);
			n1->v1[2]= 0.5*(node->v1[2]+ node->v2[2]);
			n1->v1[3]= node->v1[3];	/* color */
		}
		
		/* NEW VERTEX from edge 3 */
		if( setvertexpointersNode(n1->ed3, n1, n1->lev3, &v1, &v2) ) {	/* nodes have equal levels */
			if(v1== n1->v3) {
				n1->v4= v2;
				n2->v3= v2;
			}
			else {
				n1->v4= v1;
				n2->v3= v1;
			}
		}
		else {
			n1->v4= n2->v3= mallocVert();
			n1->v4[0]= 0.5*(node->v3[0]+ node->v4[0]);
			n1->v4[1]= 0.5*(node->v3[1]+ node->v4[1]);
			n1->v4[2]= 0.5*(node->v3[2]+ node->v4[2]);
			n1->v4[3]= node->v4[3];	/* color */
		}
	}
	/* subdivide edge 2 and 4 */
	else if(uvl==2) {
		
		/* FIRST NODE gets edge 1 */
		n1->ed3= n2;
		n1->lev3= 0;
		replaceAllNode(n1->ed1, n1);
		n1->lev2++;
		n1->lev4++;
		replaceTestNode(n1->ed2, &(n1->ed2), n1, n1->lev2, n1->v2);
		replaceTestNode(n1->ed4, &(n1->ed4), n1, n1->lev4, n1->v1);

		/* SECOND NODE gets edge 3 */
		n2->ed1= n1;
		n2->lev1= 0;
		replaceAllNode(n2->ed3, n2);
		n2->lev2++;
		n2->lev4++;
		replaceTestNode(n2->ed2, &(n2->ed2), n2, n2->lev2, n2->v3);
		replaceTestNode(n2->ed4, &(n2->ed4), n2, n2->lev4, n2->v4);

		/* NEW VERTEX from edge 2 */
		if( setvertexpointersNode(n1->ed2, n1, n1->lev2, &v1, &v2) ) {	/* nodes have equal levels */
			if(v1== n1->v2) {
				n1->v3= v2;
				n2->v2= v2;
			}
			else {
				n1->v3= v1;
				n2->v2= v1;
			}
		}
		else {
			n1->v3= n2->v2= mallocVert();
			n1->v3[0]= 0.5*(node->v2[0]+ node->v3[0]);
			n1->v3[1]= 0.5*(node->v2[1]+ node->v3[1]);
			n1->v3[2]= 0.5*(node->v2[2]+ node->v3[2]);
			n1->v3[3]= node->v3[3];	/* color */
		}

		/* NEW VERTEX from edge 4 */
		if( setvertexpointersNode(n1->ed4, n1, n1->lev4, &v1, &v2) ) {	/* nodes have equal levels */
			if(v1== n1->v1) {
				n1->v4= v2;
				n2->v1= v2;
			}
			else {
				n1->v4= v1;
				n2->v1= v1;
			}
		}
		else {
			n1->v4= n2->v1= mallocVert();
			n1->v4[0]= 0.5*(node->v1[0]+ node->v4[0]);
			n1->v4[1]= 0.5*(node->v1[1]+ node->v4[1]);
			n1->v4[2]= 0.5*(node->v1[2]+ node->v4[2]);
			n1->v4[3]= node->v4[3];	/* color */
		}
	}
	
	n1->area= AreaQ3Dfl(n1->v1, n1->v2, n1->v3, n1->v4);
	n2->area= AreaQ3Dfl(n2->v1, n2->v2, n2->v3, n2->v4);

}

int comparelevel(RNode *node, RNode *nb, int level)
{
	/* recursive descent: test with deepest node */
	/* return 1 means equal or higher */
	
	if(nb==0) return 1;
	
	if(nb->down1) {
		return 0;
		
		/*		THERE IS AN ERROR HERE, BUT WHAT?  (without this function the system
		        works too, but is slower) (ton) */

		/*
		n1= nb->down1;
		if(n1->ed1==node) return comparelevel(node, n1, level);
		if(n1->ed2==node) return comparelevel(node, n1, level);
		if(n1->ed3==node) return comparelevel(node, n1, level);
		if(n1->ed4==node) return comparelevel(node, n1, level);
		n1= nb->down2;
		if(n1->ed1==node) return comparelevel(node, n1, level);
		if(n1->ed2==node) return comparelevel(node, n1, level);
		if(n1->ed3==node) return comparelevel(node, n1, level);
		if(n1->ed4==node) return comparelevel(node, n1, level);
		printf(" dit kan niet ");
		return 0;
		*/
		
	}
	
	if(nb->down1==0) {
		/* if(nb->ed1==node) return (nb->lev1<=level); */
		/* if(nb->ed2==node) return (nb->lev2<=level); */
		/* if(nb->ed3==node) return (nb->lev3<=level); */
		/* if(nb->ed4==node) return (nb->lev4<=level); */
		
		return 1;	/* is higher node */
	}
	return 1;
}

void deleteTriNodes(RNode *node) 	/* both children of node */
{
	RNode *n1, *n2;
	
	/* if neighbour nodes are deeper: no delete */
	/* just test 2 nodes, from the others the level doesn't change */
	
	n1= node->down1;
	n2= node->down2;

	if(n1==0 || n2==0) return;
	
	if(n1->down1 || n2->down1) return;
	
	/* at the edges no subdivided node is allowed */

	if(n1->ed1 && n1->ed1->down1) return;
	if(n1->ed2 && n1->ed2->down1) return;
	if(n1->ed3 && n1->ed3->down1) return;

	if(n2->ed1 && n2->ed1->down1) return;
	if(n2->ed2 && n2->ed2->down1) return;
	if(n2->ed3 && n2->ed3->down1) return;
				
	replaceAllNodeInv(n1->ed1, n1);
	replaceAllNodeInv(n1->ed2, n1);
	replaceAllNodeInv(n1->ed3, n1);

	replaceAllNodeUp(n1->ed1, n1);
	replaceAllNodeUp(n1->ed2, n1);
	replaceAllNodeUp(n1->ed3, n1);

	replaceAllNodeInv(n2->ed1, n2);
	replaceAllNodeInv(n2->ed2, n2);
	replaceAllNodeInv(n2->ed3, n2);

	replaceAllNodeUp(n2->ed1, n2);
	replaceAllNodeUp(n2->ed2, n2);
	replaceAllNodeUp(n2->ed3, n2);

	n1->down1= (RNode *)12;	/* for debug */
	n2->down1= (RNode *)12;
	
	freeNode(n1);
	freeNode(n2);
	node->down1= node->down2= 0;
	
}

	/* both children of node */
void deleteNodes(RNode *node)
{
	RNode *n1, *n2;

	/* if neighbour nodes are deeper: no delete */
	/* just test 2 nodes, from the others the level doesn't change */

	if(node->type==3) {
		deleteTriNodes(node);
		return;
	}
	
	n1= node->down1;
	n2= node->down2;

	if(n1==0 || n2==0) return;
	
	if(n1->down1 || n2->down1) return;
	
	if(n1->ed3==n2) {

		/* at the edges no subdivided node is allowed */

		if(n1->ed1 && n1->ed1->down1) return;
		if(n1->ed2 && n1->ed2->down1) return;
		if(n1->ed4 && n1->ed4->down1) return;

		if(n2->ed2 && n2->ed2->down1) return;
		if(n2->ed3 && n2->ed3->down1) return;
		if(n2->ed4 && n2->ed4->down1) return;
					
		replaceAllNodeInv(n1->ed1, n1);
		replaceAllNodeInv(n1->ed2, n1);
		replaceAllNodeInv(n1->ed4, n1);

		replaceAllNodeUp(n1->ed1, n1);
		replaceAllNodeUp(n1->ed2, n1);
		replaceAllNodeUp(n1->ed4, n1);

		replaceAllNodeInv(n2->ed2, n2);
		replaceAllNodeInv(n2->ed3, n2);
		replaceAllNodeInv(n2->ed4, n2);

		replaceAllNodeUp(n2->ed2, n2);
		replaceAllNodeUp(n2->ed3, n2);
		replaceAllNodeUp(n2->ed4, n2);

		n1->down1= (RNode *)12;	/* for debug */
		n2->down1= (RNode *)12;
		
		freeNode(n1);
		freeNode(n2);
		node->down1= node->down2= 0;
		
		return;
	}
	else if(n1->ed4==n2) {

		if(n1->ed1 && n1->ed1->down1) return;
		if(n1->ed2 && n1->ed2->down1) return;
		if(n1->ed3 && n1->ed3->down1) return;

		if(n2->ed1 && n2->ed1->down1) return;
		if(n2->ed3 && n2->ed3->down1) return;
		if(n2->ed4 && n2->ed4->down1) return;
					
		replaceAllNodeInv(n1->ed1, n1);
		replaceAllNodeInv(n1->ed2, n1);
		replaceAllNodeInv(n1->ed3, n1);

		replaceAllNodeUp(n1->ed1, n1);
		replaceAllNodeUp(n1->ed2, n1);
		replaceAllNodeUp(n1->ed3, n1);

		replaceAllNodeInv(n2->ed1, n2);
		replaceAllNodeInv(n2->ed3, n2);
		replaceAllNodeInv(n2->ed4, n2);

		replaceAllNodeUp(n2->ed1, n2);
		replaceAllNodeUp(n2->ed3, n2);
		replaceAllNodeUp(n2->ed4, n2);

		n1->down1= (RNode *)12;	/* for debug */
		n2->down1= (RNode *)12;
		
		freeNode(n1);
		freeNode(n2);
		node->down1= node->down2= 0;
		
		return;
	}

}


