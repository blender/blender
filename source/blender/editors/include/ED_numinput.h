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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_numinput.h
 *  \ingroup editors
 */

#ifndef __ED_NUMINPUT_H__
#define __ED_NUMINPUT_H__


typedef struct NumInput {
	short  idx;
	short  idx_max;
	short  flag;        /* Different flags to indicate different behaviors                                */
	char   inv[3];      /* If the value is inverted or not                                                */
	float  val[3];      /* Direct value of the input                                                      */
	int    ctrl[3];     /* Control to indicate what to do with the numbers that are typed                 */
	float  increment;
} NumInput;

/* NUMINPUT FLAGS */
#define NUM_NULL_ONE		2
#define NUM_NO_NEGATIVE		4
#define	NUM_NO_ZERO			8
#define NUM_NO_FRACTION		16
#define	NUM_AFFECT_ALL		32

/*********************** NumInput ********************************/

void initNumInput(NumInput *n);
void outputNumInput(NumInput *n, char *str);
short hasNumInput(NumInput *n);
void applyNumInput(NumInput *n, float *vec);
char handleNumInput(NumInput *n, struct wmEvent *event);

#define NUM_MODAL_INCREMENT_UP   18
#define NUM_MODAL_INCREMENT_DOWN 19

#endif
