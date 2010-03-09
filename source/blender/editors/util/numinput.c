/**
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
 * Contributor(s): Jonathan Smith
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>			/* fabs */
#include <stdio.h>			/* for sprintf		*/

#include "BKE_global.h"		/* for G			*/
#include "BKE_utildefines.h"	/* ABS */

#include "WM_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_numinput.h"

/* ************************** Functions *************************** */

/* ************************** NUMINPUT **************************** */

void initNumInput(NumInput *n)
{
	n->flag		=
	n->idx		=
	n->idx_max	=
	n->inv[0]   =
	n->inv[1]   =
	n->inv[2]   =
	n->ctrl[0]	= 
	n->ctrl[1]	= 
	n->ctrl[2]	= 0;

	n->val[0]		= 
	n->val[1]	= 
	n->val[2]	= 0.0f;
}

void outputNumInput(NumInput *n, char *str)
{
	char cur;
	char inv[] = "1/";
	short i, j;

	for (j=0; j<=n->idx_max; j++) {
		/* if AFFECTALL and no number typed and cursor not on number, use first number */
		if (n->flag & NUM_AFFECT_ALL && n->idx != j && n->ctrl[j] == 0)
			i = 0;
		else
			i = j;

		if (n->idx != i)
			cur = ' ';
		else
			cur = '|';

		if (n->inv[i])
			inv[0] = '1';
		else
			inv[0] = 0;

		if( n->val[i] > 1e10 || n->val[i] < -1e10 )
			sprintf(&str[j*20], "%s%.4e%c", inv, n->val[i], cur);
		else
			switch (n->ctrl[i]) {
			case 0:
				sprintf(&str[j*20], "%sNONE%c", inv, cur);
				break;
			case 1:
			case -1:
				sprintf(&str[j*20], "%s%.0f%c", inv, n->val[i], cur);
				break;
			case 10:
			case -10:
				sprintf(&str[j*20], "%s%.f.%c", inv, n->val[i], cur);
				break;
			case 100:
			case -100:
				sprintf(&str[j*20], "%s%.1f%c", inv, n->val[i], cur);
				break;
			case 1000:
			case -1000:
				sprintf(&str[j*20], "%s%.2f%c", inv, n->val[i], cur);
				break;
			case 10000:
			case -10000:
				sprintf(&str[j*20], "%s%.3f%c", inv, n->val[i], cur);
				break;
			default:
				sprintf(&str[j*20], "%s%.4e%c", inv, n->val[i], cur);
			}
	}
}

short hasNumInput(NumInput *n)
{
	short i;

	for (i=0; i<=n->idx_max; i++) {
		if (n->ctrl[i])
			return 1;
	}

	return 0;
}

void applyNumInput(NumInput *n, float *vec)
{
	short i, j;

	if (hasNumInput(n)) {
		for (j=0; j<=n->idx_max; j++) {
			/* if AFFECTALL and no number typed and cursor not on number, use first number */
			if (n->flag & NUM_AFFECT_ALL && n->idx != j && n->ctrl[j] == 0)
				i = 0;
			else
				i = j;

			if (n->ctrl[i] == 0 && n->flag & NUM_NULL_ONE) {
				vec[j] = 1.0f;
			}
			else if (n->val[i] == 0.0f && n->flag & NUM_NO_ZERO) {
				vec[j] = 0.0001f;
			}
			else {
				if (n->inv[i])
				{
					vec[j] = 1.0f / n->val[i];
				}
				else
				{
					vec[j] = n->val[i];
				}
			}
		}
	}
}

char handleNumInput(NumInput *n, wmEvent *event)
{
	float Val = 0;
	short idx = n->idx, idx_max = n->idx_max;

	if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
		case NUM_MODAL_INCREMENT_UP:
			if (!n->ctrl[idx])
				n->ctrl[idx] = 1;

	        n->val[idx] += n->increment;
			break;
		case NUM_MODAL_INCREMENT_DOWN:
			if (!n->ctrl[idx])
				n->ctrl[idx] = 1;

	        n->val[idx] -= n->increment;
			break;
		default:
			return 0;
		}
	} else {
		switch (event->type) {
		case BACKSPACEKEY:
			if (n->ctrl[idx] == 0) {
				n->val[0]		=
					n->val[1]	=
					n->val[2]	= 0.0f;
				n->ctrl[0]		=
					n->ctrl[1]	=
					n->ctrl[2]	= 0;
				n->inv[0]		=
					n->inv[1]	=
					n->inv[2]	= 0;
			}
			else {
				n->val[idx] = 0.0f;
				n->ctrl[idx] = 0;
				n->inv[idx] = 0;
			}
			break;
		case PERIODKEY:
		case PADPERIOD:
			if (n->flag & NUM_NO_FRACTION)
				return 0;

			switch (n->ctrl[idx])
			{
			case 0:
			case 1:
				n->ctrl[idx] = 10;
				break;
			case -1:
				n->ctrl[idx] = -10;
			}
			break;
		case PADMINUS:
			if(event->alt)
				break;
		case MINUSKEY:
			if (n->flag & NUM_NO_NEGATIVE)
				break;

			if (n->ctrl[idx]) {
				n->ctrl[idx] *= -1;
				n->val[idx] *= -1;
			}
			else
				n->ctrl[idx] = -1;
			break;
		case PADSLASHKEY:
		case SLASHKEY:
			if (n->flag & NUM_NO_FRACTION)
				return 0;

			n->inv[idx] = !n->inv[idx];
			break;
		case TABKEY:
			if (idx_max == 0)
				return 0;

			idx++;
			if (idx > idx_max)
				idx = 0;
			n->idx = idx;
			break;
		case PAD9:
		case NINEKEY:
			Val += 1.0f;
		case PAD8:
		case EIGHTKEY:
			Val += 1.0f;
		case PAD7:
		case SEVENKEY:
			Val += 1.0f;
		case PAD6:
		case SIXKEY:
			Val += 1.0f;
		case PAD5:
		case FIVEKEY:
			Val += 1.0f;
		case PAD4:
		case FOURKEY:
			Val += 1.0f;
		case PAD3:
		case THREEKEY:
			Val += 1.0f;
		case PAD2:
		case TWOKEY:
			Val += 1.0f;
		case PAD1:
		case ONEKEY:
			Val += 1.0f;
		case PAD0:
		case ZEROKEY:
			if (!n->ctrl[idx])
				n->ctrl[idx] = 1;

			if (fabs(n->val[idx]) > 9999999.0f);
			else if (n->ctrl[idx] == 1) {
				n->val[idx] *= 10;
				n->val[idx] += Val;
			}
			else if (n->ctrl[idx] == -1) {
				n->val[idx] *= 10;
				n->val[idx] -= Val;
			}
			else {
				/* float resolution breaks when over six digits after comma */
				if( ABS(n->ctrl[idx]) < 10000000) {
					n->val[idx] += Val / (float)n->ctrl[idx];
					n->ctrl[idx] *= 10;
				}
			}
			break;
		default:
			return 0;
		}
	}
	
	printf("%f\n", n->val[idx]);

	/* REDRAW SINCE NUMBERS HAVE CHANGED */
	return 1;
}
