/*
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
 */

/** \file
 * \ingroup bke
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"
#include "BLI_memarena.h"

#include "BKE_global.h"

#include "BKE_displist.h"
#include "BKE_mball_tessellate.h" /* own include */
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "BLI_strict_flags.h"

/* experimental (faster) normal calculation */
// #define USE_ACCUM_NORMAL

/* Data types */

typedef struct corner { /* corner of a cube */
  int i, j, k;          /* (i, j, k) is index within lattice */
  float co[3], value;   /* location and function value */
  struct corner *next;
} CORNER;

typedef struct cube { /* partitioning cell (cube) */
  int i, j, k;        /* lattice location of cube */
  CORNER *corners[8]; /* eight corners */
} CUBE;

typedef struct cubes { /* linked list of cubes acting as stack */
  CUBE cube;           /* a single cube */
  struct cubes *next;  /* remaining elements */
} CUBES;

typedef struct centerlist { /* list of cube locations */
  int i, j, k;              /* cube location */
  struct centerlist *next;  /* remaining elements */
} CENTERLIST;

typedef struct edgelist {     /* list of edges */
  int i1, j1, k1, i2, j2, k2; /* edge corner ids */
  int vid;                    /* vertex id */
  struct edgelist *next;      /* remaining elements */
} EDGELIST;

typedef struct intlist { /* list of integers */
  int i;                 /* an integer */
  struct intlist *next;  /* remaining elements */
} INTLIST;

typedef struct intlists { /* list of list of integers */
  INTLIST *list;          /* a list of integers */
  struct intlists *next;  /* remaining elements */
} INTLISTS;

typedef struct Box { /* an AABB with pointer to metalelem */
  float min[3], max[3];
  const MetaElem *ml;
} Box;

typedef struct MetaballBVHNode { /* BVH node */
  Box bb[2];                     /* AABB of children */
  struct MetaballBVHNode *child[2];
} MetaballBVHNode;

typedef struct process {     /* parameters, storage */
  float thresh, size;        /* mball threshold, single cube size */
  float delta;               /* small delta for calculating normals */
  unsigned int converge_res; /* converge procedure resolution (more = slower) */

  MetaElem **mainb;          /* array of all metaelems */
  unsigned int totelem, mem; /* number of metaelems */

  MetaballBVHNode metaball_bvh; /* The simplest bvh */
  Box allbb;                    /* Bounding box of all metaelems */

  MetaballBVHNode **bvh_queue; /* Queue used during bvh traversal */
  unsigned int bvh_queue_size;

  CUBES *cubes;         /* stack of cubes waiting for polygonization */
  CENTERLIST **centers; /* cube center hash table */
  CORNER **corners;     /* corner value hash table */
  EDGELIST **edges;     /* edge and vertex id hash table */

  int (*indices)[4];     /* output indices */
  unsigned int totindex; /* size of memory allocated for indices */
  unsigned int curindex; /* number of currently added indices */

  float (*co)[3], (*no)[3]; /* surface vertices - positions and normals */
  unsigned int totvertex;   /* memory size */
  unsigned int curvertex;   /* currently added vertices */

  /* memory allocation from common pool */
  MemArena *pgn_elements;
} PROCESS;

/* Forward declarations */
static int vertid(PROCESS *process, const CORNER *c1, const CORNER *c2);
static void add_cube(PROCESS *process, int i, int j, int k);
static void make_face(PROCESS *process, int i1, int i2, int i3, int i4);
static void converge(PROCESS *process, const CORNER *c1, const CORNER *c2, float r_p[3]);

/* ******************* SIMPLE BVH ********************* */

static void make_box_union(const BoundBox *a, const Box *b, Box *r_out)
{
  r_out->min[0] = min_ff(a->vec[0][0], b->min[0]);
  r_out->min[1] = min_ff(a->vec[0][1], b->min[1]);
  r_out->min[2] = min_ff(a->vec[0][2], b->min[2]);

  r_out->max[0] = max_ff(a->vec[6][0], b->max[0]);
  r_out->max[1] = max_ff(a->vec[6][1], b->max[1]);
  r_out->max[2] = max_ff(a->vec[6][2], b->max[2]);
}

static void make_box_from_metaelem(Box *r, const MetaElem *ml)
{
  copy_v3_v3(r->max, ml->bb->vec[6]);
  copy_v3_v3(r->min, ml->bb->vec[0]);
  r->ml = ml;
}

/**
 * Partitions part of mainb array [start, end) along axis s. Returns i,
 * where centroids of elements in the [start, i) segment lie "on the right side" of div,
 * and elements in the [i, end) segment lie "on the left"
 */
static unsigned int partition_mainb(
    MetaElem **mainb, unsigned int start, unsigned int end, unsigned int s, float div)
{
  unsigned int i = start, j = end - 1;
  div *= 2.0f;

  while (1) {
    while (i < j && div > (mainb[i]->bb->vec[6][s] + mainb[i]->bb->vec[0][s])) {
      i++;
    }
    while (j > i && div < (mainb[j]->bb->vec[6][s] + mainb[j]->bb->vec[0][s])) {
      j--;
    }

    if (i >= j) {
      break;
    }

    SWAP(MetaElem *, mainb[i], mainb[j]);
    i++;
    j--;
  }

  if (i == start) {
    i++;
  }

  return i;
}

