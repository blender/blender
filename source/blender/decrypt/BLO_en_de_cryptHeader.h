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
 * 
 */

#ifndef BLO_EN_DE_CRYPT_H
#define BLO_EN_DE_CRYPT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "BLO_sys_types.h"

#ifdef FREE_WINDOWS
typedef int int32_t;
#endif
	
#define EN_DE_CRYPTHEADERSTRUCTSIZE sizeof(struct BLO_en_de_cryptHeaderStruct)

// Tests showed: pubKeyLen 64, cryptedKeyLen 64 bytes
// So we pick 2*64 bytes + 2 bytes dummy tail for now :
#define MAXPUBKEYLEN 130
#define MAXCRYPTKEYLEN 130

struct BLO_en_de_cryptHeaderStruct {
	uint8_t  magic;				// poor mans header recognize check
	uint32_t length;			// how much crypted data is there
	uint8_t  pubKey[MAXPUBKEYLEN];
	uint32_t pubKeyLen;			// the actual pubKey length
	uint8_t  cryptedKey[MAXCRYPTKEYLEN];
	int32_t  cryptedKeyLen;		// the actual cryptedKey length (NOTE: signed)
	uint32_t datacrc;			// crypted data checksum
	uint32_t headercrc;			// header minus crc itself checksum
};

#ifdef __cplusplus
}
#endif

#endif /* BLO_EN_DE_CRYPT_H */

