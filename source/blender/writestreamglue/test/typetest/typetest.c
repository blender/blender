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
 * A little test to see how our type defines behave. 
 */

#include "../../../readstreamglue/BLO_sys_types.h"
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
	int verbose       = 0;
	int error_status  = 0;
	int char_size     = 0;
	int short_size     = 0;
	int int_size     = 0;
	int long_size     = 0;
	
   	switch (argc) {
	case 2:		
		verbose = atoi(argv[1]);
		if (verbose < 0) verbose = 0;
		break;		
	case 1:
	default:
		verbose = 0;
	}

	/* ----------------------------------------------------------------- */
	if (verbose > 0) {
		printf("*** Type define size test\n|\n");
	}
	/* Check if these exist, and show their sizes. */
	

	char_size     = sizeof(uint8_t);
	short_size    = sizeof(uint16_t);
	int_size      = sizeof(uint32_t);
	long_size     = sizeof(uint64_t);

	if (verbose > 1) {
		printf("|- uint8_t  : \t%4d, expected 1.\n", char_size);
		printf("|- uint16_t : \t%4d, expected 2.\n", short_size);
		printf("|- uint32_t : \t%4d, expected 4.\n", int_size);
		printf("|- uint64_t : \t%4d, expected 8.\n\n", long_size);
	}

	if ((char_size != 1)
		|| (short_size != 2)
		|| (int_size != 4)
		|| (long_size != 8)
		) {
		error_status = 1;
	}

	if (verbose > 0) {
		if (error_status) {
			printf("|-- Size mismatch detected !!!\n|\n");
		} else {
			printf("|-- Sizes are correct.\n");
		}
		printf("|\n*** End of type define size test\n");
	}
	
	exit(error_status);
}
