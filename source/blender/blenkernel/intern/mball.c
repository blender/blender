/** mball.c
 *  
 * MetaBalls are created from a single Object (with a name without number in it),
 * here the DispList and BoundBox also is located.
 * All objects with the same name (but with a number in it) are added to this.
 *  
 * texture coordinates are patched within the displist
 * 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jiri Hnidek <jiri.hnidek@vslib.cz>.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"


#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_utildefines.h"
#include "BKE_bad_level_calls.h"

#include "BKE_global.h"
#include "BKE_main.h"

/*  #include "BKE_object.h" */
#include "BKE_scene.h"
#include "BKE_blender.h"
#include "BKE_library.h"
#include "BKE_displist.h"
#include "BKE_mball.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Functions */

void unlink_mball(MetaBall *mb)
{
	int a;
	
	for(a=0; a<mb->totcol; a++) {
		if(mb->mat[a]) mb->mat[a]->id.us--;
		mb->mat[a]= 0;
	}

	
}


/* do not free mball itself */
void free_mball(MetaBall *mb)
{
	unlink_mball(mb);	
	
	if(mb->mat) MEM_freeN(mb->mat);
	if(mb->bb) MEM_freeN(mb->bb);
	BLI_freelistN(&mb->elems);
	if(mb->disp.first) freedisplist(&mb->disp);
}

MetaBall *add_mball()
{
	MetaBall *mb;
	
	mb= alloc_libblock(&G.main->mball, ID_MB, "Meta");
	
	mb->size[0]= mb->size[1]= mb->size[2]= 1.0;
	mb->texflag= MB_AUTOSPACE;
	
	mb->wiresize= 0.4f;
	mb->rendersize= 0.2f;
	mb->thresh= 0.6f;
	
	return mb;
}

MetaBall *copy_mball(MetaBall *mb)
{
	MetaBall *mbn;
	int a;
	
	mbn= copy_libblock(mb);

	duplicatelist(&mbn->elems, &mb->elems);
	
	mbn->mat= MEM_dupallocN(mb->mat);
	for(a=0; a<mbn->totcol; a++) {
		id_us_plus((ID *)mbn->mat[a]);
	}
	mbn->bb= MEM_dupallocN(mb->bb);
	
	return mbn;
}

void make_local_mball(MetaBall *mb)
{
	Object *ob;
	MetaBall *mbn;
	int local=0, lib=0;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */
	
	if(mb->id.lib==0) return;
	if(mb->id.us==1) {
		mb->id.lib= 0;
		mb->id.flag= LIB_LOCAL;
		return;
	}
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->data==mb) {
			if(ob->id.lib) lib= 1;
			else local= 1;
		}
		ob= ob->id.next;
	}
	
	if(local && lib==0) {
		mb->id.lib= 0;
		mb->id.flag= LIB_LOCAL;
	}
	else if(local && lib) {
		mbn= copy_mball(mb);
		mbn->id.us= 0;
		
		ob= G.main->object.first;
		while(ob) {
			if(ob->data==mb) {
				
				if(ob->id.lib==0) {
					ob->data= mbn;
					mbn->id.us++;
					mb->id.us--;
				}
			}
			ob= ob->id.next;
		}
	}
}
/** Compute bounding box of all MetaElems/MetaBalls.
 *
 * Bounding box is computed from polygonized surface. Object *ob is
 * basic MetaBall (usaualy with name Meta). All other MetaBalls (whith
 * names Meta.001, Meta.002, etc) are included in this Bounding Box.
 */
void tex_space_mball(Object *ob)
{
	DispList *dl;
	BoundBox *bb;
	float *data, min[3], max[3], loc[3], size[3];
	int tot, doit=0;

	if(ob->bb==0) ob->bb= MEM_callocN(sizeof(BoundBox), "mb boundbox");
	bb= ob->bb;
	
	/* Weird one, this. */
/*  	INIT_MINMAX(min, max); */
	(min)[0]= (min)[1]= (min)[2]= 1.0e30f;
	(max)[0]= (max)[1]= (max)[2]= -1.0e30f;

	dl= ob->disp.first;
	while(dl) {
		tot= dl->nr;
		if(tot) doit= 1;
		data= dl->verts;
		while(tot--) {
			/* Also weird... but longer. From utildefines. */
			DO_MINMAX(data, min, max);
			data+= 3;
		}
		dl= dl->next;
	}

	if(doit) {
		loc[0]= (min[0]+max[0])/2.0f;
		loc[1]= (min[1]+max[1])/2.0f;
		loc[2]= (min[2]+max[2])/2.0f;
		
		size[0]= (max[0]-min[0])/2.0f;
		size[1]= (max[1]-min[1])/2.0f;
		size[2]= (max[2]-min[2])/2.0f;
	}
	else {
		loc[0]= loc[1]= loc[2]= 0.0f;
		size[0]= size[1]= size[2]= 1.0f;
	}
	
	bb->vec[0][0]=bb->vec[1][0]=bb->vec[2][0]=bb->vec[3][0]= loc[0]-size[0];
	bb->vec[4][0]=bb->vec[5][0]=bb->vec[6][0]=bb->vec[7][0]= loc[0]+size[0];
	
	bb->vec[0][1]=bb->vec[1][1]=bb->vec[4][1]=bb->vec[5][1]= loc[1]-size[1];
	bb->vec[2][1]=bb->vec[3][1]=bb->vec[6][1]=bb->vec[7][1]= loc[1]+size[1];

	bb->vec[0][2]=bb->vec[3][2]=bb->vec[4][2]=bb->vec[7][2]= loc[2]-size[2];
	bb->vec[1][2]=bb->vec[2][2]=bb->vec[5][2]=bb->vec[6][2]= loc[2]+size[2];
	
}

void make_orco_mball(Object *ob)
{
	BoundBox *bb;
	DispList *dl;
	float *data;
	float loc[3], size[3];
	int a;
	
	/* restore size and loc */
	bb= ob->bb;
	loc[0]= (bb->vec[0][0]+bb->vec[4][0])/2.0f;
	size[0]= bb->vec[4][0]-loc[0];
	loc[1]= (bb->vec[0][1]+bb->vec[2][1])/2.0f;
	size[1]= bb->vec[2][1]-loc[1];
	loc[2]= (bb->vec[0][2]+bb->vec[1][2])/2.0f;
	size[2]= bb->vec[1][2]-loc[2];

	dl= ob->disp.first;
	data= dl->verts;
	a= dl->nr;
	while(a--) {
		data[0]= (data[0]-loc[0])/size[0];
		data[1]= (data[1]-loc[1])/size[1];
		data[2]= (data[2]-loc[2])/size[2];

		data+= 3;
	}
}
/** \brief Test, if Object *ob is basic MetaBall.
 *
 * It test last character of Object ID name. If last character
 * is digit it return 0, else it return 1.
 */
