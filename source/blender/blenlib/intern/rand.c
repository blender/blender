/**
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
#include "PIL_time.h"
#include "BLI_rand.h"

#ifdef WIN32
typedef unsigned __int64	r_uint64;
#else
typedef unsigned long long	r_uint64;
#endif

#define MULTIPLIER	0x5DEECE66D
#define ADDEND		0xB

#define LOWSEED		0x330E

static r_uint64 X= 0;

void BLI_srand(unsigned int seed) {
	X= (((r_uint64) seed)<<16) | LOWSEED;
}

int BLI_rand(void) {
	X= (MULTIPLIER*X + ADDEND)&0x0000FFFFFFFFFFFF;
	return (int) (X>>17);
}

double BLI_drand(void) {
	return (double) BLI_rand()/0x80000000;
}

float BLI_frand(void) {
	return (float) BLI_rand()/0x80000000;
}

void BLI_storerand(unsigned int loc_r[2]) {
	loc_r[0]= (unsigned int) (X>>32);
	loc_r[1]= (unsigned int) (X&0xFFFFFFFF);
}

void BLI_restorerand(unsigned int loc[2]) {
	X= ((r_uint64) loc[0])<<32;
	X|= loc[1];
}

void BLI_fillrand(void *addr, int len) {
	unsigned char *p= addr;
	unsigned int save[2];

	BLI_storerand(save);
	
	BLI_srand((unsigned int) (PIL_check_seconds_timer()*0x7FFFFFFF));
	while (len--) *p++= BLI_rand()&0xFF;
	BLI_restorerand(save);
}
