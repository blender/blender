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
 * Publisher only: get the public key from the .BPkeyfile
 */

#include <stdlib.h>

#include "BLO_keyStore.h"
#include "BLO_getPubKey.h"

	int
getPubKey(byte *dataStreamPubKey,
		  int dataStreamPubKeyLen,
		  byte **publisherPubKey,
		  int *publisherPubKeyLen)
{
	int err = 0;

	*publisherPubKeyLen = keyStoreGetPubKey(publisherPubKey);

	if (*publisherPubKeyLen == 0) {
		// we're a publisher without .BPkey
		*publisherPubKey = NULL;
		return 1;
	}

	if (dataStreamPubKeyLen != *publisherPubKeyLen) {
		// different keys
		*publisherPubKeyLen = 0;
		*publisherPubKey = NULL;
		return 2;
	}

	if (memcmp(dataStreamPubKey, *publisherPubKey, *publisherPubKeyLen)
		!= 0) {
		// different keys
		*publisherPubKeyLen = 0;
		*publisherPubKey = NULL;
		return 3;
	}

	return err;
}