int is_basis_mball(Object *ob)
{
	int len;
	
	/* just a quick test */
	len= strlen(ob->id.name);
	if( isdigit(ob->id.name[len-1]) ) return 0;
	return 1;
}

/** \brief This function finds basic MetaBall.
 *
 * Basic MetaBall doesn't include any number at the end of
 * its name. All MetaBalls with same base of name can be
 * blended. MetaBalls with different basic name can't be
 * blended.
 */
Object *find_basis_mball(Object *basis)
{
	Base *base;
	int basisnr;
	char basisname[32];
	
	splitIDname(basis->id.name+2, basisname, &basisnr);
	
	for (base= G.scene->base.first; base && basisnr; base= base->next) {
		Object *ob= base->object;
		
		if (ob!=basis && ob->type==OB_MBALL) {
			char obname[32];
			int obnr;
			
			splitIDname(ob->id.name+2, obname, &obnr);
			
			if ((strcmp(obname, basisname)==0) && obnr<basisnr) {
				basis= ob;
				basisnr= obnr;
			}
		}
	}

	return basis;
}


/* ******************** ARITH ************************* */

/* BASED AT CODE (but mostly rewritten) :
 * C code from the article
 * "An Implicit Surface Polygonizer"
 * by Jules Bloomenthal, jbloom@beauty.gmu.edu
 * in "Graphics Gems IV", Academic Press, 1994

 * Authored by Jules Bloomenthal, Xerox PARC.
 * Copyright (c) Xerox Corporation, 1991.  All rights reserved.
 * Permission is granted to reproduce, use and distribute this code for
 * any and all purposes, provided that this notice appears in all copies. */

#define RES	12 /* # converge iterations    */

#define L	0  /* left direction:	-x, -i */
#define R	1  /* right direction:	+x, +i */
#define B	2  /* bottom direction: -y, -j */
#define T	3  /* top direction:	+y, +j */
#define N	4  /* near direction:	-z, -k */
#define F	5  /* far direction:	+z, +k */
#define LBN	0  /* left bottom near corner  */
#define LBF	1  /* left bottom far corner   */
#define LTN	2  /* left top near corner     */
#define LTF	3  /* left top far corner      */
#define RBN	4  /* right bottom near corner */
#define RBF	5  /* right bottom far corner  */
#define RTN	6  /* right top near corner    */
#define RTF	7  /* right top far corner     */

/* the LBN corner of cube (i, j, k), corresponds with location
 * (start.x+(i-0.5)*size, start.y+(j-0.5)*size, start.z+(k-0.5)*size) */

#define HASHBIT	    (5)
#define HASHSIZE    (size_t)(1<<(3*HASHBIT))   /*! < hash table size (32768) */

#define HASH(i,j,k) ((((( (i) & 31)<<5) | ( (j) & 31))<<5 ) | ( (k) & 31) )

#define MB_BIT(i, bit) (((i)>>(bit))&1)
#define FLIP(i,bit) ((i)^1<<(bit)) /* flip the given bit of i */

typedef struct point {			/* a three-dimensional point */
	float x, y, z;				/* its coordinates */
} MB_POINT;

typedef struct vertex {			/* surface vertex */
	MB_POINT position, normal;		/* position and surface normal */
} VERTEX;

typedef struct vertices {		/* list of vertices in polygonization */
	int count, max;				/* # vertices, max # allowed */
	VERTEX *ptr;				/* dynamically allocated */
} VERTICES;

typedef struct corner {			/* corner of a cube */
	int i, j, k;				/* (i, j, k) is index within lattice */
	float x, y, z, value;		/* location and function value */
	struct corner *next;
} CORNER;

typedef struct cube {			/* partitioning cell (cube) */
	int i, j, k;				/* lattice location of cube */
	CORNER *corners[8];			/* eight corners */
} CUBE;

typedef struct cubes {			/* linked list of cubes acting as stack */
	CUBE cube;					/* a single cube */
	struct cubes *next;			/* remaining elements */
} CUBES;

typedef struct centerlist {		/* list of cube locations */
	int i, j, k;				/* cube location */
	struct centerlist *next;	/* remaining elements */
} CENTERLIST;

typedef struct edgelist {		/* list of edges */
	int i1, j1, k1, i2, j2, k2;	/* edge corner ids */
	int vid;					/* vertex id */
	struct edgelist *next;		/* remaining elements */
} EDGELIST;

typedef struct intlist {		/* list of integers */
	int i;						/* an integer */
	struct intlist *next;		/* remaining elements */
} INTLIST;

typedef struct intlists {		/* list of list of integers */
	INTLIST *list;				/* a list of integers */
	struct intlists *next;		/* remaining elements */
} INTLISTS;

typedef struct process {		/* parameters, function, storage */
	/* what happens here? floats, I think. */
	/*  float (*function)(void);	 */	/* implicit surface function */
	float (*function)(float, float, float);
	float size, delta;			/* cube size, normal delta */
	int bounds;					/* cube range within lattice */
	MB_POINT start;				/* start point on surface */
	CUBES *cubes;				/* active cubes */
	VERTICES vertices;			/* surface vertices */
	CENTERLIST **centers;		/* cube center hash table */
	CORNER **corners;			/* corner value hash table */
	EDGELIST **edges;			/* edge and vertex id hash table */
} PROCESS;

/* Some declarations are in order !!! */

/* these should go into a header ! But the compiler doesn't like that,
 * for some reason */

void freepolygonize(PROCESS *p);
void docube(CUBE *cube, PROCESS *p, MetaBall *mb);
void testface(int i, int j, int k, CUBE* old,
			  int bit, int c1, int c2, int c3, int c4, PROCESS *p);
