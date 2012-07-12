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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_SCANFILL_H__
#define __BLI_SCANFILL_H__

/** \file BLI_scanfill.h
 *  \ingroup bli
 *  \since March 2001
 *  \author nzc
 *  \brief Filling meshes.
 */

struct ScanFillVert;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScanFillContext {
	ListBase fillvertbase;
	ListBase filledgebase;
	ListBase fillfacebase;

	/* simple optimization for allocating thousands of small memory blocks
	 * only to be used within loops, and not by one function at a time
	 * free in the end, with argument '-1'
	 */
#define MEM_ELEM_BLOCKSIZE 16384
	struct mem_elements *melem__cur;
	int melem__offs;                   /* the current free address */
	ListBase melem__lb;

	/* private */
	struct ScanFillVertLink *_scdata;
} ScanFillContext;

/* note; changing this also might affect the undo copy in editmesh.c */
typedef struct ScanFillVert {
	struct ScanFillVert *next, *prev;
	union {
		struct ScanFillVert *v;
		void                *p;
		intptr_t             l;
		unsigned int         u;
	} tmp;
	float co[3]; /* vertex location */
	float xy[2]; /* 2D copy of vertex location (using dominant axis) */
	unsigned int keyindex; /* original index #, for restoring  key information */
	short poly_nr;
	unsigned char f, h;
} ScanFillVert;

typedef struct ScanFillEdge {
	struct ScanFillEdge *next, *prev;
	struct ScanFillVert *v1, *v2;
	short poly_nr;
	unsigned char f;
	union {
		unsigned char c;
	} tmp;
} ScanFillEdge;

typedef struct ScanFillFace {
	struct ScanFillFace *next, *prev;
	struct ScanFillVert *v1, *v2, *v3;
} ScanFillFace;

/* scanfill.c: used in displist only... */
struct ScanFillVert *BLI_scanfill_vert_add(ScanFillContext *sf_ctx, const float vec[3]);
struct ScanFillEdge *BLI_scanfill_edge_add(ScanFillContext *sf_ctx, struct ScanFillVert *v1, struct ScanFillVert *v2);

int BLI_scanfill_begin(ScanFillContext *sf_ctx);
int BLI_scanfill_calc(ScanFillContext *sf_ctx, const short do_quad_tri_speedup);
int BLI_scanfill_calc_ex(ScanFillContext *sf_ctx, const short do_quad_tri_speedup,
                         const float nor_proj[3]);
void BLI_scanfill_end(ScanFillContext *sf_ctx);

/* These callbacks are needed to make the lib finction properly */

/**
 * Set a function taking a char* as argument to flag errors. If the
 * callback is not set, the error is discarded.
 * \param f The function to use as callback
 * \attention used in creator.c
 */
void BLI_setErrorCallBack(void (*f)(const char *));

/**
 * Set a function to be able to interrupt the execution of processing
 * in this module. If the function returns true, the execution will
 * terminate gracefully. If the callback is not set, interruption is
 * not possible.
 * \param f The function to use as callback
 * \attention used in creator.c
 */
void BLI_setInterruptCallBack(int (*f)(void));

#ifdef __cplusplus
}
#endif

#endif

