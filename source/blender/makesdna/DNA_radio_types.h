/**
 * radio_types.h    dec 2000 Nzc
 *
 * All type defs for the Blender core.
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * 
 */

#ifndef DNA_RADIO_TYPES_H
#define DNA_RADIO_TYPES_H

typedef struct Radio {
	short hemires, maxiter;
	short drawtype, flag;			/* bit 0 and 1: show limits */
	short subshootp, subshoote, nodelim, maxsublamp;
	short pama, pami, elma, elmi;	/* patch and elem limits */
	int maxnode;
	float convergence;
	float radfac, gamma;		/* for display */
	
} Radio;

#endif