CORNER *setcorner (PROCESS* p, int i, int j, int k);
int vertid (CORNER *c1, CORNER *c2, PROCESS *p, MetaBall *mb);
int setcenter(CENTERLIST *table[], int i, int j, int k);
int otherface (int edge, int face);
void makecubetable (void);
void setedge (EDGELIST *table[],
			  int i1, int j1,
			  int k1, int i2,
			  int j2, int k2,
			  int vid);
int getedge (EDGELIST *table[],
			 int i1, int j1, int k1,
			 int i2, int j2, int k2);
void addtovertices (VERTICES *vertices, VERTEX v);
void vnormal (MB_POINT *point, PROCESS *p, MB_POINT *v);
void converge (MB_POINT *p1, MB_POINT *p2, float v1, float v2,
			   float (*function)(float, float, float), MB_POINT *p, MetaBall *mb, int f);
void add_cube(PROCESS *mbproc, int i, int j, int k, int count);
void find_first_points(PROCESS *mbproc, MetaBall *mb, int a);
void polygonize(PROCESS *mbproc, MetaBall *mb);
float init_meta(Object *ob);

/* **************** METABALL ************************ */

float thresh= 0.6f;
int totelem=0;
MetaElem **mainb;

void calc_mballco(MetaElem *ml, float *vec)
{
	if(ml->mat) {
		Mat4MulVecfl(ml->mat, vec);
	}
}

float densfunc(MetaElem *ball, float x, float y, float z)
{
	float dist2 = 0.0, dx, dy, dz;
	float vec[3];

	if(ball->imat) {
		vec[0]= x;
		vec[1]= y;
		vec[2]= z;
		Mat4MulVecfl(ball->imat, vec);
		dx= ball->x - vec[0];
		dy= ball->y - vec[1];
		dz= ball->z - vec[2];
	}
	else {
		dx= ball->x - x;
		dy= ball->y - y;
		dz= ball->z - z;
	}

	if(ball->type==MB_BALL) {
	}
	else if(ball->type==MB_TUBEX) {
		if( dx > ball->len) dx-= ball->len;
		else if(dx< -ball->len) dx+= ball->len;
		else dx= 0.0;
	}
	else if(ball->type==MB_TUBEY) {
		if( dy > ball->len) dy-= ball->len;
		else if(dy< -ball->len) dy+= ball->len;
		else dy= 0.0;
	}
	else if(ball->type==MB_TUBEZ) {
		if( dz > ball->len) dz-= ball->len;
		else if(dz< -ball->len) dz+= ball->len;
		else dz= 0.0;
	}
	else if(ball->type==MB_TUBE) {
		if( dx > ball->expx) dx-= ball->expx;
		else if(dx< -ball->expx) dx+= ball->expx;
		else dx= 0.0;
	}
	else if(ball->type==MB_PLANE) {
		if( dx > ball->expx) dx-= ball->expx;
		else if(dx< -ball->expx) dx+= ball->expx;
		else dx= 0.0;
		if( dy > ball->expy) dy-= ball->expy;
		else if(dy< -ball->expy) dy+= ball->expy;
		else dy= 0.0;
	}
	else if(ball->type==MB_ELIPSOID) {
		dx *= 1/ball->expx;
		dy *= 1/ball->expy;
		dz *= 1/ball->expz;
	}
	else if(ball->type==MB_CUBE) {
		if( dx > ball->expx) dx-= ball->expx;
		else if(dx< -ball->expx) dx+= ball->expx;
		else dx= 0.0;
		if( dy > ball->expy) dy-= ball->expy;
		else if(dy< -ball->expy) dy+= ball->expy;
		else dy= 0.0;
		if( dz > ball->expz) dz-= ball->expz;
		else if(dz< -ball->expz) dz+= ball->expz;
		else dz= 0.0;
	}

	dist2= (dx*dx + dy*dy + dz*dz);

	if(ball->flag & MB_NEGATIVE) {
		dist2= 1.0f-(dist2/ball->rad2);
		if(dist2 < 0.0) return 0.5f;

		return 0.5f-ball->s*dist2*dist2*dist2;
	}
	else {
		dist2= 1.0f-(dist2/ball->rad2);
		if(dist2 < 0.0) return -0.5f;

		return ball->s*dist2*dist2*dist2 -0.5f;
	}
}


float metaball(float x, float y, float z)
/*  float x, y, z; */
{
	float dens=0;
	int a;
	
	for(a=0; a<totelem; a++) {
		dens+= densfunc( mainb[a], x, y, z);
	}

	return thresh - dens;
}

/* ******************************************** */

int *indices=0;
int totindex, curindex;


void accum_mballfaces(int i1, int i2, int i3, int i4)
{
	int *newi, *cur;
	/* static int i=0; I would like to delete altogether, but I don't dare to, yet */

	if(totindex==curindex) {
		totindex+= 256;
		newi= MEM_mallocN(4*sizeof(int)*totindex, "vertindex");
		
		if(indices) {
			memcpy(newi, indices, 4*sizeof(int)*(totindex-256));
			MEM_freeN(indices);
		}
		indices= newi;
	}
	
	cur= indices+4*curindex;

	/* prevent zero codes for faces indices */
	if(i3==0) {
		if(i4) {
			i3= i4;
			i4= i1;
			i1= i2;
			i2= 0;
		}
		else {
			i3= i2;
			i2= i1;
			i1= 0;
		}
	}
	
	cur[0]= i1;
	cur[1]= i2;
	cur[2]= i3;
	cur[3]= i4;
	
	curindex++;

}

/* ******************* MEMORY MANAGEMENT *********************** */

struct pgn_elements {
	struct pgn_elements *next, *prev;
	char *data;
	
};

void *new_pgn_element(int size)
{
	/* during polygonize 1000s of elements are allocated
	 * and never freed inbetween. Freeing only done at the end.
	 */
	int blocksize= 16384;
	static int offs= 0;		/* the current free address */
	static struct pgn_elements *cur= 0;
	static ListBase lb= {0, 0};
	void *adr;
	
	if(size>10000 || size==0) {
		printf("incorrect use of new_pgn_element\n");
	}
	else if(size== -1) {
		cur= lb.first;
		while(cur) {
			MEM_freeN(cur->data);
			cur= cur->next;
		}
		BLI_freelistN(&lb);
		
		return NULL;	
	}
	
	size= 4*( (size+3)/4 );
	
	if(cur) {
		if(size+offs < blocksize) {
			adr= (void *) (cur->data+offs);
		 	offs+= size;
			return adr;
		}
	}
	
	cur= MEM_callocN( sizeof(struct pgn_elements), "newpgn");
	cur->data= MEM_callocN(blocksize, "newpgn");
	BLI_addtail(&lb, cur);
	
	offs= size;
	return cur->data;
}

