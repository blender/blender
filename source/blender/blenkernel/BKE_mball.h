/**
 * blenlib/BKE_mball.h (mar-2001 nzc)
 *	
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_MBALL_H
#define BKE_MBALL_H

struct MetaBall;
struct Object;
struct Scene;
struct MetaElem;

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
	CUBES *cubes;				/* active cubes */
	VERTICES vertices;			/* surface vertices */
	CENTERLIST **centers;		/* cube center hash table */
	CORNER **corners;			/* corner value hash table */
	EDGELIST **edges;			/* edge and vertex id hash table */
} PROCESS;

/* dividing scene using octal tree makes polygonisation faster */
typedef struct ml_pointer {
	struct ml_pointer *next, *prev;
	struct MetaElem *ml;
} ml_pointer;

typedef struct octal_node {
	struct octal_node *nodes[8];	/* children of current node */
	struct octal_node *parent;	/* parent of current node */
	struct ListBase elems;		/* ListBase of MetaElem pointers (ml_pointer) */
	float x_min, y_min, z_min;	/* 1st border point */
	float x_max, y_max, z_max;	/* 7th border point */
	float x,y,z;			/* center of node */
	int pos, neg;			/* number of positive and negative MetaElements in the node */
	int count;			/* number of MetaElems, which belongs to the node */
} octal_node;

typedef struct octal_tree {
	struct octal_node *first;	/* first node */
	int pos, neg;			/* number of positive and negative MetaElements in the scene */
	short depth;			/* number of scene subdivision */
} octal_tree;

struct pgn_elements {
	struct pgn_elements *next, *prev;
	char *data;
};

void calc_mballco(struct MetaElem *ml, float *vec);
float densfunc(struct MetaElem *ball, float x, float y, float z);
octal_node* find_metaball_octal_node(octal_node *node, float x, float y, float z, short depth);
float metaball(float x, float y, float z);
void accum_mballfaces(int i1, int i2, int i3, int i4);
void *new_pgn_element(int size);

void freepolygonize(PROCESS *p);
void docube(CUBE *cube, PROCESS *p, struct MetaBall *mb);
void testface(int i, int j, int k, CUBE* old, int bit, int c1, int c2, int c3, int c4, PROCESS *p);
CORNER *setcorner (PROCESS* p, int i, int j, int k);
int vertid (CORNER *c1, CORNER *c2, PROCESS *p, struct MetaBall *mb);
int setcenter(CENTERLIST *table[], int i, int j, int k);
int otherface (int edge, int face);
void makecubetable (void);
void setedge (EDGELIST *table[], int i1, int j1, int k1, int i2, int j2, int k2, int vid);
int getedge (EDGELIST *table[], int i1, int j1, int k1, int i2, int j2, int k2);
void addtovertices (VERTICES *vertices, VERTEX v);
void vnormal (MB_POINT *point, PROCESS *p, MB_POINT *v);
void converge (MB_POINT *p1, MB_POINT *p2, float v1, float v2, float (*function)(float, float, float), MB_POINT *p, struct MetaBall *mb, int f);
void add_cube(PROCESS *mbproc, int i, int j, int k, int count);
void find_first_points(PROCESS *mbproc, struct MetaBall *mb, int a);

void fill_metaball_octal_node(octal_node *node, struct MetaElem *ml, short i);
void subdivide_metaball_octal_node(octal_node *node, float size_x, float size_y, float size_z, short depth);
void free_metaball_octal_node(octal_node *node);
void init_metaball_octal_tree(int depth);
void polygonize(PROCESS *mbproc, struct MetaBall *mb);
float init_meta(struct Scene *scene, struct Object *ob);

void unlink_mball(struct MetaBall *mb);
void free_mball(struct MetaBall *mb);
struct MetaBall *add_mball(char *name);
struct MetaBall *copy_mball(struct MetaBall *mb);
void make_local_mball(struct MetaBall *mb);
void tex_space_mball(struct Object *ob);
float *make_orco_mball(struct Object *ob);
void copy_mball_properties(struct Scene *scene, struct Object *active_object);
struct Object *find_basis_mball(struct Scene *scene, struct Object *ob);
int is_basis_mball(struct Object *ob);
void metaball_polygonize(struct Scene *scene, struct Object *ob);
void calc_mballco(struct MetaElem *ml, float *vec);
float densfunc(struct MetaElem *ball, float x, float y, float z);
float metaball(float x, float y, float z);
void accum_mballfaces(int i1, int i2, int i3, int i4);
void *new_pgn_element(int size);
int nextcwedge (int edge, int face);
void BKE_freecubetable(void);

#endif

