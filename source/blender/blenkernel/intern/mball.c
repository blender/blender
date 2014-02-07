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

#include "BKE_global.h"
#include "BKE_main.h"

/*  #include "BKE_object.h" */
#include "BKE_animsys.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_scene.h"
#include "BKE_library.h"
#include "BKE_displist.h"
#include "BKE_mball.h"
#include "BKE_object.h"
#include "BKE_material.h"

/* Data types */

typedef struct vertex {         /* surface vertex */
	float co[3];  /* position and surface normal */
	float no[3];
} VERTEX;

typedef struct vertices {       /* list of vertices in polygonization */
	int count, max;             /* # vertices, max # allowed */
	VERTEX *ptr;                /* dynamically allocated */
} VERTICES;

typedef struct corner {         /* corner of a cube */
	int i, j, k;                /* (i, j, k) is index within lattice */
	float co[3], value;       /* location and function value */
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

typedef struct process {        /* parameters, function, storage */
	/* ** old G_mb contents ** */
	float thresh;
	int totelem;
	MetaElem **mainb;
	octal_tree *metaball_tree;

	/* ** old process contents ** */

	/* what happens here? floats, I think. */
	/*  float (*function)(void);	 */	/* implicit surface function */
	float (*function)(struct process *, float, float, float);
	float size, delta;          /* cube size, normal delta */
	int bounds;                 /* cube range within lattice */
	CUBES *cubes;               /* active cubes */
	VERTICES vertices;          /* surface vertices */
	CENTERLIST **centers;       /* cube center hash table */
	CORNER **corners;           /* corner value hash table */
	EDGELIST **edges;           /* edge and vertex id hash table */

	/* Runtime things */
	int *indices;
	int totindex, curindex;

	int pgn_offset;
	struct pgn_elements *pgn_current;
	ListBase pgn_list;
} PROCESS;

/* Forward declarations */
static int vertid(PROCESS *process, const CORNER *c1, const CORNER *c2, MetaBall *mb);
static int setcenter(PROCESS *process, CENTERLIST *table[], const int i, const int j, const int k);
static CORNER *setcorner(PROCESS *process, int i, int j, int k);
static void converge(PROCESS *process, const float p1[3], const float p2[3], float v1, float v2,
                     float p[3], MetaBall *mb, int f);

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
	BLI_freelistN(&mb->elems);
	if (mb->disp.first) BKE_displist_free(&mb->disp);
}