void freepolygonize(PROCESS *p)
{
	MEM_freeN(p->corners);
	MEM_freeN(p->edges);
	MEM_freeN(p->centers);

	new_pgn_element(-1);
	
	if(p->vertices.ptr) MEM_freeN(p->vertices.ptr);
}

/**** Cubical Polygonization (optional) ****/


#define LB	0  /* left bottom edge	*/
#define LT	1  /* left top edge	*/
#define LN	2  /* left near edge	*/
#define LF	3  /* left far edge	*/
#define RB	4  /* right bottom edge */
#define RT	5  /* right top edge	*/
#define RN	6  /* right near edge	*/
#define RF	7  /* right far edge	*/
#define BN	8  /* bottom near edge	*/
#define BF	9  /* bottom far edge	*/
#define TN	10 /* top near edge	*/
#define TF	11 /* top far edge	*/

static INTLISTS *cubetable[256];

/*			edge: LB, LT, LN, LF, RB, RT, RN, RF, BN, BF, TN, TF */
static int corner1[12]	   = {
	LBN,LTN,LBN,LBF,RBN,RTN,RBN,RBF,LBN,LBF,LTN,LTF};
static int corner2[12]	   = {
	LBF,LTF,LTN,LTF,RBF,RTF,RTN,RTF,RBN,RBF,RTN,RTF};
static int leftface[12]	   = {
	B,  L,  L,  F,  R,  T,  N,  R,  N,  B,  T,  F};
/* face on left when going corner1 to corner2 */
static int rightface[12]   = {
	L,  T,  N,  L,  B,  R,  R,  F,  B,  F,  N,  T};
/* face on right when going corner1 to corner2 */


/* docube: triangulate the cube directly, without decomposition */

void docube(CUBE *cube, PROCESS *p, MetaBall *mb)
{
	INTLISTS *polys;
	CORNER *c1, *c2;
	int i, index = 0, count, indexar[8];
	
	for (i = 0; i < 8; i++) if (cube->corners[i]->value > 0.0) index += (1<<i);
	
	for (polys = cubetable[index]; polys; polys = polys->next) {
		INTLIST *edges;
		
		count = 0;
		
		for (edges = polys->list; edges; edges = edges->next) {
			c1 = cube->corners[corner1[edges->i]];
			c2 = cube->corners[corner2[edges->i]];
			
			indexar[count] = vertid(c1, c2, p, mb);
			count++;
		}
		if(count>2) {
			switch(count) {
			case 3:
				accum_mballfaces(indexar[2], indexar[1], indexar[0], 0);
				break;
			case 4:
				if(indexar[0]==0) accum_mballfaces(indexar[0], indexar[3], indexar[2], indexar[1]);
				else accum_mballfaces(indexar[3], indexar[2], indexar[1], indexar[0]);
				break;
			case 5:
				if(indexar[0]==0) accum_mballfaces(indexar[0], indexar[3], indexar[2], indexar[1]);
				else accum_mballfaces(indexar[3], indexar[2], indexar[1], indexar[0]);

				accum_mballfaces(indexar[4], indexar[3], indexar[0], 0);
				break;
			case 6:
				if(indexar[0]==0) {
					accum_mballfaces(indexar[0], indexar[3], indexar[2], indexar[1]);
					accum_mballfaces(indexar[0], indexar[5], indexar[4], indexar[3]);
				}
				else {
					accum_mballfaces(indexar[3], indexar[2], indexar[1], indexar[0]);
					accum_mballfaces(indexar[5], indexar[4], indexar[3], indexar[0]);
				}
				break;
			case 7:
				if(indexar[0]==0) {
					accum_mballfaces(indexar[0], indexar[3], indexar[2], indexar[1]);
					accum_mballfaces(indexar[0], indexar[5], indexar[4], indexar[3]);
				}
				else {
					accum_mballfaces(indexar[3], indexar[2], indexar[1], indexar[0]);
					accum_mballfaces(indexar[5], indexar[4], indexar[3], indexar[0]);
				}
				
				accum_mballfaces(indexar[6], indexar[5], indexar[0], 0);
				
				break;
			}
		}
	}
}


/* testface: given cube at lattice (i, j, k), and four corners of face,
 * if surface crosses face, compute other four corners of adjacent cube
 * and add new cube to cube stack */

void testface(int i, int j, int k, CUBE* old, int bit, int c1, int c2, int c3, int c4, PROCESS *p)
{
	CUBE newc;
	CUBES *oldcubes = p->cubes;
	CORNER *corn1, *corn2, *corn3, *corn4;
	int n, pos;

	corn1= old->corners[c1];
	corn2= old->corners[c2];
	corn3= old->corners[c3];
	corn4= old->corners[c4];
	
	pos = corn1->value > 0.0 ? 1 : 0;

	/* test if no surface crossing */
	if( (corn2->value > 0) == pos && (corn3->value > 0) == pos && (corn4->value > 0) == pos) return;
	/* test if cube out of bounds */
	/*if ( abs(i) > p->bounds || abs(j) > p->bounds || abs(k) > p->bounds) return;*/
	/* test if already visited (always as last) */
	if (setcenter(p->centers, i, j, k)) return;


	/* create new cube and add cube to top of stack: */
	p->cubes = (CUBES *) new_pgn_element(sizeof(CUBES));
	p->cubes->next = oldcubes;
	
	newc.i = i;
	newc.j = j;
	newc.k = k;
	for (n = 0; n < 8; n++) newc.corners[n] = NULL;
	
	newc.corners[FLIP(c1, bit)] = corn1;
	newc.corners[FLIP(c2, bit)] = corn2;
	newc.corners[FLIP(c3, bit)] = corn3;
	newc.corners[FLIP(c4, bit)] = corn4;

	if(newc.corners[0]==0) newc.corners[0] = setcorner(p, i, j, k);
	if(newc.corners[1]==0) newc.corners[1] = setcorner(p, i, j, k+1);
	if(newc.corners[2]==0) newc.corners[2] = setcorner(p, i, j+1, k);
	if(newc.corners[3]==0) newc.corners[3] = setcorner(p, i, j+1, k+1);
	if(newc.corners[4]==0) newc.corners[4] = setcorner(p, i+1, j, k);
	if(newc.corners[5]==0) newc.corners[5] = setcorner(p, i+1, j, k+1);
	if(newc.corners[6]==0) newc.corners[6] = setcorner(p, i+1, j+1, k);
	if(newc.corners[7]==0) newc.corners[7] = setcorner(p, i+1, j+1, k+1);

	p->cubes->cube= newc;	
}