/**
 * Recursively builds a BVH, dividing elements along the middle of the longest axis of allbox.
 */
static void build_bvh_spatial(PROCESS *process,
                              MetaballBVHNode *node,
                              unsigned int start,
                              unsigned int end,
                              const Box *allbox)
{
  unsigned int part, j, s;
  float dim[3], div;

  /* Maximum bvh queue size is number of nodes which are made, equals calls to this function. */
  process->bvh_queue_size++;

  dim[0] = allbox->max[0] - allbox->min[0];
  dim[1] = allbox->max[1] - allbox->min[1];
  dim[2] = allbox->max[2] - allbox->min[2];

  s = 0;
  if (dim[1] > dim[0] && dim[1] > dim[2]) {
    s = 1;
  }
  else if (dim[2] > dim[1] && dim[2] > dim[0]) {
    s = 2;
  }

  div = allbox->min[s] + (dim[s] / 2.0f);

  part = partition_mainb(process->mainb, start, end, s, div);

  make_box_from_metaelem(&node->bb[0], process->mainb[start]);
  node->child[0] = NULL;

  if (part > start + 1) {
    for (j = start; j < part; j++) {
      make_box_union(process->mainb[j]->bb, &node->bb[0], &node->bb[0]);
    }

    node->child[0] = BLI_memarena_alloc(process->pgn_elements, sizeof(MetaballBVHNode));
    build_bvh_spatial(process, node->child[0], start, part, &node->bb[0]);
  }

  node->child[1] = NULL;
  if (part < end) {
    make_box_from_metaelem(&node->bb[1], process->mainb[part]);

    if (part < end - 1) {
      for (j = part; j < end; j++) {
        make_box_union(process->mainb[j]->bb, &node->bb[1], &node->bb[1]);
      }

      node->child[1] = BLI_memarena_alloc(process->pgn_elements, sizeof(MetaballBVHNode));
      build_bvh_spatial(process, node->child[1], part, end, &node->bb[1]);
    }
  }
  else {
    INIT_MINMAX(node->bb[1].min, node->bb[1].max);
  }
}

/* ******************** ARITH ************************* */

/**
 * BASED AT CODE (but mostly rewritten) :
 * C code from the article
 * "An Implicit Surface Polygonizer"
 * by Jules Bloomenthal, jbloom@beauty.gmu.edu
 * in "Graphics Gems IV", Academic Press, 1994
 *
 * Authored by Jules Bloomenthal, Xerox PARC.
 * Copyright (c) Xerox Corporation, 1991.  All rights reserved.
 * Permission is granted to reproduce, use and distribute this code for
 * any and all purposes, provided that this notice appears in all copies.
 */

#define L 0   /* left direction:   -x, -i */
#define R 1   /* right direction:  +x, +i */
#define B 2   /* bottom direction: -y, -j */
#define T 3   /* top direction:    +y, +j */
#define N 4   /* near direction:   -z, -k */
#define F 5   /* far direction:    +z, +k */
#define LBN 0 /* left bottom near corner  */
#define LBF 1 /* left bottom far corner   */
#define LTN 2 /* left top near corner     */
#define LTF 3 /* left top far corner      */
#define RBN 4 /* right bottom near corner */
#define RBF 5 /* right bottom far corner  */
#define RTN 6 /* right top near corner    */
#define RTF 7 /* right top far corner     */

/**
 * the LBN corner of cube (i, j, k), corresponds with location
 * (i-0.5)*size, (j-0.5)*size, (k-0.5)*size)
 */

#define HASHBIT (5)
#define HASHSIZE (size_t)(1 << (3 * HASHBIT)) /*! < hash table size (32768) */

#define HASH(i, j, k) ((((((i)&31) << 5) | ((j)&31)) << 5) | ((k)&31))

#define MB_BIT(i, bit) (((i) >> (bit)) & 1)
// #define FLIP(i, bit) ((i) ^ 1 << (bit)) /* flip the given bit of i */

/* ******************** DENSITY COPMPUTATION ********************* */

/**
 * Computes density from given metaball at given position.
 * Metaball equation is: ``(1 - r^2 / R^2)^3 * s``
 *
 * r = distance from center
 * R = metaball radius
 * s - metaball stiffness
 */
