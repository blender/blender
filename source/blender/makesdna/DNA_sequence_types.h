/**
 * blenlib/DNA_sequence_types.h (mar-2001 nzc)
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
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef DNA_SEQUENCE_TYPES_H
#define DNA_SEQUENCE_TYPES_H

#include "DNA_listBase.h"

struct Ipo;
struct Scene;

typedef struct StripElem {
	char name[40];
	struct ImBuf *ibuf;
	struct StripElem *se1, *se2, *se3;
	short ok, nr;
	int pad;
		
} StripElem;

typedef struct Strip {
	struct Strip *next, *prev;
	short rt, len, us, done;
	StripElem *stripdata;
	char dir[80];
	short orx, ory;
	int pad;
		
} Strip;


typedef struct PluginSeq {
	char name[80];
	void *handle;
	
	char *pname;
	
	int vars, version;

	void *varstr;
	float *cfra;
	
	float data[32];

	void (*doit)(void);

	void (*callback)(void);
} PluginSeq;


/* LET OP: eerste stuk identiek aan ID (ivm ipo's) */

typedef struct Sequence {

	struct Sequence *next, *prev, *newseq;
	void *lib;
	char name[24];
	
	short flag, type;
	int len;
	int start, startofs, endofs;
	int startstill, endstill;
	int machine, depth;
	int startdisp, enddisp;
	float mul, handsize;
	int sfra;
	
	Strip *strip;
	StripElem *curelem;
	
	struct Ipo *ipo;
	struct Scene *scene;
	struct anim *anim;
	float facf0, facf1;
	
	PluginSeq *plugin;

	/* pointers voor effecten: */
	struct Sequence *seq1, *seq2, *seq3;
	
	/* meta */
	ListBase seqbase;
	
} Sequence;


#
#
typedef struct MetaStack {
	struct MetaStack *next, *prev;
	ListBase *oldbasep;
	Sequence *parseq;
} MetaStack;

typedef struct Editing {
	ListBase *seqbasep;
	ListBase seqbase;
	ListBase metastack;
	short flag, rt;
	int pad;
} Editing;

/* ***************** SEQUENCE ****************** */

/* seq->flag */
#define SEQ_LEFTSEL		2
#define SEQ_RIGHTSEL	4
#define SEQ_OVERLAP		8
#define SEQ_FILTERY		16

/* seq->type LET OP BITJE 3!!! */
#define SEQ_IMAGE		0
#define SEQ_META		1
#define SEQ_SCENE		2
#define SEQ_MOVIE		3

#define SEQ_EFFECT		8
#define SEQ_CROSS		8
#define SEQ_ADD			9
#define SEQ_SUB			10
#define SEQ_ALPHAOVER	11
#define SEQ_ALPHAUNDER	12
#define SEQ_GAMCROSS	13
#define SEQ_MUL			14
#define SEQ_OVERDROP	15
#define SEQ_PLUGIN		24

#endif