/* setcorner: return corner with the given lattice location
   set (and cache) its function value */

CORNER *setcorner (PROCESS* p, int i, int j, int k)
{
	/* for speed, do corner value caching here */
	CORNER *c;
	int index;

	/* does corner exist? */
	index = HASH(i, j, k);
	c = p->corners[index];
	
	for (; c != NULL; c = c->next) {
		if (c->i == i && c->j == j && c->k == k) {
			return c;
		}
	}

	c = (CORNER *) new_pgn_element(sizeof(CORNER));

	c->i = i; 
	c->x = p->start.x+((float)i-0.5f)*p->size;
	c->j = j; 
	c->y = p->start.y+((float)j-0.5f)*p->size;
	c->k = k; 
	c->z = p->start.z+((float)k-0.5f)*p->size;
	c->value = p->function(c->x, c->y, c->z);
	
	c->next = p->corners[index];
	p->corners[index] = c;
	
	return c;
}


/* nextcwedge: return next clockwise edge from given edge around given face */

int nextcwedge (int edge, int face)
{
	switch (edge) {
	case LB: 
		return (face == L)? LF : BN;
	case LT: 
		return (face == L)? LN : TF;
	case LN: 
		return (face == L)? LB : TN;
	case LF: 
		return (face == L)? LT : BF;
	case RB: 
		return (face == R)? RN : BF;
	case RT: 
		return (face == R)? RF : TN;
	case RN: 
		return (face == R)? RT : BN;
	case RF: 
		return (face == R)? RB : TF;
	case BN: 
		return (face == B)? RB : LN;
	case BF: 
		return (face == B)? LB : RF;
	case TN: 
		return (face == T)? LT : RN;
	case TF: 
		return (face == T)? RT : LF;
	}
	return 0;
}


/* otherface: return face adjoining edge that is not the given face */

int otherface (int edge, int face)
{
	int other = leftface[edge];
	return face == other? rightface[edge] : other;
}


/* makecubetable: create the 256 entry table for cubical polygonization */

void makecubetable (void)
{
	static int isdone= 0;
	int i, e, c, done[12], pos[8];

	if(isdone) return;
	isdone= 1;

	for (i = 0; i < 256; i++) {
		for (e = 0; e < 12; e++) done[e] = 0;
		for (c = 0; c < 8; c++) pos[c] = MB_BIT(i, c);
		for (e = 0; e < 12; e++)
			if (!done[e] && (pos[corner1[e]] != pos[corner2[e]])) {
				INTLIST *ints = 0;
				INTLISTS *lists = (INTLISTS *) MEM_callocN(sizeof(INTLISTS), "mball_intlist");
				int start = e, edge = e;
				
				/* get face that is to right of edge from pos to neg corner: */
				int face = pos[corner1[e]]? rightface[e] : leftface[e];
				
				while (1) {
					edge = nextcwedge(edge, face);
					done[edge] = 1;
					if (pos[corner1[edge]] != pos[corner2[edge]]) {
						INTLIST *tmp = ints;
						
						ints = (INTLIST *) MEM_callocN(sizeof(INTLIST), "mball_intlist");
						ints->i = edge;
						ints->next = tmp; /* add edge to head of list */
						
						if (edge == start) break;
						face = otherface(edge, face);
					}
				}
				lists->list = ints; /* add ints to head of table entry */
				lists->next = cubetable[i];
				cubetable[i] = lists;
			}
	}
}

void BKE_freecubetable(void)
{
	int i;
	INTLISTS *lists, *nlists;
	INTLIST *ints, *nints;

	for (i = 0; i < 256; i++) {
		lists= cubetable[i];
		while(lists) {
			nlists= lists->next;
			
			ints= lists->list;
			while(ints) {
				nints= ints->next;
				MEM_freeN(ints);
				ints= nints;
			}
			
			MEM_freeN(lists);
			lists= nlists;
		}
		cubetable[i]= 0;
	}
}

/**** Storage ****/

/* setcenter: set (i,j,k) entry of table[]
 * return 1 if already set; otherwise, set and return 0 */

int setcenter(CENTERLIST *table[], int i, int j, int k)
{
	int index;
	CENTERLIST *newc, *l, *q;

	index= HASH(i, j, k);
	q= table[index];

	for (l = q; l != NULL; l = l->next) {
		if (l->i == i && l->j == j && l->k == k) return 1;
	}
	
	newc = (CENTERLIST *) new_pgn_element(sizeof(CENTERLIST));
	newc->i = i; 
	newc->j = j; 
	newc->k = k; 
	newc->next = q;
	table[index] = newc;
	
	return 0;
}


/* setedge: set vertex id for edge */

void setedge (EDGELIST *table[],
			  int i1, int j1,
			  int k1, int i2,
			  int j2, int k2,
			  int vid)
{
	unsigned int index;
	EDGELIST *newe;
	
	if (i1>i2 || (i1==i2 && (j1>j2 || (j1==j2 && k1>k2)))) {
		int t=i1; 
		i1=i2; 
		i2=t; 
		t=j1; 
		j1=j2; 
		j2=t; 
		t=k1; 
		k1=k2; 
		k2=t;
	}
	index = HASH(i1, j1, k1) + HASH(i2, j2, k2);
	newe = (EDGELIST *) new_pgn_element(sizeof(EDGELIST));
	newe->i1 = i1; 
	newe->j1 = j1; 
	newe->k1 = k1;
	newe->i2 = i2; 
	newe->j2 = j2; 
	newe->k2 = k2;
	newe->vid = vid;
	newe->next = table[index];
	table[index] = newe;
}


/* getedge: return vertex id for edge; return -1 if not set */

