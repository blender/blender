/**
 * blenlib/DNA_effect_types.h (mar-2001 nzc)
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
#ifndef DNA_EFFECT_TYPES_H
#define DNA_EFFECT_TYPES_H

/* DENK ERAAN: NIEUWE EFFECTEN OOK IN DE WRITEFILE.C IVM DNA!!! */

#define PAF_MAXMULT		4

	/* paf->flag (bitje 0 vrij houden ivm compatibility) */
#define PAF_BSPLINE		2
#define PAF_STATIC		4
#define PAF_FACE		8

	/* eff->type */
#define EFF_BUILD		0
#define EFF_PARTICLE	1
#define EFF_WAVE		2

	/* eff->flag */
#define EFF_SELECT		1
#define EFF_CYCLIC		2

	/* paf->stype */
#define PAF_NORMAL		0
#define PAF_VECT		1

	/* paf->texmap */
#define PAF_TEXINT		0
#define PAF_TEXRGB		1
#define PAF_TEXGRAD		2

	/* wav->flag */
#define WAV_X			2
#define WAV_Y			4
#define WAV_CYCL		8


typedef struct Effect {
	struct Effect *next, *prev;
	short type, flag, buttype, rt;
	
} Effect;

typedef struct BuildEff {
	struct BuildEff *next, *prev;
	short type, flag, buttype, rt;
	
	float len, sfra;
	
} BuildEff;

#
#
typedef struct Particle {
	float co[3], no[3];
	float time, lifetime;
	short mat_nr, rt;
} Particle;


typedef struct PartEff {
	struct PartEff *next, *prev;
	short type, flag, buttype, stype;
	
	float sta, end, lifetime;
	int totpart, totkey, seed;
	
	float normfac, obfac, randfac, texfac, randlife;
	float force[3];
	float damp;
	
	float nabla, vectsize, defvec[3];
	
	float mult[4], life[4];
	short child[4], mat[4];
	short texmap, curmult;
	short staticstep, pad;
	
	Particle *keys;
	
} PartEff;


typedef struct WaveEff {
	struct WaveEff *next, *prev;
	short type, flag, buttype, stype;
	
	float startx, starty, height, width;
	float narrow, speed, minfac, damp;
	
	float timeoffs, lifetime;
	
} WaveEff;

#endif