static float densfunc(const MetaElem *ball, float x, float y, float z)
{
  float dist2;
  float dvec[3] = {x, y, z};

  mul_m4_v3((float(*)[4])ball->imat, dvec);

  switch (ball->type) {
    case MB_BALL:
      /* do nothing */
      break;
    case MB_CUBE:
      if (dvec[2] > ball->expz) {
        dvec[2] -= ball->expz;
      }
      else if (dvec[2] < -ball->expz) {
        dvec[2] += ball->expz;
      }
      else {
        dvec[2] = 0.0;
      }
      ATTR_FALLTHROUGH;
    case MB_PLANE:
      if (dvec[1] > ball->expy) {
        dvec[1] -= ball->expy;
      }
      else if (dvec[1] < -ball->expy) {
        dvec[1] += ball->expy;
      }
      else {
        dvec[1] = 0.0;
      }
      ATTR_FALLTHROUGH;
    case MB_TUBE:
      if (dvec[0] > ball->expx) {
        dvec[0] -= ball->expx;
      }
      else if (dvec[0] < -ball->expx) {
        dvec[0] += ball->expx;
      }
      else {
        dvec[0] = 0.0;
      }
      break;
    case MB_ELIPSOID:
      dvec[0] /= ball->expx;
      dvec[1] /= ball->expy;
      dvec[2] /= ball->expz;
      break;

    /* *** deprecated, could be removed?, do-versioned at least *** */
    case MB_TUBEX:
      if (dvec[0] > ball->len) {
        dvec[0] -= ball->len;
      }
      else if (dvec[0] < -ball->len) {
        dvec[0] += ball->len;
      }
      else {
        dvec[0] = 0.0;
      }
      break;
    case MB_TUBEY:
      if (dvec[1] > ball->len) {
        dvec[1] -= ball->len;
      }
      else if (dvec[1] < -ball->len) {
        dvec[1] += ball->len;
      }
      else {
        dvec[1] = 0.0;
      }
      break;
    case MB_TUBEZ:
      if (dvec[2] > ball->len) {
        dvec[2] -= ball->len;
      }
      else if (dvec[2] < -ball->len) {
        dvec[2] += ball->len;
      }
      else {
        dvec[2] = 0.0;
      }
      break;
      /* *** end deprecated *** */
  }

  /* ball->rad2 is inverse of squared rad */
  dist2 = 1.0f - (len_squared_v3(dvec) * ball->rad2);

  /* ball->s is negative if metaball is negative */
  return (dist2 < 0.0f) ? 0.0f : (ball->s * dist2 * dist2 * dist2);
}

/**
 * Computes density at given position form all metaballs which contain this point in their box.
 * Traverses BVH using a queue.
 */
static float metaball(PROCESS *process, float x, float y, float z)
{
  int i;
  float dens = 0.0f;
  unsigned int front = 0, back = 0;
  MetaballBVHNode *node;

  process->bvh_queue[front++] = &process->metaball_bvh;

  while (front != back) {
    node = process->bvh_queue[back++];

    for (i = 0; i < 2; i++) {
      if ((node->bb[i].min[0] <= x) && (node->bb[i].max[0] >= x) && (node->bb[i].min[1] <= y) &&
          (node->bb[i].max[1] >= y) && (node->bb[i].min[2] <= z) && (node->bb[i].max[2] >= z)) {
        if (node->child[i]) {
          process->bvh_queue[front++] = node->child[i];
        }
        else {
          dens += densfunc(node->bb[i].ml, x, y, z);
        }
      }
    }
  }

  return process->thresh - dens;
}

/**
 * Adds face to indices, expands memory if needed.
 */
static void make_face(PROCESS *process, int i1, int i2, int i3, int i4)
{
  int *cur;

#ifdef USE_ACCUM_NORMAL
  float n[3];
#endif

  if (UNLIKELY(process->totindex == process->curindex)) {
    process->totindex += 4096;
    process->indices = MEM_reallocN(process->indices, sizeof(int[4]) * process->totindex);
  }

  cur = process->indices[process->curindex++];

  /* displists now support array drawing, we treat tri's as fake quad */

  cur[0] = i1;
  cur[1] = i2;
  cur[2] = i3;
  cur[3] = i4;

#ifdef USE_ACCUM_NORMAL
  if (i4 == i3) {
    normal_tri_v3(n, process->co[i1], process->co[i2], process->co[i3]);
    accumulate_vertex_normals_v3(process->no[i1],
                                 process->no[i2],
                                 process->no[i3],
                                 NULL,
                                 n,
                                 process->co[i1],
                                 process->co[i2],
                                 process->co[i3],
                                 NULL);
  }
  else {
    normal_quad_v3(n, process->co[i1], process->co[i2], process->co[i3], process->co[i4]);
    accumulate_vertex_normals_v3(process->no[i1],
                                 process->no[i2],
                                 process->no[i3],
                                 process->no[i4],
                                 n,
                                 process->co[i1],
                                 process->co[i2],
                                 process->co[i3],
                                 process->co[i4]);
  }
#endif
}

/* Frees allocated memory */
static void freepolygonize(PROCESS *process)
{
  if (process->corners) {
    MEM_freeN(process->corners);
  }
  if (process->edges) {
    MEM_freeN(process->edges);
  }
  if (process->centers) {
    MEM_freeN(process->centers);
  }
  if (process->mainb) {
    MEM_freeN(process->mainb);
  }
  if (process->bvh_queue) {
    MEM_freeN(process->bvh_queue);
  }
  if (process->pgn_elements) {
    BLI_memarena_free(process->pgn_elements);
  }
}

/* **************** POLYGONIZATION ************************ */

/**** Cubical Polygonization (optional) ****/

#define LB 0  /* left bottom edge */
#define LT 1  /* left top edge */
#define LN 2  /* left near edge */
#define LF 3  /* left far edge */
#define RB 4  /* right bottom edge */
#define RT 5  /* right top edge */
#define RN 6  /* right near edge */
#define RF 7  /* right far edge */
#define BN 8  /* bottom near edge */
#define BF 9  /* bottom far edge */
#define TN 10 /* top near edge */
#define TF 11 /* top far edge */