MetaBall *BKE_mball_add(Main *bmain, const char *name)
{
	MetaBall *mb;
	
	mb = BKE_libblock_alloc(bmain, ID_MB, name);
	
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
	int tot, do_it = FALSE;

	if (ob->bb == NULL) ob->bb = MEM_callocN(sizeof(BoundBox), "mb boundbox");
	bb = ob->bb;
	
	/* Weird one, this. */
/*      INIT_MINMAX(min, max); */
	(min)[0] = (min)[1] = (min)[2] = 1.0e30f;
	(max)[0] = (max)[1] = (max)[2] = -1.0e30f;

	dl = ob->curve_cache->disp.first;
	while (dl) {
		tot = dl->nr;
		if (tot) do_it = TRUE;
		data = dl->verts;
		while (tot--) {
			/* Also weird... but longer. From utildefines. */
			minmax_v3v3_v3(min, max, data);
			data += 3;
		}
		dl = dl->next;
	}

	if (!do_it) {
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
bool BKE_mball_is_basis(Object *ob)
{
	/* just a quick test */
	const int len = strlen(ob->id.name);
	return (!isdigit(ob->id.name[len - 1]));
}

/* return nonzero if ob1 is a basis mball for ob */
bool BKE_mball_is_basis_for(Object *ob1, Object *ob2)
{
	int basis1nr, basis2nr;
	char basis1name[MAX_ID_NAME], basis2name[MAX_ID_NAME];

	BLI_split_name_num(basis1name, &basis1nr, ob1->id.name + 2, '.');
	BLI_split_name_num(basis2name, &basis2nr, ob2->id.name + 2, '.');

	if (!strcmp(basis1name, basis2name)) {
		return BKE_mball_is_basis(ob1);
	}
	else {
		return false;
	}
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
	SceneBaseIter iter;
	EvaluationContext *eval_ctx = G.main->eval_ctx;

	BLI_split_name_num(basisname, &basisnr, active_object->id.name + 2, '.');

	/* XXX recursion check, see scene.c, just too simple code this BKE_scene_base_iter_next() */
	if (F_ERROR == BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 0, NULL, NULL))
		return;
	
	while (BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 1, &base, &ob)) {
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
	int basisnr, obnr;
	char basisname[MAX_ID_NAME], obname[MAX_ID_NAME];
	SceneBaseIter iter;
	EvaluationContext *eval_ctx = G.main->eval_ctx;

	BLI_split_name_num(basisname, &basisnr, basis->id.name + 2, '.');

	/* XXX recursion check, see scene.c, just too simple code this BKE_scene_base_iter_next() */
	if (F_ERROR == BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 0, NULL, NULL))
		return NULL;

	while (BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 1, &base, &ob)) {
		if (ob->type == OB_MBALL) {
			if (ob != bob) {
				BLI_split_name_num(obname, &obnr, ob->id.name + 2, '.');

				/* object ob has to be in same "group" ... it means, that it has to have
				 * same base of its name */
				if (strcmp(obname, basisname) == 0) {
					if (obnr < basisnr) {
						if (!(ob->flag & OB_FROMDUPLI)) {
							basis = ob;
							basisnr = obnr;
						}
					}
				}
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
	float dist2;
	float dvec[3] = {x, y, z};

	mul_m4_v3((float (*)[4])ball->imat, dvec);

	switch (ball->type) {
		case MB_BALL:
			/* do nothing */
			break;
		case MB_TUBE:
			if      (dvec[0] >  ball->expx) dvec[0] -= ball->expx;
			else if (dvec[0] < -ball->expx) dvec[0] += ball->expx;
			else                            dvec[0] = 0.0;
			break;
		case MB_PLANE:
			if      (dvec[0] >  ball->expx) dvec[0] -= ball->expx;
			else if (dvec[0] < -ball->expx) dvec[0] += ball->expx;
			else                            dvec[0] = 0.0;
			if      (dvec[1] >  ball->expy) dvec[1] -= ball->expy;
			else if (dvec[1] < -ball->expy) dvec[1] += ball->expy;
			else                            dvec[1] = 0.0;
			break;
		case MB_ELIPSOID:
			dvec[0] /= ball->expx;
			dvec[1] /= ball->expy;
			dvec[2] /= ball->expz;
			break;
		case MB_CUBE:
			if      (dvec[0] >  ball->expx) dvec[0] -= ball->expx;
			else if (dvec[0] < -ball->expx) dvec[0] += ball->expx;
			else                            dvec[0] = 0.0;

			if      (dvec[1] >  ball->expy) dvec[1] -= ball->expy;
			else if (dvec[1] < -ball->expy) dvec[1] += ball->expy;
			else                            dvec[1] = 0.0;

			if      (dvec[2] >  ball->expz) dvec[2] -= ball->expz;
			else if (dvec[2] < -ball->expz) dvec[2] += ball->expz;
			else                            dvec[2] = 0.0;
			break;

		/* *** deprecated, could be removed?, do-versioned at least *** */
		case MB_TUBEX:
			if      (dvec[0] >  ball->len) dvec[0] -= ball->len;
			else if (dvec[0] < -ball->len) dvec[0] += ball->len;
			else                           dvec[0] = 0.0;
			break;
		case MB_TUBEY:
			if      (dvec[1] >  ball->len) dvec[1] -= ball->len;
			else if (dvec[1] < -ball->len) dvec[1] += ball->len;
			else                           dvec[1] = 0.0;
			break;
		case MB_TUBEZ:
			if      (dvec[2] >  ball->len) dvec[2] -= ball->len;
			else if (dvec[2] < -ball->len) dvec[2] += ball->len;
			else                           dvec[2] = 0.0;
			break;
		/* *** end deprecated *** */
	}

	dist2 = 1.0f - (len_squared_v3(dvec) / ball->rad2);

	if ((ball->flag & MB_NEGATIVE) == 0) {
		return (dist2 < 0.0f) ? -0.5f : (ball->s * dist2 * dist2 * dist2) - 0.5f;
	}
	else {
		return (dist2 < 0.0f) ? 0.5f : 0.5f - (ball->s * dist2 * dist2 * dist2);
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

static float metaball(PROCESS *process, float x, float y, float z)
/*  float x, y, z; */
{
	octal_tree *metaball_tree = process->metaball_tree;
	struct octal_node *node;
	struct ml_pointer *ml_p;
	float dens = 0;
	int a;
	
	if (process->totelem > 1) {
		node = find_metaball_octal_node(metaball_tree->first, x, y, z, metaball_tree->depth);
		if (node) {
			for (ml_p = node->elems.first; ml_p; ml_p = ml_p->next) {
				dens += densfunc(ml_p->ml, x, y, z);
			}

			dens += -0.5f * (metaball_tree->pos - node->pos);
			dens +=  0.5f * (metaball_tree->neg - node->neg);
		}
		else {
			for (a = 0; a < process->totelem; a++) {
				dens += densfunc(process->mainb[a], x, y, z);
			}
		}
	}
	else {
		dens += densfunc(process->mainb[0], x, y, z);
	}

	return process->thresh - dens;
}

/* ******************************************** */

static void accum_mballfaces(PROCESS *process, int i1, int i2, int i3, int i4)
{
	int *newi, *cur;
	/* static int i = 0; I would like to delete altogether, but I don't dare to, yet */

	if (process->totindex == process->curindex) {
		process->totindex += 256;
		newi = MEM_mallocN(4 * sizeof(int) * process->totindex, "vertindex");
		
		if (process->indices) {
			memcpy(newi, process->indices, 4 * sizeof(int) * (process->totindex - 256));
			MEM_freeN(process->indices);
		}
		process->indices = newi;
	}
	
	cur = process->indices + 4 * process->curindex;

	/* displists now support array drawing, we treat tri's as fake quad */
	
	cur[0] = i1;
	cur[1] = i2;
	cur[2] = i3;
	if (i4 == 0)
		cur[3] = i3;
	else 
		cur[3] = i4;
	
	process->curindex++;

}

/* ******************* MEMORY MANAGEMENT *********************** */
static void *new_pgn_element(PROCESS *process, int size)
{
	/* during polygonize 1000s of elements are allocated
	 * and never freed in between. Freeing only done at the end.
	 */
	int blocksize = 16384;
	void *adr;
	
	if (size > 10000 || size == 0) {
		printf("incorrect use of new_pgn_element\n");
	}
	else if (size == -1) {
		struct pgn_elements *cur = process->pgn_list.first;
		while (cur) {
			MEM_freeN(cur->data);
			cur = cur->next;
		}
		BLI_freelistN(&process->pgn_list);
		
		return NULL;
	}
	
	size = 4 * ( (size + 3) / 4);
	
	if (process->pgn_current) {
		if (size + process->pgn_offset < blocksize) {
			adr = (void *) (process->pgn_current->data + process->pgn_offset);
			process->pgn_offset += size;
			return adr;
		}
	}
	
	process->pgn_current = MEM_callocN(sizeof(struct pgn_elements), "newpgn");
	process->pgn_current->data = MEM_callocN(blocksize, "newpgn");
	BLI_addtail(&process->pgn_list, process->pgn_current);
	
	process->pgn_offset = size;
	return process->pgn_current->data;
}

static void freepolygonize(PROCESS *process)
{
	MEM_freeN(process->corners);
	MEM_freeN(process->edges);
	MEM_freeN(process->centers);

	new_pgn_element(process, -1);

	if (process->vertices.ptr) {
		MEM_freeN(process->vertices.ptr);
	}
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

static void docube(PROCESS *process, CUBE *cube, MetaBall *mb)
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
			
			indexar[count] = vertid(process, c1, c2, mb);
			count++;
		}
		if (count > 2) {
			switch (count) {
				case 3:
					accum_mballfaces(process, indexar[2], indexar[1], indexar[0], 0);
					break;
				case 4:
					if (indexar[0] == 0) accum_mballfaces(process, indexar[0], indexar[3], indexar[2], indexar[1]);
					else accum_mballfaces(process, indexar[3], indexar[2], indexar[1], indexar[0]);
					break;
				case 5:
					if (indexar[0] == 0) accum_mballfaces(process, indexar[0], indexar[3], indexar[2], indexar[1]);
					else accum_mballfaces(process, indexar[3], indexar[2], indexar[1], indexar[0]);
				
					accum_mballfaces(process, indexar[4], indexar[3], indexar[0], 0);
					break;
				case 6:
					if (indexar[0] == 0) {
						accum_mballfaces(process, indexar[0], indexar[3], indexar[2], indexar[1]);
						accum_mballfaces(process, indexar[0], indexar[5], indexar[4], indexar[3]);
					}
					else {
						accum_mballfaces(process, indexar[3], indexar[2], indexar[1], indexar[0]);
						accum_mballfaces(process, indexar[5], indexar[4], indexar[3], indexar[0]);
					}
					break;
				case 7:
					if (indexar[0] == 0) {
						accum_mballfaces(process, indexar[0], indexar[3], indexar[2], indexar[1]);
						accum_mballfaces(process, indexar[0], indexar[5], indexar[4], indexar[3]);
					}
					else {
						accum_mballfaces(process, indexar[3], indexar[2], indexar[1], indexar[0]);
						accum_mballfaces(process, indexar[5], indexar[4], indexar[3], indexar[0]);
					}
				
					accum_mballfaces(process, indexar[6], indexar[5], indexar[0], 0);

					break;
			}
		}
	}
}


/* testface: given cube at lattice (i, j, k), and four corners of face,
 * if surface crosses face, compute other four corners of adjacent cube
 * and add new cube to cube stack */

static void testface(PROCESS *process, int i, int j, int k, CUBE *old, int bit, int c1, int c2, int c3, int c4)
{
	CUBE newc;
	CUBES *oldcubes = process->cubes;
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
	if (setcenter(process, process->centers, i, j, k)) {
		return;
	}

	/* create new cube and add cube to top of stack: */
	process->cubes = (CUBES *) new_pgn_element(process, sizeof(CUBES));
	process->cubes->next = oldcubes;
	
	newc.i = i;
	newc.j = j;
	newc.k = k;
	for (n = 0; n < 8; n++) newc.corners[n] = NULL;
	
	newc.corners[FLIP(c1, bit)] = corn1;
	newc.corners[FLIP(c2, bit)] = corn2;
	newc.corners[FLIP(c3, bit)] = corn3;
	newc.corners[FLIP(c4, bit)] = corn4;

	if (newc.corners[0] == NULL) newc.corners[0] = setcorner(process, i, j, k);
	if (newc.corners[1] == NULL) newc.corners[1] = setcorner(process, i, j, k + 1);
	if (newc.corners[2] == NULL) newc.corners[2] = setcorner(process, i, j + 1, k);
	if (newc.corners[3] == NULL) newc.corners[3] = setcorner(process, i, j + 1, k + 1);
	if (newc.corners[4] == NULL) newc.corners[4] = setcorner(process, i + 1, j, k);
	if (newc.corners[5] == NULL) newc.corners[5] = setcorner(process, i + 1, j, k + 1);
	if (newc.corners[6] == NULL) newc.corners[6] = setcorner(process, i + 1, j + 1, k);
	if (newc.corners[7] == NULL) newc.corners[7] = setcorner(process, i + 1, j + 1, k + 1);

	process->cubes->cube = newc;
}

/* setcorner: return corner with the given lattice location
 * set (and cache) its function value */

static CORNER *setcorner(PROCESS *process, int i, int j, int k)
{
	/* for speed, do corner value caching here */
	CORNER *c;
	int index;

	/* does corner exist? */
	index = HASH(i, j, k);
	c = process->corners[index];
	
	for (; c != NULL; c = c->next) {
		if (c->i == i && c->j == j && c->k == k) {
			return c;
		}
	}

	c = (CORNER *) new_pgn_element(process, sizeof(CORNER));

	c->i = i; 
	c->co[0] = ((float)i - 0.5f) * process->size;
	c->j = j; 
	c->co[1] = ((float)j - 0.5f) * process->size;
	c->k = k; 
	c->co[2] = ((float)k - 0.5f) * process->size;
	c->value = process->function(process, c->co[0], c->co[1], c->co[2]);
	
	c->next = process->corners[index];
	process->corners[index] = c;
	
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
	static int is_done = FALSE;
	int i, e, c, done[12], pos[8];

	if (is_done) return;
	is_done = TRUE;

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

static int setcenter(PROCESS *process, CENTERLIST *table[], const int i, const int j, const int k)
{
	int index;
	CENTERLIST *newc, *l, *q;

	index = HASH(i, j, k);
	q = table[index];

	for (l = q; l != NULL; l = l->next) {
		if (l->i == i && l->j == j && l->k == k) return 1;
	}
	
	newc = (CENTERLIST *) new_pgn_element(process, sizeof(CENTERLIST));
	newc->i = i; 
	newc->j = j; 
	newc->k = k; 
	newc->next = q;
	table[index] = newc;
	
	return 0;
}


/* setedge: set vertex id for edge */

static void setedge(PROCESS *process,
                    EDGELIST *table[],
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
	newe = (EDGELIST *) new_pgn_element(process, sizeof(EDGELIST));
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

static void vnormal(PROCESS *process, const float point[3], float r_no[3])
{
	const float delta = 0.2f * process->delta;
	const float f = process->function(process, point[0], point[1], point[2]);

	r_no[0] = process->function(process, point[0] + delta, point[1], point[2]) - f;
	r_no[1] = process->function(process, point[0], point[1] + delta, point[2]) - f;
	r_no[2] = process->function(process, point[0], point[1], point[2] + delta) - f;

#if 1
	normalize_v3(r_no);
#else
	f = normalize_v3(r_no);
	
	if (0) {
		float tvec[3];
		
		delta *= 2.0f;
		
		f = process->function(process, point[0], point[1], point[2]);
	
		tvec[0] = process->function(process, point[0] + delta, point[1], point[2]) - f;
		tvec[1] = process->function(process, point[0], point[1] + delta, point[2]) - f;
		tvec[2] = process->function(process, point[0], point[1], point[2] + delta) - f;
	
		if (normalize_v3(tvec) != 0.0f) {
			add_v3_v3(r_no, tvec);
			normalize_v3(r_no);
		}
	}
#endif
}


static int vertid(PROCESS *process, const CORNER *c1, const CORNER *c2, MetaBall *mb)
{
	VERTEX v;
	int vid = getedge(process->edges, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k);

	if (vid != -1) {
		return vid;  /* previously computed */
	}

	converge(process, c1->co, c2->co, c1->value, c2->value, v.co, mb, 1); /* position */
	vnormal(process, v.co, v.no);

	addtovertices(&process->vertices, v);            /* save vertex */
	vid = process->vertices.count - 1;
	setedge(process, process->edges, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k, vid);
	
	return vid;
}


/* converge: from two points of differing sign, converge to zero crossing */
/* watch it: p1 and p2 are used to calculate */
static void converge(PROCESS *process, const float p1[3], const float p2[3], float v1, float v2,
                     float p[3], MetaBall *mb, int f)
{
	int i = 0;
	float pos[3], neg[3];
	float positive = 0.0f, negative = 0.0f;
	float dvec[3];
	
	if (v1 < 0) {
		copy_v3_v3(pos, p2);
		copy_v3_v3(neg, p1);
		positive = v2;
		negative = v1;
	}
	else {
		copy_v3_v3(pos, p1);
		copy_v3_v3(neg, p2);
		positive = v1;
		negative = v2;
	}

	sub_v3_v3v3(dvec, pos, neg);

/* Approximation by linear interpolation is faster then binary subdivision,
 * but it results sometimes (mb->thresh < 0.2) into the strange results */
	if ((mb->thresh > 0.2f) && (f == 1)) {
		if ((dvec[1] == 0.0f) && (dvec[2] == 0.0f)) {
			p[0] = neg[0] - negative * dvec[0] / (positive - negative);
			p[1] = neg[1];
			p[2] = neg[2];
			return;
		}
		if ((dvec[0] == 0.0f) && (dvec[2] == 0.0f)) {
			p[0] = neg[0];
			p[1] = neg[1] - negative * dvec[1] / (positive - negative);
			p[2] = neg[2];
			return;
		}
		if ((dvec[0] == 0.0f) && (dvec[1] == 0.0f)) {
			p[0] = neg[0];
			p[1] = neg[1];
			p[2] = neg[2] - negative * dvec[2] / (positive - negative);
			return;
		}
	}

	if ((dvec[1] == 0.0f) && (dvec[2] == 0.0f)) {
		p[1] = neg[1];
		p[2] = neg[2];
		while (1) {
			if (i++ == RES) return;
			p[0] = 0.5f * (pos[0] + neg[0]);
			if ((process->function(process, p[0], p[1], p[2])) > 0.0f) pos[0] = p[0];
			else                                                       neg[0] = p[0];
		}
	}

	if ((dvec[0] == 0.0f) && (dvec[2] == 0.0f)) {
		p[0] = neg[0];
		p[2] = neg[2];
		while (1) {
			if (i++ == RES) return;
			p[1] = 0.5f * (pos[1] + neg[1]);
			if ((process->function(process, p[0], p[1], p[2])) > 0.0f) pos[1] = p[1];
			else                                                       neg[1] = p[1];
		}
	}

	if ((dvec[0] == 0.0f) && (dvec[1] == 0.0f)) {
		p[0] = neg[0];
		p[1] = neg[1];
		while (1) {
			if (i++ == RES) return;
			p[2] = 0.5f * (pos[2] + neg[2]);
			if ((process->function(process, p[0], p[1], p[2])) > 0.0f) pos[2] = p[2];
			else                                                       neg[2] = p[2];
		}
	}

	/* This is necessary to find start point */
	while (1) {
		mid_v3_v3v3(&p[0], pos, neg);

		if (i++ == RES) {
			return;
		}

		if ((process->function(process, p[0], p[1], p[2])) > 0.0f) {
			copy_v3_v3(pos, &p[0]);
		}
		else {
			copy_v3_v3(neg, &p[0]);
		}
	}
}

/* ************************************** */
static void add_cube(PROCESS *process, int i, int j, int k, int count)
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
				if (setcenter(process, process->centers, a, b, c) == 0) {
					/* push cube on stack: */
					ncube = (CUBES *) new_pgn_element(process, sizeof(CUBES));
					ncube->next = process->cubes;
					process->cubes = ncube;

					ncube->cube.i = a;
					ncube->cube.j = b;
					ncube->cube.k = c;

					/* set corners of initial cube: */
					for (n = 0; n < 8; n++)
						ncube->cube.corners[n] = setcorner(process, a + MB_BIT(n, 2), b + MB_BIT(n, 1), c + MB_BIT(n, 0));
				}
			}
}


static void find_first_points(PROCESS *process, MetaBall *mb, int a)
{
	MetaElem *ml;
	float f;

	ml = process->mainb[a];
	f = 1.0f - (mb->thresh / ml->s);

	/* Skip, when Stiffness of MetaElement is too small ... MetaElement can't be
	 * visible alone ... but still can influence others MetaElements :-) */
	if (f > 0.0f) {
		float IN[3] = {0.0f}, OUT[3] = {0.0f}, in[3] = {0.0f}, out[3];
		int i, j, k, c_i, c_j, c_k;
		int index[3] = {1, 0, -1};
		float in_v /*, out_v*/;
		float workp[3];
		float dvec[3];
		float tmp_v, workp_v, max_len_sq, nx, ny, nz, max_dim;

		calc_mballco(ml, in);
		in_v = process->function(process, in[0], in[1], in[2]);

		for (i = 0; i < 3; i++) {
			switch (ml->type) {
				case MB_BALL:
					OUT[0] = out[0] = IN[0] + index[i] * ml->rad;
					break;
				case MB_TUBE:
				case MB_PLANE:
				case MB_ELIPSOID:
				case MB_CUBE:
					OUT[0] = out[0] = IN[0] + index[i] * (ml->expx + ml->rad);
					break;
			}

			for (j = 0; j < 3; j++) {
				switch (ml->type) {
					case MB_BALL:
						OUT[1] = out[1] = IN[1] + index[j] * ml->rad;
						break;
					case MB_TUBE:
					case MB_PLANE:
					case MB_ELIPSOID:
					case MB_CUBE:
						OUT[1] = out[1] = IN[1] + index[j] * (ml->expy + ml->rad);
						break;
				}
			
				for (k = 0; k < 3; k++) {
					out[0] = OUT[0];
					out[1] = OUT[1];
					switch (ml->type) {
						case MB_BALL:
						case MB_TUBE:
						case MB_PLANE:
							out[2] = IN[2] + index[k] * ml->rad;
							break;
						case MB_ELIPSOID:
						case MB_CUBE:
							out[2] = IN[2] + index[k] * (ml->expz + ml->rad);
							break;
					}

					calc_mballco(ml, out);

					/*out_v = process->function(out[0], out[1], out[2]);*/ /*UNUSED*/

					/* find "first points" on Implicit Surface of MetaElemnt ml */
					copy_v3_v3(workp, in);
					workp_v = in_v;
					max_len_sq = len_squared_v3v3(out, in);

					nx = fabsf((out[0] - in[0]) / process->size);
					ny = fabsf((out[1] - in[1]) / process->size);
					nz = fabsf((out[2] - in[2]) / process->size);
					
					max_dim = max_fff(nx, ny, nz);
					if (max_dim != 0.0f) {
						float len_sq = 0.0f;

						dvec[0] = (out[0] - in[0]) / max_dim;
						dvec[1] = (out[1] - in[1]) / max_dim;
						dvec[2] = (out[2] - in[2]) / max_dim;

						while (len_sq <= max_len_sq) {
							add_v3_v3(workp, dvec);

							/* compute value of implicite function */
							tmp_v = process->function(process, workp[0], workp[1], workp[2]);
							/* add cube to the stack, when value of implicite function crosses zero value */
							if ((tmp_v < 0.0f && workp_v >= 0.0f) || (tmp_v > 0.0f && workp_v <= 0.0f)) {

								/* indexes of CUBE, which includes "first point" */
								c_i = (int)floor(workp[0] / process->size);
								c_j = (int)floor(workp[1] / process->size);
								c_k = (int)floor(workp[2] / process->size);
								
								/* add CUBE (with indexes c_i, c_j, c_k) to the stack,
								 * this cube includes found point of Implicit Surface */
								if ((ml->flag & MB_NEGATIVE) == 0) {
									add_cube(process, c_i, c_j, c_k, 1);
								}
								else {
									add_cube(process, c_i, c_j, c_k, 2);
								}
							}
							len_sq = len_squared_v3v3(workp, in);
							workp_v = tmp_v;

						}
					}
				}
			}
		}
	}
}

static void polygonize(PROCESS *process, MetaBall *mb)
{
	CUBE c;
	int a;

	process->vertices.count = process->vertices.max = 0;
	process->vertices.ptr = NULL;

	/* allocate hash tables and build cube polygon table: */
	process->centers = MEM_callocN(HASHSIZE * sizeof(CENTERLIST *), "mbproc->centers");
	process->corners = MEM_callocN(HASHSIZE * sizeof(CORNER *), "mbproc->corners");
	process->edges = MEM_callocN(2 * HASHSIZE * sizeof(EDGELIST *), "mbproc->edges");
	makecubetable();

	for (a = 0; a < process->totelem; a++) {

		/* try to find 8 points on the surface for each MetaElem */
		find_first_points(process, mb, a);
	}

	/* polygonize all MetaElems of current MetaBall */
	while (process->cubes != NULL) { /* process active cubes till none left */
		c = process->cubes->cube;

		/* polygonize the cube directly: */
		docube(process, &c, mb);
		
		/* pop current cube from stack */
		process->cubes = process->cubes->next;
		
		/* test six face directions, maybe add to stack: */
		testface(process, c.i - 1, c.j, c.k, &c, 2, LBN, LBF, LTN, LTF);
		testface(process, c.i + 1, c.j, c.k, &c, 2, RBN, RBF, RTN, RTF);
		testface(process, c.i, c.j - 1, c.k, &c, 1, LBN, LBF, RBN, RBF);
		testface(process, c.i, c.j + 1, c.k, &c, 1, LTN, LTF, RTN, RTF);
		testface(process, c.i, c.j, c.k - 1, &c, 0, LBN, LTN, RBN, RTN);
		testface(process, c.i, c.j, c.k + 1, &c, 0, LBF, LTF, RBF, RTF);
	}
}

static float init_meta(EvaluationContext *eval_ctx, PROCESS *process, Scene *scene, Object *ob)    /* return totsize */
{
	Scene *sce_iter = scene;
	Base *base;
	Object *bob;
	MetaBall *mb;
	MetaElem *ml;
	float size, totsize, obinv[4][4], obmat[4][4], vec[3];
	//float max = 0.0f;
	int a, obnr, zero_size = 0;
	char obname[MAX_ID_NAME];
	SceneBaseIter iter;

	copy_m4_m4(obmat, ob->obmat);   /* to cope with duplicators from BKE_scene_base_iter_next */
	invert_m4_m4(obinv, ob->obmat);
	a = 0;
	
	BLI_split_name_num(obname, &obnr, ob->id.name + 2, '.');
	
	/* make main array */
	BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 0, NULL, NULL);
	while (BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 1, &base, &bob)) {

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
			if (has_zero_axis_m4(bob->obmat)) {
				zero_size = 1;
			}
			else if (bob->parent) {
				struct Object *pob = bob->parent;
				while (pob) {
					if (has_zero_axis_m4(pob->obmat)) {
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
				process->totelem -= ml_count;
			}
			else {
				while (ml) {
					if (!(ml->flag & MB_HIDE)) {
						int i;
						float temp1[4][4], temp2[4][4], temp3[4][4];
						float (*mat)[4] = NULL, (*imat)[4] = NULL;
						float max_x, max_y, max_z, min_x, min_y, min_z;
						float expx, expy, expz;

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

						mul_m4_m4m4(temp1, temp2, temp3);

						/* make a copy because of duplicates */
						process->mainb[a] = new_pgn_element(process, sizeof(MetaElem));
						*(process->mainb[a]) = *ml;
						process->mainb[a]->bb = new_pgn_element(process, sizeof(BoundBox));

						mat = new_pgn_element(process, 4 * 4 * sizeof(float));
						imat = new_pgn_element(process, 4 * 4 * sizeof(float));

						/* mat is the matrix to transform from mball into the basis-mball */
						invert_m4_m4(obinv, obmat);
						mul_m4_m4m4(temp2, obinv, bob->obmat);
						/* MetaBall transformation */
						mul_m4_m4m4(mat, temp2, temp1);

						invert_m4_m4(imat, mat);

						process->mainb[a]->rad2 = ml->rad * ml->rad;

						process->mainb[a]->mat = (float *) mat;
						process->mainb[a]->imat = (float *) imat;

						if (!MB_TYPE_SIZE_SQUARED(ml->type)) {
							expx = ml->expx;
							expy = ml->expy;
							expz = ml->expz;
						}
						else {
							expx = ml->expx * ml->expx;
							expy = ml->expy * ml->expy;
							expz = ml->expz * ml->expz;
						}

						/* untransformed Bounding Box of MetaElem */
						/* TODO, its possible the elem type has been changed and the exp* values can use a fallback */
						copy_v3_fl3(process->mainb[a]->bb->vec[0], -expx, -expy, -expz);  /* 0 */
						copy_v3_fl3(process->mainb[a]->bb->vec[1], +expx, -expy, -expz);  /* 1 */
						copy_v3_fl3(process->mainb[a]->bb->vec[2], +expx, +expy, -expz);  /* 2 */
						copy_v3_fl3(process->mainb[a]->bb->vec[3], -expx, +expy, -expz);  /* 3 */
						copy_v3_fl3(process->mainb[a]->bb->vec[4], -expx, -expy, +expz);  /* 4 */
						copy_v3_fl3(process->mainb[a]->bb->vec[5], +expx, -expy, +expz);  /* 5 */
						copy_v3_fl3(process->mainb[a]->bb->vec[6], +expx, +expy, +expz);  /* 6 */
						copy_v3_fl3(process->mainb[a]->bb->vec[7], -expx, +expy, +expz);  /* 7 */

						/* transformation of Metalem bb */
						for (i = 0; i < 8; i++)
							mul_m4_v3((float (*)[4])mat, process->mainb[a]->bb->vec[i]);

						/* find max and min of transformed bb */
						for (i = 0; i < 8; i++) {
							/* find maximums */
							if (process->mainb[a]->bb->vec[i][0] > max_x) max_x = process->mainb[a]->bb->vec[i][0];
							if (process->mainb[a]->bb->vec[i][1] > max_y) max_y = process->mainb[a]->bb->vec[i][1];
							if (process->mainb[a]->bb->vec[i][2] > max_z) max_z = process->mainb[a]->bb->vec[i][2];
							/* find  minimums */
							if (process->mainb[a]->bb->vec[i][0] < min_x) min_x = process->mainb[a]->bb->vec[i][0];
							if (process->mainb[a]->bb->vec[i][1] < min_y) min_y = process->mainb[a]->bb->vec[i][1];
							if (process->mainb[a]->bb->vec[i][2] < min_z) min_z = process->mainb[a]->bb->vec[i][2];
						}
					
						/* create "new" bb, only point 0 and 6, which are
						 * necessary for octal tree filling */
						process->mainb[a]->bb->vec[0][0] = min_x - ml->rad;
						process->mainb[a]->bb->vec[0][1] = min_y - ml->rad;
						process->mainb[a]->bb->vec[0][2] = min_z - ml->rad;

						process->mainb[a]->bb->vec[6][0] = max_x + ml->rad;
						process->mainb[a]->bb->vec[6][1] = max_y + ml->rad;
						process->mainb[a]->bb->vec[6][2] = max_z + ml->rad;

						a++;
					}
					ml = ml->next;
				}
			}
		}
	}

	
	/* totsize (= 'manhattan' radius) */
	totsize = 0.0;
	for (a = 0; a < process->totelem; a++) {
		
		vec[0] = process->mainb[a]->x + process->mainb[a]->rad + process->mainb[a]->expx;
		vec[1] = process->mainb[a]->y + process->mainb[a]->rad + process->mainb[a]->expy;
		vec[2] = process->mainb[a]->z + process->mainb[a]->rad + process->mainb[a]->expz;

		calc_mballco(process->mainb[a], vec);
	
		size = fabsf(vec[0]);
		if (size > totsize) totsize = size;
		size = fabsf(vec[1]);
		if (size > totsize) totsize = size;
		size = fabsf(vec[2]);
		if (size > totsize) totsize = size;

		vec[0] = process->mainb[a]->x - process->mainb[a]->rad;
		vec[1] = process->mainb[a]->y - process->mainb[a]->rad;
		vec[2] = process->mainb[a]->z - process->mainb[a]->rad;
				
		calc_mballco(process->mainb[a], vec);
	
		size = fabsf(vec[0]);
		if (size > totsize) totsize = size;
		size = fabsf(vec[1]);
		if (size > totsize) totsize = size;
		size = fabsf(vec[2]);
		if (size > totsize) totsize = size;
	}

	for (a = 0; a < process->totelem; a++) {
		process->thresh += densfunc(process->mainb[a], 2.0f * totsize, 2.0f * totsize, 2.0f * totsize);
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
	
	if ((ml->flag & MB_NEGATIVE) == 0) {
		node->nodes[i]->pos++;
	}
	else {
		node->nodes[i]->neg++;
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
		BLI_listbase_clear(&node->nodes[a]->elems);
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

/* If scene include more than one MetaElem, then octree is used */
static void init_metaball_octal_tree(PROCESS *process, int depth)
{
	struct octal_node *node;
	ml_pointer *ml_p;
	float size[3];
	int a;
	
	process->metaball_tree = MEM_mallocN(sizeof(octal_tree), "metaball_octal_tree");
	process->metaball_tree->first = node = MEM_mallocN(sizeof(octal_node), "metaball_octal_node");
	/* maximal depth of octree */
	process->metaball_tree->depth = depth;

	process->metaball_tree->neg = node->neg = 0;
	process->metaball_tree->pos = node->pos = 0;
	
	BLI_listbase_clear(&node->elems);
	node->count = 0;

	for (a = 0; a < 8; a++)
		node->nodes[a] = NULL;

	node->x_min = node->y_min = node->z_min = FLT_MAX;
	node->x_max = node->y_max = node->z_max = -FLT_MAX;

	/* size of octal tree scene */
	for (a = 0; a < process->totelem; a++) {
		if (process->mainb[a]->bb->vec[0][0] < node->x_min) node->x_min = process->mainb[a]->bb->vec[0][0];
		if (process->mainb[a]->bb->vec[0][1] < node->y_min) node->y_min = process->mainb[a]->bb->vec[0][1];
		if (process->mainb[a]->bb->vec[0][2] < node->z_min) node->z_min = process->mainb[a]->bb->vec[0][2];

		if (process->mainb[a]->bb->vec[6][0] > node->x_max) node->x_max = process->mainb[a]->bb->vec[6][0];
		if (process->mainb[a]->bb->vec[6][1] > node->y_max) node->y_max = process->mainb[a]->bb->vec[6][1];
		if (process->mainb[a]->bb->vec[6][2] > node->z_max) node->z_max = process->mainb[a]->bb->vec[6][2];

		ml_p = MEM_mallocN(sizeof(ml_pointer), "ml_pointer");
		ml_p->ml = process->mainb[a];
		BLI_addtail(&node->elems, ml_p);

		if ((process->mainb[a]->flag & MB_NEGATIVE) == 0) {
			/* number of positive MetaElem in scene */
			process->metaball_tree->pos++;
		}
		else {
			/* number of negative MetaElem in scene */
			process->metaball_tree->neg++;
		}
	}

	/* size of first node */
	size[0] = node->x_max - node->x_min;
	size[1] = node->y_max - node->y_min;
	size[2] = node->z_max - node->z_min;

	/* first node is subdivided recursively */
	subdivide_metaball_octal_node(node, size[0], size[1], size[2], process->metaball_tree->depth);
}

static void mball_count(EvaluationContext *eval_ctx, PROCESS *process, Scene *scene, Object *basis)
{
	Scene *sce_iter = scene;
	Base *base;
	Object *ob, *bob = basis;
	MetaElem *ml = NULL;
	int basisnr, obnr;
	char basisname[MAX_ID_NAME], obname[MAX_ID_NAME];
	SceneBaseIter iter;

	BLI_split_name_num(basisname, &basisnr, basis->id.name + 2, '.');
	process->totelem = 0;

	/* XXX recursion check, see scene.c, just too simple code this BKE_scene_base_iter_next() */
	if (F_ERROR == BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 0, NULL, NULL))
		return;

	while (BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 1, &base, &ob)) {
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
				}
			}

			for ( ; ml; ml = ml->next) {
				if (!(ml->flag & MB_HIDE)) {
					process->totelem++;
				}
			}
		}
	}
}

