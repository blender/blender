/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 *
 * BMesh walker API.
 */

#include "bmesh_class.hh"

struct BMWalker;

extern BMWalker *bm_walker_types[];
extern const int bm_totwalkers;

/* Pointer hiding */
struct BMwGenericWalker {
  Link link;
  int depth;
};

struct BMwShellWalker {
  BMwGenericWalker header;
  BMEdge *curedge;
};

struct BMwLoopShellWalker {
  BMwGenericWalker header;
  BMLoop *curloop;
};

struct BMwLoopShellWireWalker {
  BMwGenericWalker header;
  BMElem *curelem;
};

struct BMwIslandboundWalker {
  BMwGenericWalker header;
  BMLoop *base;
  BMVert *lastv;
  BMLoop *curloop;
};

struct BMwIslandWalker {
  BMwGenericWalker header;
  BMFace *cur;
};

struct BMwEdgeLoopWalker {
  BMwGenericWalker header;
  BMEdge *cur, *start;
  BMVert *lastv, *startv;
  BMFace *f_hub;
  bool is_boundary; /* boundary looping changes behavior */
  bool is_single;   /* single means the edge verts are only connected to 1 face */
};

struct BMwFaceLoopWalker {
  BMwGenericWalker header;
  BMLoop *l;
  bool no_calc;
};

struct BMwEdgeringWalker {
  BMwGenericWalker header;
  BMLoop *l;
  BMEdge *wireedge;
};

struct BMwEdgeboundaryWalker {
  BMwGenericWalker header;
  BMEdge *e;
};

struct BMwNonManifoldEdgeLoopWalker {
  BMwGenericWalker header;
  BMEdge *start, *cur;
  BMVert *startv, *lastv;
  int face_count; /* face count around the edge. */
};

struct BMwUVEdgeWalker {
  BMwGenericWalker header;
  BMLoop *l;
};

struct BMwConnectedVertexWalker {
  BMwGenericWalker header;
  BMVert *curvert;
};
