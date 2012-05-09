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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Jiri Hnidek <jiri.hnidek@vslib.cz>.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * MetaBalls are created from a single Object (with a name without number in it),
 * here the DispList and BoundBox also is located.
 * All objects with the same name (but with a number in it) are added to this.
 *
 * texture coordinates are patched within the displist
 */

/** \file blender/blenkernel/intern/mball.c
 *  \ingroup bke
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <float.h>
#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"


#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_bpath.h"


#include "BKE_global.h"
#include "BKE_main.h"

/*  #include "BKE_object.h" */
#include "BKE_animsys.h"
#include "BKE_scene.h"
#include "BKE_library.h"
#include "BKE_displist.h"
#include "BKE_mball.h"
#include "BKE_object.h"
#include "BKE_material.h"

/* Data types */

typedef struct point {          /* a three-dimensional point */
	float x, y, z;              /* its coordinates */
} MB_POINT;

typedef struct vertex {         /* surface vertex */
	MB_POINT position, normal;  /* position and surface normal */
} VERTEX;

typedef struct vertices {       /* list of vertices in polygonization */
	int count, max;             /* # vertices, max # allowed */
	VERTEX *ptr;                /* dynamically allocated */
} VERTICES;

typedef struct corner {         /* corner of a cube */
	int i, j, k;                /* (i, j, k) is index within lattice */
	float x, y, z, value;       /* location and function value */
	struct corner *next;
} CORNER;

typedef struct cube {           /* partitioning cell (cube) */
	int i, j, k;                /* lattice location of cube */
	CORNER *corners[8];         /* eight corners */
} CUBE;

typedef struct cubes {          /* linked list of cubes acting as stack */
	CUBE cube;                  /* a single cube */
	struct cubes *next;         /* remaining elements */
} CUBES;

typedef struct centerlist {     /* list of cube locations */
	int i, j, k;                /* cube location */
	struct centerlist *next;    /* remaining elements */
} CENTERLIST;

typedef struct edgelist {       /* list of edges */
	int i1, j1, k1, i2, j2, k2; /* edge corner ids */
	int vid;                    /* vertex id */
	struct edgelist *next;      /* remaining elements */
} EDGELIST;

typedef struct intlist {        /* list of integers */
	int i;                      /* an integer */
	struct intlist *next;       /* remaining elements */
} INTLIST;

typedef struct intlists {       /* list of list of integers */
	INTLIST *list;              /* a list of integers */
	struct intlists *next;      /* remaining elements */
} INTLISTS;

typedef struct process {        /* parameters, function, storage */
	/* what happens here? floats, I think. */
	/*  float (*function)(void);	 */	/* implicit surface function */
	float (*function)(float, float, float);
	float size, delta;          /* cube size, normal delta */
	int bounds;                 /* cube range within lattice */
	CUBES *cubes;               /* active cubes */
	VERTICES vertices;          /* surface vertices */
	CENTERLIST **centers;       /* cube center hash table */
	CORNER **corners;           /* corner value hash table */
	EDGELIST **edges;           /* edge and vertex id hash table */
} PROCESS;

/* dividing scene using octal tree makes polygonisation faster */
typedef struct ml_pointer {
	struct ml_pointer *next, *prev;
	struct MetaElem *ml;
} ml_pointer;

typedef struct octal_node {
	struct octal_node *nodes[8];/* children of current node */
	struct octal_node *parent;  /* parent of current node */
	struct ListBase elems;      /* ListBase of MetaElem pointers (ml_pointer) */
	float x_min, y_min, z_min;  /* 1st border point */
	float x_max, y_max, z_max;  /* 7th border point */
	float x, y, z;              /* center of node */
	int pos, neg;               /* number of positive and negative MetaElements in the node */
	int count;                  /* number of MetaElems, which belongs to the node */
} octal_node;

typedef struct octal_tree {
	struct octal_node *first;   /* first node */
	int pos, neg;               /* number of positive and negative MetaElements in the scene */
	short depth;                /* number of scene subdivision */
} octal_tree;

struct pgn_elements {
	struct pgn_elements *next, *prev;
	char *data;
};

/* Forward declarations */
static int vertid(CORNER *c1, CORNER *c2, PROCESS *p, MetaBall *mb);
static int setcenter(CENTERLIST *table[], int i, int j, int k);
static CORNER *setcorner(PROCESS *p, int i, int j, int k);
static void converge(MB_POINT *p1, MB_POINT *p2, float v1, float v2,
                     float (*function)(float, float, float), MB_POINT *p, MetaBall *mb, int f);

/* Global variables */

static float thresh = 0.6f;
static int totelem = 0;
static MetaElem **mainb;
static octal_tree *metaball_tree = NULL;
/* Functions */

void BKE_mball_unlink(MetaBall *mb)
{
	int a;
	
	for (a = 0; a < mb->totcol; a++) {
		if (mb->mat[a]) mb->mat[a]->id.us--;
		mb->mat[a] = NULL;
	}
}


/* do not free mball itself */
void BKE_mball_free(MetaBall *mb)
{
	BKE_mball_unlink(mb);	
	
	if (mb->adt) {
		BKE_free_animdata((ID *)mb);
		mb->adt = NULL;
	}
	if (mb->mat) MEM_freeN(mb->mat);
	if (mb->bb) MEM_freeN(mb->bb);
	BLI_freelistN(&mb->elems);
	if (mb->disp.first) BKE_displist_free(&mb->disp);
}

MetaBall *BKE_mball_add(const char *name)
{
	MetaBall *mb;
	
	mb = BKE_libblock_alloc(&G.main->mball, ID_MB, name);
	
	mb->size[0] = mb->size[1] = mb->size[2] = 1.0;
	mb->texflag = MB_AUTOSPACE;
	
	mb->wiresize = 0.4f;
	mb->rendersize = 0.2f;
	mb->thresh = 0.6f;
	
	return mb;
}

MetaBall *BKE_mball_copy(MetaBall *mb)
{
	MetaBall *mbn;
	int a;
	
	mbn = BKE_libblock_copy(&mb->id);

	BLI_duplicatelist(&mbn->elems, &mb->elems);
	
	mbn->mat = MEM_dupallocN(mb->mat);
	for (a = 0; a < mbn->totcol; a++) {
		id_us_plus((ID *)mbn->mat[a]);
	}
	mbn->bb = MEM_dupallocN(mb->bb);

	mbn->editelems = NULL;
	mbn->lastelem = NULL;
	
	return mbn;
}

static void extern_local_mball(MetaBall *mb)
{
	if (mb->mat) {
		extern_local_matarar(mb->mat, mb->totcol);
	}
}

void BKE_mball_make_local(MetaBall *mb)
{
	Main *bmain = G.main;
	Object *ob;
	int is_local = FALSE, is_lib = FALSE;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */
	
	if (mb->id.lib == NULL) return;
	if (mb->id.us == 1) {
		id_clear_lib_data(bmain, &mb->id);
		extern_local_mball(mb);
		
		return;
	}

	for (ob = G.main->object.first; ob && ELEM(0, is_lib, is_local); ob = ob->id.next) {
		if (ob->data == mb) {
			if (ob->id.lib) is_lib = TRUE;
			else is_local = TRUE;
		}
	}
	
	if (is_local && is_lib == FALSE) {
		id_clear_lib_data(bmain, &mb->id);
		extern_local_mball(mb);
	}
	else if (is_local && is_lib) {
		MetaBall *mb_new = BKE_mball_copy(mb);
		mb_new->id.us = 0;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, mb->id.lib, &mb_new->id);

		for (ob = G.main->object.first; ob; ob = ob->id.next) {
			if (ob->data == mb) {
				if (ob->id.lib == NULL) {
					ob->data = mb_new;
					mb_new->id.us++;
					mb->id.us--;
				}
			}
		}
	}
}

/* most simple meta-element adding function
 * don't do context manipulation here (rna uses) */
MetaElem *BKE_mball_element_add(MetaBall *mb, const int type)
{
	MetaElem *ml = MEM_callocN(sizeof(MetaElem), "metaelem");

	unit_qt(ml->quat);

	ml->rad = 2.0;
	ml->s = 2.0;
	ml->flag = MB_SCALE_RAD;

	switch (type) {
		case MB_BALL:
			ml->type = MB_BALL;
			ml->expx = ml->expy = ml->expz = 1.0;

			break;
		case MB_TUBE:
			ml->type = MB_TUBE;
			ml->expx = ml->expy = ml->expz = 1.0;

			break;
		case MB_PLANE:
			ml->type = MB_PLANE;
			ml->expx = ml->expy = ml->expz = 1.0;

			break;
		case MB_ELIPSOID:
			ml->type = MB_ELIPSOID;
			ml->expx = 1.2f;
			ml->expy = 0.8f;
			ml->expz = 1.0;

			break;
		case MB_CUBE:
			ml->type = MB_CUBE;
			ml->expx = ml->expy = ml->expz = 1.0;

			break;
		default:
			break;
	}

	BLI_addtail(&mb->elems, ml);

	return ml;
}
/** Compute bounding box of all MetaElems/MetaBalls.
 *
 * Bounding box is computed from polygonized surface. Object *ob is
 * basic MetaBall (usually with name Meta). All other MetaBalls (with
 * names Meta.001, Meta.002, etc) are included in this Bounding Box.
 */
