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

/**
 * \attention Defined in scanfill.c
 */
extern struct ListBase fillvertbase;
extern struct ListBase filledgebase;
extern struct ListBase fillfacebase;

struct ScanFillVert;

#ifdef __cplusplus
extern "C" {
#endif

/* note; changing this also might affect the undo copy in editmesh.c */
typedef struct ScanFillVert
{
	struct ScanFillVert *next, *prev;
	union {
		struct ScanFillVert *v;
		void            *p;
		intptr_t         l;
	} tmp;
	float co[3]; /*vertex location */
	int keyindex; /* original index #, for restoring  key information */
	short poly_nr;
	unsigned char f, h;
} ScanFillVert;

typedef struct ScanFillEdge
{
	struct ScanFillEdge *next, *prev;
	struct ScanFillVert *v1, *v2;
	short poly_nr;
	unsigned char f;
} ScanFillEdge;

typedef struct ScanFillFace
{
	struct ScanFillFace *next, *prev;
	struct ScanFillVert *v1, *v2, *v3;
} ScanFillFace;

/* scanfill.c: used in displist only... */
struct ScanFillVert *BLI_addfillvert(const float vec[3]);
struct ScanFillEdge *BLI_addfilledge(struct ScanFillVert *v1, struct ScanFillVert *v2);

/* Optionally set ScanFillEdge f to this to mark original boundary edges.
 * Only needed if there are internal diagonal edges passed to BLI_edgefill. */
#define FILLBOUNDARY 1

int BLI_begin_edgefill(void);
int BLI_edgefill(short mat_nr);
void BLI_end_edgefill(void);

/* These callbacks are needed to make the lib finction properly */

/**
 * Set a function taking a char* as argument to flag errors. If the
 * callback is not set, the error is discarded.
 * \param f The function to use as callback
 * \attention used in creator.c
 */
void BLI_setErrorCallBack(void (*f)(const char*));

/**
 * Set a function to be able to interrupt the execution of processing
 * in this module. If the function returns true, the execution will
 * terminate gracefully. If the callback is not set, interruption is
 * not possible.
 * \param f The function to use as callback
 * \attention used in creator.c
 */
void BLI_setInterruptCallBack(int (*f)(void));

void BLI_scanfill_free(void);

#ifdef __cplusplus
}
#endif

#endif