void BKE_mball_polygonize(EvaluationContext *eval_ctx, Scene *scene, Object *ob, ListBase *dispbase)
{
	MetaBall *mb;
	DispList *dl;
	int a, nr_cubes;
	float *co, *no, totsize, width;
	PROCESS process = {0};

	mb = ob->data;

	mball_count(eval_ctx, &process, scene, ob);

	if (process.totelem == 0) return;
	if ((eval_ctx->for_render == false) && (mb->flag == MB_UPDATE_NEVER)) return;
	if ((G.moving & (G_TRANSFORM_OBJ | G_TRANSFORM_EDIT)) && mb->flag == MB_UPDATE_FAST) return;

	process.thresh = mb->thresh;

	/* total number of MetaElems (totelem) is precomputed in find_basis_mball() function */
	process.mainb = MEM_mallocN(sizeof(void *) * process.totelem, "mainb");
	
	/* initialize all mainb (MetaElems) */
	totsize = init_meta(eval_ctx, &process, scene, ob);

	/* if scene includes more than one MetaElem, then octal tree optimization is used */
	if ((process.totelem >    1) && (process.totelem <=   64)) init_metaball_octal_tree(&process, 1);
	if ((process.totelem >   64) && (process.totelem <=  128)) init_metaball_octal_tree(&process, 2);
	if ((process.totelem >  128) && (process.totelem <=  512)) init_metaball_octal_tree(&process, 3);
	if ((process.totelem >  512) && (process.totelem <= 1024)) init_metaball_octal_tree(&process, 4);
	if (process.totelem  > 1024)                               init_metaball_octal_tree(&process, 5);

	/* don't polygonize metaballs with too high resolution (base mball to small)
	 * note: Eps was 0.0001f but this was giving problems for blood animation for durian, using 0.00001f */
	if (process.metaball_tree) {
		if (ob->size[0] <= 0.00001f * (process.metaball_tree->first->x_max - process.metaball_tree->first->x_min) ||
		    ob->size[1] <= 0.00001f * (process.metaball_tree->first->y_max - process.metaball_tree->first->y_min) ||
		    ob->size[2] <= 0.00001f * (process.metaball_tree->first->z_max - process.metaball_tree->first->z_min))
		{
			new_pgn_element(&process, -1); /* free values created by init_meta */

			MEM_freeN(process.mainb);

			/* free tree */
			free_metaball_octal_node(process.metaball_tree->first);
			MEM_freeN(process.metaball_tree);

			return;
		}
	}

	/* width is size per polygonize cube */
	if (eval_ctx->for_render) {
		width = mb->rendersize;
	}
	else {
		width = mb->wiresize;
		if ((G.moving & (G_TRANSFORM_OBJ | G_TRANSFORM_EDIT)) && mb->flag == MB_UPDATE_HALFRES) {
			width *= 2;
		}
	}
	/* nr_cubes is just for safety, minimum is totsize */
	nr_cubes = (int)(0.5f + totsize / width);

	/* init process */
	process.function = metaball;
	process.size = width;
	process.bounds = nr_cubes;
	process.cubes = NULL;
	process.delta = width / (float)(RES * RES);

	polygonize(&process, mb);
	
	MEM_freeN(process.mainb);

	/* free octal tree */
	if (process.totelem > 1) {
		free_metaball_octal_node(process.metaball_tree->first);
		MEM_freeN(process.metaball_tree);
		process.metaball_tree = NULL;
	}

	if (process.curindex) {
		VERTEX *ptr = process.vertices.ptr;

		dl = MEM_callocN(sizeof(DispList), "mbaldisp");
		BLI_addtail(dispbase, dl);
		dl->type = DL_INDEX4;
		dl->nr = process.vertices.count;
		dl->parts = process.curindex;

		dl->index = process.indices;
		process.indices = NULL;

		a = process.vertices.count;
		dl->verts = co = MEM_mallocN(sizeof(float) * 3 * a, "mballverts");
		dl->nors = no = MEM_mallocN(sizeof(float) * 3 * a, "mballnors");

		for (a = 0; a < process.vertices.count; ptr++, a++, no += 3, co += 3) {
			copy_v3_v3(co, ptr->co);
			copy_v3_v3(no, ptr->no);
		}
	}

	freepolygonize(&process);
}