void BKE_mball_texspace_calc(Object *ob)
{
	DispList *dl;
	BoundBox *bb;
	float *data, min[3], max[3] /*, loc[3], size[3] */;
	int tot, doit = 0;

	if (ob->bb == NULL) ob->bb = MEM_callocN(sizeof(BoundBox), "mb boundbox");
	bb = ob->bb;
	
	/* Weird one, this. */
/*      INIT_MINMAX(min, max); */
	(min)[0] = (min)[1] = (min)[2] = 1.0e30f;
	(max)[0] = (max)[1] = (max)[2] = -1.0e30f;

	dl = ob->disp.first;
	while (dl) {
		tot = dl->nr;
		if (tot) doit = 1;
		data = dl->verts;
		while (tot--) {
			/* Also weird... but longer. From utildefines. */
			DO_MINMAX(data, min, max);
			data += 3;
		}
		dl = dl->next;
	}

	if (!doit) {
		min[0] = min[1] = min[2] = -1.0f;
		max[0] = max[1] = max[2] = 1.0f;
	}
#if 0
	loc[0] = (min[0] + max[0]) / 2.0f;
	loc[1] = (min[1] + max[1]) / 2.0f;
	loc[2] = (min[2] + max[2]) / 2.0f;

	size[0] = (max[0] - min[0]) / 2.0f;
	size[1] = (max[1] - min[1]) / 2.0f;
	size[2] = (max[2] - min[2]) / 2.0f;
#endif
	BKE_boundbox_init_from_minmax(bb, min, max);
}

float *BKE_mball_make_orco(Object *ob, ListBase *dispbase)
{
	BoundBox *bb;
	DispList *dl;
	float *data, *orco, *orcodata;
	float loc[3], size[3];
	int a;

	/* restore size and loc */
	bb = ob->bb;
	loc[0] = (bb->vec[0][0] + bb->vec[4][0]) / 2.0f;
	size[0] = bb->vec[4][0] - loc[0];
	loc[1] = (bb->vec[0][1] + bb->vec[2][1]) / 2.0f;
	size[1] = bb->vec[2][1] - loc[1];
	loc[2] = (bb->vec[0][2] + bb->vec[1][2]) / 2.0f;
	size[2] = bb->vec[1][2] - loc[2];

	dl = dispbase->first;
	orcodata = MEM_mallocN(sizeof(float) * 3 * dl->nr, "MballOrco");

	data = dl->verts;
	orco = orcodata;
	a = dl->nr;
	while (a--) {
		orco[0] = (data[0] - loc[0]) / size[0];
		orco[1] = (data[1] - loc[1]) / size[1];
		orco[2] = (data[2] - loc[2]) / size[2];

		data += 3;
		orco += 3;
	}

	return orcodata;
}

/* Note on mball basis stuff 2.5x (this is a can of worms)
 * This really needs a rewrite/refactor its totally broken in anything other then basic cases
 * Multiple Scenes + Set Scenes & mixing mball basis SHOULD work but fails to update the depsgraph on rename
 * and linking into scenes or removal of basis mball. so take care when changing this code.
 * 
 * Main idiot thing here is that the system returns find_basis_mball() objects which fail a is_basis_mball() test.
 *
 * Not only that but the depsgraph and their areas depend on this behavior!, so making small fixes here isn't worth it.
 * - Campbell
 */


/** \brief Test, if Object *ob is basic MetaBall.
 *
 * It test last character of Object ID name. If last character
 * is digit it return 0, else it return 1.
 */
int BKE_mball_is_basis(Object *ob)
{
	int len;
	
	/* just a quick test */
	len = strlen(ob->id.name);
	if (isdigit(ob->id.name[len - 1]) ) return 0;
	return 1;
}

/* return nonzero if ob1 is a basis mball for ob */
int BKE_mball_is_basis_for(Object *ob1, Object *ob2)
{
	int basis1nr, basis2nr;
	char basis1name[MAX_ID_NAME], basis2name[MAX_ID_NAME];

	BLI_split_name_num(basis1name, &basis1nr, ob1->id.name + 2, '.');
	BLI_split_name_num(basis2name, &basis2nr, ob2->id.name + 2, '.');

	if (!strcmp(basis1name, basis2name)) return BKE_mball_is_basis(ob1);
	else return 0;
}

/* \brief copy some properties from object to other metaball object with same base name
 *
 * When some properties (wiresize, threshold, update flags) of metaball are changed, then this properties
 * are copied to all metaballs in same "group" (metaballs with same base name: MBall,
 * MBall.001, MBall.002, etc). The most important is to copy properties to the base metaball,
 * because this metaball influence polygonisation of metaballs. */
void BKE_mball_properties_copy(Scene *scene, Object *active_object)
{
	Scene *sce_iter = scene;
	Base *base;
	Object *ob;
	MetaBall *active_mball = (MetaBall *)active_object->data;
	int basisnr, obnr;
	char basisname[MAX_ID_NAME], obname[MAX_ID_NAME];
	
	BLI_split_name_num(basisname, &basisnr, active_object->id.name + 2, '.');

	/* XXX recursion check, see scene.c, just too simple code this BKE_scene_base_iter_next() */
	if (F_ERROR == BKE_scene_base_iter_next(&sce_iter, 0, NULL, NULL))
		return;
	
	while (BKE_scene_base_iter_next(&sce_iter, 1, &base, &ob)) {
		if (ob->type == OB_MBALL) {
			if (ob != active_object) {
				BLI_split_name_num(obname, &obnr, ob->id.name + 2, '.');

				/* Object ob has to be in same "group" ... it means, that it has to have
				 * same base of its name */
				if (strcmp(obname, basisname) == 0) {
					MetaBall *mb = ob->data;

					/* Copy properties from selected/edited metaball */
					mb->wiresize = active_mball->wiresize;
					mb->rendersize = active_mball->rendersize;
					mb->thresh = active_mball->thresh;
					mb->flag = active_mball->flag;
				}
			}
		}
	}
}

/** \brief This function finds basic MetaBall.
 *
 * Basic MetaBall doesn't include any number at the end of
 * its name. All MetaBalls with same base of name can be
 * blended. MetaBalls with different basic name can't be
 * blended.
 *
 * warning!, is_basis_mball() can fail on returned object, see long note above.
 */
