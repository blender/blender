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
 * make all key elements available through functions
 */

#include <stdlib.h>
#include <assert.h>

#include "BLO_keyStore.h"
#include "BLO_keyStorePrivate.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// our ugly but private global pointer
static struct keyStoreStruct *keyStore = NULL;

	void
keyStoreConstructor(
	UserStruct *keyUserStruct,
	char *privHexKey,
	char *pubHexKey,
	byte *ByteChecks,
	char *HexPython)
{
	assert(keyStore == NULL);
	keyStore = malloc(sizeof(struct keyStoreStruct));
	assert(keyStore);
	// TODO check for malloc errors

	keyStore->keyUserStruct = *keyUserStruct;

	keyStore->privKey = DeHexify(privHexKey);
	keyStore->privKeyLen = strlen(privHexKey) / 2;

	keyStore->pubKey = DeHexify(pubHexKey);
	keyStore->pubKeyLen = strlen(pubHexKey) / 2;

	memcpy(keyStore->ByteChecks, ByteChecks, 1000);

	keyStore->PythonCode = DeHexify(HexPython);
	keyStore->PythonCodeLen = strlen(HexPython) / 2;
}

	void
keyStoreDestructor(
	void)
{
	assert(keyStore);
	if (!keyStore) {
		return;
	}
	free(keyStore->privKey);
	free(keyStore->pubKey);
	free(keyStore->PythonCode);
	free(keyStore);
	keyStore = NULL;
}

	int
keyStoreGetPubKey(
	byte **PubKey)
{
	if (!keyStore) {
		*PubKey = NULL;
		return 0;
	}
	*PubKey = keyStore->pubKey;
	return(keyStore->pubKeyLen);
}

	int
keyStoreGetPrivKey(
	byte **PrivKey)
{
	if (!keyStore) {
		*PrivKey = NULL;
		return 0;
	}
	*PrivKey = keyStore->privKey;
	return(keyStore->privKeyLen);
}

	char *
keyStoreGetUserName(
	void)
{
	if (!keyStore) {
		return NULL;
	}
	return(keyStore->keyUserStruct.name);
}

	char *
keyStoreGetEmail(
	void)
{
	if (!keyStore) {
		return NULL;
	}
	return(keyStore->keyUserStruct.email);
}