int getedge (EDGELIST *table[],
			 int i1, int j1, int k1,
			 int i2, int j2, int k2)
{
	EDGELIST *q;
	
	if (i1>i2 || (i1==i2 && (j1>j2 || (j1==j2 && k1>k2)))) {
		int t=i1; 
		i1=i2; 
		i2=t; 
		t=j1; 
		j1=j2; 
		j2=t; 
		t=k1; 
		k1=k2; 
		k2=t;
	}
	q = table[HASH(i1, j1, k1)+HASH(i2, j2, k2)];
	for (; q != NULL; q = q->next)
		if (q->i1 == i1 && q->j1 == j1 && q->k1 == k1 &&
		    q->i2 == i2 && q->j2 == j2 && q->k2 == k2)
			return q->vid;
	return -1;
}


/**** Vertices ****/

#undef R



/* vertid: return index for vertex on edge:
 * c1->value and c2->value are presumed of different sign
 * return saved index if any; else compute vertex and save */

/* addtovertices: add v to sequence of vertices */

void addtovertices (VERTICES *vertices, VERTEX v)
{
	if (vertices->count == vertices->max) {
		int i;
		VERTEX *newv;
		vertices->max = vertices->count == 0 ? 10 : 2*vertices->count;
		newv = (VERTEX *) MEM_callocN(vertices->max * sizeof(VERTEX), "addtovertices");
		
		for (i = 0; i < vertices->count; i++) newv[i] = vertices->ptr[i];
		
		if (vertices->ptr != NULL) MEM_freeN(vertices->ptr);
		vertices->ptr = newv;
	}
	vertices->ptr[vertices->count++] = v;
}

/* vnormal: compute unit length surface normal at point */

void vnormal (MB_POINT *point, PROCESS *p, MB_POINT *v)
{
	float delta= 0.2f*p->delta;
	float f = p->function(point->x, point->y, point->z);

	v->x = p->function(point->x+delta, point->y, point->z)-f;
	v->y = p->function(point->x, point->y+delta, point->z)-f;
	v->z = p->function(point->x, point->y, point->z+delta)-f;
	f = (float)sqrt(v->x*v->x + v->y*v->y + v->z*v->z);

	if (f != 0.0) {
		v->x /= f; 
		v->y /= f; 
		v->z /= f;
	}
	
	if(FALSE) {
	/* if(R.flag & R_RENDERING) { */
		MB_POINT temp;
		
		delta*= 2.0;
		
		f = p->function(point->x, point->y, point->z);
	
		temp.x = p->function(point->x+delta, point->y, point->z)-f;
		temp.y = p->function(point->x, point->y+delta, point->z)-f;
		temp.z = p->function(point->x, point->y, point->z+delta)-f;
		f = (float)sqrt(temp.x*temp.x + temp.y*temp.y + temp.z*temp.z);
	
		if (f != 0.0) {
			temp.x /= f; 
			temp.y /= f; 
			temp.z /= f;
			
			v->x+= temp.x;
			v->y+= temp.y;
			v->z+= temp.z;
			
			f = (float)sqrt(v->x*v->x + v->y*v->y + v->z*v->z);
		
			if (f != 0.0) {
				v->x /= f; 
				v->y /= f; 
				v->z /= f;
			}
		}
	}
	
}


int vertid (CORNER *c1, CORNER *c2, PROCESS *p, MetaBall *mb)
{
	VERTEX v;
	MB_POINT a, b;
	int vid = getedge(p->edges, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k);

	if (vid != -1) return vid;			     /* previously computed */
	a.x = c1->x; 
	a.y = c1->y; 
	a.z = c1->z;
	b.x = c2->x; 
	b.y = c2->y; 
	b.z = c2->z;

	converge(&a, &b, c1->value, c2->value, p->function, &v.position, mb, 1); /* position */
	vnormal(&v.position, p, &v.normal);

	addtovertices(&p->vertices, v);			   /* save vertex */
	vid = p->vertices.count-1;
	setedge(p->edges, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k, vid);
	
	return vid;
}




/* converge: from two points of differing sign, converge to zero crossing */
/* watch it: p1 and p2 are used to calculate */
void converge (MB_POINT *p1, MB_POINT *p2, float v1, float v2,
			   float (*function)(float, float, float), MB_POINT *p, MetaBall *mb, int f)
{
	int i = 0;
	MB_POINT pos, neg;
	float positive = 0.0f, negative = 0.0f;
	float dx = 0.0f ,dy = 0.0f ,dz = 0.0f;
	
	if (v1 < 0) {
		pos= *p2;
		neg= *p1;
		positive = v2;
		negative = v1;
	}
	else {
		pos= *p1;
		neg= *p2;
		positive = v1;
		negative = v2;
	}

	dx = pos.x - neg.x;
	dy = pos.y - neg.y;
	dz = pos.z - neg.z;

/* Aproximation by linear interpolation is faster then binary subdivision,
 * but it results sometimes (mb->thresh < 0.2) into the strange results */
	if((mb->thresh >0.2) && (f==1)){
	if((dy == 0.0f) && (dz == 0.0f)){
		p->x = neg.x - negative*dx/(positive-negative);
		p->y = neg.y;
		p->z = neg.z;
		return;
	}
  	if((dx == 0.0f) && (dz == 0.0f)){
		p->x = neg.x;
		p->y = neg.y - negative*dy/(positive-negative);
		p->z = neg.z;
		return;
	}
	if((dx == 0.0f) && (dy == 0.0f)){
		p->x = neg.x;
		p->y = neg.y;
		p->z = neg.z - negative*dz/(positive-negative);
		return;
	}
	}

	if((dy == 0.0f) && (dz == 0.0f)){
		p->y = neg.y;
		p->z = neg.z;
		while (1) {
			p->x = 0.5f*(pos.x + neg.x);
			if (i++ == RES) return;
			if ((function(p->x,p->y,p->z)) > 0.0)	pos.x = p->x; else neg.x = p->x; 
		}
	}

	if((dx == 0.0f) && (dz == 0.0f)){
		p->x = neg.x;
		p->z = neg.z;
		while (1) {
			p->y = 0.5f*(pos.y + neg.y);
			if (i++ == RES) return;
			if ((function(p->x,p->y,p->z)) > 0.0)	pos.y = p->y; else neg.y = p->y;
		}
  	}
   
	if((dx == 0.0f) && (dy == 0.0f)){
		p->x = neg.x;
		p->y = neg.y;
		while (1) {
			p->z = 0.5f*(pos.z + neg.z);
			if (i++ == RES) return;
			if ((function(p->x,p->y,p->z)) > 0.0)	pos.z = p->z; else neg.z = p->z;
		}
	}

	/* This is necessary to find start point */
	while (1) {
		p->x = 0.5f*(pos.x + neg.x);
		p->y = 0.5f*(pos.y + neg.y);
		p->z = 0.5f*(pos.z + neg.z);
    
		if (i++ == RES) return;
   
		if ((function(p->x, p->y, p->z)) > 0.0){
			pos.x = p->x;
			pos.y = p->y;
			pos.z = p->z;
		}
		else{
			neg.x = p->x;
			neg.y = p->y;
			neg.z = p->z;
		}
	}
}