static INTLISTS *cubetable[256];
static char faces[256];

/* edge: LB, LT, LN, LF, RB, RT, RN, RF, BN, BF, TN, TF */
static int corner1[12] = {
    LBN,
    LTN,
    LBN,
    LBF,
    RBN,
    RTN,
    RBN,
    RBF,
    LBN,
    LBF,
    LTN,
    LTF,
};
static int corner2[12] = {
    LBF,
    LTF,
    LTN,
    LTF,
    RBF,
    RTF,
    RTN,
    RTF,
    RBN,
    RBF,
    RTN,
    RTF,
};
static int leftface[12] = {
    B,
    L,
    L,
    F,
    R,
    T,
    N,
    R,
    N,
    B,
    T,
    F,
};
/* face on left when going corner1 to corner2 */
static int rightface[12] = {
    L,
    T,
    N,
    L,
    B,
    R,
    R,
    F,
    B,
    F,
    N,
    T,
};
/* face on right when going corner1 to corner2 */

/**
 * triangulate the cube directly, without decomposition
 */
static void docube(PROCESS *process, CUBE *cube)
{
  INTLISTS *polys;
  CORNER *c1, *c2;
  int i, index = 0, count, indexar[8];

  /* Determine which case cube falls into. */
  for (i = 0; i < 8; i++) {
    if (cube->corners[i]->value > 0.0f) {
      index += (1 << i);
    }
  }

  /* Using faces[] table, adds neighbouring cube if surface intersects face in this direction. */
  if (MB_BIT(faces[index], 0)) {
    add_cube(process, cube->i - 1, cube->j, cube->k);
  }
  if (MB_BIT(faces[index], 1)) {
    add_cube(process, cube->i + 1, cube->j, cube->k);
  }
  if (MB_BIT(faces[index], 2)) {
    add_cube(process, cube->i, cube->j - 1, cube->k);
  }
  if (MB_BIT(faces[index], 3)) {
    add_cube(process, cube->i, cube->j + 1, cube->k);
  }
  if (MB_BIT(faces[index], 4)) {
    add_cube(process, cube->i, cube->j, cube->k - 1);
  }
  if (MB_BIT(faces[index], 5)) {
    add_cube(process, cube->i, cube->j, cube->k + 1);
  }

  /* Using cubetable[], determines polygons for output. */
  for (polys = cubetable[index]; polys; polys = polys->next) {
    INTLIST *edges;

    count = 0;
    /* Sets needed vertex id's lying on the edges. */
    for (edges = polys->list; edges; edges = edges->next) {
      c1 = cube->corners[corner1[edges->i]];
      c2 = cube->corners[corner2[edges->i]];

      indexar[count] = vertid(process, c1, c2);
      count++;
    }

    /* Adds faces to output. */
    if (count > 2) {
      switch (count) {
        case 3:
          make_face(process, indexar[2], indexar[1], indexar[0], indexar[0]); /* triangle */
          break;
        case 4:
          make_face(process, indexar[3], indexar[2], indexar[1], indexar[0]);
          break;
        case 5:
          make_face(process, indexar[3], indexar[2], indexar[1], indexar[0]);
          make_face(process, indexar[4], indexar[3], indexar[0], indexar[0]); /* triangle */
          break;
        case 6:
          make_face(process, indexar[3], indexar[2], indexar[1], indexar[0]);
          make_face(process, indexar[5], indexar[4], indexar[3], indexar[0]);
          break;
        case 7:
          make_face(process, indexar[3], indexar[2], indexar[1], indexar[0]);
          make_face(process, indexar[5], indexar[4], indexar[3], indexar[0]);
          make_face(process, indexar[6], indexar[5], indexar[0], indexar[0]); /* triangle */
          break;
      }
    }
  }
}

/**
 * return corner with the given lattice location
 * set (and cache) its function value
 */
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

  c = BLI_memarena_alloc(process->pgn_elements, sizeof(CORNER));

  c->i = i;
  c->co[0] = ((float)i - 0.5f) * process->size;
  c->j = j;
  c->co[1] = ((float)j - 0.5f) * process->size;
  c->k = k;
  c->co[2] = ((float)k - 0.5f) * process->size;

  c->value = metaball(process, c->co[0], c->co[1], c->co[2]);

  c->next = process->corners[index];
  process->corners[index] = c;

  return c;
}

/**
 * return next clockwise edge from given edge around given face
 */
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

/**
 * \return the face adjoining edge that is not the given face
 */
static int otherface(int edge, int face)
{
  int other = leftface[edge];
  return face == other ? rightface[edge] : other;
}

/**
 * create the 256 entry table for cubical polygonization
 */