bool BKE_mball_minmax_ex(MetaBall *mb, float min[3], float max[3],
                         float obmat[4][4], const short flag)
{
	const float scale = obmat ? mat4_to_scale(obmat) : 1.0f;
	MetaElem *ml;
	bool changed = false;
	float centroid[3], vec[3];

	INIT_MINMAX(min, max);

	for (ml = mb->elems.first; ml; ml = ml->next) {
		if ((ml->flag & flag) == flag) {
			const float scale_mb = (ml->rad * 0.5f) * scale;
			int i;

			if (obmat) {
				mul_v3_m4v3(centroid, obmat, &ml->x);
			}
			else {
				copy_v3_v3(centroid, &ml->x);
			}

			/* TODO, non circle shapes cubes etc, probably nobody notices - campbell */
			for (i = -1; i != 3; i += 2) {
				copy_v3_v3(vec, centroid);
				add_v3_fl(vec, scale_mb * i);
				minmax_v3v3_v3(min, max, vec);
			}
			changed = true;
		}
	}

	return changed;
}


/* basic vertex data functions */
bool BKE_mball_minmax(MetaBall *mb, float min[3], float max[3])
{
	MetaElem *ml;

	INIT_MINMAX(min, max);

	for (ml = mb->elems.first; ml; ml = ml->next) {
		minmax_v3v3_v3(min, max, &ml->x);
	}

	return (BLI_listbase_is_empty(&mb->elems) == false);
}

