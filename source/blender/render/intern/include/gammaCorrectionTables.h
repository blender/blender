/*
 * gammacorrectiontables.h
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

#ifndef GAMMACORRECTIONTABLES_H
#define GAMMACORRECTIONTABLES_H



/* Default gamma. For most CRTs, gamma ranges from 2.2 to 2.5 (Foley), so    */
/* 2.35 seems appropriate enough. Experience teaches a different number      */
/* though. Old blender: 2.0. It  might be nice to make this a slider         */
#define RE_DEFAULT_GAMMA 2.0
/* This 400 is sort of based on the number of intensity levels needed for    */
/* the typical dynamic range of a medium, in this case CRTs. (Foley)         */
/* (Actually, it says the number should be between 400 and 535.)             */
#define RE_GAMMA_TABLE_SIZE 400

/**
 * Initialise the gamma lookup tables
 */
void makeGammaTables(float gamma);

/**
 * Returns true if the table is initialised, false otherwise
 */
int gammaTableIsInitialised(void);

/**
 * Apply gamma correction on col
 */
float gammaCorrect(float col);

/**
 * Apply inverse gamma correction on col
 */
float invGammaCorrect(float col);

/**
 * Tell whether or not to do gamma.
 */
int doGamma(void);

/**
 * Set/unset performing gamma corrections.
 */
void setDoGamma(int);

#endif