static void makecubetable(void)
{
  static bool is_done = false;
  int i, e, c, done[12], pos[8];

  if (is_done) {
    return;
  }
  is_done = true;

  for (i = 0; i < 256; i++) {
    for (e = 0; e < 12; e++) {
      done[e] = 0;
    }
    for (c = 0; c < 8; c++) {
      pos[c] = MB_BIT(i, c);
    }
    for (e = 0; e < 12; e++) {
      if (!done[e] && (pos[corner1[e]] != pos[corner2[e]])) {
        INTLIST *ints = NULL;
        INTLISTS *lists = MEM_callocN(sizeof(INTLISTS), "mball_intlist");
        int start = e, edge = e;

        /* get face that is to right of edge from pos to neg corner: */
        int face = pos[corner1[e]] ? rightface[e] : leftface[e];

        while (1) {
          edge = nextcwedge(edge, face);
          done[edge] = 1;
          if (pos[corner1[edge]] != pos[corner2[edge]]) {
            INTLIST *tmp = ints;

            ints = MEM_callocN(sizeof(INTLIST), "mball_intlist");
            ints->i = edge;
            ints->next = tmp; /* add edge to head of list */

            if (edge == start) {
              break;
            }
            face = otherface(edge, face);
          }
        }
        lists->list = ints; /* add ints to head of table entry */
        lists->next = cubetable[i];
        cubetable[i] = lists;
      }
    }
  }

  for (i = 0; i < 256; i++) {
    INTLISTS *polys;
    faces[i] = 0;
    for (polys = cubetable[i]; polys; polys = polys->next) {
      INTLIST *edges;

      for (edges = polys->list; edges; edges = edges->next) {
        if (edges->i == LB || edges->i == LT || edges->i == LN || edges->i == LF) {
          faces[i] |= 1 << L;
        }
        if (edges->i == RB || edges->i == RT || edges->i == RN || edges->i == RF) {
          faces[i] |= 1 << R;
        }
        if (edges->i == LB || edges->i == RB || edges->i == BN || edges->i == BF) {
          faces[i] |= 1 << B;
        }
        if (edges->i == LT || edges->i == RT || edges->i == TN || edges->i == TF) {
          faces[i] |= 1 << T;
        }
        if (edges->i == LN || edges->i == RN || edges->i == BN || edges->i == TN) {
          faces[i] |= 1 << N;
        }
        if (edges->i == LF || edges->i == RF || edges->i == BF || edges->i == TF) {
          faces[i] |= 1 << F;
        }
      }
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

/**
 * Inserts cube at lattice i, j, k into hash table, marking it as "done"
 */
static int setcenter(PROCESS *process, CENTERLIST *table[], const int i, const int j, const int k)
{
  int index;
  CENTERLIST *newc, *l, *q;

  index = HASH(i, j, k);
  q = table[index];

  for (l = q; l != NULL; l = l->next) {
    if (l->i == i && l->j == j && l->k == k) {
      return 1;
    }
  }

  newc = BLI_memarena_alloc(process->pgn_elements, sizeof(CENTERLIST));
  newc->i = i;
  newc->j = j;
  newc->k = k;
  newc->next = q;
  table[index] = newc;

  return 0;
}

/**
 * Sets vid of vertex lying on given edge.
 */
static void setedge(PROCESS *process, int i1, int j1, int k1, int i2, int j2, int k2, int vid)
{
  int index;
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
  newe = BLI_memarena_alloc(process->pgn_elements, sizeof(EDGELIST));

  newe->i1 = i1;
  newe->j1 = j1;
  newe->k1 = k1;
  newe->i2 = i2;
  newe->j2 = j2;
  newe->k2 = k2;
  newe->vid = vid;
  newe->next = process->edges[index];
  process->edges[index] = newe;
}

/**
 * \return vertex id for edge; return -1 if not set
 */
static int getedge(EDGELIST *table[], int i1, int j1, int k1, int i2, int j2, int k2)
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
    if (q->i1 == i1 && q->j1 == j1 && q->k1 == k1 && q->i2 == i2 && q->j2 == j2 && q->k2 == k2) {
      return q->vid;
    }
  }
  return -1;
}

/**
 * Adds a vertex, expands memory if needed.
 */
static void addtovertices(PROCESS *process, const float v[3], const float no[3])
{
  if (process->curvertex == process->totvertex) {
    process->totvertex += 4096;
    process->co = MEM_reallocN(process->co, process->totvertex * sizeof(float[3]));
    process->no = MEM_reallocN(process->no, process->totvertex * sizeof(float[3]));
  }

  copy_v3_v3(process->co[process->curvertex], v);
  copy_v3_v3(process->no[process->curvertex], no);

  process->curvertex++;
}

#ifndef USE_ACCUM_NORMAL
/**
 * Computes normal from density field at given point.
 *
 * \note Doesn't do normalization!
 */
static void vnormal(PROCESS *process, const float point[3], float r_no[3])
{
  const float delta = process->delta;
  const float f = metaball(process, point[0], point[1], point[2]);

  r_no[0] = metaball(process, point[0] + delta, point[1], point[2]) - f;
  r_no[1] = metaball(process, point[0], point[1] + delta, point[2]) - f;
  r_no[2] = metaball(process, point[0], point[1], point[2] + delta) - f;
}
#endif /* USE_ACCUM_NORMAL */

/**
 * \return the id of vertex between two corners.
 *
 * If it wasn't previously computed, does #converge() and adds vertex to process.
 */
static int vertid(PROCESS *process, const CORNER *c1, const CORNER *c2)
{
  float v[3], no[3];
  int vid = getedge(process->edges, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k);

  if (vid != -1) {
    return vid; /* previously computed */
  }

  converge(process, c1, c2, v); /* position */

#ifdef USE_ACCUM_NORMAL
  zero_v3(no);
#else
  vnormal(process, v, no);
#endif

  addtovertices(process, v, no); /* save vertex */
  vid = (int)process->curvertex - 1;
  setedge(process, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k, vid);

  return vid;
}

/**
 * Given two corners, computes approximation of surface intersection point between them.
 * In case of small threshold, do bisection.
 */
static void converge(PROCESS *process, const CORNER *c1, const CORNER *c2, float r_p[3])
{
  float tmp, dens;
  unsigned int i;
  float c1_value, c1_co[3];
  float c2_value, c2_co[3];

  if (c1->value < c2->value) {
    c1_value = c2->value;
    copy_v3_v3(c1_co, c2->co);
    c2_value = c1->value;
    copy_v3_v3(c2_co, c1->co);
  }
  else {
    c1_value = c1->value;
    copy_v3_v3(c1_co, c1->co);
    c2_value = c2->value;
    copy_v3_v3(c2_co, c2->co);
  }

  for (i = 0; i < process->converge_res; i++) {
    interp_v3_v3v3(r_p, c1_co, c2_co, 0.5f);
    dens = metaball(process, r_p[0], r_p[1], r_p[2]);

    if (dens > 0.0f) {
      c1_value = dens;
      copy_v3_v3(c1_co, r_p);
    }
    else {
      c2_value = dens;
      copy_v3_v3(c2_co, r_p);
    }
  }

  tmp = -c1_value / (c2_value - c1_value);
  interp_v3_v3v3(r_p, c1_co, c2_co, tmp);
}

/**
 * Adds cube at given lattice position to cube stack of process.
 */
static void add_cube(PROCESS *process, int i, int j, int k)
{
  CUBES *ncube;
  int n;

  /* test if cube has been found before */
  if (setcenter(process, process->centers, i, j, k) == 0) {
    /* push cube on stack: */
    ncube = BLI_memarena_alloc(process->pgn_elements, sizeof(CUBES));
    ncube->next = process->cubes;
    process->cubes = ncube;

    ncube->cube.i = i;
    ncube->cube.j = j;
    ncube->cube.k = k;

    /* set corners of initial cube: */
    for (n = 0; n < 8; n++) {
      ncube->cube.corners[n] = setcorner(
          process, i + MB_BIT(n, 2), j + MB_BIT(n, 1), k + MB_BIT(n, 0));
    }
  }
}

static void next_lattice(int r[3], const float pos[3], const float size)
{
  r[0] = (int)ceil((pos[0] / size) + 0.5f);
  r[1] = (int)ceil((pos[1] / size) + 0.5f);
  r[2] = (int)ceil((pos[2] / size) + 0.5f);
}
static void prev_lattice(int r[3], const float pos[3], const float size)
{
  next_lattice(r, pos, size);
  r[0]--;
  r[1]--;
  r[2]--;
}
static void closest_latice(int r[3], const float pos[3], const float size)
{
  r[0] = (int)floorf(pos[0] / size + 1.0f);
  r[1] = (int)floorf(pos[1] / size + 1.0f);
  r[2] = (int)floorf(pos[2] / size + 1.0f);
}

/**
 * Find at most 26 cubes to start polygonization from.
 */
static void find_first_points(PROCESS *process, const unsigned int em)
{
  const MetaElem *ml;
  int center[3], lbn[3], rtf[3], it[3], dir[3], add[3];
  float tmp[3], a, b;

  ml = process->mainb[em];

  mid_v3_v3v3(tmp, ml->bb->vec[0], ml->bb->vec[6]);
  closest_latice(center, tmp, process->size);
  prev_lattice(lbn, ml->bb->vec[0], process->size);
  next_lattice(rtf, ml->bb->vec[6], process->size);

  for (dir[0] = -1; dir[0] <= 1; dir[0]++) {
    for (dir[1] = -1; dir[1] <= 1; dir[1]++) {
      for (dir[2] = -1; dir[2] <= 1; dir[2]++) {
        if (dir[0] == 0 && dir[1] == 0 && dir[2] == 0) {
          continue;
        }

        copy_v3_v3_int(it, center);

        b = setcorner(process, it[0], it[1], it[2])->value;
        do {
          it[0] += dir[0];
          it[1] += dir[1];
          it[2] += dir[2];
          a = b;
          b = setcorner(process, it[0], it[1], it[2])->value;

          if (a * b < 0.0f) {
            add[0] = it[0] - dir[0];
            add[1] = it[1] - dir[1];
            add[2] = it[2] - dir[2];
            DO_MIN(it, add);
            add_cube(process, add[0], add[1], add[2]);
            break;
          }
        } while ((it[0] > lbn[0]) && (it[1] > lbn[1]) && (it[2] > lbn[2]) && (it[0] < rtf[0]) &&
                 (it[1] < rtf[1]) && (it[2] < rtf[2]));
      }
    }
  }
}

/**
 * The main polygonization proc.
 * Allocates memory, makes cubetable,
 * finds starting surface points
 * and processes cubes on the stack until none left.
 */
static void polygonize(PROCESS *process)
{
  CUBE c;
  unsigned int i;

  process->centers = MEM_callocN(HASHSIZE * sizeof(CENTERLIST *), "mbproc->centers");
  process->corners = MEM_callocN(HASHSIZE * sizeof(CORNER *), "mbproc->corners");
  process->edges = MEM_callocN(2 * HASHSIZE * sizeof(EDGELIST *), "mbproc->edges");
  process->bvh_queue = MEM_callocN(sizeof(MetaballBVHNode *) * process->bvh_queue_size,
                                   "Metaball BVH Queue");

  makecubetable();

  for (i = 0; i < process->totelem; i++) {
    find_first_points(process, i);
  }

  while (process->cubes != NULL) {
    c = process->cubes->cube;
    process->cubes = process->cubes->next;

    docube(process, &c);
  }
}

/**
 * Iterates over ALL objects in the scene and all of its sets, including
 * making all duplis(not only metas). Copies metas to mainb array.
 * Computes bounding boxes for building BVH. */
static void init_meta(Depsgraph *depsgraph, PROCESS *process, Scene *scene, Object *ob)
{
  Scene *sce_iter = scene;
  Base *base;
  Object *bob;
  MetaBall *mb;
  const MetaElem *ml;
  float obinv[4][4], obmat[4][4];
  unsigned int i;
  int obnr, zero_size = 0;
  char obname[MAX_ID_NAME];
  SceneBaseIter iter;

  copy_m4_m4(obmat, ob->obmat); /* to cope with duplicators from BKE_scene_base_iter_next */
  invert_m4_m4(obinv, ob->obmat);

  BLI_split_name_num(obname, &obnr, ob->id.name + 2, '.');

  /* make main array */
  BKE_scene_base_iter_next(depsgraph, &iter, &sce_iter, 0, NULL, NULL);
  while (BKE_scene_base_iter_next(depsgraph, &iter, &sce_iter, 1, &base, &bob)) {
    if (bob->type == OB_MBALL) {
      zero_size = 0;
      ml = NULL;

      if (bob == ob && (base->flag_legacy & OB_FROMDUPLI) == 0) {
        mb = ob->data;

        if (mb->editelems) {
          ml = mb->editelems->first;
        }
        else {
          ml = mb->elems.first;
        }
      }
      else {
        char name[MAX_ID_NAME];
        int nr;

        BLI_split_name_num(name, &nr, bob->id.name + 2, '.');
        if (STREQ(obname, name)) {
          mb = bob->data;

          if (mb->editelems) {
            ml = mb->editelems->first;
          }
          else {
            ml = mb->elems.first;
          }
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
        while (ml) {
          ml = ml->next;
        }
      }
      else {
        while (ml) {
          if (!(ml->flag & MB_HIDE)) {
            float pos[4][4], rot[4][4];
            float expx, expy, expz;
            float tempmin[3], tempmax[3];

            MetaElem *new_ml;

            /* make a copy because of duplicates */
            new_ml = BLI_memarena_alloc(process->pgn_elements, sizeof(MetaElem));
            *(new_ml) = *ml;
            new_ml->bb = BLI_memarena_alloc(process->pgn_elements, sizeof(BoundBox));
            new_ml->mat = BLI_memarena_alloc(process->pgn_elements, 4 * 4 * sizeof(float));
            new_ml->imat = BLI_memarena_alloc(process->pgn_elements, 4 * 4 * sizeof(float));

            /* too big stiffness seems only ugly due to linear interpolation
             * no need to have possibility for too big stiffness */
            if (ml->s > 10.0f) {
              new_ml->s = 10.0f;
            }
            else {
              new_ml->s = ml->s;
            }

            /* if metaball is negative, set stiffness negative */
            if (new_ml->flag & MB_NEGATIVE) {
              new_ml->s = -new_ml->s;
            }

            /* Translation of MetaElem */
            unit_m4(pos);
            pos[3][0] = ml->x;
            pos[3][1] = ml->y;
            pos[3][2] = ml->z;

            /* Rotation of MetaElem is stored in quat */
            quat_to_mat4(rot, ml->quat);

            /* Matrix multiply is as follows:
             *   basis object space ->
             *   world ->
             *   ml object space ->
             *   position ->
             *   rotation ->
             *   ml local space
             */
            mul_m4_series((float(*)[4])new_ml->mat, obinv, bob->obmat, pos, rot);
            /* ml local space -> basis object space */
            invert_m4_m4((float(*)[4])new_ml->imat, (float(*)[4])new_ml->mat);

            /* rad2 is inverse of squared radius */
            new_ml->rad2 = 1 / (ml->rad * ml->rad);

            /* initial dimensions = radius */
            expx = ml->rad;
            expy = ml->rad;
            expz = ml->rad;

            switch (ml->type) {
              case MB_BALL:
                break;
              case MB_CUBE: /* cube is "expanded" by expz, expy and expx */
                expz += ml->expz;
                ATTR_FALLTHROUGH;
              case MB_PLANE: /* plane is "expanded" by expy and expx */
                expy += ml->expy;
                ATTR_FALLTHROUGH;
              case MB_TUBE: /* tube is "expanded" by expx */
                expx += ml->expx;
                break;
              case MB_ELIPSOID: /* ellipsoid is "stretched" by exp* */
                expx *= ml->expx;
                expy *= ml->expy;
                expz *= ml->expz;
                break;
            }

            /* untransformed Bounding Box of MetaElem */
            /* TODO, its possible the elem type has been changed and the exp*
             * values can use a fallback. */
            copy_v3_fl3(new_ml->bb->vec[0], -expx, -expy, -expz); /* 0 */
            copy_v3_fl3(new_ml->bb->vec[1], +expx, -expy, -expz); /* 1 */
            copy_v3_fl3(new_ml->bb->vec[2], +expx, +expy, -expz); /* 2 */
            copy_v3_fl3(new_ml->bb->vec[3], -expx, +expy, -expz); /* 3 */
            copy_v3_fl3(new_ml->bb->vec[4], -expx, -expy, +expz); /* 4 */
            copy_v3_fl3(new_ml->bb->vec[5], +expx, -expy, +expz); /* 5 */
            copy_v3_fl3(new_ml->bb->vec[6], +expx, +expy, +expz); /* 6 */
            copy_v3_fl3(new_ml->bb->vec[7], -expx, +expy, +expz); /* 7 */

            /* transformation of Metalem bb */
            for (i = 0; i < 8; i++) {
              mul_m4_v3((float(*)[4])new_ml->mat, new_ml->bb->vec[i]);
            }

            /* find max and min of transformed bb */
            INIT_MINMAX(tempmin, tempmax);
            for (i = 0; i < 8; i++) {
              DO_MINMAX(new_ml->bb->vec[i], tempmin, tempmax);
            }

            /* set only point 0 and 6 - AABB of Metaelem */
            copy_v3_v3(new_ml->bb->vec[0], tempmin);
            copy_v3_v3(new_ml->bb->vec[6], tempmax);

            /* add new_ml to mainb[] */
            if (UNLIKELY(process->totelem == process->mem)) {
              process->mem = process->mem * 2 + 10;
              process->mainb = MEM_reallocN(process->mainb, sizeof(MetaElem *) * process->mem);
            }
            process->mainb[process->totelem++] = new_ml;
          }
          ml = ml->next;
        }
      }
    }
  }

  /* compute AABB of all Metaelems */
  if (process->totelem > 0) {
    copy_v3_v3(process->allbb.min, process->mainb[0]->bb->vec[0]);
    copy_v3_v3(process->allbb.max, process->mainb[0]->bb->vec[6]);
    for (i = 1; i < process->totelem; i++) {
      make_box_union(process->mainb[i]->bb, &process->allbb, &process->allbb);
    }
  }
}

void BKE_mball_polygonize(Depsgraph *depsgraph, Scene *scene, Object *ob, ListBase *dispbase)
{
  MetaBall *mb;
  DispList *dl;
  unsigned int a;
  PROCESS process = {0};
  bool is_render = DEG_get_mode(depsgraph) == DAG_EVAL_RENDER;

  mb = ob->data;

  process.thresh = mb->thresh;

  if (process.thresh < 0.001f) {
    process.converge_res = 16;
  }
  else if (process.thresh < 0.01f) {
    process.converge_res = 8;
  }
  else if (process.thresh < 0.1f) {
    process.converge_res = 4;
  }
  else {
    process.converge_res = 2;
  }

  if (is_render && (mb->flag == MB_UPDATE_NEVER)) {
    return;
  }
  if ((G.moving & (G_TRANSFORM_OBJ | G_TRANSFORM_EDIT)) && mb->flag == MB_UPDATE_FAST) {
    return;
  }

  if (is_render) {
    process.size = mb->rendersize;
  }
  else {
    process.size = mb->wiresize;
    if ((G.moving & (G_TRANSFORM_OBJ | G_TRANSFORM_EDIT)) && mb->flag == MB_UPDATE_HALFRES) {
      process.size *= 2.0f;
    }
  }

  process.delta = process.size * 0.001f;

  process.pgn_elements = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "Metaball memarena");

  /* initialize all mainb (MetaElems) */
  init_meta(depsgraph, &process, scene, ob);

  if (process.totelem > 0) {
    build_bvh_spatial(&process, &process.metaball_bvh, 0, process.totelem, &process.allbb);

    /* Don't polygonize metaballs with too high resolution (base mball to small)
     * note: Eps was 0.0001f but this was giving problems for blood animation for durian,
     * using 0.00001f. */
    if (ob->scale[0] > 0.00001f * (process.allbb.max[0] - process.allbb.min[0]) ||
        ob->scale[1] > 0.00001f * (process.allbb.max[1] - process.allbb.min[1]) ||
        ob->scale[2] > 0.00001f * (process.allbb.max[2] - process.allbb.min[2])) {
      polygonize(&process);

      /* add resulting surface to displist */
      if (process.curindex) {
        dl = MEM_callocN(sizeof(DispList), "mballdisp");
        BLI_addtail(dispbase, dl);
        dl->type = DL_INDEX4;
        dl->nr = (int)process.curvertex;
        dl->parts = (int)process.curindex;

        dl->index = (int *)process.indices;

        for (a = 0; a < process.curvertex; a++) {
          normalize_v3(process.no[a]);
        }

        dl->verts = (float *)process.co;
        dl->nors = (float *)process.no;
      }
    }
  }

  freepolygonize(&process);
}