bool BKE_mball_center_median(MetaBall *mb, float r_cent[3])
{
	MetaElem *ml;
	int total = 0;

	zero_v3(r_cent);

	for (ml = mb->elems.first; ml; ml = ml->next) {
		add_v3_v3(r_cent, &ml->x);
		total++;
	}

	if (total) {
		mul_v3_fl(r_cent, 1.0f / (float)total);
	}

	return (total != 0);
}

bool BKE_mball_center_bounds(MetaBall *mb, float r_cent[3])
{
	float min[3], max[3];

	if (BKE_mball_minmax(mb, min, max)) {
		mid_v3_v3v3(r_cent, min, max);
		return 1;
	}

	return 0;
}

void BKE_mball_translate(MetaBall *mb, const float offset[3])
{
	MetaElem *ml;

	for (ml = mb->elems.first; ml; ml = ml->next) {
		add_v3_v3(&ml->x, offset);
	}
}

/* *** select funcs *** */
void BKE_mball_select_all(struct MetaBall *mb)
{
	MetaElem *ml;

	for (ml = mb->editelems->first; ml; ml = ml->next) {
		ml->flag |= SELECT;
	}
}

void BKE_mball_deselect_all(MetaBall *mb)
{
	MetaElem *ml;

	for (ml = mb->editelems->first; ml; ml = ml->next) {
		ml->flag &= ~SELECT;
	}
}

void BKE_mball_select_swap(struct MetaBall *mb)
{
	MetaElem *ml;

	for (ml = mb->editelems->first; ml; ml = ml->next) {
		ml->flag ^= SELECT;
	}
}