/* ************************************** */
void add_cube(PROCESS *mbproc, int i, int j, int k, int count)
{
	CUBES *ncube;
	int n;
	int a, b, c;

	/* hmmm, not only one, but eight cube will be added on the stack 
	 * ... */
	for(a=i-1; a<i+count; a++)
		for(b=j-1; b<j+count; b++)
			for(c=k-1; c<k+count; c++) {
				/* test if cube has been found before */
				if( setcenter(mbproc->centers, a, b, c)==0 ) {
					/* push cube on stack: */
					ncube= (CUBES *) new_pgn_element(sizeof(CUBES));
					ncube->next= mbproc->cubes;
					mbproc->cubes= ncube;

					ncube->cube.i= a;
					ncube->cube.j= b;
					ncube->cube.k= c;

					/* set corners of initial cube: */
					for (n = 0; n < 8; n++)
					ncube->cube.corners[n] = setcorner(mbproc, a+MB_BIT(n,2), b+MB_BIT(n,1), c+MB_BIT(n,0));
				}
			}
}


void find_first_points(PROCESS *mbproc, MetaBall *mb, int a)
{
	MB_POINT IN, in, OUT, out; /*point;*/
	MetaElem *ml;
	int i, j, k, c_i, c_j, c_k;
	int index[3]={1,0,-1};
	float f =0.0f;
	float in_v, out_v;

	ml = mainb[a];

	f = 1-(mb->thresh/ml->s);

	/* Skip, when Stiffness of MetaElement is too small ... MetaElement can't be
	 * visible alone ... but still can influence others MetaElements :-) */
	if(f > 0.0) {
		IN.x = in.x= ml->x;
		IN.y = in.y= ml->y;
		IN.z = in.z= ml->z;

		calc_mballco(ml, (float *)&in);
		in_v = mbproc->function(in.x, in.y, in.z);

		for(i=0;i<3;i++){
			switch (ml->type) {
				case MB_BALL:
					OUT.x = out.x= IN.x + index[i]*ml->rad;
					break;
				case MB_TUBE:
				case MB_PLANE:
				case MB_ELIPSOID:
				case MB_CUBE:
					OUT.x = out.x= IN.x + index[i]*(ml->expx + ml->rad);
					break;
			}

			for(j=0;j<3;j++) {
				switch (ml->type) {
					case MB_BALL:
						OUT.y = out.y= IN.y + index[j]*ml->rad;
						break;
					case MB_TUBE:
					case MB_PLANE:
					case MB_ELIPSOID:
					case MB_CUBE:
						OUT.y = out.y= IN.y + index[j]*(ml->expy + ml->rad);
						break;
				}
			
				for(k=0;k<3;k++) {
					out.x = OUT.x;
					out.y = OUT.y;
					switch (ml->type) {
						case MB_BALL:
						case MB_TUBE:
						case MB_PLANE:
							out.z= IN.z + index[k]*ml->rad;
							break;
						case MB_ELIPSOID:
						case MB_CUBE:
							out.z= IN.z + index[k]*(ml->expz + ml->rad);
							break;
					}

					calc_mballco(ml, (float *)&out);

					out_v = mbproc->function(out.x, out.y, out.z);

					/* find "first point" on Implicit Surface of MetaElemnt ml */
					converge(&in, &out, in_v, out_v, mbproc->function, &mbproc->start, mb, 0);
	
					/* indexes of CUBE, which includes "first point" */
					c_i= (int)floor(mbproc->start.x/mbproc->size );
					c_j= (int)floor(mbproc->start.y/mbproc->size );
					c_k= (int)floor(mbproc->start.z/mbproc->size );
		
					mbproc->start.x= mbproc->start.y= mbproc->start.z= 0.0;

					/* add CUBE (with indexes c_i, c_j, c_k) to the stack,
					 * this cube includes found point of Implicit Surface */
					if (ml->flag & MB_NEGATIVE)
						add_cube(mbproc, c_i, c_j, c_k, 2);
					else
						add_cube(mbproc, c_i, c_j, c_k, 1);
						
					
				}
			}
		}
	}
}

void polygonize(PROCESS *mbproc, MetaBall *mb)
{
	CUBE c;
	int a;

	mbproc->vertices.count = mbproc->vertices.max = 0;
	mbproc->vertices.ptr = NULL;

	/* allocate hash tables and build cube polygon table: */
	mbproc->centers = MEM_callocN(HASHSIZE * sizeof(CENTERLIST *), "mbproc->centers");
	mbproc->corners = MEM_callocN(HASHSIZE * sizeof(CORNER *), "mbproc->corners");
	mbproc->edges =	MEM_callocN(2*HASHSIZE * sizeof(EDGELIST *), "mbproc->edges");
	makecubetable();

	for(a=0; a<totelem; a++) {

		/* try to find 8 points on the surface for each MetaElem */
		find_first_points(mbproc, mb, a);	
	}

	/* polygonize all MetaElems of current MetaBall */
	while (mbproc->cubes != NULL) { /* process active cubes till none left */
		c = mbproc->cubes->cube;

		/* polygonize the cube directly: */
		docube(&c, mbproc, mb);
		
		/* pop current cube from stack */
		mbproc->cubes = mbproc->cubes->next;
		
		/* test six face directions, maybe add to stack: */
		testface(c.i-1, c.j, c.k, &c, 2, LBN, LBF, LTN, LTF, mbproc);
		testface(c.i+1, c.j, c.k, &c, 2, RBN, RBF, RTN, RTF, mbproc);
		testface(c.i, c.j-1, c.k, &c, 1, LBN, LBF, RBN, RBF, mbproc);
		testface(c.i, c.j+1, c.k, &c, 1, LTN, LTF, RTN, RTF, mbproc);
		testface(c.i, c.j, c.k-1, &c, 0, LBN, LTN, RBN, RTN, mbproc);
		testface(c.i, c.j, c.k+1, &c, 0, LBF, LTF, RBF, RTF, mbproc);
	}
}