Object *BKE_mball_basis_find(Scene *scene, Object *basis)
{
	Scene *sce_iter = scene;
	Base *base;
	Object *ob, *bob = basis;
	MetaElem *ml = NULL;
	int basisnr, obnr;
	char basisname[MAX_ID_NAME], obname[MAX_ID_NAME];

	BLI_split_name_num(basisname, &basisnr, basis->id.name + 2, '.');
	totelem = 0;

	/* XXX recursion check, see scene.c, just too simple code this BKE_scene_base_iter_next() */
	if (F_ERROR == BKE_scene_base_iter_next(&sce_iter, 0, NULL, NULL))
		return NULL;
	
	while (BKE_scene_base_iter_next(&sce_iter, 1, &base, &ob)) {
		
		if (ob->type == OB_MBALL) {
			if (ob == bob) {
				MetaBall *mb = ob->data;
				
				/* if bob object is in edit mode, then dynamic list of all MetaElems
				 * is stored in editelems */
				if (mb->editelems) ml = mb->editelems->first;
				/* if bob object is in object mode */
				else ml = mb->elems.first;
			}
			else {
				BLI_split_name_num(obname, &obnr, ob->id.name + 2, '.');

				/* object ob has to be in same "group" ... it means, that it has to have
				 * same base of its name */
				if (strcmp(obname, basisname) == 0) {
					MetaBall *mb = ob->data;
					
					/* if object is in edit mode, then dynamic list of all MetaElems
					 * is stored in editelems */
					if (mb->editelems) ml = mb->editelems->first;
					/* if bob object is in object mode */
					else ml = mb->elems.first;
					
					if (obnr < basisnr) {
						if (!(ob->flag & OB_FROMDUPLI)) {
							basis = ob;
							basisnr = obnr;
						}
					}	
				}
			}
			
			while (ml) {
				if (!(ml->flag & MB_HIDE)) totelem++;
				ml = ml->next;
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
 *
 * Authored by Jules Bloomenthal, Xerox PARC.
 * Copyright (c) Xerox Corporation, 1991.  All rights reserved.
 * Permission is granted to reproduce, use and distribute this code for
 * any and all purposes, provided that this notice appears in all copies. */

#define RES 12 /* # converge iterations    */

#define L   0  /* left direction:	-x, -i */
#define R   1  /* right direction:	+x, +i */
#define B   2  /* bottom direction: -y, -j */
#define T   3  /* top direction:	+y, +j */
#define N   4  /* near direction:	-z, -k */
#define F   5  /* far direction:	+z, +k */
#define LBN 0  /* left bottom near corner  */
#define LBF 1  /* left bottom far corner   */
#define LTN 2  /* left top near corner     */
#define LTF 3  /* left top far corner      */
#define RBN 4  /* right bottom near corner */
#define RBF 5  /* right bottom far corner  */
#define RTN 6  /* right top near corner    */
#define RTF 7  /* right top far corner     */

/* the LBN corner of cube (i, j, k), corresponds with location
 * (i-0.5)*size, (j-0.5)*size, (k-0.5)*size) */

#define HASHBIT     (5)
#define HASHSIZE    (size_t)(1 << (3 * HASHBIT))   /*! < hash table size (32768) */

#define HASH(i, j, k) ((((( (i) & 31) << 5) | ( (j) & 31)) << 5) | ( (k) & 31) )

#define MB_BIT(i, bit) (((i) >> (bit)) & 1)
#define FLIP(i, bit) ((i) ^ 1 << (bit)) /* flip the given bit of i */


/* **************** POLYGONIZATION ************************ */

static void calc_mballco(MetaElem *ml, float vec[3])
{
	if (ml->mat) {
		mul_m4_v3((float (*)[4])ml->mat, vec);
	}
}

static float densfunc(MetaElem *ball, float x, float y, float z)
{
	float dist2 = 0.0, dx, dy, dz;
	float vec[3];

	vec[0] = x;
	vec[1] = y;
	vec[2] = z;
	mul_m4_v3((float (*)[4])ball->imat, vec);
	dx = vec[0];
	dy = vec[1];
	dz = vec[2];

	if (ball->type == MB_BALL) {
	}
	else if (ball->type == MB_TUBEX) {
		if (dx > ball->len) dx -= ball->len;
		else if (dx < -ball->len) dx += ball->len;
		else dx = 0.0;
	}
	else if (ball->type == MB_TUBEY) {
		if (dy > ball->len) dy -= ball->len;
		else if (dy < -ball->len) dy += ball->len;
		else dy = 0.0;
	}
	else if (ball->type == MB_TUBEZ) {
		if (dz > ball->len) dz -= ball->len;
		else if (dz < -ball->len) dz += ball->len;
		else dz = 0.0;
	}
	else if (ball->type == MB_TUBE) {
		if (dx > ball->expx) dx -= ball->expx;
		else if (dx < -ball->expx) dx += ball->expx;
		else dx = 0.0;
	}
	else if (ball->type == MB_PLANE) {
		if (dx > ball->expx) dx -= ball->expx;
		else if (dx < -ball->expx) dx += ball->expx;
		else dx = 0.0;
		if (dy > ball->expy) dy -= ball->expy;
		else if (dy < -ball->expy) dy += ball->expy;
		else dy = 0.0;
	}
	else if (ball->type == MB_ELIPSOID) {
		dx *= 1 / ball->expx;
		dy *= 1 / ball->expy;
		dz *= 1 / ball->expz;
	}
	else if (ball->type == MB_CUBE) {
		if (dx > ball->expx) dx -= ball->expx;
		else if (dx < -ball->expx) dx += ball->expx;
		else dx = 0.0;
		if (dy > ball->expy) dy -= ball->expy;
		else if (dy < -ball->expy) dy += ball->expy;
		else dy = 0.0;
		if (dz > ball->expz) dz -= ball->expz;
		else if (dz < -ball->expz) dz += ball->expz;
		else dz = 0.0;
	}

	dist2 = (dx * dx + dy * dy + dz * dz);

	if (ball->flag & MB_NEGATIVE) {
		dist2 = 1.0f - (dist2 / ball->rad2);
		if (dist2 < 0.0f) return 0.5f;

		return 0.5f - ball->s * dist2 * dist2 * dist2;
	}
	else {
		dist2 = 1.0f - (dist2 / ball->rad2);
		if (dist2 < 0.0f) return -0.5f;

		return ball->s * dist2 * dist2 * dist2 - 0.5f;
	}
}

static octal_node *find_metaball_octal_node(octal_node *node, float x, float y, float z, short depth)
{
	if (!depth) return node;
	
	if (z < node->z) {
		if (y < node->y) {
			if (x < node->x) {
				if (node->nodes[0])
					return find_metaball_octal_node(node->nodes[0], x, y, z, depth--);
				else
					return node;
			}
			else {
				if (node->nodes[1])
					return find_metaball_octal_node(node->nodes[1], x, y, z, depth--);
				else
					return node;
			}
		}
		else {
			if (x < node->x) {
				if (node->nodes[3])
					return find_metaball_octal_node(node->nodes[3], x, y, z, depth--);
				else
					return node;
			}
			else {
				if (node->nodes[2])
					return find_metaball_octal_node(node->nodes[2], x, y, z, depth--);
				else
					return node;
			}		
		}
	}
	else {
		if (y < node->y) {
			if (x < node->x) {
				if (node->nodes[4])
					return find_metaball_octal_node(node->nodes[4], x, y, z, depth--);
				else
					return node;
			}
			else {
				if (node->nodes[5])
					return find_metaball_octal_node(node->nodes[5], x, y, z, depth--);
				else
					return node;
			}
		}
		else {
			if (x < node->x) {
				if (node->nodes[7])
					return find_metaball_octal_node(node->nodes[7], x, y, z, depth--);
				else
					return node;
			}
			else {
				if (node->nodes[6])
					return find_metaball_octal_node(node->nodes[6], x, y, z, depth--);
				else
					return node;
			}		
		}
	}
	
	return node;
}

static float metaball(float x, float y, float z)
/*  float x, y, z; */
{
	struct octal_node *node;
	struct ml_pointer *ml_p;
	float dens = 0;
	int a;
	
	if (totelem > 1) {
		node = find_metaball_octal_node(metaball_tree->first, x, y, z, metaball_tree->depth);
		if (node) {
			ml_p = node->elems.first;

			while (ml_p) {
				dens += densfunc(ml_p->ml, x, y, z);
				ml_p = ml_p->next;
			}

			dens += -0.5f * (metaball_tree->pos - node->pos);
			dens += 0.5f * (metaball_tree->neg - node->neg);
		}
		else {
			for (a = 0; a < totelem; a++) {
				dens += densfunc(mainb[a], x, y, z);
			}
		}
	}
	else {
		dens += densfunc(mainb[0], x, y, z);
	}

	return thresh - dens;
}

/* ******************************************** */

static int *indices = NULL;
static int totindex, curindex;


static void accum_mballfaces(int i1, int i2, int i3, int i4)
{
	int *newi, *cur;
	/* static int i=0; I would like to delete altogether, but I don't dare to, yet */

	if (totindex == curindex) {
		totindex += 256;
		newi = MEM_mallocN(4 * sizeof(int) * totindex, "vertindex");
		
		if (indices) {
			memcpy(newi, indices, 4 * sizeof(int) * (totindex - 256));
			MEM_freeN(indices);
		}
		indices = newi;
	}
	
	cur = indices + 4 * curindex;

	/* displists now support array drawing, we treat tri's as fake quad */
	
	cur[0] = i1;
	cur[1] = i2;
	cur[2] = i3;
	if (i4 == 0)
		cur[3] = i3;
	else 
		cur[3] = i4;
	
	curindex++;

}

/* ******************* MEMORY MANAGEMENT *********************** */
static void *new_pgn_element(int size)
{
	/* during polygonize 1000s of elements are allocated
	 * and never freed in between. Freeing only done at the end.
	 */
	int blocksize = 16384;
	static int offs = 0;     /* the current free address */
	static struct pgn_elements *cur = NULL;
	static ListBase lb = {NULL, NULL};
	void *adr;
	
	if (size > 10000 || size == 0) {
		printf("incorrect use of new_pgn_element\n");
	}
	else if (size == -1) {
		cur = lb.first;
		while (cur) {
			MEM_freeN(cur->data);
			cur = cur->next;
		}
		BLI_freelistN(&lb);
		
		return NULL;	
	}
	
	size = 4 * ( (size + 3) / 4);
	
	if (cur) {
		if (size + offs < blocksize) {
			adr = (void *) (cur->data + offs);
			offs += size;
			return adr;
		}
	}
	
	cur = MEM_callocN(sizeof(struct pgn_elements), "newpgn");
	cur->data = MEM_callocN(blocksize, "newpgn");
	BLI_addtail(&lb, cur);
	
	offs = size;
	return cur->data;
}

static void freepolygonize(PROCESS *p)
{
	MEM_freeN(p->corners);
	MEM_freeN(p->edges);
	MEM_freeN(p->centers);

	new_pgn_element(-1);
	
	if (p->vertices.ptr) MEM_freeN(p->vertices.ptr);
}

/**** Cubical Polygonization (optional) ****/

#define LB  0  /* left bottom edge	*/
#define LT  1  /* left top edge	*/
#define LN  2  /* left near edge	*/
#define LF  3  /* left far edge	*/
#define RB  4  /* right bottom edge */
#define RT  5  /* right top edge	*/
#define RN  6  /* right near edge	*/
#define RF  7  /* right far edge	*/
#define BN  8  /* bottom near edge	*/
#define BF  9  /* bottom far edge	*/
#define TN  10 /* top near edge	*/
#define TF  11 /* top far edge	*/

static INTLISTS *cubetable[256];

/* edge: LB, LT, LN, LF, RB, RT, RN, RF, BN, BF, TN, TF */
static int corner1[12] = {
	LBN, LTN, LBN, LBF, RBN, RTN, RBN, RBF, LBN, LBF, LTN, LTF
};
static int corner2[12] = {
	LBF, LTF, LTN, LTF, RBF, RTF, RTN, RTF, RBN, RBF, RTN, RTF
};
static int leftface[12] = {
	B,  L,  L,  F,  R,  T,  N,  R,  N,  B,  T,  F
};
/* face on left when going corner1 to corner2 */
static int rightface[12] = {
	L,  T,  N,  L,  B,  R,  R,  F,  B,  F,  N,  T
};
/* face on right when going corner1 to corner2 */


/* docube: triangulate the cube directly, without decomposition */

static void docube(CUBE *cube, PROCESS *p, MetaBall *mb)
{
	INTLISTS *polys;
	CORNER *c1, *c2;
	int i, index = 0, count, indexar[8];
	
	for (i = 0; i < 8; i++) if (cube->corners[i]->value > 0.0f) index += (1 << i);
	
	for (polys = cubetable[index]; polys; polys = polys->next) {
		INTLIST *edges;
		
		count = 0;
		
		for (edges = polys->list; edges; edges = edges->next) {
			c1 = cube->corners[corner1[edges->i]];
			c2 = cube->corners[corner2[edges->i]];
			
			indexar[count] = vertid(c1, c2, p, mb);
			count++;
		}
		if (count > 2) {
			switch (count) {
				case 3:
					accum_mballfaces(indexar[2], indexar[1], indexar[0], 0);
					break;
				case 4:
					if (indexar[0] == 0) accum_mballfaces(indexar[0], indexar[3], indexar[2], indexar[1]);
					else accum_mballfaces(indexar[3], indexar[2], indexar[1], indexar[0]);
					break;
				case 5:
					if (indexar[0] == 0) accum_mballfaces(indexar[0], indexar[3], indexar[2], indexar[1]);
					else accum_mballfaces(indexar[3], indexar[2], indexar[1], indexar[0]);
				
					accum_mballfaces(indexar[4], indexar[3], indexar[0], 0);
					break;
				case 6:
					if (indexar[0] == 0) {
						accum_mballfaces(indexar[0], indexar[3], indexar[2], indexar[1]);
						accum_mballfaces(indexar[0], indexar[5], indexar[4], indexar[3]);
					}
					else {
						accum_mballfaces(indexar[3], indexar[2], indexar[1], indexar[0]);
						accum_mballfaces(indexar[5], indexar[4], indexar[3], indexar[0]);
					}
					break;
				case 7:
					if (indexar[0] == 0) {
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

static void testface(int i, int j, int k, CUBE *old, int bit, int c1, int c2, int c3, int c4, PROCESS *p)
{
	CUBE newc;
	CUBES *oldcubes = p->cubes;
	CORNER *corn1, *corn2, *corn3, *corn4;
	int n, pos;

	corn1 = old->corners[c1];
	corn2 = old->corners[c2];
	corn3 = old->corners[c3];
	corn4 = old->corners[c4];
	
	pos = corn1->value > 0.0f ? 1 : 0;

	/* test if no surface crossing */
	if ( (corn2->value > 0) == pos && (corn3->value > 0) == pos && (corn4->value > 0) == pos) return;
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

	if (newc.corners[0] == NULL) newc.corners[0] = setcorner(p, i, j, k);
	if (newc.corners[1] == NULL) newc.corners[1] = setcorner(p, i, j, k + 1);
	if (newc.corners[2] == NULL) newc.corners[2] = setcorner(p, i, j + 1, k);
	if (newc.corners[3] == NULL) newc.corners[3] = setcorner(p, i, j + 1, k + 1);
	if (newc.corners[4] == NULL) newc.corners[4] = setcorner(p, i + 1, j, k);
	if (newc.corners[5] == NULL) newc.corners[5] = setcorner(p, i + 1, j, k + 1);
	if (newc.corners[6] == NULL) newc.corners[6] = setcorner(p, i + 1, j + 1, k);
	if (newc.corners[7] == NULL) newc.corners[7] = setcorner(p, i + 1, j + 1, k + 1);

	p->cubes->cube = newc;
}

/* setcorner: return corner with the given lattice location
 * set (and cache) its function value */

static CORNER *setcorner(PROCESS *p, int i, int j, int k)
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
	c->x = ((float)i - 0.5f) * p->size;
	c->j = j; 
	c->y = ((float)j - 0.5f) * p->size;
	c->k = k; 
	c->z = ((float)k - 0.5f) * p->size;
	c->value = p->function(c->x, c->y, c->z);
	
	c->next = p->corners[index];
	p->corners[index] = c;
	
	return c;
}


/* nextcwedge: return next clockwise edge from given edge around given face */

static int nextcwedge(int edge, int face)
{
	switch (edge) {
		case LB:
			return (face == L) ? LF : BN;
		case LT:
			return (face == L) ? LN : TF;
		case LN:
			return (face == L) ? LB : TN;
		case LF:
			return (face == L) ? LT : BF;
		case RB:
			return (face == R) ? RN : BF;
		case RT:
			return (face == R) ? RF : TN;
		case RN:
			return (face == R) ? RT : BN;
		case RF:
			return (face == R) ? RB : TF;
		case BN:
			return (face == B) ? RB : LN;
		case BF:
			return (face == B) ? LB : RF;
		case TN:
			return (face == T) ? LT : RN;
		case TF:
			return (face == T) ? RT : LF;
	}
	return 0;
}


/* otherface: return face adjoining edge that is not the given face */

static int otherface(int edge, int face)
{
	int other = leftface[edge];
	return face == other ? rightface[edge] : other;
}


/* makecubetable: create the 256 entry table for cubical polygonization */

static void makecubetable(void)
{
	static int isdone = 0;
	int i, e, c, done[12], pos[8];

	if (isdone) return;
	isdone = 1;

	for (i = 0; i < 256; i++) {
		for (e = 0; e < 12; e++) done[e] = 0;
		for (c = 0; c < 8; c++) pos[c] = MB_BIT(i, c);
		for (e = 0; e < 12; e++)
			if (!done[e] && (pos[corner1[e]] != pos[corner2[e]])) {
				INTLIST *ints = NULL;
				INTLISTS *lists = (INTLISTS *) MEM_callocN(sizeof(INTLISTS), "mball_intlist");
				int start = e, edge = e;
				
				/* get face that is to right of edge from pos to neg corner: */
				int face = pos[corner1[e]] ? rightface[e] : leftface[e];
				
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

void BKE_mball_cubeTable_free(void)
{
	int i;
	INTLISTS *lists, *nlists;
	INTLIST *ints, *nints;

	for (i = 0; i < 256; i++) {
		lists = cubetable[i];
		while (lists) {
			nlists = lists->next;
			
			ints = lists->list;
			while (ints) {
				nints = ints->next;
				MEM_freeN(ints);
				ints = nints;
			}
			
			MEM_freeN(lists);
			lists = nlists;
		}
		cubetable[i] = NULL;
	}
}

/**** Storage ****/

/* setcenter: set (i, j, k) entry of table[]
 * return 1 if already set; otherwise, set and return 0 */

static int setcenter(CENTERLIST *table[], int i, int j, int k)
{
	int index;
	CENTERLIST *newc, *l, *q;

	index = HASH(i, j, k);
	q = table[index];

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

static void setedge(EDGELIST *table[],
                    int i1, int j1,
                    int k1, int i2,
                    int j2, int k2,
                    int vid)
{
	unsigned int index;
	EDGELIST *newe;
	
	if (i1 > i2 || (i1 == i2 && (j1 > j2 || (j1 == j2 && k1 > k2)))) {
		int t = i1;
		i1 = i2;
		i2 = t;
		t = j1;
		j1 = j2;
		j2 = t;
		t = k1;
		k1 = k2;
		k2 = t;
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

static int getedge(EDGELIST *table[],
                   int i1, int j1, int k1,
                   int i2, int j2, int k2)
{
	EDGELIST *q;
	
	if (i1 > i2 || (i1 == i2 && (j1 > j2 || (j1 == j2 && k1 > k2)))) {
		int t = i1;
		i1 = i2;
		i2 = t;
		t = j1;
		j1 = j2;
		j2 = t;
		t = k1;
		k1 = k2;
		k2 = t;
	}
	q = table[HASH(i1, j1, k1) + HASH(i2, j2, k2)];
	for (; q != NULL; q = q->next) {
		if (q->i1 == i1 && q->j1 == j1 && q->k1 == k1 &&
		    q->i2 == i2 && q->j2 == j2 && q->k2 == k2)
		{
			return q->vid;
		}
	}
	return -1;
}


/**** Vertices ****/

#undef R



/* vertid: return index for vertex on edge:
 * c1->value and c2->value are presumed of different sign
 * return saved index if any; else compute vertex and save */

/* addtovertices: add v to sequence of vertices */

static void addtovertices(VERTICES *vertices, VERTEX v)
{
	if (vertices->count == vertices->max) {
		int i;
		VERTEX *newv;
		vertices->max = vertices->count == 0 ? 10 : 2 * vertices->count;
		newv = (VERTEX *) MEM_callocN(vertices->max * sizeof(VERTEX), "addtovertices");
		
		for (i = 0; i < vertices->count; i++) newv[i] = vertices->ptr[i];
		
		if (vertices->ptr != NULL) MEM_freeN(vertices->ptr);
		vertices->ptr = newv;
	}
	vertices->ptr[vertices->count++] = v;
}

/* vnormal: compute unit length surface normal at point */

static void vnormal(MB_POINT *point, PROCESS *p, MB_POINT *v)
{
	float delta = 0.2f * p->delta;
	float f = p->function(point->x, point->y, point->z);

	v->x = p->function(point->x + delta, point->y, point->z) - f;
	v->y = p->function(point->x, point->y + delta, point->z) - f;
	v->z = p->function(point->x, point->y, point->z + delta) - f;
	f = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);

	if (f != 0.0f) {
		v->x /= f; 
		v->y /= f; 
		v->z /= f;
	}
	
	if (FALSE) {
		MB_POINT temp;
		
		delta *= 2.0f;
		
		f = p->function(point->x, point->y, point->z);
	
		temp.x = p->function(point->x + delta, point->y, point->z) - f;
		temp.y = p->function(point->x, point->y + delta, point->z) - f;
		temp.z = p->function(point->x, point->y, point->z + delta) - f;
		f = sqrtf(temp.x * temp.x + temp.y * temp.y + temp.z * temp.z);
	
		if (f != 0.0f) {
			temp.x /= f; 
			temp.y /= f; 
			temp.z /= f;
			
			v->x += temp.x;
			v->y += temp.y;
			v->z += temp.z;
			
			f = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
		
			if (f != 0.0f) {
				v->x /= f; 
				v->y /= f; 
				v->z /= f;
			}
		}
	}
	
}


static int vertid(CORNER *c1, CORNER *c2, PROCESS *p, MetaBall *mb)
{
	VERTEX v;
	MB_POINT a, b;
	int vid = getedge(p->edges, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k);

	if (vid != -1) return vid;               /* previously computed */
	a.x = c1->x; 
	a.y = c1->y; 
	a.z = c1->z;
	b.x = c2->x; 
	b.y = c2->y; 
	b.z = c2->z;

	converge(&a, &b, c1->value, c2->value, p->function, &v.position, mb, 1); /* position */
	vnormal(&v.position, p, &v.normal);

	addtovertices(&p->vertices, v);            /* save vertex */
	vid = p->vertices.count - 1;
	setedge(p->edges, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k, vid);
	
	return vid;
}




/* converge: from two points of differing sign, converge to zero crossing */
/* watch it: p1 and p2 are used to calculate */
static void converge(MB_POINT *p1, MB_POINT *p2, float v1, float v2,
                     float (*function)(float, float, float), MB_POINT *p, MetaBall *mb, int f)
{
	int i = 0;
	MB_POINT pos, neg;
	float positive = 0.0f, negative = 0.0f;
	float dx = 0.0f, dy = 0.0f, dz = 0.0f;
	
	if (v1 < 0) {
		pos = *p2;
		neg = *p1;
		positive = v2;
		negative = v1;
	}
	else {
		pos = *p1;
		neg = *p2;
		positive = v1;
		negative = v2;
	}

	dx = pos.x - neg.x;
	dy = pos.y - neg.y;
	dz = pos.z - neg.z;

/* Approximation by linear interpolation is faster then binary subdivision,
 * but it results sometimes (mb->thresh < 0.2) into the strange results */
	if ((mb->thresh > 0.2f) && (f == 1)) {
		if ((dy == 0.0f) && (dz == 0.0f)) {
			p->x = neg.x - negative * dx / (positive - negative);
			p->y = neg.y;
			p->z = neg.z;
			return;
		}
		if ((dx == 0.0f) && (dz == 0.0f)) {
			p->x = neg.x;
			p->y = neg.y - negative * dy / (positive - negative);
			p->z = neg.z;
			return;
		}
		if ((dx == 0.0f) && (dy == 0.0f)) {
			p->x = neg.x;
			p->y = neg.y;
			p->z = neg.z - negative * dz / (positive - negative);
			return;
		}
	}

	if ((dy == 0.0f) && (dz == 0.0f)) {
		p->y = neg.y;
		p->z = neg.z;
		while (1) {
			if (i++ == RES) return;
			p->x = 0.5f * (pos.x + neg.x);
			if ((function(p->x, p->y, p->z)) > 0.0f) pos.x = p->x; else neg.x = p->x;
		}
	}

	if ((dx == 0.0f) && (dz == 0.0f)) {
		p->x = neg.x;
		p->z = neg.z;
		while (1) {
			if (i++ == RES) return;
			p->y = 0.5f * (pos.y + neg.y);
			if ((function(p->x, p->y, p->z)) > 0.0f) pos.y = p->y; else neg.y = p->y;
		}
	}
   
	if ((dx == 0.0f) && (dy == 0.0f)) {
		p->x = neg.x;
		p->y = neg.y;
		while (1) {
			if (i++ == RES) return;
			p->z = 0.5f * (pos.z + neg.z);
			if ((function(p->x, p->y, p->z)) > 0.0f) pos.z = p->z; else neg.z = p->z;
		}
	}

	/* This is necessary to find start point */
	while (1) {
		p->x = 0.5f * (pos.x + neg.x);
		p->y = 0.5f * (pos.y + neg.y);
		p->z = 0.5f * (pos.z + neg.z);

		if (i++ == RES) return;
   
		if ((function(p->x, p->y, p->z)) > 0.0f) {
			pos.x = p->x;
			pos.y = p->y;
			pos.z = p->z;
		}
		else {
			neg.x = p->x;
			neg.y = p->y;
			neg.z = p->z;
		}
	}
}

/* ************************************** */
static void add_cube(PROCESS *mbproc, int i, int j, int k, int count)
{
	CUBES *ncube;
	int n;
	int a, b, c;

	/* hmmm, not only one, but eight cube will be added on the stack 
	 * ... */
	for (a = i - 1; a < i + count; a++)
		for (b = j - 1; b < j + count; b++)
			for (c = k - 1; c < k + count; c++) {
				/* test if cube has been found before */
				if (setcenter(mbproc->centers, a, b, c) == 0) {
					/* push cube on stack: */
					ncube = (CUBES *) new_pgn_element(sizeof(CUBES));
					ncube->next = mbproc->cubes;
					mbproc->cubes = ncube;

					ncube->cube.i = a;
					ncube->cube.j = b;
					ncube->cube.k = c;

					/* set corners of initial cube: */
					for (n = 0; n < 8; n++)
						ncube->cube.corners[n] = setcorner(mbproc, a + MB_BIT(n, 2), b + MB_BIT(n, 1), c + MB_BIT(n, 0));
				}
			}
}


static void find_first_points(PROCESS *mbproc, MetaBall *mb, int a)
{
	MB_POINT IN, in, OUT, out; /*point;*/
	MetaElem *ml;
	int i, j, k, c_i, c_j, c_k;
	int index[3] = {1, 0, -1};
	float f = 0.0f;
	float in_v /*, out_v*/;
	MB_POINT workp;
	float tmp_v, workp_v, max_len, len, dx, dy, dz, nx, ny, nz, MAXN;

	ml = mainb[a];

	f = 1 - (mb->thresh / ml->s);

	/* Skip, when Stiffness of MetaElement is too small ... MetaElement can't be
	 * visible alone ... but still can influence others MetaElements :-) */
	if (f > 0.0f) {
		OUT.x = IN.x = in.x = 0.0;
		OUT.y = IN.y = in.y = 0.0;
		OUT.z = IN.z = in.z = 0.0;

		calc_mballco(ml, (float *)&in);
		in_v = mbproc->function(in.x, in.y, in.z);

		for (i = 0; i < 3; i++) {
			switch (ml->type) {
				case MB_BALL:
					OUT.x = out.x = IN.x + index[i] * ml->rad;
					break;
				case MB_TUBE:
				case MB_PLANE:
				case MB_ELIPSOID:
				case MB_CUBE:
					OUT.x = out.x = IN.x + index[i] * (ml->expx + ml->rad);
					break;
			}

			for (j = 0; j < 3; j++) {
				switch (ml->type) {
					case MB_BALL:
						OUT.y = out.y = IN.y + index[j] * ml->rad;
						break;
					case MB_TUBE:
					case MB_PLANE:
					case MB_ELIPSOID:
					case MB_CUBE:
						OUT.y = out.y = IN.y + index[j] * (ml->expy + ml->rad);
						break;
				}
			
				for (k = 0; k < 3; k++) {
					out.x = OUT.x;
					out.y = OUT.y;
					switch (ml->type) {
						case MB_BALL:
						case MB_TUBE:
						case MB_PLANE:
							out.z = IN.z + index[k] * ml->rad;
							break;
						case MB_ELIPSOID:
						case MB_CUBE:
							out.z = IN.z + index[k] * (ml->expz + ml->rad);
							break;
					}

					calc_mballco(ml, (float *)&out);

					/*out_v = mbproc->function(out.x, out.y, out.z);*/ /*UNUSED*/

					/* find "first points" on Implicit Surface of MetaElemnt ml */
					workp.x = in.x;
					workp.y = in.y;
					workp.z = in.z;
					workp_v = in_v;
					max_len = sqrtf((out.x - in.x) * (out.x - in.x) + (out.y - in.y) * (out.y - in.y) + (out.z - in.z) * (out.z - in.z));

					nx = abs((out.x - in.x) / mbproc->size);
					ny = abs((out.y - in.y) / mbproc->size);
					nz = abs((out.z - in.z) / mbproc->size);
					
					MAXN = MAX3(nx, ny, nz);
					if (MAXN != 0.0f) {
						dx = (out.x - in.x) / MAXN;
						dy = (out.y - in.y) / MAXN;
						dz = (out.z - in.z) / MAXN;

						len = 0.0;
						while (len <= max_len) {
							workp.x += dx;
							workp.y += dy;
							workp.z += dz;
							/* compute value of implicite function */
							tmp_v = mbproc->function(workp.x, workp.y, workp.z);
							/* add cube to the stack, when value of implicite function crosses zero value */
							if ((tmp_v < 0.0f && workp_v >= 0.0f) || (tmp_v > 0.0f && workp_v <= 0.0f)) {

								/* indexes of CUBE, which includes "first point" */
								c_i = (int)floor(workp.x / mbproc->size);
								c_j = (int)floor(workp.y / mbproc->size);
								c_k = (int)floor(workp.z / mbproc->size);
								
								/* add CUBE (with indexes c_i, c_j, c_k) to the stack,
								 * this cube includes found point of Implicit Surface */
								if (ml->flag & MB_NEGATIVE)
									add_cube(mbproc, c_i, c_j, c_k, 2);
								else
									add_cube(mbproc, c_i, c_j, c_k, 1);
							}
							len = sqrtf((workp.x - in.x) * (workp.x - in.x) + (workp.y - in.y) * (workp.y - in.y) + (workp.z - in.z) * (workp.z - in.z));
							workp_v = tmp_v;

						}
					}
				}
			}
		}
	}
}

static void polygonize(PROCESS *mbproc, MetaBall *mb)
{
	CUBE c;
	int a;

	mbproc->vertices.count = mbproc->vertices.max = 0;
	mbproc->vertices.ptr = NULL;

	/* allocate hash tables and build cube polygon table: */
	mbproc->centers = MEM_callocN(HASHSIZE * sizeof(CENTERLIST *), "mbproc->centers");
	mbproc->corners = MEM_callocN(HASHSIZE * sizeof(CORNER *), "mbproc->corners");
	mbproc->edges = MEM_callocN(2 * HASHSIZE * sizeof(EDGELIST *), "mbproc->edges");
	makecubetable();

	for (a = 0; a < totelem; a++) {

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
		testface(c.i - 1, c.j, c.k, &c, 2, LBN, LBF, LTN, LTF, mbproc);
		testface(c.i + 1, c.j, c.k, &c, 2, RBN, RBF, RTN, RTF, mbproc);
		testface(c.i, c.j - 1, c.k, &c, 1, LBN, LBF, RBN, RBF, mbproc);
		testface(c.i, c.j + 1, c.k, &c, 1, LTN, LTF, RTN, RTF, mbproc);
		testface(c.i, c.j, c.k - 1, &c, 0, LBN, LTN, RBN, RTN, mbproc);
		testface(c.i, c.j, c.k + 1, &c, 0, LBF, LTF, RBF, RTF, mbproc);
	}
}

static float init_meta(Scene *scene, Object *ob)    /* return totsize */
{
	Scene *sce_iter = scene;
	Base *base;
	Object *bob;
	MetaBall *mb;
	MetaElem *ml;
	float size, totsize, obinv[4][4], obmat[4][4], vec[3];
	//float max=0.0;
	int a, obnr, zero_size = 0;
	char obname[MAX_ID_NAME];
	
	copy_m4_m4(obmat, ob->obmat);   /* to cope with duplicators from BKE_scene_base_iter_next */
	invert_m4_m4(obinv, ob->obmat);
	a = 0;
	
	BLI_split_name_num(obname, &obnr, ob->id.name + 2, '.');
	
	/* make main array */
	BKE_scene_base_iter_next(&sce_iter, 0, NULL, NULL);
	while (BKE_scene_base_iter_next(&sce_iter, 1, &base, &bob)) {

		if (bob->type == OB_MBALL) {
			zero_size = 0;
			ml = NULL;

			if (bob == ob && (base->flag & OB_FROMDUPLI) == 0) {
				mb = ob->data;
	
				if (mb->editelems) ml = mb->editelems->first;
				else ml = mb->elems.first;
			}
			else {
				char name[MAX_ID_NAME];
				int nr;
				
				BLI_split_name_num(name, &nr, bob->id.name + 2, '.');
				if (strcmp(obname, name) == 0) {
					mb = bob->data;
					
					if (mb->editelems) ml = mb->editelems->first;
					else ml = mb->elems.first;
				}
			}

			/* when metaball object has zero scale, then MetaElem to this MetaBall
			 * will not be put to mainb array */
			if (bob->size[0] == 0.0f || bob->size[1] == 0.0f || bob->size[2] == 0.0f) {
				zero_size = 1;
			}
			else if (bob->parent) {
				struct Object *pob = bob->parent;
				while (pob) {
					if (pob->size[0] == 0.0f || pob->size[1] == 0.0f || pob->size[2] == 0.0f) {
						zero_size = 1;
						break;
					}
					pob = pob->parent;
				}
			}

			if (zero_size) {
				unsigned int ml_count = 0;
				while (ml) {
					ml_count++;
					ml = ml->next;
				}
				totelem -= ml_count;
			}
			else {
				while (ml) {
					if (!(ml->flag & MB_HIDE)) {
						int i;
						float temp1[4][4], temp2[4][4], temp3[4][4];
						float (*mat)[4] = NULL, (*imat)[4] = NULL;
						float max_x, max_y, max_z, min_x, min_y, min_z;

						max_x = max_y = max_z = -3.4e38;
						min_x = min_y = min_z =  3.4e38;

						/* too big stiffness seems only ugly due to linear interpolation
						 * no need to have possibility for too big stiffness */
						if (ml->s > 10.0f) ml->s = 10.0f;

						/* Rotation of MetaElem is stored in quat */
						quat_to_mat4(temp3, ml->quat);

						/* Translation of MetaElem */
						unit_m4(temp2);
						temp2[3][0] = ml->x;
						temp2[3][1] = ml->y;
						temp2[3][2] = ml->z;

						mult_m4_m4m4(temp1, temp2, temp3);

						/* make a copy because of duplicates */
						mainb[a] = new_pgn_element(sizeof(MetaElem));
						*(mainb[a]) = *ml;
						mainb[a]->bb = new_pgn_element(sizeof(BoundBox));

						mat = new_pgn_element(4 * 4 * sizeof(float));
						imat = new_pgn_element(4 * 4 * sizeof(float));

						/* mat is the matrix to transform from mball into the basis-mball */
						invert_m4_m4(obinv, obmat);
						mult_m4_m4m4(temp2, obinv, bob->obmat);
						/* MetaBall transformation */
						mult_m4_m4m4(mat, temp2, temp1);

						invert_m4_m4(imat, mat);

						mainb[a]->rad2 = ml->rad * ml->rad;

						mainb[a]->mat = (float *) mat;
						mainb[a]->imat = (float *) imat;

						/* untransformed Bounding Box of MetaElem */
						/* 0 */
						mainb[a]->bb->vec[0][0] = -ml->expx;
						mainb[a]->bb->vec[0][1] = -ml->expy;
						mainb[a]->bb->vec[0][2] = -ml->expz;
						/* 1 */
						mainb[a]->bb->vec[1][0] =  ml->expx;
						mainb[a]->bb->vec[1][1] = -ml->expy;
						mainb[a]->bb->vec[1][2] = -ml->expz;
						/* 2 */
						mainb[a]->bb->vec[2][0] =  ml->expx;
						mainb[a]->bb->vec[2][1] =  ml->expy;
						mainb[a]->bb->vec[2][2] = -ml->expz;
						/* 3 */
						mainb[a]->bb->vec[3][0] = -ml->expx;
						mainb[a]->bb->vec[3][1] =  ml->expy;
						mainb[a]->bb->vec[3][2] = -ml->expz;
						/* 4 */
						mainb[a]->bb->vec[4][0] = -ml->expx;
						mainb[a]->bb->vec[4][1] = -ml->expy;
						mainb[a]->bb->vec[4][2] =  ml->expz;
						/* 5 */
						mainb[a]->bb->vec[5][0] =  ml->expx;
						mainb[a]->bb->vec[5][1] = -ml->expy;
						mainb[a]->bb->vec[5][2] =  ml->expz;
						/* 6 */
						mainb[a]->bb->vec[6][0] =  ml->expx;
						mainb[a]->bb->vec[6][1] =  ml->expy;
						mainb[a]->bb->vec[6][2] =  ml->expz;
						/* 7 */
						mainb[a]->bb->vec[7][0] = -ml->expx;
						mainb[a]->bb->vec[7][1] =  ml->expy;
						mainb[a]->bb->vec[7][2] =  ml->expz;

						/* transformation of Metalem bb */
						for (i = 0; i < 8; i++)
							mul_m4_v3((float (*)[4])mat, mainb[a]->bb->vec[i]);

						/* find max and min of transformed bb */
						for (i = 0; i < 8; i++) {
							/* find maximums */
							if (mainb[a]->bb->vec[i][0] > max_x) max_x = mainb[a]->bb->vec[i][0];
							if (mainb[a]->bb->vec[i][1] > max_y) max_y = mainb[a]->bb->vec[i][1];
							if (mainb[a]->bb->vec[i][2] > max_z) max_z = mainb[a]->bb->vec[i][2];
							/* find  minimums */
							if (mainb[a]->bb->vec[i][0] < min_x) min_x = mainb[a]->bb->vec[i][0];
							if (mainb[a]->bb->vec[i][1] < min_y) min_y = mainb[a]->bb->vec[i][1];
							if (mainb[a]->bb->vec[i][2] < min_z) min_z = mainb[a]->bb->vec[i][2];
						}
					
						/* create "new" bb, only point 0 and 6, which are
						 * necessary for octal tree filling */
						mainb[a]->bb->vec[0][0] = min_x - ml->rad;
						mainb[a]->bb->vec[0][1] = min_y - ml->rad;
						mainb[a]->bb->vec[0][2] = min_z - ml->rad;

						mainb[a]->bb->vec[6][0] = max_x + ml->rad;
						mainb[a]->bb->vec[6][1] = max_y + ml->rad;
						mainb[a]->bb->vec[6][2] = max_z + ml->rad;

						a++;
					}
					ml = ml->next;
				}
			}
		}
	}

	
	/* totsize (= 'manhattan' radius) */
	totsize = 0.0;
	for (a = 0; a < totelem; a++) {
		
		vec[0] = mainb[a]->x + mainb[a]->rad + mainb[a]->expx;
		vec[1] = mainb[a]->y + mainb[a]->rad + mainb[a]->expy;
		vec[2] = mainb[a]->z + mainb[a]->rad + mainb[a]->expz;

		calc_mballco(mainb[a], vec);
	
		size = fabsf(vec[0]);
		if (size > totsize) totsize = size;
		size = fabsf(vec[1]);
		if (size > totsize) totsize = size;
		size = fabsf(vec[2]);
		if (size > totsize) totsize = size;

		vec[0] = mainb[a]->x - mainb[a]->rad;
		vec[1] = mainb[a]->y - mainb[a]->rad;
		vec[2] = mainb[a]->z - mainb[a]->rad;
				
		calc_mballco(mainb[a], vec);
	
		size = fabsf(vec[0]);
		if (size > totsize) totsize = size;
		size = fabsf(vec[1]);
		if (size > totsize) totsize = size;
		size = fabsf(vec[2]);
		if (size > totsize) totsize = size;
	}

	for (a = 0; a < totelem; a++) {
		thresh += densfunc(mainb[a], 2.0f * totsize, 2.0f * totsize, 2.0f * totsize);
	}

	return totsize;
}

/* if MetaElem lies in node, then node includes MetaElem pointer (ml_p)
 * pointing at MetaElem (ml)
 */
static void fill_metaball_octal_node(octal_node *node, MetaElem *ml, short i)
{
	ml_pointer *ml_p;

	ml_p = MEM_mallocN(sizeof(ml_pointer), "ml_pointer");
	ml_p->ml = ml;
	BLI_addtail(&(node->nodes[i]->elems), ml_p);
	node->count++;
	
	if (ml->flag & MB_NEGATIVE) {
		node->nodes[i]->neg++;
	}
	else {
		node->nodes[i]->pos++;
	}
}

/* Node is subdivided as is illustrated on the following figure:
 * 
 *      +------+------+
 *     /      /      /|
 *    +------+------+ |
 *   /      /      /| +
 *  +------+------+ |/|
 *  |      |      | + |
 *  |      |      |/| +
 *  +------+------+ |/
 *  |      |      | +
 *  |      |      |/
 *  +------+------+
 *  
 */
static void subdivide_metaball_octal_node(octal_node *node, float size_x, float size_y, float size_z, short depth)
{
	MetaElem *ml;
	ml_pointer *ml_p;
	float x, y, z;
	int a, i;

	/* create new nodes */
	for (a = 0; a < 8; a++) {
		node->nodes[a] = MEM_mallocN(sizeof(octal_node), "octal_node");
		for (i = 0; i < 8; i++)
			node->nodes[a]->nodes[i] = NULL;
		node->nodes[a]->parent = node;
		node->nodes[a]->elems.first = NULL;
		node->nodes[a]->elems.last = NULL;
		node->nodes[a]->count = 0;
		node->nodes[a]->neg = 0;
		node->nodes[a]->pos = 0;
	}

	size_x /= 2;
	size_y /= 2;
	size_z /= 2;
	
	/* center of node */
	node->x = x = node->x_min + size_x;
	node->y = y = node->y_min + size_y;
	node->z = z = node->z_min + size_z;

	/* setting up of border points of new nodes */
	node->nodes[0]->x_min = node->x_min;
	node->nodes[0]->y_min = node->y_min;
	node->nodes[0]->z_min = node->z_min;
	node->nodes[0]->x = node->nodes[0]->x_min + size_x / 2;
	node->nodes[0]->y = node->nodes[0]->y_min + size_y / 2;
	node->nodes[0]->z = node->nodes[0]->z_min + size_z / 2;
	
	node->nodes[1]->x_min = x;
	node->nodes[1]->y_min = node->y_min;
	node->nodes[1]->z_min = node->z_min;
	node->nodes[1]->x = node->nodes[1]->x_min + size_x / 2;
	node->nodes[1]->y = node->nodes[1]->y_min + size_y / 2;
	node->nodes[1]->z = node->nodes[1]->z_min + size_z / 2;

	node->nodes[2]->x_min = x;
	node->nodes[2]->y_min = y;
	node->nodes[2]->z_min = node->z_min;
	node->nodes[2]->x = node->nodes[2]->x_min + size_x / 2;
	node->nodes[2]->y = node->nodes[2]->y_min + size_y / 2;
	node->nodes[2]->z = node->nodes[2]->z_min + size_z / 2;

	node->nodes[3]->x_min = node->x_min;
	node->nodes[3]->y_min = y;
	node->nodes[3]->z_min = node->z_min;
	node->nodes[3]->x = node->nodes[3]->x_min + size_x / 2;
	node->nodes[3]->y = node->nodes[3]->y_min + size_y / 2;
	node->nodes[3]->z = node->nodes[3]->z_min + size_z / 2;

	node->nodes[4]->x_min = node->x_min;
	node->nodes[4]->y_min = node->y_min;
	node->nodes[4]->z_min = z;
	node->nodes[4]->x = node->nodes[4]->x_min + size_x / 2;
	node->nodes[4]->y = node->nodes[4]->y_min + size_y / 2;
	node->nodes[4]->z = node->nodes[4]->z_min + size_z / 2;
	
	node->nodes[5]->x_min = x;
	node->nodes[5]->y_min = node->y_min;
	node->nodes[5]->z_min = z;
	node->nodes[5]->x = node->nodes[5]->x_min + size_x / 2;
	node->nodes[5]->y = node->nodes[5]->y_min + size_y / 2;
	node->nodes[5]->z = node->nodes[5]->z_min + size_z / 2;

	node->nodes[6]->x_min = x;
	node->nodes[6]->y_min = y;
	node->nodes[6]->z_min = z;
	node->nodes[6]->x = node->nodes[6]->x_min + size_x / 2;
	node->nodes[6]->y = node->nodes[6]->y_min + size_y / 2;
	node->nodes[6]->z = node->nodes[6]->z_min + size_z / 2;

	node->nodes[7]->x_min = node->x_min;
	node->nodes[7]->y_min = y;
	node->nodes[7]->z_min = z;
	node->nodes[7]->x = node->nodes[7]->x_min + size_x / 2;
	node->nodes[7]->y = node->nodes[7]->y_min + size_y / 2;
	node->nodes[7]->z = node->nodes[7]->z_min + size_z / 2;

	ml_p = node->elems.first;
	
	/* setting up references of MetaElems for new nodes */
	while (ml_p) {
		ml = ml_p->ml;
		if (ml->bb->vec[0][2] < z) {
			if (ml->bb->vec[0][1] < y) {
				/* vec[0][0] lies in first octant */
				if (ml->bb->vec[0][0] < x) {
					/* ml belongs to the (0)1st node */
					fill_metaball_octal_node(node, ml, 0);

					/* ml belongs to the (3)4th node */
					if (ml->bb->vec[6][1] >= y) {
						fill_metaball_octal_node(node, ml, 3);

						/* ml belongs to the (7)8th node */
						if (ml->bb->vec[6][2] >= z) {
							fill_metaball_octal_node(node, ml, 7);
						}
					}
	
					/* ml belongs to the (1)2nd node */
					if (ml->bb->vec[6][0] >= x) {
						fill_metaball_octal_node(node, ml, 1);

						/* ml belongs to the (5)6th node */
						if (ml->bb->vec[6][2] >= z) {
							fill_metaball_octal_node(node, ml, 5);
						}
					}

					/* ml belongs to the (2)3th node */
					if ((ml->bb->vec[6][0] >= x) && (ml->bb->vec[6][1] >= y)) {
						fill_metaball_octal_node(node, ml, 2);
						
						/* ml belong to the (6)7th node */
						if (ml->bb->vec[6][2] >= z) {
							fill_metaball_octal_node(node, ml, 6);
						}
						
					}
			
					/* ml belongs to the (4)5th node too */	
					if (ml->bb->vec[6][2] >= z) {
						fill_metaball_octal_node(node, ml, 4);
					}

					
					
				}
				/* vec[0][0] is in the (1)second octant */
				else {
					/* ml belong to the (1)2nd node */
					fill_metaball_octal_node(node, ml, 1);

					/* ml belongs to the (2)3th node */
					if (ml->bb->vec[6][1] >= y) {
						fill_metaball_octal_node(node, ml, 2);

						/* ml belongs to the (6)7th node */
						if (ml->bb->vec[6][2] >= z) {
							fill_metaball_octal_node(node, ml, 6);
						}
						
					}
					
					/* ml belongs to the (5)6th node */
					if (ml->bb->vec[6][2] >= z) {
						fill_metaball_octal_node(node, ml, 5);
					}
				}
			}
			else {
				/* vec[0][0] is in the (3)4th octant */
				if (ml->bb->vec[0][0] < x) {
					/* ml belongs to the (3)4nd node */
					fill_metaball_octal_node(node, ml, 3);
					
					/* ml belongs to the (7)8th node */
					if (ml->bb->vec[6][2] >= z) {
						fill_metaball_octal_node(node, ml, 7);
					}
				

					/* ml belongs to the (2)3th node */
					if (ml->bb->vec[6][0] >= x) {
						fill_metaball_octal_node(node, ml, 2);
					
						/* ml belongs to the (6)7th node */
						if (ml->bb->vec[6][2] >= z) {
							fill_metaball_octal_node(node, ml, 6);
						}
					}
				}

			}

			/* vec[0][0] is in the (2)3th octant */
			if ((ml->bb->vec[0][0] >= x) && (ml->bb->vec[0][1] >= y)) {
				/* ml belongs to the (2)3th node */
				fill_metaball_octal_node(node, ml, 2);
				
				/* ml belongs to the (6)7th node */
				if (ml->bb->vec[6][2] >= z) {
					fill_metaball_octal_node(node, ml, 6);
				}
			}
		}
		else {
			if (ml->bb->vec[0][1] < y) {
				/* vec[0][0] lies in (4)5th octant */
				if (ml->bb->vec[0][0] < x) {
					/* ml belongs to the (4)5th node */
					fill_metaball_octal_node(node, ml, 4);

					if (ml->bb->vec[6][0] >= x) {
						fill_metaball_octal_node(node, ml, 5);
					}

					if (ml->bb->vec[6][1] >= y) {
						fill_metaball_octal_node(node, ml, 7);
					}
					
					if ((ml->bb->vec[6][0] >= x) && (ml->bb->vec[6][1] >= y)) {
						fill_metaball_octal_node(node, ml, 6);
					}
				}
				/* vec[0][0] lies in (5)6th octant */
				else {
					fill_metaball_octal_node(node, ml, 5);

					if (ml->bb->vec[6][1] >= y) {
						fill_metaball_octal_node(node, ml, 6);
					}
				}
			}
			else {
				/* vec[0][0] lies in (7)8th octant */
				if (ml->bb->vec[0][0] < x) {
					fill_metaball_octal_node(node, ml, 7);

					if (ml->bb->vec[6][0] >= x) {
						fill_metaball_octal_node(node, ml, 6);
					}
				}

			}
			
			/* vec[0][0] lies in (6)7th octant */
			if ((ml->bb->vec[0][0] >= x) && (ml->bb->vec[0][1] >= y)) {
				fill_metaball_octal_node(node, ml, 6);
			}
		}
		ml_p = ml_p->next;
	}

	/* free references of MetaElems for curent node (it is not needed anymore) */
	BLI_freelistN(&node->elems);

	depth--;
	
	if (depth > 0) {
		for (a = 0; a < 8; a++) {
			if (node->nodes[a]->count > 0) /* if node is not empty, then it is subdivided */
				subdivide_metaball_octal_node(node->nodes[a], size_x, size_y, size_z, depth);
		}
	}
}

/* free all octal nodes recursively */
static void free_metaball_octal_node(octal_node *node)
{
	int a;
	for (a = 0; a < 8; a++) {
		if (node->nodes[a] != NULL) free_metaball_octal_node(node->nodes[a]);
	}
	BLI_freelistN(&node->elems);
	MEM_freeN(node);
}

/* If scene include more then one MetaElem, then octree is used */
static void init_metaball_octal_tree(int depth)
{
	struct octal_node *node;
	ml_pointer *ml_p;
	float size[3];
	int a;
	
	metaball_tree = MEM_mallocN(sizeof(octal_tree), "metaball_octal_tree");
	metaball_tree->first = node = MEM_mallocN(sizeof(octal_node), "metaball_octal_node");
	/* maximal depth of octree */
	metaball_tree->depth = depth;

	metaball_tree->neg = node->neg = 0;
	metaball_tree->pos = node->pos = 0;
	
	node->elems.first = NULL;
	node->elems.last = NULL;
	node->count = 0;

	for (a = 0; a < 8; a++)
		node->nodes[a] = NULL;

	node->x_min = node->y_min = node->z_min = FLT_MAX;
	node->x_max = node->y_max = node->z_max = -FLT_MAX;

	/* size of octal tree scene */
	for (a = 0; a < totelem; a++) {
		if (mainb[a]->bb->vec[0][0] < node->x_min) node->x_min = mainb[a]->bb->vec[0][0];
		if (mainb[a]->bb->vec[0][1] < node->y_min) node->y_min = mainb[a]->bb->vec[0][1];
		if (mainb[a]->bb->vec[0][2] < node->z_min) node->z_min = mainb[a]->bb->vec[0][2];

		if (mainb[a]->bb->vec[6][0] > node->x_max) node->x_max = mainb[a]->bb->vec[6][0];
		if (mainb[a]->bb->vec[6][1] > node->y_max) node->y_max = mainb[a]->bb->vec[6][1];
		if (mainb[a]->bb->vec[6][2] > node->z_max) node->z_max = mainb[a]->bb->vec[6][2];

		ml_p = MEM_mallocN(sizeof(ml_pointer), "ml_pointer");
		ml_p->ml = mainb[a];
		BLI_addtail(&node->elems, ml_p);

		if (mainb[a]->flag & MB_NEGATIVE) {
			/* number of negative MetaElem in scene */
			metaball_tree->neg++;
		}
		else {
			/* number of positive MetaElem in scene */
			metaball_tree->pos++;
		}
	}

	/* size of first node */	
	size[0] = node->x_max - node->x_min;
	size[1] = node->y_max - node->y_min;
	size[2] = node->z_max - node->z_min;

	/* first node is subdivided recursively */
	subdivide_metaball_octal_node(node, size[0], size[1], size[2], metaball_tree->depth);
}

void BKE_mball_polygonize(Scene *scene, Object *ob, ListBase *dispbase)
{
	PROCESS mbproc;
	MetaBall *mb;
	DispList *dl;
	int a, nr_cubes;
	float *ve, *no, totsize, width;

	mb = ob->data;

	if (totelem == 0) return;
	if (!(G.rendering) && (mb->flag == MB_UPDATE_NEVER)) return;
	if (G.moving && mb->flag == MB_UPDATE_FAST) return;

	curindex = totindex = 0;
	indices = NULL;
	thresh = mb->thresh;

	/* total number of MetaElems (totelem) is precomputed in find_basis_mball() function */
	mainb = MEM_mallocN(sizeof(void *) * totelem, "mainb");
	
	/* initialize all mainb (MetaElems) */
	totsize = init_meta(scene, ob);

	if (metaball_tree) {
		free_metaball_octal_node(metaball_tree->first);
		MEM_freeN(metaball_tree);
		metaball_tree = NULL;
	}

	/* if scene includes more then one MetaElem, then octal tree optimalisation is used */	
	if ((totelem > 1) && (totelem <= 64)) init_metaball_octal_tree(1);
	if ((totelem > 64) && (totelem <= 128)) init_metaball_octal_tree(2);
	if ((totelem > 128) && (totelem <= 512)) init_metaball_octal_tree(3);
	if ((totelem > 512) && (totelem <= 1024)) init_metaball_octal_tree(4);
	if (totelem > 1024) init_metaball_octal_tree(5);

	/* don't polygonize metaballs with too high resolution (base mball to small)
	 * note: Eps was 0.0001f but this was giving problems for blood animation for durian, using 0.00001f */
	if (metaball_tree) {
		if (ob->size[0] <= 0.00001f * (metaball_tree->first->x_max - metaball_tree->first->x_min) ||
		    ob->size[1] <= 0.00001f * (metaball_tree->first->y_max - metaball_tree->first->y_min) ||
		    ob->size[2] <= 0.00001f * (metaball_tree->first->z_max - metaball_tree->first->z_min))
		{
			new_pgn_element(-1); /* free values created by init_meta */

			MEM_freeN(mainb);

			/* free tree */
			free_metaball_octal_node(metaball_tree->first);
			MEM_freeN(metaball_tree);
			metaball_tree = NULL;

			return;
		}
	}

	/* width is size per polygonize cube */
	if (G.rendering) width = mb->rendersize;
	else {
		width = mb->wiresize;
		if (G.moving && mb->flag == MB_UPDATE_HALFRES) width *= 2;
	}
	/* nr_cubes is just for safety, minimum is totsize */
	nr_cubes = (int)(0.5f + totsize / width);

	/* init process */
	mbproc.function = metaball;
	mbproc.size = width;
	mbproc.bounds = nr_cubes;
	mbproc.cubes = NULL;
	mbproc.delta = width / (float)(RES * RES);

	polygonize(&mbproc, mb);
	
	MEM_freeN(mainb);

	/* free octal tree */
	if (totelem > 1) {
		free_metaball_octal_node(metaball_tree->first);
		MEM_freeN(metaball_tree);
		metaball_tree = NULL;
	}

	if (curindex) {
		dl = MEM_callocN(sizeof(DispList), "mbaldisp");
		BLI_addtail(dispbase, dl);
		dl->type = DL_INDEX4;
		dl->nr = mbproc.vertices.count;
		dl->parts = curindex;

		dl->index = indices;
		indices = NULL;

		a = mbproc.vertices.count;
		dl->verts = ve = MEM_mallocN(sizeof(float) * 3 * a, "mballverts");
		dl->nors = no = MEM_mallocN(sizeof(float) * 3 * a, "mballnors");

		for (a = 0; a < mbproc.vertices.count; a++, no += 3, ve += 3) {
			ve[0] = mbproc.vertices.ptr[a].position.x;
			ve[1] = mbproc.vertices.ptr[a].position.y;
			ve[2] = mbproc.vertices.ptr[a].position.z;

			no[0] = mbproc.vertices.ptr[a].normal.x;
			no[1] = mbproc.vertices.ptr[a].normal.y;
			no[2] = mbproc.vertices.ptr[a].normal.z;
		}
	}

	freepolygonize(&mbproc);
}

/* basic vertex data functions */
int BKE_mball_minmax(MetaBall *mb, float min[3], float max[3])
{
	MetaElem *ml;

	INIT_MINMAX(min, max);

	for (ml = mb->elems.first; ml; ml = ml->next) {
		DO_MINMAX(&ml->x, min, max);
	}

	return (mb->elems.first != NULL);
}

int BKE_mball_center_median(MetaBall *mb, float cent[3])
{
	MetaElem *ml;
	int total = 0;

	zero_v3(cent);

	for (ml = mb->elems.first; ml; ml = ml->next) {
		add_v3_v3(cent, &ml->x);
	}

	if (total)
		mul_v3_fl(cent, 1.0f / (float)total);

	return (total != 0);
}

int BKE_mball_center_bounds(MetaBall *mb, float cent[3])
{
	float min[3], max[3];

	if (BKE_mball_minmax(mb, min, max)) {
		mid_v3_v3v3(cent, min, max);
		return 1;
	}

	return 0;
}

void BKE_mball_translate(MetaBall *mb, float offset[3])
{
	MetaElem *ml;

	for (ml = mb->elems.first; ml; ml = ml->next) {
		add_v3_v3(&ml->x, offset);
	}
}
