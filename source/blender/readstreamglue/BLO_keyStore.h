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

#ifndef BLO_KEYSTORE_H
#define BLO_KEYSTORE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char byte;

typedef struct UserStructType {
        char name[100];
        char email[100];
        char shopid[100];
        unsigned long reldate;
        int keytype;    /* 1 = Individual, 2 = Corporate, 3 = Unlimited */
        int keylevel;   /* key disclosure level, starts at 1 */
        int keyformat;  /* if we change the keyformat, up BLENKEYFORMAT */
} UserStruct;

	void
keyStoreConstructor(
	UserStruct *keyUserStruct,
	char *privHexKey,
	char *pubHexKey,
	byte *ByteChecks,
	char *HexPython);

	void
keyStoreDestructor(
	void);

	int
keyStoreGetPubKey(
	byte **PubKey);

	int
keyStoreGetPrivKey(
	byte **PrivKey);

	char *
keyStoreGetUserName(
	void);

	char *
keyStoreGetEmail(
	void);

#ifdef __cplusplus
}
#endif

#endif /* BLO_KEYSTORE_H */