float init_meta(Object *ob)	/* return totsize */
{
	Base *base;
	Object *bob;
	MetaBall *mb;
	MetaElem *ml;
	float size, totsize, (*mat)[4] = NULL, (*imat)[4] = NULL, obinv[4][4], vec[3];
	int a, obnr;
	char obname[32];
	
	Mat4Invert(obinv, ob->obmat);
	totelem= 0;
	
	splitIDname(ob->id.name+2, obname, &obnr);
	
	/* make main array */
	
	next_object(0, 0, 0);
	while(next_object(1, &base, &bob)) {

		if(bob->type==OB_MBALL) {
			ml= 0;
			if(bob==ob) {
				mat= imat= 0;
				mb= ob->data;
	
				if(ob==G.obedit) ml= editelems.first;
				else if(G.obedit && G.obedit->type==OB_MBALL && G.obedit->data==mb) ml= editelems.first;
				else ml= mb->elems.first;
			}
			else {
				char name[32];
				int nr;
				
				splitIDname(bob->id.name+2, name, &nr);
				if( strcmp(obname, name)==0 ) {
					mb= bob->data;
					
					if(G.obedit && G.obedit->type==OB_MBALL && G.obedit->data==mb) 
						ml= editelems.first;
					else ml= mb->elems.first;

					/* mat is the matrix to transform from mball into the basis-mbal */
					mat= new_pgn_element(4*4*sizeof(float));
					Mat4MulMat4(mat, bob->obmat, obinv);
					
					imat= new_pgn_element(4*4*sizeof(float));
					Mat4Invert(imat, mat);
					
				}
			}
			while(ml && totelem<MB_MAXELEM) {
				a= totelem;
				
				/* make a copy because of duplicates */
				mainb[a]= new_pgn_element(sizeof(MetaElem));
				*(mainb[a])= *ml;
				
				/* if(mainb[a]->flag & MB_NEGATIVE) mainb[a]->s= 1.0-mainb[a]->s; */
				mainb[a]->rad2= mainb[a]->rad*mainb[a]->rad;
				
				mainb[a]->mat= (float*) mat;
				mainb[a]->imat= (float*) imat;
				
				ml= ml->next;
				totelem++;
				
			}
		}
	}

	
	/* totsize (= 'manhattan' radius) */
	totsize= 0.0;
	for(a=0; a<totelem; a++) {
		
		vec[0]= mainb[a]->x + mainb[a]->rad;
		vec[1]= mainb[a]->y + mainb[a]->rad;
		vec[2]= mainb[a]->z + mainb[a]->rad;
		
		calc_mballco(mainb[a], vec);
	
		size= (float)fabs( vec[0] );
		if( size > totsize ) totsize= size;
		size= (float)fabs( vec[1] );
		if( size > totsize ) totsize= size;
		size= (float)fabs( vec[2] );
		if( size > totsize ) totsize= size;
		
		vec[0]= mainb[a]->x - mainb[a]->rad;
		vec[1]= mainb[a]->y - mainb[a]->rad;
		vec[2]= mainb[a]->z - mainb[a]->rad;
		
		calc_mballco(mainb[a], vec);
	
		size= (float)fabs( vec[0] );
		if( size > totsize ) totsize= size;
		size= (float)fabs( vec[1] );
		if( size > totsize ) totsize= size;
		size= (float)fabs( vec[2] );
		if( size > totsize ) totsize= size;
	}

	for(a=0; a<totelem; a++) {
		thresh+= densfunc( mainb[a], 2.0f*totsize, 2.0f*totsize, 2.0f*totsize);
	}

	return totsize;
}

void metaball_polygonize(Object *ob)
{
	PROCESS mbproc;
	MetaBall *mb;
	DispList *dl;
	int a, nr_cubes;
	float *ve, *no, totsize, width;
	
	mb= ob->data;

	freedisplist(&ob->disp);
	curindex= totindex= 0;
	indices= 0;
	thresh= mb->thresh;
	
	if(G.moving && mb->flag==MB_UPDATE_FAST) return;
	
	mainb= MEM_mallocN(sizeof(void *)*MB_MAXELEM, "mainb");
	
	totsize= init_meta(ob);
	if(totelem==0) {
		MEM_freeN(mainb);
		return;
	}

	/* width is size per polygonize cube */
	if(R.flag & R_RENDERING) width= mb->rendersize;
	else {
		width= mb->wiresize;
		if(G.moving && mb->flag==MB_UPDATE_HALFRES) width*= 2;
	}
	/* nr_cubes is just for safety, minimum is totsize */
	nr_cubes= (int)(0.5+totsize/width);

	/* init process */
	mbproc.function = metaball;
	mbproc.size = width;
	mbproc.bounds = nr_cubes;
	mbproc.cubes= 0;
	mbproc.delta = width/(float)(RES*RES);

	polygonize(&mbproc, mb);
	
	MEM_freeN(mainb);

	if(curindex) {
	
		dl= MEM_callocN(sizeof(DispList), "mbaldisp");
		BLI_addtail(&ob->disp, dl);
		dl->type= DL_INDEX4;
		dl->nr= mbproc.vertices.count;
		dl->parts= curindex;

		dl->index= indices;
		indices= 0;
		
		a= mbproc.vertices.count;
		dl->verts= ve= MEM_mallocN(sizeof(float)*3*a, "mballverts");
		dl->nors= no= MEM_mallocN(sizeof(float)*3*a, "mballnors");

		for(a=0; a<mbproc.vertices.count; a++, no+=3, ve+=3) {
			ve[0]= mbproc.vertices.ptr[a].position.x;
			ve[1]= mbproc.vertices.ptr[a].position.y;
			ve[2]= mbproc.vertices.ptr[a].position.z;

			no[0]= mbproc.vertices.ptr[a].normal.x;
			no[1]= mbproc.vertices.ptr[a].normal.y;
			no[2]= mbproc.vertices.ptr[a].normal.z;
		}
	}

	freepolygonize(&mbproc);
	
}


