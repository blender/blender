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
 * decrypt STUB
 */

#include <stdio.h>

#include "GEN_messaging.h"

#include "BLO_readStreamErrors.h"
#include "BLO_decrypt.h"

	BLO_decryptStructHandle
BLO_decrypt_begin(
	void *endControl)
{
#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"Error BLO_decrypt_begin is a stub\n");
#endif
	return (NULL);
}

	int
BLO_decrypt_process(
	BLO_decryptStructHandle BLO_decryptHandle,
	unsigned char *data,
	unsigned int dataIn)
{
	int err = 0;
	BLO_decryptHandle = NULL;
	data = NULL;
	dataIn = 0;
#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"Error BLO_decrypt_process is a stub\n");
#endif
	err = BRS_SETFUNCTION(BRS_DECRYPT) |
		  BRS_SETGENERR(BRS_STUB);
	return (err);
}

	int
BLO_decrypt_end(
	BLO_decryptStructHandle BLO_decryptHandle)
{
	int err = 0;
	BLO_decryptHandle = NULL;
#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"Error BLO_decrypt_end is a stub\n");
#endif	
	err = BRS_SETFUNCTION(BRS_DECRYPT) |
		  BRS_SETGENERR(BRS_STUB);
	return (err);
}

