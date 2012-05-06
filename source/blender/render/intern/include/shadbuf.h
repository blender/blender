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

#ifndef __SHADBUF_H__
#define __SHADBUF_H__

/** \file blender/render/intern/include/shadbuf.h
 *  \ingroup render
 */

#include "render_types.h"

struct ObjectRen;

/**
 * Calculates shadowbuffers for a vector of shadow-giving lamps
 * \param lar The vector of lamps
 */
void makeshadowbuf(struct Render *re, LampRen *lar);
void freeshadowbuf(struct LampRen *lar);

void threaded_makeshadowbufs(struct Render *re);

/**
 * Determines the shadow factor for a face and lamp. There is some
 * communication with global variables here.
 * \return The shadow factors: 1.0 for no shadow, 0.0 for complete
 *         shadow.
 * \param shb The shadowbuffer to find the shadow factor in.
 * \param inp The inproduct between viewvector and ?
 *
 */
float testshadowbuf(struct Render *re, struct ShadBuf *shb, const float rco[3], const float dxco[3], const float dyco[3], float inp, float mat_bias);

/**
 * Determines the shadow factor for lamp <lar>, between <p1>
 * and <p2>. (Which CS?)
 */
float shadow_halo(LampRen *lar, const float p1[3], const float p2[3]);

/**
 * Irregular shadowbuffer
 */

struct MemArena;
struct APixstr;

void ISB_create(RenderPart *pa, struct APixstr *apixbuf);
void ISB_free(RenderPart *pa);
float ISB_getshadow(ShadeInput *shi, ShadBuf *shb);

/* data structures have to be accessible both in camview(x, y) as in lampview(x, y) */
/* since they're created per tile rendered, speed goes over memory requirements */


/* buffer samples, allocated in camera buffer and pointed to in lampbuffer nodes */
typedef struct ISBSample {
	float zco[3];			/* coordinate in lampview projection */
	short *shadfac;			/* initialized zero = full lighted */
	int obi;				/* object for face lookup */
	int facenr;				/* index in faces list */	
} ISBSample;

/* transparent version of buffer sample */
typedef struct ISBSampleA {
	float zco[3];				/* coordinate in lampview projection */
	short *shadfac;				/* NULL = full lighted */
	int obi;					/* object for face lookup */
	int facenr;					/* index in faces list */	
	struct ISBSampleA *next;	/* in end, we want the first items to align with ISBSample */
} ISBSampleA;

/* used for transparent storage only */
typedef struct ISBShadfacA {
	struct ISBShadfacA *next;
	int obi;
	int facenr;
	float shadfac;
} ISBShadfacA;

/* What needs to be stored to evaluate shadow, for each thread in ShadBuf */
typedef struct ISBData {
	short *shadfacs;				/* simple storage for solid only */
	ISBShadfacA **shadfaca;
	struct MemArena *memarena;
	int minx, miny, rectx, recty;	/* copy from part disprect */
} ISBData;

#endif /* __SHADBUF_H__ */

